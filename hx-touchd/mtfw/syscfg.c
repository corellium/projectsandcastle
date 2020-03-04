// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) 2018-20 Corellium LLC
 */

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>

#define DEFAULT_SIZE 131072
#define MAX_SIZE 8192

struct syscfg_hdr {
    char magic[4];
    uint32_t unk_0; /* 0x7C */
    uint32_t size;
    uint32_t version;
    uint32_t unk_1; /* 0 */
    uint32_t nkeys;
};

struct syscfg_key {
    char name[4];
    union {
        uint8_t value[16];
        struct {
            char name[4];
            uint32_t size;
            uint32_t offset;
            uint32_t rsvd; /* -1 */
        } jumbo;
    };
};

static void flip4(char *out, char *in)
{
    unsigned i;
    for(i=0; i<4; i++)
        out[i] = in[3-i];
}

void *syscfg_get(const char *fname, const char *elem, unsigned long *plen)
{
    FILE *f = fopen(fname, "rb");
    unsigned size, elen = 0;
    uint8_t *buf;
    struct syscfg_hdr *hdr;
    struct syscfg_key *key;
    unsigned idx;
    char name[5];
    void *eval = NULL, *res = NULL;

    if(!f) {
        fprintf(stderr, "Could not open file '%s'.\n", fname);
        return NULL;
    }
    fseek(f, 0, SEEK_END);
    size = ftell(f);
    fseek(f, 0, SEEK_SET);

    if(!size)
        size = DEFAULT_SIZE;

    buf = malloc(size);
    if(!buf) {
        fclose(f);
        fprintf(stderr, "Could not allocate memory.\n");
        return NULL;
    }
    size = fread(buf, 1, size, f);
    fclose(f);

    if(size < sizeof(struct syscfg_hdr)) {
        fprintf(stderr, "SysCfg too small for header.\n");
        free(buf);
        return NULL;
    }

    hdr = (void *)buf;
    if(memcmp(hdr->magic, "gfCS", 4)) {
        fprintf(stderr, "SysCfg header magic value incorrect.\n");
        free(buf);
        return NULL;
    }
    if(hdr->size > size) {
        fprintf(stderr, "SysCfg header declares %d bytes, but only %d in file.\n", hdr->size, size);
        free(buf);
        return NULL;
    }
    if(hdr->nkeys * sizeof(struct syscfg_key) + sizeof(struct syscfg_hdr) > hdr->size) {
        fprintf(stderr, "SysCfg header declares %d entries, does not fit in %d bytes.\n", hdr->nkeys, hdr->size);
        free(buf);
        return NULL;
    }

    key = (void *)(hdr + 1);
    name[4] = 0;
    for(idx=0; idx<hdr->nkeys; idx++)
        if(memcmp(key[idx].name, "BTNC", 4)) {
            flip4(name, key[idx].name);
            if(!strcmp(name, elem)) {
                eval = key[idx].value;
                elen = sizeof(key[idx].value);
                break;
            }
        } else {
            flip4(name, key[idx].jumbo.name);
            if(!strcmp(name, elem)) {
                if(key[idx].jumbo.offset > hdr->size || key[idx].jumbo.offset + key[idx].jumbo.size > hdr->size) {
                    fprintf(stderr, "SysCfg jumbo key '%s' does not fit in %d bytes (%d+%d).\n", name, hdr->size, key[idx].jumbo.offset, key[idx].jumbo.size);
                    free(buf);
                    return NULL;
                }
                eval = buf + key[idx].jumbo.offset;
                elen = key[idx].jumbo.size;
                break;
            }
        }

    if(eval) {
        res = malloc(elen);
        if(!res) {
            free(buf);
            fprintf(stderr, "Could not allocate memory.\n");
            return NULL;
        }

        memcpy(res, eval, elen);
    }

    free(buf);
    if(plen)
        *plen = elen;
    return res;
}
