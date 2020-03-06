// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) 2020 Corellium LLC
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "eplist.h"
#include "syscfg.h"
#include "mtfw.h"

#define GEN_1   1
#define GEN_2   2

static const struct {
    const char *provider;
    const char *syscfg;
} mtfw_providers[] = {
    { "multi-touch-calibration", "MtCl" },
    { "orb-gap-cal", "OrbG" },
    { "orb-force-cal", "OFCl" },
    { "shape-dynamic-accel-cal", "SDAC" },
    { "prox-calibration", "PxCl" },
    { "multi-touch-calibration", "MtCl" } };

static mtfw_item_t *mtfw_item_add(mtfw_item_t ***pptail, unsigned type, void *data, unsigned size, int copy)
{
    mtfw_item_t *item = calloc(1, sizeof(mtfw_item_t));
    if(!item)
        return NULL;
    item->type = type;
    if(copy) {
        item->data = malloc(size);
        if(!item->data) {
            free(item);
            return NULL;
        }
        if(data)
            memcpy(item->data, data, size);
        else
            memset(item->data, 0, size);
    } else
        item->data = data;
    item->size = size;
    **pptail = item;
    *pptail = &(item->next);
    return item;
}

static inline void mtfw_put16be(uint8_t *buf, uint16_t val)
{
    buf[0] = val >> 8;
    buf[1] = val;
}

static inline void mtfw_put32xe(uint8_t *buf, uint32_t val)
{
    buf[0] = val >> 8;
    buf[1] = val;
    buf[2] = val >> 24;
    buf[3] = val >> 16;
}

static uint32_t mtfw_sum(uint8_t *buf, unsigned size)
{
    uint32_t sum = 0;
    while(size --) {
        sum += *buf;
        buf ++;
    }
    return sum;
}

static mtfw_item_t *mtfw_item_add_regwr(mtfw_item_t ***pptail, uint32_t addr, uint32_t mask, uint32_t val)
{
    uint8_t buf[16];
    mtfw_put16be(&buf[0], 0x1E33);
    mtfw_put32xe(&buf[2], addr);
    mtfw_put32xe(&buf[6], mask);
    mtfw_put32xe(&buf[10], val);
    mtfw_put16be(&buf[14], mtfw_sum(&buf[2], 12));
    return mtfw_item_add(pptail, MTFW_WRITE_ACK, buf, sizeof(buf), 1);
}

static void mtfw_copy16be(uint8_t *dst, uint8_t *src, unsigned len)
{
    unsigned i;
    for(i=0; i<len; i++)
        dst[i^1] = src[i];
}

static mtfw_item_t *mtfw_item_add_calload(mtfw_item_t ***pptail, uint32_t addr, void *data, unsigned len)
{
    mtfw_item_t *mtfw = mtfw_item_add(pptail, MTFW_WRITE_ACK, NULL, 16 + ((len + 3) & -4), 1);
    uint8_t *buf;

    if(!mtfw) {
        free(data);
        return NULL;
    }
    buf = (uint8_t *)mtfw->data;

    mtfw_put32xe(&buf[0], 0x300118E1);
    mtfw_put16be(&buf[4], (len + 3) >> 2);
    mtfw_put32xe(&buf[6], addr);
    mtfw_put16be(&buf[10], mtfw_sum(&buf[4], 6));
    mtfw_copy16be(&buf[12], data, len);
    mtfw_put32xe(&buf[12 + ((len + 3) & -4)], mtfw_sum(data, len));

    return mtfw;
}

static void *mtfw_request_cal(const char *syscfg, const char *name, unsigned long *len)
{
    unsigned i;
    for(i=0; i<sizeof(mtfw_providers)/sizeof(mtfw_providers[0]); i++)
        if(!strcmp(mtfw_providers[i].provider, name))
            return syscfg_get(syscfg, mtfw_providers[i].syscfg, len);
    return NULL;
}

mtfw_item_t *mtfw_load_firmware(const char *pers, const char *fname, const char *syscfg)
{
    mtfw_item_t *head = NULL, **ptail = &head;
    FILE *f;
    eplist_t epl = NULL;
    epelem_t root, fw, fwcfg, seq, seql, act;
    void *bits, *fwcfgbits = NULL;
    unsigned long len;
    unsigned long long addr, mask, val;
    const char *acts;
    int mode, i;

    f = fopen(fname, "r");
    if(!f) {
        fprintf(stderr, "Failed to open input file.\n");
        goto fail;
    }
    epl = eplist_load(EPLIST_LOAD_FILE, f);
    fclose(f);

    if(!epl) {
        fprintf(stderr, "Failed to load input file.\n");
        goto fail;
    }

    root = eplist_root(epl);
    fw = eplist_dict_find(root, pers, EPLIST_DICT);
    if(!fw) {
        fprintf(stderr, "Firmware for the specified personality (%s) not found.\n", pers);
        goto fail;
    }

    seq = eplist_dict_find(fw, "Constructed Firmware", EPLIST_ARRAY);
    if(!seq) {
        seq = eplist_dict_find(fw, "Constructed Firmware", EPLIST_DATA);
        if(!seq) {
            fprintf(stderr, "Firmware does not contain preconstructed blobs.\n");
            goto fail;
        }
        mode = GEN_1;
    } else
        mode = GEN_2;

    if(!mtfw_item_add(&ptail, MTFW_SET_TYPE, &mode, 4, 1))
        goto fail;

    switch(mode) {
    case GEN_1:

        if(!mtfw_item_add(&ptail, MTFW_WRITE, "\x19\xC1", 2, 1))
            goto fail;
        for(i=0; i<3; i++)
            if(!mtfw_item_add(&ptail, MTFW_WRITE, "\x1A\xA1\x18\xE1\x18\xE1\x18\xE1\x18\xE1\x18\xE1\x18\xE1\x18\xE1", 16, 1))
                goto fail;

        bits = mtfw_request_cal(syscfg, "prox-calibration", &len);
        if(bits)
            if(!mtfw_item_add_calload(&ptail, 0x10009600, bits, len))
                goto fail;

        bits = mtfw_request_cal(syscfg, "multi-touch-calibration", &len);
        if(!bits) {
            fprintf(stderr, "Calibration sequence provider unavailable (%s).\n", "multi-touc-calibration");
            goto fail;
        }
        if(!mtfw_item_add_calload(&ptail, 0x10009000, bits, len))
            goto fail;

        bits = eplist_get_data(seq, &len);
        if(!bits) {
            fprintf(stderr, "Preconstructed blob item did not decode correctly.\n");
            goto fail;
        }
        if(!mtfw_item_add(&ptail, MTFW_WRITE_ACK, bits, len, 0)) {
            free(bits);
            goto fail;
        }

        if(!mtfw_item_add_regwr(&ptail, 0x10003060, -1u, 6099))
            goto fail;
        if(!mtfw_item_add_regwr(&ptail, 0x1000305c, -1u, 2))
            goto fail;
        if(!mtfw_item_add_regwr(&ptail, 0x10003058, -1u, 6))
            goto fail;
        if(!mtfw_item_add_regwr(&ptail, 0x10003000, -1u, 3))
            goto fail;
        if(!mtfw_item_add_regwr(&ptail, 0x10003518, -1u, 1))
            goto fail;

        if(!mtfw_item_add(&ptail, MTFW_WRITE_ACK, "\x1F\x01", 2, 1))
            goto fail;
        if(!mtfw_item_add(&ptail, MTFW_WRITE, "\x1D\x53\x34\x00\x10\x00\x00\x01\x00\x00\x00\x45", 12, 1))
            goto fail;

        break;

    case GEN_2:
        if(!mtfw_item_add(&ptail, MTFW_WRITE, "\x1A\xA1\x18\xE1", 4, 1))
            goto fail;

        seql = eplist_array_first(seq);
        while(seql) {
            if(eplist_type(seql) != EPLIST_DATA) {
                fprintf(stderr, "Non-data item in preconstructed blob array.\n");
                goto fail;
            }
            bits = eplist_get_data(seql, &len);
            if(!bits) {
                fprintf(stderr, "Preconstructed blob item did not decode correctly.\n");
                goto fail;
            }
            if(!mtfw_item_add(&ptail, MTFW_WRITE_ACK, bits, len, 0)) {
                free(bits);
                goto fail;
            }
            seql = eplist_next(seql);
        }

        fwcfg = eplist_dict_find(fw, "Firmware Config", EPLIST_DATA);
        if(!fwcfg) {
            fprintf(stderr, "Firmware does not contain configuration blob.\n");
            goto fail;
        }

        fwcfgbits = eplist_get_data(fwcfg, &len);
        if(!fwcfgbits) {
            fprintf(stderr, "Configuration blob did not decode correctly.\n");
            goto fail;
        }

        eplist_free(epl);

        epl = eplist_load(EPLIST_LOAD_STRING, fwcfgbits);
        if(!epl) {
            fprintf(stderr, "Failed to load configuration blob.\n");
            goto fail;
        }

        root = eplist_root(epl);

        seq = eplist_dict_find(root, "Calibration Sequence", EPLIST_ARRAY);
        if(!seq) {
            fprintf(stderr, "Failed to find calibration sequence.\n");
            goto fail;
        }

        seql = eplist_array_first(seq);
        while(seql) {
            if(eplist_type(seql) != EPLIST_DICT) {
                fprintf(stderr, "Non-dictionary item in calibration sequence array.\n");
                goto fail;
            }
            fw = eplist_dict_find(seql, "Address", EPLIST_INTEGER);
            if(!fw) {
                fprintf(stderr, "Incomplete item in calibration sequence array (no address).\n");
                goto fail;
            }
            addr = eplist_get_integer(fw);
            acts = eplist_get_string(eplist_dict_find(seql, "Provider", EPLIST_STRING));
            if(!acts) {
                fprintf(stderr, "Incomplete item in calibration sequence array (no provider).\n");
                goto fail;
            }
            bits = mtfw_request_cal(syscfg, acts, &len);
            if(!bits) {
                fprintf(stderr, "Calibration sequence provider unavailable (%s).\n", acts);
                goto fail;
            }
            if(!mtfw_item_add_calload(&ptail, addr, bits, len))
                goto fail;
            seql = eplist_next(seql);
        }

        seq = eplist_dict_find(root, "Boot Sequence", EPLIST_ARRAY);
        if(!seq) {
            fprintf(stderr, "Failed to find boot sequence.\n");
            goto fail;
        }

        seql = eplist_array_first(seq);
        while(seql) {
            if(eplist_type(seql) != EPLIST_DICT) {
                fprintf(stderr, "Non-dictionary item in boot sequence array.\n");
                goto fail;
            }
            act = eplist_dict_find(seql, "Action", EPLIST_STRING);
            acts = eplist_get_string(act);
            if(acts) {
                if(!strcmp(acts, "RequestCalibration")) {
                    if(!mtfw_item_add(&ptail, MTFW_WRITE_ACK, "\x1F\x01", 2, 1))
                        goto fail;
                } else {
                    fprintf(stderr, "Unexpected action item (%s) in boot sequence array.\n", acts);
                    goto fail;
                }
            } else {
                fw = eplist_dict_find(seql, "Address", EPLIST_INTEGER);
                if(!fw) {
                    fprintf(stderr, "Unexpected non-action item in boot sequence array.\n");
                    goto fail;
                }
                addr = eplist_get_integer(fw);
                mask = eplist_get_integer(eplist_dict_find(seql, "Mask", EPLIST_INTEGER));
                val = eplist_get_integer(eplist_dict_find(seql, "Value", EPLIST_INTEGER));
                if(!mtfw_item_add_regwr(&ptail, addr, mask, val))
                    goto fail;
            }
            seql = eplist_next(seql);
        }

        if(!mtfw_item_add(&ptail, MTFW_WAIT_IRQ, NULL, 0, 0))
            goto fail;

        break;
    }

    eplist_free(epl);
    free(fwcfgbits);

    return head;

fail:
    eplist_free(epl);
    free(fwcfgbits);
    return NULL;
}
