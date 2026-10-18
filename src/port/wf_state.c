/*
 * Savestates of the Game & Watch (pause menu of retro-go, resume from the
 * launcher) for Wolfenstein 3D.
 *
 * Wolfenstein 3D saves a game at any moment of a level: a savestate is a
 * saved game of the game itself (SaveTheGame(), without the 32 byte name
 * of the savegam?.wl6 files), made while a level is being played.
 *
 * Loading is deferred: the file name waits (pending) until the game gets
 * control back. At boot (resume from the launcher) DemoLoop() loads it
 * before the title screen; while the game runs, wf_core_poll_restart()
 * (after each service of the platform) jumps back to DemoLoop() (longjmp),
 * which loads it and starts GameLoop(), like a game loaded from the menu.
 */
#include "wl_def.h"
#include "wf_state.h"

static jmp_buf restart_env;
static bool restart_ready;
static char pending[256];
static bool have_pending;

jmp_buf *wf_state_restart_env(void)
{
    restart_ready = true;
    return &restart_env;
}

bool wf_core_can_save(void)
{
    return wf_sdl_in_level() && ingame && !demoplayback && !demorecord && player;
}

int wf_sound_music_pos(void);

bool wf_core_save_state(const char *path)
{
    if (!wf_core_can_save())
        return false;
    FILE *file = fopen(path, "wb");
    if (!file)
        return false;
    lastgamemusicoffset = wf_sound_music_pos();
    bool ok = SaveTheGame(file, 0, 0);
    fclose(file);
    return ok;
}

bool wf_core_load_state(const char *path)
{
    FILE *file = fopen(path, "rb");
    if (!file)
        return false;
    fclose(file);
    snprintf(pending, sizeof(pending), "%s", path);
    have_pending = true;
    return true;
}

void wf_core_poll_restart(void)
{
    if (have_pending && restart_ready)
        longjmp(restart_env, 1);
}

/* back in DemoLoop() from anywhere in the game */
void wf_state_after_restart(void)
{
    wf_sdl_set_in_level(false);
    SD_StopSound();
    SD_MusicOff();
    demoplayback = false;
    demorecord = false;
    startgame = false;
    loadedgame = false;
    ingame = false;
    playstate = ex_stillplaying;
    Paused = false;
    IN_ClearKeysDown();
}

bool wf_state_apply_pending(void)
{
    if (!have_pending)
        return false;
    have_pending = false;
    FILE *file = fopen(pending, "rb");
    if (!file)
        return false;
    VW_FadeOut();
    ClearMemory();
    loadedgame = true;
    LoadTheGame(file, 0, 0);
    fclose(file);
    EnableEndGameMenuItem();
    IN_ClearKeysDown();
    return true;
}
