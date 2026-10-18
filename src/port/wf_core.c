/*
 * Runs Wolf4SDL on the mini SDL of the port (single threaded: the platform
 * gets control back in SDL_Delay() / SDL_PollEvent() / the presents).
 */
#include "wl_def.h"

int wf_wolf_main(int argc, char *argv[]);

void wf_core_set_buttons(uint32_t buttons)
{
    wf_sdl_set_buttons(buttons);
}

void wf_core_set_options(bool swap_ab, bool always_run)
{
    wf_sdl_set_options(swap_ab, always_run);
}

void wf_exit(int code)
{
    if (code == 0)
        wf_plat_quit();
    wf_plat_fatal("The game stopped (exit)");
}

void wf_core_run(const wf_core_options_t *opts)
{
    wf_core_set_sound(opts->music, opts->sound);
    /* the configuration and the saved games: the save folder */
    snprintf(configdir, sizeof(configdir), "%s", wf_plat_save_dir());
    static char arg0[] = "wolf3d";
    char *argv[] = { arg0, NULL };
    wf_wolf_main(1, argv);
    wf_plat_quit();
}
