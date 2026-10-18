/* Savestates of the console (wf_state.c) and the game */
#ifndef WF_STATE_H
#define WF_STATE_H

#include <setjmp.h>
#include <stdbool.h>

/* DemoLoop(): setjmp(*wf_state_restart_env()) before its main loop */
jmp_buf *wf_state_restart_env(void);
void wf_state_after_restart(void);
/* DemoLoop(): load the savestate waiting, true if a game was loaded (then
 * GameLoop()) */
bool wf_state_apply_pending(void);

#endif
