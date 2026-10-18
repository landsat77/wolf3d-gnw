#ifndef WF_PACK_H
#define WF_PACK_H

#include <stdint.h>
#include <stdbool.h>

#define WF_PACK_MAGIC "GWWF01\0\0"

enum {
    WF_PACK_OK = 0,
    WF_PACK_ERR_MAGIC = -1,
    WF_PACK_ERR_CORRUPT = -2,
};

#ifdef __cplusplus
extern "C" {
#endif

int wf_pack_open(const uint8_t *data, uint32_t size);
bool wf_pack_find(const char *name, const uint8_t **data, uint32_t *size);
uint32_t wf_pack_count(void);
const char *wf_pack_name(uint32_t index);

#ifdef __cplusplus
}
#endif

#endif
