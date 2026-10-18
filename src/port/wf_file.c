/*
 * Files of Wolf4SDL on the Game & Watch (gnw_wf_glue.h redirects the
 * game's stdio to this file).
 *
 *  - read only names of the pack ("vswap.wl6", "gamemaps.wl6"...): read in
 *    place from the pack (memory mapped flash on the console);
 *  - everything else (config.wl6, savegam?.wl6) is a real file in the save
 *    folder of the platform (paths without '/' go there).
 *
 * This file is built without the glue: the real stdio below.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "wf_platform.h"
#include "wf_file.h"

struct wf_file {
    const uint8_t *mem;   /* a file of the pack, or NULL */
    uint32_t size;
    uint32_t pos;
    FILE *f;              /* a real file */
};

const uint8_t *wf_file_data(const char *name, uint32_t *size)
{
    const uint8_t *data;
    uint32_t sz;
    if (strchr(name, '/'))
        return NULL;
    if (!wf_plat_find_file(name, &data, &sz))
        return NULL;
    if (size)
        *size = sz;
    return data;
}

static void real_path(const char *name, char *out, size_t size)
{
    if (strchr(name, '/'))
        snprintf(out, size, "%s", name);
    else
        snprintf(out, size, "%s/%s", wf_plat_save_dir(), name);
}

wf_file_t *wf_fopen(const char *name, const char *mode)
{
    bool writing = strchr(mode, 'w') || strchr(mode, 'a') || strchr(mode, '+');
    uint32_t size;
    const uint8_t *data = writing ? NULL : wf_file_data(name, &size);
    wf_file_t *f;
    if (data) {
        f = wf_calloc(1, sizeof(*f));
        if (!f)
            return NULL;
        f->mem = data;
        f->size = size;
        return f;
    }
    char path[256];
    real_path(name, path, sizeof(path));
    FILE *rf = fopen(path, mode);
    if (!rf)
        return NULL;
    f = wf_calloc(1, sizeof(*f));
    if (!f) {
        fclose(rf);
        return NULL;
    }
    f->f = rf;
    return f;
}

int wf_fclose(wf_file_t *f)
{
    if (!f)
        return EOF;
    int r = 0;
    if (f->f)
        r = fclose(f->f);
    wf_free(f);
    return r;
}

size_t wf_fread(void *ptr, size_t size, size_t n, wf_file_t *f)
{
    if (!f || !size)
        return 0;
    if (f->f)
        return fread(ptr, size, n, f->f);
    size_t avail = f->pos < f->size ? f->size - f->pos : 0;
    size_t items = avail / size;
    if (items > n)
        items = n;
    memcpy(ptr, f->mem + f->pos, items * size);
    f->pos += (uint32_t)(items * size);
    if (items < n && avail % size) {
        /* a partial item: read what is left, like stdio */
        memcpy((uint8_t *)ptr + items * size, f->mem + f->pos, avail % size);
        f->pos = f->size;
    }
    return items;
}

size_t wf_fwrite(const void *ptr, size_t size, size_t n, wf_file_t *f)
{
    if (!f || !f->f)
        return 0;
    return fwrite(ptr, size, n, f->f);
}

int wf_fseek(wf_file_t *f, long offset, int whence)
{
    if (!f)
        return -1;
    if (f->f)
        return fseek(f->f, offset, whence);
    long base = whence == SEEK_SET ? 0 : whence == SEEK_CUR ? (long)f->pos : (long)f->size;
    long p = base + offset;
    if (p < 0)
        return -1;
    f->pos = (uint32_t)p;
    return 0;
}

long wf_ftell(wf_file_t *f)
{
    if (!f)
        return -1;
    if (f->f)
        return ftell(f->f);
    return (long)f->pos;
}

int wf_feof(wf_file_t *f)
{
    if (!f)
        return 1;
    if (f->f)
        return feof(f->f);
    return f->pos >= f->size;
}

int wf_stat(const char *path, struct stat *st)
{
    uint32_t size;
    if (st)
        memset(st, 0, sizeof(*st));
    if (wf_file_data(path, &size)) {
        if (st)
            st->st_size = size;
        return 0;
    }
    /* the save folder exists (made by the platform) */
    if (!strcmp(path, wf_plat_save_dir()))
        return 0;
    char p[256];
    real_path(path, p, sizeof(p));
    FILE *rf = fopen(p, "rb");
    if (!rf) {
        errno = ENOENT;
        return -1;
    }
    fclose(rf);
    return 0;
}

int wf_mkdir(const char *path)
{
    (void)path; /* only the save folder is used: made by the platform */
    return 0;
}

int wf_unlink(const char *path)
{
    char p[256];
    real_path(path, p, sizeof(p));
    return remove(p);
}
