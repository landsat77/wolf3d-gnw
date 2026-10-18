#ifndef WF_MINI_SDL_ENDIAN_H
#define WF_MINI_SDL_ENDIAN_H

#include "SDL_types.h"

/* little endian: the console (Cortex-M7) and the PCs of the harness */
#define SDL_LIL_ENDIAN 1234
#define SDL_BIG_ENDIAN 4321
#define SDL_BYTEORDER SDL_LIL_ENDIAN

#define SDL_SwapLE16(x) ((Uint16)(x))
#define SDL_SwapLE32(x) ((Uint32)(x))
#define SDL_SwapBE16(x) ((Uint16)((((Uint16)(x)) >> 8) | (((Uint16)(x)) << 8)))
#define SDL_SwapBE32(x) __builtin_bswap32((Uint32)(x))

#endif
