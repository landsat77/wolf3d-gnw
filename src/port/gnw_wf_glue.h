/*
 * Forced include (-include) of the Wolf4SDL sources of the port: GNW is
 * defined, the C allocation functions use the game heap (wf_alloc.c), the
 * stdio files of the game are the pack in place or real files in the save
 * folder (wf_file.c), exit() goes back to the platform.
 */
#ifndef GNW_WF_GLUE_H
#define GNW_WF_GLUE_H

#ifndef GNW
#define GNW 1
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "wf_platform.h"
#include "wf_file.h"

#define malloc(n)     wf_malloc(n)
#define calloc(n, s)  wf_calloc(n, s)
#define realloc(p, n) wf_realloc(p, n)
#define free(p)       wf_free(p)

/* the game ends with exit(): back to the platform */
void wf_exit(int code) __attribute__((noreturn));
#define exit(c)       wf_exit(c)
#define atexit(f)     ((void)(f))

/* files: the pack (read only, in place) or the save folder */
#undef feof
#define FILE          wf_file_t
#define fopen(n, m)   wf_fopen(n, m)
#define fclose(f)     wf_fclose(f)
#define fread(p, s, n, f)  wf_fread(p, s, n, f)
#define fwrite(p, s, n, f) wf_fwrite(p, s, n, f)
#define fseek(f, o, w)     wf_fseek(f, o, w)
#define ftell(f)           wf_ftell(f)
#define feof(f)            wf_feof(f)
#define stat(p, b)         wf_stat(p, b)
#define mkdir(p, m)        wf_mkdir(p)
#define unlink(p)          wf_unlink(p)

/* newlib declares its own itoa / ltoa (wl_def.h defines them) */
#define itoa wf_itoa
#define ltoa wf_ltoa

#endif
