/*
 * Mini SDL 2 of the port (src/port/sdl/SDL.h): 8 bit surfaces with a
 * palette, the time, the screen given to the platform, key events made from
 * the buttons of the console.
 *
 * Buttons -> keys (wf_core_set_buttons()). Wolfenstein 3D is played with
 * the keyboard. During a level (PlayLoop(), wf_sdl_set_in_level()) the
 * D-pad gives the arrows and GAME gives Escape (the game's menu); the other
 * buttons are read by wf_sdl_poll_buttons() (PollControls()): A = fire,
 * B = open / use (held with left / right: strafe), TIME = next weapon.
 * A and B also give Left Ctrl / Space (the keys of these actions), so that
 * "press a key" works. In the menus A = Enter + Y, B = Escape + N (the
 * yes / no questions), GAME = Escape.
 */
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "SDL.h"
#include "wf_platform.h"
#include "wf_sdl.h"

/* ------------------------------------------------------------------ */
/* Misc                                                               */
/* ------------------------------------------------------------------ */
int SDL_ShowSimpleMessageBox(Uint32 flags, const char *title, const char *msg, void *window)
{
    (void)window;
    printf("%s: %s\n", title, msg);
    if (flags & SDL_MESSAGEBOX_ERROR)
        wf_plat_fatal(msg);
    return 0;
}

/* ------------------------------------------------------------------ */
/* Time                                                               */
/* ------------------------------------------------------------------ */
Uint32 SDL_GetTicks(void)
{
    return wf_plat_time_ms();
}

void SDL_Delay(Uint32 ms)
{
    Uint32 start = wf_plat_time_ms();
    do {
        wf_plat_service(1);
    } while (wf_plat_time_ms() - start < ms);
}

/* ------------------------------------------------------------------ */
/* Surfaces                                                           */
/* ------------------------------------------------------------------ */
SDL_Surface *SDL_CreateRGBSurface(Uint32 flags, int w, int h, int depth,
                                  Uint32 r, Uint32 g, Uint32 b, Uint32 a)
{
    (void)flags; (void)r; (void)g; (void)b; (void)a;
    if (depth != 8)
        return NULL;
    void *pixels = calloc((size_t)w * h, 1);
    if (!pixels)
        return NULL;
    SDL_Surface *s = wf_sdl_surface_on(pixels, w, h, w);
    if (!s) {
        free(pixels);
        return NULL;
    }
    s->flags = 0; /* owns its pixels */
    return s;
}

SDL_Surface *wf_sdl_surface_on(void *pixels, int w, int h, int pitch)
{
    SDL_Surface *s = calloc(1, sizeof(*s));
    SDL_PixelFormat *f = calloc(1, sizeof(*f));
    SDL_Palette *p = calloc(1, sizeof(*p));
    if (!s || !f || !p) {
        free(s);
        free(f);
        free(p);
        return NULL;
    }
    p->ncolors = 256;
    f->palette = p;
    f->BitsPerPixel = 8;
    f->BytesPerPixel = 1;
    s->flags = 1; /* pixels not owned */
    s->pixels = pixels;
    s->format = f;
    s->w = w;
    s->h = h;
    s->pitch = pitch;
    return s;
}

void SDL_FreeSurface(SDL_Surface *s)
{
    if (!s)
        return;
    if (!(s->flags & 1))
        free(s->pixels);
    free(s->format->palette);
    free(s->format);
    free(s);
}

static bool clip(const SDL_Surface *s, SDL_Rect *r)
{
    int x0 = r->x < 0 ? 0 : r->x;
    int y0 = r->y < 0 ? 0 : r->y;
    int x1 = r->x + r->w > s->w ? s->w : r->x + r->w;
    int y1 = r->y + r->h > s->h ? s->h : r->y + r->h;
    if (x1 <= x0 || y1 <= y0)
        return false;
    r->x = x0;
    r->y = y0;
    r->w = x1 - x0;
    r->h = y1 - y0;
    return true;
}

int SDL_FillRect(SDL_Surface *s, const SDL_Rect *rect, Uint32 color)
{
    SDL_Rect r = rect ? *rect : (SDL_Rect){ 0, 0, s->w, s->h };
    if (!clip(s, &r))
        return 0;
    Uint8 *p = (Uint8 *)s->pixels + r.y * s->pitch + r.x;
    for (int y = 0; y < r.h; y++, p += s->pitch)
        memset(p, (int)color, (size_t)r.w);
    return 0;
}

int SDL_BlitSurface(SDL_Surface *src, const SDL_Rect *sr, SDL_Surface *dst, SDL_Rect *dr)
{
    SDL_Rect s = sr ? *sr : (SDL_Rect){ 0, 0, src->w, src->h };
    int dx = dr ? dr->x : 0, dy = dr ? dr->y : 0;
    if (!clip(src, &s))
        return 0;
    SDL_Rect d = { dx, dy, s.w, s.h };
    int ox = d.x, oy = d.y;
    if (!clip(dst, &d))
        return 0;
    s.x += d.x - ox;
    s.y += d.y - oy;
    if (src->pixels == dst->pixels)
        return 0;
    if (d.x == 0 && s.x == 0 && d.w == src->w && src->pitch == d.w && dst->pitch == d.w) {
        memcpy((Uint8 *)dst->pixels + d.y * dst->pitch, (Uint8 *)src->pixels + s.y * src->pitch,
               (size_t)d.w * d.h);
    } else {
        for (int y = 0; y < d.h; y++)
            memmove((Uint8 *)dst->pixels + (d.y + y) * dst->pitch + d.x,
                    (Uint8 *)src->pixels + (s.y + y) * src->pitch + s.x, (size_t)d.w);
    }
    if (dr)
        *dr = d;
    return 0;
}

int SDL_SetPaletteColors(SDL_Palette *p, const SDL_Color *colors, int first, int n)
{
    if (!p || first < 0 || first + n > 256)
        return -1;
    if (memcmp(&p->colors[first], colors, (size_t)n * sizeof(SDL_Color)) != 0) {
        memcpy(&p->colors[first], colors, (size_t)n * sizeof(SDL_Color));
        p->version++;
    }
    return 0;
}

/* ------------------------------------------------------------------ */
/* Present                                                            */
/* ------------------------------------------------------------------ */
static const SDL_Palette *present_palette;
static const Uint8 *present_pixels;
static int present_pitch;
static uint32_t rgb[256];
static uint32_t rgb_version = 0xffffffffu;
static const SDL_Palette *rgb_palette;

void wf_sdl_set_present_palette(const SDL_Palette *p)
{
    present_palette = p;
}

int SDL_UpdateTexture(SDL_Texture *t, const SDL_Rect *r, const void *pixels, int pitch)
{
    (void)t; (void)r;
    present_pixels = pixels;
    present_pitch = pitch;
    return 0;
}

void SDL_RenderPresent(SDL_Renderer *r)
{
    (void)r;
    if (!present_pixels || !present_palette)
        return;
    if (rgb_palette != present_palette || rgb_version != present_palette->version) {
        for (int i = 0; i < 256; i++) {
            const SDL_Color *c = &present_palette->colors[i];
            rgb[i] = ((uint32_t)c->r << 16) | ((uint32_t)c->g << 8) | c->b;
        }
        rgb_palette = present_palette;
        rgb_version = present_palette->version;
    }
    /* the version for the platform: a hash of the colours */
    uint32_t h = 2166136261u;
    for (int i = 0; i < 256; i++)
        h = (h ^ rgb[i]) * 16777619u;
    wf_plat_present(present_pixels, present_pitch, rgb, h);
}

/* ------------------------------------------------------------------ */
/* Keys                                                               */
/* ------------------------------------------------------------------ */
static uint32_t buttons;
static bool swap_ab, always_run;
static bool in_level;
static bool held[SDL_NUM_SCANCODES];
static SDL_Event queue[32];
static int q_head, q_count;

void wf_sdl_set_buttons(uint32_t b)
{
    if (swap_ab) {
        uint32_t ab = b & (WF_BTN_A | WF_BTN_B);
        b &= ~(WF_BTN_A | WF_BTN_B);
        if (ab & WF_BTN_A) b |= WF_BTN_B;
        if (ab & WF_BTN_B) b |= WF_BTN_A;
    }
    buttons = b;
}

void wf_sdl_set_options(bool swap, bool run)
{
    swap_ab = swap;
    always_run = run;
}

void wf_sdl_set_in_level(bool on)
{
    in_level = on;
}

bool wf_sdl_in_level(void)
{
    return in_level;
}

/* PollControls(): the buttons of the actions (indexes of buttonstate[]) */
void wf_sdl_poll_buttons(int8_t *state, int attack, int strafe, int run, int use, int next_weapon)
{
    if (buttons & WF_BTN_A)
        state[attack] = true;
    if (buttons & WF_BTN_B) {
        state[use] = true;
        if (buttons & (WF_BTN_LEFT | WF_BTN_RIGHT))
            state[strafe] = true;
    }
    if (buttons & WF_BTN_TIME)
        state[next_weapon] = true;
    if (always_run)
        state[run] = true;
}

static int push_event(const SDL_Event *e)
{
    if (q_count == (int)(sizeof(queue) / sizeof(queue[0])))
        return 0;
    queue[(q_head + q_count) % (sizeof(queue) / sizeof(queue[0]))] = *e;
    q_count++;
    return 1;
}

static void push_key(SDL_Scancode sc, bool down)
{
#ifdef WF_DEBUG_KEYS
    printf("key %d %s at %u\n", (int)sc, down ? "down" : "up", (unsigned)wf_plat_time_ms());
#endif
    SDL_Event e;
    memset(&e, 0, sizeof(e));
    e.type = down ? SDL_KEYDOWN : SDL_KEYUP;
    e.key.state = down ? SDL_PRESSED : SDL_RELEASED;
    e.key.keysym.scancode = sc;
    push_event(&e);
}

/* the keys the buttons hold now */
static void wanted_keys(bool *want)
{
    uint32_t b = buttons;
    if (b & WF_BTN_LEFT)  want[SDL_SCANCODE_LEFT] = true;
    if (b & WF_BTN_RIGHT) want[SDL_SCANCODE_RIGHT] = true;
    if (b & WF_BTN_UP)    want[SDL_SCANCODE_UP] = true;
    if (b & WF_BTN_DOWN)  want[SDL_SCANCODE_DOWN] = true;
    if (b & WF_BTN_GAME)  want[SDL_SCANCODE_ESCAPE] = true;
    if (in_level) {
        if (b & WF_BTN_A) want[SDL_SCANCODE_LCTRL] = true;
        if (b & WF_BTN_B) want[SDL_SCANCODE_SPACE] = true;
    } else {
        if (b & WF_BTN_A) want[SDL_SCANCODE_RETURN] = want[SDL_SCANCODE_Y] = true;
        if (b & WF_BTN_B) want[SDL_SCANCODE_ESCAPE] = want[SDL_SCANCODE_N] = true;
    }
}

static void update_keys(void)
{
    static const SDL_Scancode used[] = {
        SDL_SCANCODE_LEFT, SDL_SCANCODE_RIGHT, SDL_SCANCODE_UP, SDL_SCANCODE_DOWN,
        SDL_SCANCODE_ESCAPE, SDL_SCANCODE_SPACE, SDL_SCANCODE_LCTRL,
        SDL_SCANCODE_RETURN, SDL_SCANCODE_Y, SDL_SCANCODE_N,
    };
    bool want[SDL_NUM_SCANCODES];
    memset(want, 0, sizeof(want));
    wanted_keys(want);
    for (size_t i = 0; i < sizeof(used) / sizeof(used[0]); i++) {
        SDL_Scancode sc = used[i];
        if (want[sc] && !held[sc]) {
            held[sc] = true;
            push_key(sc, true);
        } else if (!want[sc] && held[sc]) {
            held[sc] = false;
            push_key(sc, false);
        }
    }
}

int SDL_PollEvent(SDL_Event *e)
{
    if (q_count == 0) {
        wf_plat_service(0);
        update_keys();
    }
    if (q_count == 0)
        return 0;
    if (e)
        *e = queue[q_head];
    q_head = (q_head + 1) % (int)(sizeof(queue) / sizeof(queue[0]));
    q_count--;
    return 1;
}

int SDL_WaitEvent(SDL_Event *e)
{
    while (q_count == 0) {
        wf_plat_service(1);
        update_keys();
    }
    return SDL_PollEvent(e);
}

/* "press a key" also after the buttons are released (a savestate load) */
void wf_sdl_release_all(void)
{
    buttons = 0;
    update_keys();
}
