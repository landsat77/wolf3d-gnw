/*
 * Pre-rendered music of Tyrian (the Loudness / AdLib songs of music.mus).
 *
 * Synthesizing the AdLib music (OPL emulator) costs too much on the Game &
 * Watch: tools/wf_pack.py renders the songs once on the PC (harness, option
 * -R, wf_render.c) and stores them in the pack as "musicNN.mus" files. The
 * console plays them back in place from the pack (memory mapped flash),
 * looping from their loop start, or once for the songs that end
 * (wf_loudness.c).
 *
 * The playback code comes from the Prince of Persia port (gnw_music.c).
 *
 * .MUS format (little endian):
 *   0x00 char magic[4]   "GWMU"
 *   0x04 u16  version    1
 *   0x06 u16  codec      0 = signed 16 bit PCM, 1 = IMA ADPCM; bit 15: the
 *                        piece ends (it does not loop)
 *   0x08 u32  rate       sample rate: 22050 (the output rate of the game) or
 *                        11025 (half the size, played back with a 16 tap
 *                        interpolation filter; other rates: linear)
 *   0x0C u32  samples    number of mono samples
 *   0x10 u32  loop_start version 2 only: the piece goes on there after its
 *                        end (version 1: at 0)
 *   0x10/0x14 data
 *               PCM:   int16 samples[samples]
 *               ADPCM: blocks of GWMU_ADPCM_BLOCK samples:
 *                        int16 predictor, u8 step index, u8 0,
 *                        then 4 bit codes (low nibble first)
 */
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>

#include "wf_platform.h"
#include "wf_music.h"

#define GWMU_HEADER_SIZE   16
#define GWMU_ADPCM_BLOCK   1024
#define GWMU_ADPCM_BLOCK_BYTES (4 + GWMU_ADPCM_BLOCK / 2)

enum { CODEC_PCM16 = 0, CODEC_ADPCM = 1 };

struct wf_mus {
    const uint8_t *data; /* after the header */
    uint32_t data_size;
    uint32_t samples;
    uint32_t rate;
    int codec;
    uint32_t pos;        /* next sample to decode */
    /* playback at the output rate: linear interpolation between s0 and s1,
     * the source samples around the current position */
    bool primed;
    uint32_t step;       /* source samples per output frame, 16.16 */
    uint32_t frac;       /* position between s0 and s1, 16.16 */
    uint32_t src_index;  /* index of s0 */
    int32_t s0, s1;
    /* exact 2x upsampling: source samples s[i-7] .. s[i+8] */
    bool halfband;
    bool odd;            /* next output frame is between s[i] and s[i+1] */
    int16_t ring[16];
    /* ADPCM decoder state */
    int32_t predictor;
    int step_index;
    uint32_t loop_start;
    bool ends;
};


static const int16_t ima_step[89] = {
    7, 8, 9, 10, 11, 12, 13, 14, 16, 17, 19, 21, 23, 25, 28, 31, 34, 37, 41, 45,
    50, 55, 60, 66, 73, 80, 88, 97, 107, 118, 130, 143, 157, 173, 190, 209, 230,
    253, 279, 307, 337, 371, 408, 449, 494, 544, 598, 658, 724, 796, 876, 963,
    1060, 1166, 1282, 1411, 1552, 1707, 1878, 2066, 2272, 2499, 2749, 3024, 3327,
    3660, 4026, 4428, 4871, 5358, 5894, 6484, 7132, 7845, 8630, 9493, 10442,
    11487, 12635, 13899, 15289, 16818, 18500, 20350, 22385, 24623, 27086, 29794,
    32767
};
static const int8_t ima_index[8] = { -1, -1, -1, -1, 2, 4, 6, 8 };

static uint32_t rd32(const uint8_t *p) { return p[0] | (p[1] << 8) | (p[2] << 16) | ((uint32_t)p[3] << 24); }
static uint16_t rd16(const uint8_t *p) { return (uint16_t)(p[0] | (p[1] << 8)); }

wf_mus_t *wf_mus_open(const unsigned char *data, int len)
{
    if (!data || len < GWMU_HEADER_SIZE + 4 || memcmp(data, "GWMU", 4) != 0)
        return NULL;
    int version = rd16(data + 4);
    if (version != 1 && version != 2)
        return NULL;
    uint32_t header = version == 2 ? GWMU_HEADER_SIZE + 4 : GWMU_HEADER_SIZE;
    int codec = rd16(data + 6);
    bool ends = (codec & 0x8000) != 0;
    codec &= 0xff;
    uint32_t samples = rd32(data + 12);
    uint32_t loop_start = version == 2 ? rd32(data + 16) : 0;
    if (samples == 0 || loop_start >= samples)
        return NULL;
    uint32_t size = (uint32_t)len - header;
    if (codec == CODEC_PCM16) {
        if ((uint64_t)samples * 2 > size)
            return NULL;
    } else if (codec == CODEC_ADPCM) {
        uint32_t blocks = (samples + GWMU_ADPCM_BLOCK - 1) / GWMU_ADPCM_BLOCK;
        if ((uint64_t)blocks * GWMU_ADPCM_BLOCK_BYTES > (uint64_t)size + GWMU_ADPCM_BLOCK / 2)
            return NULL;
    } else {
        return NULL;
    }
    wf_mus_t *f = wf_malloc(sizeof(*f));
    if (!f)
        return NULL;
    memset(f, 0, sizeof(*f));
    f->data = data + header;
    f->data_size = size;
    f->samples = samples;
    f->loop_start = loop_start;
    f->rate = rd32(data + 8);
    f->codec = codec;
    f->ends = ends;
    return f;
}

bool wf_mus_ends(const wf_mus_t *f)
{
    return f && f->ends;
}

uint32_t wf_mus_length(const wf_mus_t *f, int out_rate)
{
    if (!f || f->rate == 0)
        return 0;
    return (uint32_t)((uint64_t)f->samples * (uint32_t)out_rate / f->rate);
}

void wf_mus_close(wf_mus_t *f)
{
    wf_free(f);
}

static void seek_decoder(wf_mus_t *f, uint32_t target);

int wf_mus_rewind(wf_mus_t *f)
{
    if (!f)
        return 0;
    seek_decoder(f, 0);
    f->primed = false;
    return 1;
}

static inline int16_t next_sample(wf_mus_t *f)
{
    uint32_t pos = f->pos++;
    if (f->codec == CODEC_PCM16) {
        const uint8_t *p = f->data + pos * 2;
        return (int16_t)(p[0] | (p[1] << 8));
    }
    uint32_t in_block = pos % GWMU_ADPCM_BLOCK;
    const uint8_t *block = f->data + (pos / GWMU_ADPCM_BLOCK) * GWMU_ADPCM_BLOCK_BYTES;
    if (in_block == 0) {
        f->predictor = (int16_t)rd16(block);
        f->step_index = block[2] > 88 ? 88 : block[2];
    }
    uint8_t byte = block[4 + in_block / 2];
    int code = (in_block & 1) ? (byte >> 4) : (byte & 15);
    int step = ima_step[f->step_index];
    int diff = step >> 3;
    if (code & 4) diff += step;
    if (code & 2) diff += step >> 1;
    if (code & 1) diff += step >> 2;
    int32_t p = (code & 8) ? f->predictor - diff : f->predictor + diff;
    if (p > 32767) p = 32767;
    if (p < -32768) p = -32768;
    f->predictor = p;
    int idx = f->step_index + ima_index[code & 7];
    f->step_index = idx < 0 ? 0 : idx > 88 ? 88 : idx;
    return (int16_t)p;
}

/* the decoder at sample `target` (ADPCM: from the start of its block) */
static void seek_decoder(wf_mus_t *f, uint32_t target)
{
    if (f->codec == CODEC_PCM16) {
        f->pos = target;
        return;
    }
    f->pos = target - target % GWMU_ADPCM_BLOCK;
    while (f->pos < target)
        (void)next_sample(f);
}

/* the next sample of the endless piece: after the end, the loop start */
static inline int32_t fetch(wf_mus_t *f)
{
    if (f->pos >= f->samples)
        seek_decoder(f, f->loop_start);
    return next_sample(f);
}

/* 2x interpolation filter (windowed sinc, Blackman, 16 taps, Q15): the
 * sample halfway between s[i] and s[i+1] from s[i-7] .. s[i+8]. Measured on
 * the music: 47 dB in band, images 43 dB down (linear interpolation: 28 dB
 * and 27 dB). */
static const int16_t halfband_q15[16] = {
    -7, 49, -183, 520, -1242, 2696, -5955, 20506, 20506, -5955, 2696, -1242, 520, -183, 49, -7
};

/* Adds the next `frames` frames at `out_rate` to `acc` (mono, 32 bit),
 * scaled by `gain` (256 = unity; 0: only moves forward). The piece loops
 * from its loop start after its end. At twice the rate of the piece a 16 tap
 * interpolation filter makes the samples in between; other rates: linear. */
int wf_mus_mix(wf_mus_t *f, int32_t *acc, int frames, int gain, int out_rate)
{
    if (!f)
        return 0;
    if (out_rate <= 0)
        out_rate = 22050;
    if (!f->primed) {
        f->halfband = (f->rate * 2 == (uint32_t)out_rate);
        f->step = (uint32_t)(((uint64_t)f->rate << 16) / (uint32_t)out_rate);
        f->frac = 0;
        f->src_index = f->pos;
        if (f->halfband) {
            memset(f->ring, 0, sizeof(f->ring)); /* s[-7] .. s[-1] = 0 */
            for (int k = 7; k < 16; k++)
                f->ring[k] = (int16_t)fetch(f);
            f->odd = false;
        } else {
            f->s0 = fetch(f);
            f->s1 = fetch(f);
        }
        f->primed = true;
    }
    int n = 0;
    if (f->halfband) {
        while (n < frames) {
            if (gain) {
                int32_t s;
                if (!f->odd) {
                    s = f->ring[7];
                } else {
                    int32_t a = 0;
                    for (int k = 0; k < 16; k++)
                        a += halfband_q15[k] * f->ring[k];
                    s = (a + 0x4000) >> 15;
                }
                acc[n] += (s * gain) >> 8;
            }
            if (f->odd) {
                memmove(&f->ring[0], &f->ring[1], 15 * sizeof(int16_t));
                f->ring[15] = (int16_t)fetch(f);
                f->src_index++;
            }
            f->odd = !f->odd;
            n++;
        }
        return n;
    }
    while (n < frames) {
        if (gain) {
            int32_t s = f->s0 + (int32_t)(((int64_t)(f->s1 - f->s0) * (int32_t)f->frac) >> 16);
            acc[n] += (s * gain) >> 8;
        }
        f->frac += f->step;
        while (f->frac >= 0x10000) {
            f->frac -= 0x10000;
            f->s0 = f->s1;
            f->s1 = fetch(f);
            f->src_index++;
        }
        n++;
    }
    return n;
}

/* source sample being played (wraps at the loop) */
uint32_t wf_mus_tell(const wf_mus_t *f)
{
    if (!f)
        return 0;
    uint32_t i = f->src_index;
    if (i >= f->samples)
        i = f->loop_start + (i - f->samples) % (f->samples - f->loop_start);
    return i;
}

/* continue from source sample `pos` */
void wf_mus_seek(wf_mus_t *f, uint32_t pos)
{
    if (!f)
        return;
    if (pos >= f->samples)
        pos = 0;
    seek_decoder(f, pos);
    f->primed = false;
}

uint32_t wf_mus_rate(const wf_mus_t *f)
{
    return f ? f->rate : 0;
}
