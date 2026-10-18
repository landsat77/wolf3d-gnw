/*
 * Sound manager of Wolfenstein 3D on the Game & Watch: replaces id_sd.c of
 * Wolf4SDL (same interface, id_sd.h), without SDL_mixer and without an OPL
 * emulator.
 *
 *  - digitized sounds (VSWAP, 8 bit unsigned, 7042 Hz): mixed in place from
 *    the pack, on 8 channels like Wolf4SDL (0: the player's weapons, 1: the
 *    bosses' weapons, the others shared), the stereo position of a sound
 *    sets its volume (mono console);
 *  - AdLib sound effects and music: pre-rendered on the PC by tools/wf_pack.py
 *    (wf_render: the game's sequencer + an OPL emulator) as "adlibNN.mus"
 *    and "musicNN.mus" in the pack, played by wf_music.c. One AdLib sound
 *    at a time, by priority, as on the AdLib card;
 *  - no PC speaker.
 *
 * The mixing runs when the platform asks for samples
 * (wf_core_audio_render(), from its services): no thread.
 */
#include "wl_def.h"
#include <SDL_mixer.h>

#include "wf_music.h"

#define ORIGSAMPLERATE 7042

/* Global variables (id_sd.h) */
boolean AdLibPresent, SoundBlasterPresent, SoundPositioned;
byte    SoundMode, MusicMode, DigiMode;
word    NumDigi;
digiinfo *DigiList;
int     DigiMap[LASTSOUND];
int     DigiChannel[STARTMUSIC - STARTDIGISOUNDS];
globalsoundpos channelSoundPos[MIX_CHANNELS];

static byte  **SoundTable;
static boolean SD_Started;
static boolean nextsoundpos;
static int     SoundNumber, DigiNumber;
static word    SoundPriority, DigiPriority;
static int     LeftPosition, RightPosition;

/* options of the pause menu */
static bool opt_music = true, opt_sound = true;

/* ------------------------------------------------------------------ */
/* Channels                                                           */
/* ------------------------------------------------------------------ */
typedef struct {
    const byte *data;      /* unsigned 8 bit samples, in place */
    uint32_t length;
    uint32_t pos;          /* 16.16 */
    int gain;              /* 0..256 */
    uint32_t started;      /* order of the starts (oldest channel) */
    bool on;
} digichan_t;

static digichan_t chans[MIX_CHANNELS];
static uint32_t start_counter;
static uint32_t digi_step;  /* 7042 / output rate, 16.16 */
static int out_rate;

static wf_mus_t *alsound;          /* the AdLib sound effect playing */
static uint32_t alsound_frames;    /* output frames played */
static uint32_t alsound_active;    /* frames while the sound "plays" (the game) */
static uint32_t alsound_total;     /* frames of the file (with its release) */
static bool alsound_on;

static wf_mus_t *music;
static bool music_on;
static int music_chunk = -1;

static int32_t mixbuf[512];

void Delay(int32_t wolfticks)
{
    if (wolfticks > 0)
        SDL_Delay((wolfticks * 100) / 7);
}

/* a file of the pack opened for playback */
static wf_mus_t *open_piece(const char *fmt, int n)
{
    char name[24];
    snprintf(name, sizeof(name), fmt, n);
    uint32_t size;
    const uint8_t *data = wf_file_data(name, &size);
    if (!data)
        return NULL;
    return wf_mus_open(data, (int)size);
}

/* ------------------------------------------------------------------ */
/* Mixing                                                             */
/* ------------------------------------------------------------------ */
static void mix_digi(digichan_t *c, int32_t *acc, int frames)
{
    int gain = opt_sound ? c->gain : 0;
    for (int i = 0; i < frames; i++) {
        uint32_t idx = c->pos >> 16;
        if (idx >= c->length) {
            c->on = false;
            int ch = (int)(c - chans);
            channelSoundPos[ch].valid = 0;
            return;
        }
        if (gain) {
            int32_t s0 = (int32_t)c->data[idx] - 128;
            int32_t s1 = idx + 1 < c->length ? (int32_t)c->data[idx + 1] - 128 : 0;
            int32_t f = (int32_t)(c->pos & 0xffff);
            int32_t s = s0 * 256 + (((s1 - s0) * f) >> 8);
            acc[i] += (s * gain) >> 8;
        }
        c->pos += digi_step;
    }
}

void wf_core_audio_render(int16_t *out, int frames)
{
    while (frames > 0) {
        int n = frames > (int)(sizeof(mixbuf) / sizeof(mixbuf[0])) ? (int)(sizeof(mixbuf) / sizeof(mixbuf[0])) : frames;
        memset(mixbuf, 0, (size_t)n * sizeof(int32_t));
        if (SD_Started) {
            if (music && music_on)
                wf_mus_mix(music, mixbuf, n, opt_music ? 256 : 0, out_rate);
            if (alsound && alsound_on) {
                int m = n;
                if (alsound_frames + (uint32_t)m > alsound_total)
                    m = (int)(alsound_total - alsound_frames);
                if (m > 0)
                    wf_mus_mix(alsound, mixbuf, m, opt_sound ? 256 : 0, out_rate);
                alsound_frames += (uint32_t)n;
                if (alsound_frames >= alsound_active && SoundNumber && SoundMode == sdm_AdLib) {
                    /* the game sees the end of the sound (id_sd.c) */
                    SoundNumber = 0;
                    SoundPriority = 0;
                }
                if (alsound_frames >= alsound_total)
                    alsound_on = false;
            }
            for (int c = 0; c < MIX_CHANNELS; c++)
                if (chans[c].on)
                    mix_digi(&chans[c], mixbuf, n);
        }
        for (int i = 0; i < n; i++) {
            int32_t s = mixbuf[i];
            if (s > 32767) s = 32767;
            if (s < -32768) s = -32768;
            out[i] = (int16_t)s;
        }
        out += n;
        frames -= n;
    }
}

void wf_core_set_sound(bool music_enabled, bool sound_enabled)
{
    opt_music = music_enabled;
    opt_sound = sound_enabled;
}

/* ------------------------------------------------------------------ */
/* Digitized sounds                                                   */
/* ------------------------------------------------------------------ */
static void SDL_SetupDigi(void)
{
    // Correct padding enforced by PM_Startup()
    word *soundInfoPage = (word *) (void *) PM_GetPage(ChunksInFile-1);
    NumDigi = (word) PM_GetPageSize(ChunksInFile - 1) / 4;

    DigiList = SafeMalloc(NumDigi * sizeof(*DigiList));
    int i,page;
    for(i = 0; i < NumDigi; i++)
    {
        // Calculate the size of the digi from the sizes of the pages between
        // the start page and the start page of the next sound

        DigiList[i].startpage = soundInfoPage[i * 2];
        if((int) DigiList[i].startpage >= ChunksInFile - 1)
        {
            NumDigi = i;
            break;
        }

        int lastPage;
        if(i < NumDigi - 1)
        {
            lastPage = soundInfoPage[i * 2 + 2];
            if(lastPage == 0 || lastPage + PMSoundStart > ChunksInFile - 1) lastPage = ChunksInFile - 1;
            else lastPage += PMSoundStart;
        }
        else lastPage = ChunksInFile - 1;

        int size = 0;
        for(page = PMSoundStart + DigiList[i].startpage; page < lastPage; page++)
            size += PM_GetPageSize(page);

        // Don't include padding of sound info page, if padding was added
        if(lastPage == ChunksInFile - 1 && PMSoundInfoPagePadded) size--;

        // Patch lower 16-bit of size with size from sound info page.
        // The original VSWAP contains padding which is included in the page size,
        // but not included in the 16-bit size. So we use the more precise value.
        if((size & 0xffff0000) != 0 && (size & 0xffff) < soundInfoPage[i * 2 + 1])
            size -= 0x10000;
        size = (size & 0xffff0000) | soundInfoPage[i * 2 + 1];

        DigiList[i].length = size;
    }

    for(i = 0; i < LASTSOUND; i++)
    {
        DigiMap[i] = -1;
        DigiChannel[i] = -1;
    }
}

void SD_PrepareSound(int which)
{
    (void)which; /* played in place */
}

int SD_GetChannelForDigi(int which)
{
    if (DigiChannel[which] != -1)
        return DigiChannel[which];
    /* a free channel of the group (2 .. MIX_CHANNELS-1), else the oldest */
    int oldest = 2;
    for (int c = 2; c < MIX_CHANNELS; c++) {
        if (!chans[c].on)
            return c;
        if ((int32_t)(chans[c].started - chans[oldest].started) < 0)
            oldest = c;
    }
    return oldest;
}

static int position_gain(int leftpos, int rightpos)
{
    /* Mix_SetPanning(((15 - leftpos) << 4) + 15, ...): the mean of the two */
    int l = ((15 - leftpos) << 4) + 15;
    int r = ((15 - rightpos) << 4) + 15;
    /* 255 -> 208: some room for the music next to the shots */
    return (l + r) * 13 / 32;
}

void SD_SetPosition(int channel, int leftpos, int rightpos)
{
    if ((leftpos < 0) || (leftpos > 15) || (rightpos < 0) || (rightpos > 15)
            || ((leftpos == 15) && (rightpos == 15)))
        Quit("SD_SetPosition: Illegal position");
    if (channel >= 0 && channel < MIX_CHANNELS)
        chans[channel].gain = position_gain(leftpos, rightpos);
}

int SD_PlayDigitized(word which, int leftpos, int rightpos)
{
    if (!DigiMode)
        return 0;
    if (which >= NumDigi)
        Quit("SD_PlayDigitized: bad sound number %i", which);

    int channel = SD_GetChannelForDigi(which);
    digichan_t *c = &chans[channel];
    c->data = PM_GetSoundPage(DigiList[which].startpage);
    c->length = DigiList[which].length;
    if (c->data + c->length > PM_GetPageEnd())
        c->length = (uint32_t)(PM_GetPageEnd() - c->data);
    c->pos = 0;
    c->gain = position_gain(leftpos, rightpos);
    c->started = ++start_counter;
    c->on = true;
    return channel;
}

void SD_StopDigitized(void)
{
    DigiNumber = 0;
    DigiPriority = 0;
    SoundPositioned = false;
    for (int c = 0; c < MIX_CHANNELS; c++) {
        chans[c].on = false;
        channelSoundPos[c].valid = 0;
    }
}

void SD_SetDigiDevice(byte mode)
{
    if (mode == DigiMode)
        return;
    SD_StopDigitized();
    if (mode == sds_SoundBlaster && !SoundBlasterPresent)
        return;
    DigiMode = mode;
}

/* ------------------------------------------------------------------ */
/* AdLib sound effects                                                */
/* ------------------------------------------------------------------ */
static void al_stop(void)
{
    alsound_on = false;
}

static void al_play(int sound, const SoundCommon *s)
{
    al_stop();
    wf_mus_close(alsound);
    alsound = open_piece("adlib%02d.mus", sound);
    if (!alsound)
        return;
    alsound_frames = 0;
    alsound_total = wf_mus_length(alsound, out_rate);
    /* one byte of the sound per 1/140 s (SDL_IMFMusicPlayer) */
    alsound_active = (uint32_t)((uint64_t)s->length * (uint32_t)out_rate / 140);
    if (alsound_active > alsound_total)
        alsound_active = alsound_total;
    alsound_on = true;
}

/* ------------------------------------------------------------------ */
/* Public routines                                                    */
/* ------------------------------------------------------------------ */
boolean SD_SetSoundMode(byte mode)
{
    boolean result = false;
    word tableoffset;

    SD_StopSound();

    if ((mode == sdm_AdLib) && !AdLibPresent)
        mode = sdm_PC;

    switch (mode)
    {
        case sdm_Off:
            tableoffset = STARTADLIBSOUNDS;
            result = true;
            break;
        case sdm_PC:
            /* no PC speaker on the console: the AdLib sounds */
            mode = sdm_AdLib;
            tableoffset = STARTADLIBSOUNDS;
            result = true;
            break;
        case sdm_AdLib:
            tableoffset = STARTADLIBSOUNDS;
            result = AdLibPresent;
            break;
        default:
            Quit("SD_SetSoundMode: Invalid sound mode %i", mode);
            return false;
    }
    SoundTable = &audiosegs[tableoffset];

    if (result && (mode != SoundMode))
    {
        al_stop();
        SoundMode = mode;
        SoundNumber = 0;
        SoundPriority = 0;
    }
    return result;
}

boolean SD_SetMusicMode(byte mode)
{
    boolean result = false;

    SD_FadeOutMusic();
    switch (mode)
    {
        case smm_Off:
            result = true;
            break;
        case smm_AdLib:
            result = AdLibPresent;
            break;
    }
    if (result)
        MusicMode = mode;
    return result;
}

void SD_Startup(void)
{
    if (SD_Started)
        return;
    out_rate = wf_plat_audio_rate();
    digi_step = (uint32_t)(((uint64_t)ORIGSAMPLERATE << 16) / (uint32_t)out_rate);
    AdLibPresent = true;
    SoundBlasterPresent = true;

    SD_SetSoundMode(sdm_Off);
    SD_SetMusicMode(smm_Off);

    SDL_SetupDigi();

    SD_Started = true;
}

void SD_Shutdown(void)
{
    if (!SD_Started)
        return;
    SD_MusicOff();
    SD_StopSound();
    wf_mus_close(alsound);
    alsound = NULL;
    wf_mus_close(music);
    music = NULL;
    free(DigiList);
    DigiList = NULL;
    SD_Started = false;
}

void SD_PositionSound(int leftvol, int rightvol)
{
    LeftPosition = leftvol;
    RightPosition = rightvol;
    nextsoundpos = true;
}

boolean SD_PlaySound(int sound)
{
    boolean ispos;
    SoundCommon *s;
    int lp, rp;

    lp = LeftPosition;
    rp = RightPosition;
    LeftPosition = 0;
    RightPosition = 0;

    ispos = nextsoundpos;
    nextsoundpos = false;

    if (sound == -1 || (DigiMode == sds_Off && SoundMode == sdm_Off))
        return 0;

    s = (SoundCommon *) SoundTable[sound];

    if ((SoundMode != sdm_Off) && !s)
        Quit("SD_PlaySound() - Uncached sound");

    if ((DigiMode != sds_Off) && (DigiMap[sound] != -1))
    {
        int channel = SD_PlayDigitized(DigiMap[sound], lp, rp);
        SoundPositioned = ispos;
        DigiNumber = sound;
        DigiPriority = s ? s->priority : 0;
        return channel + 1;
    }

    if (SoundMode == sdm_Off)
        return 0;

    if (!s->length)
        Quit("SD_PlaySound() - Zero length sound");
    if (s->priority < SoundPriority)
        return 0;

    al_play(sound, s);

    SoundNumber = sound;
    SoundPriority = s->priority;
    return 0;
}

word SD_SoundPlaying(void)
{
    if (SoundMode == sdm_AdLib && alsound_on && SoundNumber)
        return SoundNumber;
    return 0;
}

void SD_StopSound(void)
{
    SD_StopDigitized();
    al_stop();
    SoundPositioned = false;
    SoundNumber = 0;
    SoundPriority = 0;
}

void SD_WaitSoundDone(void)
{
    while (SD_SoundPlaying())
        SDL_Delay(5);
}

/* ------------------------------------------------------------------ */
/* Music                                                              */
/* ------------------------------------------------------------------ */
void SD_MusicOn(void)
{
    if (music)
        music_on = true;
}

int SD_MusicOff(void)
{
    music_on = false;
    return (int)wf_mus_tell(music);
}

static void open_music(int chunk)
{
    if (music && chunk == music_chunk)
        return;
    wf_mus_close(music);
    music = open_piece("music%02d.mus", chunk - STARTMUSIC);
    music_chunk = music ? chunk : -1;
}

void SD_StartMusic(int chunk)
{
    SD_MusicOff();
    if (MusicMode == smm_AdLib)
    {
        open_music(chunk);
        if (music) {
            wf_mus_rewind(music);
            SD_MusicOn();
        }
    }
}

void SD_ContinueMusic(int chunk, int startoffs)
{
    SD_MusicOff();
    if (MusicMode == smm_AdLib)
    {
        open_music(chunk);
        if (music) {
            wf_mus_seek(music, startoffs > 0 ? (uint32_t)startoffs : 0);
            SD_MusicOn();
        }
    }
}

void SD_FadeOutMusic(void)
{
    if (MusicMode == smm_AdLib)
        SD_MusicOff();
}

boolean SD_MusicPlaying(void)
{
    return MusicMode == smm_AdLib && music_on;
}

/* the position of the music, for a savestate (lastgamemusicoffset) */
int wf_sound_music_pos(void)
{
    return music ? (int)wf_mus_tell(music) : 0;
}
