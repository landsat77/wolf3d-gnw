/* Files of the game (wf_file.c): the pack in place, or the save folder. */
#ifndef WF_FILE_H
#define WF_FILE_H

#include <stdint.h>
#include <stddef.h>
#include <sys/stat.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct wf_file wf_file_t;

wf_file_t *wf_fopen(const char *name, const char *mode);
int wf_fclose(wf_file_t *f);
size_t wf_fread(void *ptr, size_t size, size_t n, wf_file_t *f);
size_t wf_fwrite(const void *ptr, size_t size, size_t n, wf_file_t *f);
int wf_fseek(wf_file_t *f, long offset, int whence);
long wf_ftell(wf_file_t *f);
int wf_feof(wf_file_t *f);
int wf_stat(const char *path, struct stat *st);
int wf_mkdir(const char *path);
int wf_unlink(const char *path);

/* a file of the pack in place (NULL if absent) */
const uint8_t *wf_file_data(const char *name, uint32_t *size);

#ifdef __cplusplus
}
#endif

#endif
