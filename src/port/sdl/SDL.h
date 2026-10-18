/*
 * Mini SDL 2 for Wolf4SDL on the Game & Watch (src/port/wf_sdl.c).
 *
 * Only what Wolf4SDL uses: 8 bit software surfaces with a palette, key
 * events made from the buttons of the console, the time. The "texture" and
 * the "renderer" only hand the screen surface to the platform
 * (SDL_RenderPresent()). Windows, joysticks and the mouse are stubs. The
 * sound does not go through SDL (wf_sound.c replaces id_sd.c).
 */
#ifndef WF_MINI_SDL_H
#define WF_MINI_SDL_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <ctype.h>

#include "SDL_types.h"
#include "SDL_endian.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum { SDL_FALSE = 0, SDL_TRUE = 1 } SDL_bool;

/* ---- init / errors / misc ---- */
#define SDL_INIT_AUDIO    0x10u
#define SDL_INIT_VIDEO    0x20u
#define SDL_INIT_JOYSTICK 0x200u

static inline int SDL_Init(Uint32 f) { (void)f; return 0; }
static inline void SDL_Quit(void) {}
static inline const char *SDL_GetError(void) { return "unsupported"; }
static inline bool SDL_SetHint(const char *n, const char *v) { (void)n; (void)v; return true; }
#define SDL_HINT_RENDER_SCALE_QUALITY "SDL_RENDER_SCALE_QUALITY"

#define SDL_MESSAGEBOX_ERROR       0x10
#define SDL_MESSAGEBOX_INFORMATION 0x40
int SDL_ShowSimpleMessageBox(Uint32 flags, const char *title, const char *msg, void *window);

/* ---- time ---- */
Uint32 SDL_GetTicks(void);
void SDL_Delay(Uint32 ms);

/* ---- surfaces (8 bit only) ---- */
typedef struct SDL_Color { Uint8 r, g, b, a; } SDL_Color;
typedef struct SDL_Rect { int x, y, w, h; } SDL_Rect;
#define SDL_ALPHA_OPAQUE 255

typedef struct SDL_Palette {
    int ncolors;
    SDL_Color colors[256];
    Uint32 version;         /* changes with the colors */
} SDL_Palette;

typedef struct SDL_PixelFormat {
    Uint32 format;
    SDL_Palette *palette;
    Uint8 BitsPerPixel;
    Uint8 BytesPerPixel;
} SDL_PixelFormat;

typedef struct SDL_Surface {
    Uint32 flags;
    SDL_PixelFormat *format;
    int w, h;
    int pitch;
    void *pixels;
} SDL_Surface;

#define SDL_MUSTLOCK(s) 0
static inline int SDL_LockSurface(SDL_Surface *s) { (void)s; return 0; }
static inline void SDL_UnlockSurface(SDL_Surface *s) { (void)s; }

#define SDL_PIXELFORMAT_ARGB8888 0x16362004u
static inline SDL_bool SDL_PixelFormatEnumToMasks(Uint32 format, int *bpp, Uint32 *r, Uint32 *g,
                                                  Uint32 *b, Uint32 *a)
{
    (void)format;
    *bpp = 8; /* the port draws with 8 bit surfaces only */
    *r = *g = *b = *a = 0;
    return SDL_TRUE;
}

SDL_Surface *SDL_CreateRGBSurface(Uint32 flags, int w, int h, int depth,
                                  Uint32 r, Uint32 g, Uint32 b, Uint32 a);
void SDL_FreeSurface(SDL_Surface *s);
int SDL_FillRect(SDL_Surface *s, const SDL_Rect *r, Uint32 color);
int SDL_BlitSurface(SDL_Surface *src, const SDL_Rect *sr, SDL_Surface *dst, SDL_Rect *dr);
int SDL_SetPaletteColors(SDL_Palette *p, const SDL_Color *colors, int first, int n);
static inline Uint32 SDL_MapRGBA(const SDL_PixelFormat *f, Uint8 r, Uint8 g, Uint8 b, Uint8 a)
{
    (void)f; (void)a;
    return ((Uint32)r << 16) | ((Uint32)g << 8) | b;
}
static inline int SDL_SaveBMP(SDL_Surface *s, const char *file) { (void)s; (void)file; return -1; }

/* ---- window / renderer / texture: the texture is the screen surface ---- */
typedef struct SDL_Window SDL_Window;
typedef struct SDL_Renderer SDL_Renderer;
typedef struct SDL_Texture SDL_Texture;
#define SDL_WINDOWPOS_UNDEFINED 0x1FFF0000u
#define SDL_WINDOW_FULLSCREEN 0x1u
#define SDL_WINDOW_OPENGL 0x2u
#define SDL_RENDERER_ACCELERATED 0x2u
#define SDL_RENDERER_PRESENTVSYNC 0x4u
#define SDL_TEXTUREACCESS_STREAMING 1
#define SDL_BLENDMODE_BLEND 1
static inline SDL_Window *SDL_CreateWindow(const char *t, int x, int y, int w, int h, Uint32 f)
{ (void)t; (void)x; (void)y; (void)w; (void)h; (void)f; return (SDL_Window *)1; }
static inline void SDL_DestroyWindow(SDL_Window *w) { (void)w; }
static inline SDL_Renderer *SDL_CreateRenderer(SDL_Window *w, int i, Uint32 f)
{ (void)w; (void)i; (void)f; return (SDL_Renderer *)1; }
static inline void SDL_DestroyRenderer(SDL_Renderer *r) { (void)r; }
static inline int SDL_SetRenderDrawBlendMode(SDL_Renderer *r, int m) { (void)r; (void)m; return 0; }
static inline SDL_Texture *SDL_CreateTexture(SDL_Renderer *r, Uint32 f, int a, int w, int h)
{ (void)r; (void)f; (void)a; (void)w; (void)h; return (SDL_Texture *)1; }
static inline void SDL_DestroyTexture(SDL_Texture *t) { (void)t; }
/* the pixels of the screen surface (8 bit, its palette) */
int SDL_UpdateTexture(SDL_Texture *t, const SDL_Rect *r, const void *pixels, int pitch);
static inline int SDL_RenderCopy(SDL_Renderer *r, SDL_Texture *t, const SDL_Rect *s, const SDL_Rect *d)
{ (void)r; (void)t; (void)s; (void)d; return 0; }
void SDL_RenderPresent(SDL_Renderer *r);

/* ---- events ---- */
typedef enum {
    SDL_SCANCODE_UNKNOWN = 0,
    SDL_SCANCODE_A = 4, SDL_SCANCODE_B, SDL_SCANCODE_C, SDL_SCANCODE_D, SDL_SCANCODE_E,
    SDL_SCANCODE_F, SDL_SCANCODE_G, SDL_SCANCODE_H, SDL_SCANCODE_I, SDL_SCANCODE_J,
    SDL_SCANCODE_K, SDL_SCANCODE_L, SDL_SCANCODE_M, SDL_SCANCODE_N, SDL_SCANCODE_O,
    SDL_SCANCODE_P, SDL_SCANCODE_Q, SDL_SCANCODE_R, SDL_SCANCODE_S, SDL_SCANCODE_T,
    SDL_SCANCODE_U, SDL_SCANCODE_V, SDL_SCANCODE_W, SDL_SCANCODE_X, SDL_SCANCODE_Y,
    SDL_SCANCODE_Z,
    SDL_SCANCODE_1 = 30, SDL_SCANCODE_2, SDL_SCANCODE_3, SDL_SCANCODE_4, SDL_SCANCODE_5,
    SDL_SCANCODE_6, SDL_SCANCODE_7, SDL_SCANCODE_8, SDL_SCANCODE_9, SDL_SCANCODE_0,
    SDL_SCANCODE_RETURN = 40, SDL_SCANCODE_ESCAPE = 41, SDL_SCANCODE_BACKSPACE = 42,
    SDL_SCANCODE_TAB = 43, SDL_SCANCODE_SPACE = 44, SDL_SCANCODE_MINUS = 45,
    SDL_SCANCODE_EQUALS = 46, SDL_SCANCODE_LEFTBRACKET = 47, SDL_SCANCODE_RIGHTBRACKET = 48,
    SDL_SCANCODE_BACKSLASH = 49, SDL_SCANCODE_SEMICOLON = 51, SDL_SCANCODE_APOSTROPHE = 52,
    SDL_SCANCODE_GRAVE = 53, SDL_SCANCODE_COMMA = 54, SDL_SCANCODE_PERIOD = 55,
    SDL_SCANCODE_SLASH = 56, SDL_SCANCODE_CAPSLOCK = 57,
    SDL_SCANCODE_F1 = 58, SDL_SCANCODE_F2, SDL_SCANCODE_F3, SDL_SCANCODE_F4, SDL_SCANCODE_F5,
    SDL_SCANCODE_F6, SDL_SCANCODE_F7, SDL_SCANCODE_F8, SDL_SCANCODE_F9, SDL_SCANCODE_F10,
    SDL_SCANCODE_F11, SDL_SCANCODE_F12,
    SDL_SCANCODE_PRINTSCREEN = 70, SDL_SCANCODE_SCROLLLOCK = 71, SDL_SCANCODE_PAUSE = 72,
    SDL_SCANCODE_INSERT = 73, SDL_SCANCODE_HOME = 74,
    SDL_SCANCODE_PAGEUP = 75, SDL_SCANCODE_DELETE = 76, SDL_SCANCODE_END = 77,
    SDL_SCANCODE_PAGEDOWN = 78, SDL_SCANCODE_RIGHT = 79, SDL_SCANCODE_LEFT = 80,
    SDL_SCANCODE_DOWN = 81, SDL_SCANCODE_UP = 82, SDL_SCANCODE_NUMLOCKCLEAR = 83,
    SDL_SCANCODE_KP_DIVIDE = 84, SDL_SCANCODE_KP_MULTIPLY = 85, SDL_SCANCODE_KP_MINUS = 86,
    SDL_SCANCODE_KP_PLUS = 87, SDL_SCANCODE_KP_ENTER = 88,
    SDL_SCANCODE_KP_1 = 89, SDL_SCANCODE_KP_2, SDL_SCANCODE_KP_3, SDL_SCANCODE_KP_4,
    SDL_SCANCODE_KP_5, SDL_SCANCODE_KP_6, SDL_SCANCODE_KP_7, SDL_SCANCODE_KP_8,
    SDL_SCANCODE_KP_9, SDL_SCANCODE_KP_0, SDL_SCANCODE_KP_PERIOD,
    SDL_SCANCODE_LCTRL = 224, SDL_SCANCODE_LSHIFT = 225, SDL_SCANCODE_LALT = 226,
    SDL_SCANCODE_LGUI = 227,
    SDL_SCANCODE_RCTRL = 228, SDL_SCANCODE_RSHIFT = 229, SDL_SCANCODE_RALT = 230,
    SDL_NUM_SCANCODES = 512
} SDL_Scancode;

typedef Sint32 SDL_Keycode;

typedef enum {
    KMOD_NONE = 0,
    KMOD_LSHIFT = 0x1, KMOD_RSHIFT = 0x2,
    KMOD_LCTRL = 0x40, KMOD_RCTRL = 0x80,
    KMOD_LALT = 0x100, KMOD_RALT = 0x200,
    KMOD_NUM = 0x1000,
} SDL_Keymod;
static inline SDL_Keymod SDL_GetModState(void) { return KMOD_NUM; }

typedef struct SDL_Keysym {
    SDL_Scancode scancode;
    SDL_Keycode sym;
    Uint16 mod;
} SDL_Keysym;

enum {
    SDL_QUIT = 0x100,
    SDL_KEYDOWN = 0x300,
    SDL_KEYUP,
    SDL_TEXTINPUT = 0x303,
    SDL_MOUSEMOTION = 0x400,
    SDL_JOYBUTTONDOWN = 0x603,
    SDL_JOYBUTTONUP,
};
#define SDL_RELEASED 0
#define SDL_PRESSED 1
#define SDL_TEXTINPUTEVENT_TEXT_SIZE 32

typedef struct { Uint32 type; Uint8 state; Uint8 repeat; SDL_Keysym keysym; } SDL_KeyboardEvent;
typedef struct { Uint32 type; char text[SDL_TEXTINPUTEVENT_TEXT_SIZE]; } SDL_TextInputEvent;
typedef struct { Uint32 type; Uint8 button; } SDL_JoyButtonEvent;

typedef union SDL_Event {
    Uint32 type;
    SDL_KeyboardEvent key;
    SDL_TextInputEvent text;
    SDL_JoyButtonEvent jbutton;
} SDL_Event;

int SDL_PollEvent(SDL_Event *e);
int SDL_WaitEvent(SDL_Event *e);
static inline Uint8 SDL_EventState(Uint32 type, int state) { (void)type; (void)state; return 0; }

/* ---- mouse: none ---- */
#define SDL_BUTTON(x) (1 << ((x) - 1))
#define SDL_BUTTON_LEFT 1
#define SDL_BUTTON_MIDDLE 2
#define SDL_BUTTON_RIGHT 3
#define SDL_ENABLE 1
#define SDL_DISABLE 0
#define SDL_IGNORE 0
static inline int SDL_ShowCursor(int t) { (void)t; return 0; }
static inline int SDL_SetRelativeMouseMode(SDL_bool e) { (void)e; return 0; }
static inline void SDL_SetWindowGrab(SDL_Window *w, SDL_bool g) { (void)w; (void)g; }
static inline void SDL_WarpMouseInWindow(SDL_Window *w, int x, int y) { (void)w; (void)x; (void)y; }
static inline Uint32 SDL_GetMouseState(int *x, int *y) { if (x) *x = 0; if (y) *y = 0; return 0; }
static inline Uint32 SDL_GetRelativeMouseState(int *x, int *y) { if (x) *x = 0; if (y) *y = 0; return 0; }

/* ---- joysticks: none ---- */
typedef struct SDL_Joystick SDL_Joystick;
#define SDL_HAT_CENTERED 0
#define SDL_HAT_UP 1
#define SDL_HAT_RIGHT 2
#define SDL_HAT_DOWN 4
#define SDL_HAT_LEFT 8
static inline int SDL_NumJoysticks(void) { return 0; }
static inline SDL_Joystick *SDL_JoystickOpen(int i) { (void)i; return NULL; }
static inline void SDL_JoystickClose(SDL_Joystick *j) { (void)j; }
static inline void SDL_JoystickUpdate(void) {}
static inline int SDL_JoystickNumButtons(SDL_Joystick *j) { (void)j; return 0; }
static inline int SDL_JoystickNumHats(SDL_Joystick *j) { (void)j; return 0; }
static inline Sint16 SDL_JoystickGetAxis(SDL_Joystick *j, int i) { (void)j; (void)i; return 0; }
static inline Uint8 SDL_JoystickGetButton(SDL_Joystick *j, int i) { (void)j; (void)i; return 0; }
static inline Uint8 SDL_JoystickGetHat(SDL_Joystick *j, int i) { (void)j; (void)i; return 0; }

/* ---- port: the mini SDL and the platform (wf_sdl.c) ---- */
SDL_Surface *wf_sdl_surface_on(void *pixels, int w, int h, int pitch);

#ifdef __cplusplus
}
#endif

#endif
