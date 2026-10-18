/* The mini SDL and the game (wf_sdl.c) */
#ifndef WF_SDL_H
#define WF_SDL_H

#include <stdint.h>
#include <stdbool.h>
#include "SDL.h"

#ifdef __cplusplus
extern "C" {
#endif

void wf_sdl_set_buttons(uint32_t buttons);
void wf_sdl_set_options(bool swap_ab, bool always_run);
/* PlayLoop() runs: the buttons are the actions of the player */
void wf_sdl_set_in_level(bool on);
bool wf_sdl_in_level(void);
/* PollControls(): sets the actions held (indexes in buttonstate[]) */
void wf_sdl_poll_buttons(int8_t *state, int attack, int strafe, int run, int use, int next_weapon);
/* the palette shown with the screen surface */
void wf_sdl_set_present_palette(const SDL_Palette *p);
void wf_sdl_release_all(void);

#ifdef __cplusplus
}
#endif

#endif
