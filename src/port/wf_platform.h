/*
 * Interface between Wolf4SDL (src/wolf + the mini SDL of src/port/sdl) and
 * the platform it runs on:
 *  - src/port/wf_main.c : the Game & Watch homebrew (retro-go)
 *  - harness/wf_host.c  : a headless Linux harness used to test the port
 */
#ifndef WF_PLATFORM_H
#define WF_PLATFORM_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* The game's screen (VGA mode 13h) */
#define WF_WIDTH  320
#define WF_HEIGHT 200

/* Buttons of the console, as seen by the core (wf_core_set_buttons()) */
enum {
    WF_BTN_LEFT   = 1 << 0,
    WF_BTN_RIGHT  = 1 << 1,
    WF_BTN_UP     = 1 << 2,
    WF_BTN_DOWN   = 1 << 3,
    WF_BTN_A      = 1 << 4,
    WF_BTN_B      = 1 << 5,
    WF_BTN_GAME   = 1 << 6,   /* START on Zelda units */
    WF_BTN_TIME   = 1 << 7,   /* SELECT on Zelda units */
};

/* ---- implemented by the platform ---- */

/* Monotonic time in milliseconds. */
uint32_t wf_plat_time_ms(void);

/* Called while the game waits (in_delay = 1, SDL_Delay) and when it reads
 * its events (in_delay = 0, SDL_PollEvent). The platform feeds its audio
 * output with wf_core_audio_render(), reads its buttons and gives them to
 * wf_core_set_buttons(). */
void wf_plat_service(int in_delay);

/* Show the game's screen: WF_WIDTH x WF_HEIGHT palette indexes, `pitch`
 * bytes per line, with this palette (256 colours 0x00RRGGBB; `pal_version`
 * changes with it). */
void wf_plat_present(const uint8_t *pixels, int pitch,
                     const uint32_t *palette, uint32_t pal_version);

/* Output sample rate (mono, signed 16 bit). */
int wf_plat_audio_rate(void);

/* Look up a file of the asset pack (lower case names: "vswap.wl6"...).
 * The returned memory is read-only and stays valid while the game runs. */
bool wf_plat_find_file(const char *name, const uint8_t **data, uint32_t *size);

/* Directory (without trailing '/') where the game may write its files
 * (configuration, saved games). */
const char *wf_plat_save_dir(void);

/* Memory outside the heap for the big buffers of the game ("buffer": the
 * screen the game draws into, "screen": the screen shown), NULL: use the
 * heap. The memory is zeroed. */
void *wf_plat_big_memory(const char *what, size_t size);

/* Unrecoverable error. Never returns. */
void wf_plat_fatal(const char *msg) __attribute__((noreturn));

/* The game asked to quit. Never returns. */
void wf_plat_quit(void) __attribute__((noreturn));

/* ---- implemented by the core (wf_core.c, wf_sdl.c, wf_sound.c) ---- */

typedef struct {
    bool music;
    bool sound;
} wf_core_options_t;

/* Run the game. Never returns. */
void wf_core_run(const wf_core_options_t *opts) __attribute__((noreturn));

/* Buttons currently held (WF_BTN_*). */
void wf_core_set_buttons(uint32_t buttons);

/* Savestates (wf_state.c) */
bool wf_core_can_save(void);
bool wf_core_save_state(const char *path);
bool wf_core_load_state(const char *path);
/* After a service of the platform (outside its menu): a load waiting
 * restarts the game's main loop (longjmp). */
void wf_core_poll_restart(void);

/* Options of the pause menu */
void wf_core_set_sound(bool music, bool sound);
void wf_core_set_options(bool swap_ab, bool always_run);

/* Render `frames` mono samples (sound effects and music). */
void wf_core_audio_render(int16_t *out, int frames);

/* ---- implemented by wf_alloc.c ---- */
void   wf_heap_init(void *base, size_t size);
void  *wf_malloc(size_t size);
void  *wf_calloc(size_t n, size_t size);
void  *wf_realloc(void *ptr, size_t size);
void   wf_free(void *ptr);
size_t wf_heap_used(void);
size_t wf_heap_peak(void);
size_t wf_heap_size(void);
void   wf_heap_ignore_range(const void *lo, const void *hi);

#ifdef __cplusplus
}
#endif

#endif
