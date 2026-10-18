/*
 * wf_render: renders the AdLib sound of Wolfenstein 3D (the IMF songs and
 * the AdLib sound effects of AUDIOT) to samples, for tools/wf_pack.py.
 *
 *   wf_render AUDIOHED.WL6 AUDIOT.WL6 <out dir> [--sounds N] [--rate 11025|22050]
 *             [--pcm] [--gain G]
 *
 * The Game & Watch is too small to run an OPL emulator while the game
 * plays: its pack holds the songs and the sound effects as samples.
 *
 *  - <out dir>/musicNN.mus: song NN (chunk STARTMUSIC + NN), played like
 *    Wolf4SDL plays it (the IMF sequencer at 700 Hz). The songs loop: the
 *    file is the first pass followed by the first TAIL_SECONDS of the
 *    second one (the notes held at the end of the song still sound there),
 *    and its loop starts TAIL_SECONDS after the start: seamless.
 *  - <out dir>/adlibNN.mus: AdLib sound effect NN (chunk STARTADLIBSOUNDS +
 *    NN): one byte of the sound per 1/140 s on channel 0, then its release.
 *    Flagged as ending (codec bit 15).
 *
 * GWMU format: src/port/wf_music.c. The OPL is rendered at 44100 Hz (63
 * samples per 700 Hz tick: exact timing), low-pass filtered and decimated.
 * The OPL emulator (opl.c, DOSBox team, LGPL 2.1) has a fixed noise
 * generator: the files are the same on every computer.
 */
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "opl.h"

#define RENDER_RATE   44100
#define TICK_SAMPLES  63          /* 44100 / 700 */
#define MAX_SECONDS   900
#define TAIL_SECONDS  2
#define RELEASE_MAX_MS 1500

static double gain = 2.0;  /* the OPL output is quiet next to the digitized sounds */

/* ---- files ---- */
static uint8_t *load(const char *path, long *size)
{
    FILE *f = fopen(path, "rb");
    if (!f) {
        fprintf(stderr, "wf_render: cannot open %s\n", path);
        exit(1);
    }
    fseek(f, 0, SEEK_END);
    *size = ftell(f);
    fseek(f, 0, SEEK_SET);
    uint8_t *p = malloc((size_t)*size + 1);
    if (!p || fread(p, 1, (size_t)*size, f) != (size_t)*size) {
        fprintf(stderr, "wf_render: cannot read %s\n", path);
        exit(1);
    }
    fclose(f);
    return p;
}

static uint32_t rd32(const uint8_t *p) { return p[0] | (p[1] << 8) | (p[2] << 16) | ((uint32_t)p[3] << 24); }
static uint16_t rd16(const uint8_t *p) { return (uint16_t)(p[0] | (p[1] << 8)); }

/* ---- output buffer ---- */
typedef struct { uint8_t *p; size_t n, cap; } buf_t;

static void put(buf_t *b, uint8_t v)
{
    if (b->n == b->cap) {
        b->cap = b->cap ? b->cap * 2 : 65536;
        b->p = realloc(b->p, b->cap);
    }
    b->p[b->n++] = v;
}
static void put16(buf_t *b, uint32_t x) { put(b, x & 0xFF); put(b, (x >> 8) & 0xFF); }
static void put32(buf_t *b, uint32_t x) { put16(b, x & 0xFFFF); put16(b, x >> 16); }

/* ---- IMA ADPCM (wf_music.c) ---- */
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
#define ADPCM_BLOCK 1024

static void adpcm_encode(const int16_t *s, long n, buf_t *out)
{
    for (long b = 0; b < n; b += ADPCM_BLOCK) {
        long len = n - b < ADPCM_BLOCK ? n - b : ADPCM_BLOCK;
        const int16_t *blk = s + b;
        int pred = blk[0];
        int idx = 0;
        if (len > 1) {
            int d = blk[1] - blk[0];
            if (d < 0) d = -d;
            while (idx < 88 && ima_step[idx] < d) idx++;
        }
        put(out, (uint8_t)(pred & 0xFF));
        put(out, (uint8_t)((pred >> 8) & 0xFF));
        put(out, (uint8_t)idx);
        put(out, 0);
        uint8_t byte = 0;
        for (long i = 0; i < len; i++) {
            int step = ima_step[idx];
            int diff = blk[i] - pred;
            int code = 0;
            if (diff < 0) { code = 8; diff = -diff; }
            int dq = step >> 3;
            if (diff >= step) { code |= 4; diff -= step; dq += step; }
            step >>= 1;
            if (diff >= step) { code |= 2; diff -= step; dq += step; }
            step >>= 1;
            if (diff >= step) { code |= 1; dq += step; }
            pred = (code & 8) ? pred - dq : pred + dq;
            if (pred > 32767) pred = 32767;
            if (pred < -32768) pred = -32768;
            idx += ima_index[code & 7];
            if (idx < 0) idx = 0;
            if (idx > 88) idx = 88;
            if (i & 1)
                put(out, (uint8_t)(byte | (code << 4)));
            else
                byte = (uint8_t)code;
        }
        if (len & 1)
            put(out, byte);
    }
}

/* ---- decimation (44100 -> 22050 or 11025 Hz): windowed sinc low-pass,
 * delay compensated; also applies the gain ---- */
#define DECIM_TAPS 127
#define DECIM_DELAY ((DECIM_TAPS - 1) / 2)

static long decimate(const int16_t *in, long n, int factor, int16_t *out)
{
    double h[DECIM_TAPS];
    const double pi = 3.14159265358979323846;
    const double fc = 0.45 / factor;
    double sum = 0;
    for (int k = 0; k < DECIM_TAPS; k++) {
        double m = k - DECIM_DELAY;
        double sinc = m == 0 ? 2 * fc : sin(2 * pi * fc * m) / (pi * m);
        double w = 0.42 - 0.5 * cos(2 * pi * k / (DECIM_TAPS - 1)) + 0.08 * cos(4 * pi * k / (DECIM_TAPS - 1));
        h[k] = sinc * w;
        sum += h[k];
    }
    for (int k = 0; k < DECIM_TAPS; k++)
        h[k] = h[k] / sum * gain;
    long m = 0;
    for (long c = 0; c < n; c += factor, m++) {
        double acc = 0;
        for (int k = 0; k < DECIM_TAPS; k++) {
            long i = c + DECIM_DELAY - k;
            if (i >= 0 && i < n)
                acc += h[k] * in[i];
        }
        long v = lround(acc);
        out[m] = (int16_t)(v > 32767 ? 32767 : v < -32768 ? -32768 : v);
    }
    return m;
}

/* ---- PCM accumulation ---- */
typedef struct {
    int16_t *s;
    long n, cap;
} pcm_t;

static void render(pcm_t *p, long count)
{
    if (p->n + count > p->cap) {
        while (p->n + count > p->cap)
            p->cap = p->cap ? p->cap * 2 : RENDER_RATE * 60;
        p->s = realloc(p->s, (size_t)p->cap * sizeof(int16_t));
    }
    while (count > 0) {
        long c = count > 512 ? 512 : count;
        adlib_getsample(p->s + p->n, c);
        p->n += c;
        count -= c;
    }
}

static void opl_reset(void)
{
    adlib_init(RENDER_RATE);
    for (int i = 1; i < 0xf6; i++)
        adlib_write(i, 0);
    adlib_write(1, 0x20); /* WSE = 1 */
}

/* writes <dir>/<name> in the GWMU format */
static bool write_mus(const char *dir, const char *name, const pcm_t *pcm, long total,
                      long loop_start, bool ends, int rate, bool adpcm)
{
    int factor = RENDER_RATE / rate;
    int16_t *out = malloc((size_t)(total / factor + 2) * sizeof(int16_t));
    long out_n = decimate(pcm->s, total, factor, out);
    long out_loop = loop_start / factor;
    if (out_loop >= out_n)
        out_loop = 0;

    buf_t mus = { 0 };
    put(&mus, 'G'); put(&mus, 'W'); put(&mus, 'M'); put(&mus, 'U');
    put16(&mus, 2);
    put16(&mus, (adpcm ? 1 : 0) | (ends ? 0x8000 : 0));
    put32(&mus, (uint32_t)rate);
    put32(&mus, (uint32_t)out_n);
    put32(&mus, (uint32_t)out_loop);
    if (adpcm) {
        adpcm_encode(out, out_n, &mus);
    } else {
        for (long i = 0; i < out_n; i++)
            put16(&mus, (uint16_t)out[i]);
    }
    char path[1024];
    snprintf(path, sizeof(path), "%s/%s", dir, name);
    FILE *f = fopen(path, "wb");
    bool ok = f != NULL;
    if (f) {
        ok = fwrite(mus.p, 1, mus.n, f) == mus.n;
        fclose(f);
    }
    int peak = 0;
    for (long i = 0; i < out_n; i++) {
        int v = out[i] < 0 ? -out[i] : out[i];
        if (v > peak)
            peak = v;
    }
    printf("%s: %.2f s, loop at %.2f s, peak %d%s\n", name, out_n / (double)rate,
           out_loop / (double)rate, peak, ends ? ", ends" : "");
    free(out);
    free(mus.p);
    return ok;
}

/* ---- songs (SDL_IMFMusicPlayer of Wolf4SDL) ---- */
static bool render_song(const uint8_t *chunk, long chunk_len, int num, const char *dir, int rate, bool adpcm)
{
    if (chunk_len < 4)
        return false;
    const uint8_t *data = chunk;
    long len = rd16(chunk);
    if (len == 0) {
        len = chunk_len;
    } else {
        data += 2;
        if (len > chunk_len - 2)
            len = chunk_len - 2;
    }
    len &= ~3L;
    if (len <= 0)
        return false;

    opl_reset();
    pcm_t pcm = { 0 };
    long pos = 0, left = len;
    uint32_t time_count = 0, hack_time = 0;
    long pass_end = -1;           /* samples of the first pass */
    long stop_at = (long)RENDER_RATE * MAX_SECONDS;
    while (pcm.n < stop_at) {
        do {
            if (hack_time > time_count)
                break;
            hack_time = time_count + rd16(data + pos + 2);
            adlib_write(data[pos], data[pos + 1]);
            pos += 4;
            left -= 4;
        } while (left > 0);
        time_count++;
        if (left <= 0) {
            pos = 0;
            left = len;
            hack_time = 0;
            time_count = 0;
            if (pass_end < 0) {
                /* the song starts again after this tick */
                pass_end = pcm.n + TICK_SAMPLES;
                stop_at = pass_end + (long)RENDER_RATE * TAIL_SECONDS;
            }
        }
        render(&pcm, TICK_SAMPLES);
    }
    long total = pcm.n, loop_start = 0;
    if (pass_end > 0) {
        long tail = (long)RENDER_RATE * TAIL_SECONDS;
        if (tail > pass_end)
            tail = pass_end;
        total = pass_end + tail;
        loop_start = tail;
    }
    char name[32];
    snprintf(name, sizeof(name), "music%02d.mus", num);
    bool ok = write_mus(dir, name, &pcm, total, loop_start, false, rate, adpcm);
    free(pcm.s);
    return ok;
}

/* ---- AdLib sound effects (SDL_ALPlaySound of Wolf4SDL) ---- */
static void set_fx_inst(const uint8_t *inst)
{
    /* modulator cell 0, carrier cell 3 (channel 0) */
    adlib_write(0x20 + 0, inst[0]);  /* mChar */
    adlib_write(0x40 + 0, inst[2]);  /* mScale */
    adlib_write(0x60 + 0, inst[4]);  /* mAttack */
    adlib_write(0x80 + 0, inst[6]);  /* mSus */
    adlib_write(0xe0 + 0, inst[8]);  /* mWave */
    adlib_write(0x20 + 3, inst[1]);  /* cChar */
    adlib_write(0x40 + 3, inst[3]);  /* cScale */
    adlib_write(0x60 + 3, inst[5]);  /* cAttack */
    adlib_write(0x80 + 3, inst[7]);  /* cSus */
    adlib_write(0xe0 + 3, inst[9]);  /* cWave */
    adlib_write(0xc0, 0);            /* feedback / connection */
}

static bool render_sound(const uint8_t *chunk, long chunk_len, int num, const char *dir, int rate, bool adpcm)
{
    /* SoundCommon (u32 length, u16 priority), Instrument (16 bytes), block,
     * data[length] */
    if (chunk_len < 6 + 16 + 1)
        return false;
    uint32_t length = rd32(chunk);
    const uint8_t *inst = chunk + 6;
    uint8_t block = chunk[6 + 16];
    const uint8_t *data = chunk + 6 + 16 + 1;
    if ((long)length > chunk_len - (6 + 16 + 1))
        length = (uint32_t)(chunk_len - (6 + 16 + 1));
    if (length == 0 || !(inst[6] | inst[7]))
        return false;

    opl_reset();
    adlib_write(0xbd, 0);  /* alEffects */
    static const uint8_t zero[16];
    set_fx_inst(zero);

    adlib_write(0xb0, 0);  /* SDL_ALStopSound */
    uint8_t al_block = (uint8_t)(((block & 7) << 2) | 0x20);
    set_fx_inst(inst);

    pcm_t pcm = { 0 };
    for (uint32_t i = 0; i < length; i++) {
        if (data[i]) {
            adlib_write(0xa0, data[i]);
            adlib_write(0xb0, al_block);
        } else {
            adlib_write(0xb0, 0);
        }
        render(&pcm, 5 * TICK_SAMPLES);   /* 140 Hz */
    }
    adlib_write(0xb0, 0);
    /* the release, until silence */
    long end = pcm.n, limit = pcm.n + (long)RENDER_RATE * RELEASE_MAX_MS / 1000;
    long quiet = 0;
    while (pcm.n < limit && quiet < RENDER_RATE / 50) {
        long from = pcm.n;
        render(&pcm, TICK_SAMPLES);
        bool silent = true;
        for (long k = from; k < pcm.n; k++)
            if (pcm.s[k] > 8 || pcm.s[k] < -8)
                silent = false;
        quiet = silent ? quiet + TICK_SAMPLES : 0;
    }
    (void)end;
    char name[32];
    snprintf(name, sizeof(name), "adlib%02d.mus", num);
    bool ok = write_mus(dir, name, &pcm, pcm.n, 0, true, rate, adpcm);
    free(pcm.s);
    return ok;
}

int main(int argc, char **argv)
{
    const char *head = NULL, *audio = NULL, *dir = NULL;
    int sounds = 87, rate = 11025;
    bool adpcm = true;
    const char *only = NULL;
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--sounds") && i + 1 < argc)
            sounds = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--rate") && i + 1 < argc)
            rate = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--gain") && i + 1 < argc)
            gain = atof(argv[++i]);
        else if (!strcmp(argv[i], "--only") && i + 1 < argc)
            only = argv[++i];
        else if (!strcmp(argv[i], "--pcm"))
            adpcm = false;
        else if (!head)
            head = argv[i];
        else if (!audio)
            audio = argv[i];
        else if (!dir)
            dir = argv[i];
    }
    if (!dir || (rate != 11025 && rate != 22050)) {
        fprintf(stderr, "usage: wf_render AUDIOHED AUDIOT outdir [--sounds N] [--rate 11025|22050] [--pcm] [--gain G]\n");
        return 2;
    }
    long head_size, audio_size;
    uint8_t *h = load(head, &head_size);
    uint8_t *a = load(audio, &audio_size);
    int chunks = (int)(head_size / 4) - 1;
    int start_music = 3 * sounds;
    if (chunks <= start_music) {
        fprintf(stderr, "wf_render: %s has %d chunks, %d sounds?\n", head, chunks, sounds);
        return 1;
    }
    int done = 0, failed = 0;
    for (int c = 0; c < chunks; c++) {
        bool is_sound = c >= sounds && c < 2 * sounds;
        bool is_music = c >= start_music;
        if (!is_sound && !is_music)
            continue;
        if (only && strcmp(only, is_music ? "music" : "adlib"))
            continue;
        uint32_t pos = rd32(h + 4 * c), next = rd32(h + 4 * c + 4);
        if (next <= pos || next > (uint32_t)audio_size)
            continue;
        bool ok = is_music ? render_song(a + pos, next - pos, c - start_music, dir, rate, adpcm)
                           : render_sound(a + pos, next - pos, c - sounds, dir, rate, adpcm);
        if (ok)
            done++;
        else if (is_music)
            failed++;
    }
    printf("wf_render: %d files\n", done);
    free(h);
    free(a);
    return failed ? 1 : 0;
}
