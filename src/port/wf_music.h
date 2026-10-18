/* Pre-rendered music pieces (wf_music.c) */
#ifndef WF_MUSIC_H
#define WF_MUSIC_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct wf_mus wf_mus_t;

/* A "GWMU" piece in memory (read in place), NULL if invalid. */
wf_mus_t *wf_mus_open(const unsigned char *data, int len);
void wf_mus_close(wf_mus_t *f);
/* the piece plays once (it does not loop), its length at `out_rate` */
bool wf_mus_ends(const wf_mus_t *f);
uint32_t wf_mus_length(const wf_mus_t *f, int out_rate);
int wf_mus_rewind(wf_mus_t *f);
/* Adds the next `frames` mono frames at `out_rate` to `acc`, scaled by
 * `gain` (256 = unity, 0 = silent). The piece loops forever (from its loop
 * start): returns `frames`. */
int wf_mus_mix(wf_mus_t *f, int32_t *acc, int frames, int gain, int out_rate);
/* source sample being played / continue from a source sample */
uint32_t wf_mus_tell(const wf_mus_t *f);
void wf_mus_seek(wf_mus_t *f, uint32_t pos);
uint32_t wf_mus_rate(const wf_mus_t *f);

#ifdef __cplusplus
}
#endif

#endif
