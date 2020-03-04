// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) 2020 Corellium LLC
 */

#include <mxml.h>
#include "qdict.h"
#include "eplist.h"

struct eplist_s {
    mxml_node_t *xml;
    qdict *ids;
};

eplist_t eplist_load(int srctype, void *src)
{
    eplist_t epl = calloc(1, sizeof(struct eplist_s));
    mxml_node_t *xn, **pxn;
    const char *id;

    if(!epl)
        return NULL;
    epl->ids = qdict_new(sizeof(mxml_node_t *));
    if(!epl->ids) {
        free(epl);
        return NULL;
    }

    switch(srctype) {
    case EPLIST_LOAD_FILE:
        epl->xml = mxmlLoadFile(NULL, src, MXML_OPAQUE_CALLBACK);
        break;
    case EPLIST_LOAD_STRING:
        epl->xml = mxmlLoadString(NULL, src, MXML_OPAQUE_CALLBACK);
        break;
    default:
        qdict_free(epl->ids);
        free(epl);
        return NULL;
    }

    if(!epl->xml) {
        qdict_free(epl->ids);
        free(epl);
        return NULL;
    }

    for(xn=epl->xml; xn; xn=mxmlWalkNext(xn, epl->xml, MXML_DESCEND)) {
        id = mxmlElementGetAttr(xn, "ID");
        if(id) {
            pxn = qdict_find(epl->ids, id, QDICT_ADD);
            if(pxn)
                *pxn = xn;
        }
    }

    for(xn=epl->xml; xn; xn=mxmlWalkNext(xn, epl->xml, MXML_DESCEND)) {
        id = mxmlElementGetAttr(xn, "IDREF");
        if(id) {
            pxn = qdict_find(epl->ids, id, QDICT_FIND);
            if(pxn)
                mxmlSetUserData(xn, *pxn);
        }
    }

    return epl;
}

void eplist_free(eplist_t epl)
{
    if(!epl)
        return;
    qdict_free(epl->ids);
    mxmlDelete(epl->xml);
    free(epl);
}

int eplist_type(epelem_t ee)
{
    mxml_node_t *xn = ee;
    const char *type;
    if(!ee)
        return 0;
    type = mxmlGetElement(xn);
    if(!type)
        return 0;
    if(!strcmp(type, "array"))
        return EPLIST_ARRAY;
    if(!strcmp(type, "dict"))
        return EPLIST_DICT;
    if(!strcmp(type, "integer"))
        return EPLIST_INTEGER;
    if(!strcmp(type, "string"))
        return EPLIST_STRING;
    if(!strcmp(type, "true") || !strcmp(type, "false"))
        return EPLIST_BOOL;
    if(!strcmp(type, "data"))
        return EPLIST_DATA;
    return 0;
}

epelem_t eplist_root(eplist_t epl)
{
    int type;
    mxml_node_t *xn = epl->xml;
    while(xn) {
        type = eplist_type(xn);
        if(type)
            return xn;
        xn = mxmlWalkNext(xn, epl->xml, MXML_DESCEND);
    }
    return NULL;
}

epelem_t eplist_next(epelem_t ee)
{
    mxml_node_t *xn = ee;
    const char *type;
    while(xn) {
        xn = mxmlGetNextSibling(xn);
        if(!xn)
            break;
        type = mxmlGetElement(xn);
        if(type && strcmp(type, "key"))
            return xn;
    }
    return xn;
}

epelem_t eplist_dict_find(epelem_t ee, const char *key, int expect_type)
{
    int et = eplist_type(ee);
    mxml_node_t *xn = ee;
    const char *type, *text;
    if(et != EPLIST_DICT)
        return NULL;
    xn = mxmlGetFirstChild(xn);
    while(xn) {
        type = mxmlGetElement(xn);
        if(!type) {
            xn = mxmlGetNextSibling(xn);
            continue;
        }
        if(strcmp(type, "key"))
            return NULL;
        text = mxmlGetOpaque(xn);
        xn = eplist_next(xn);
        if(!xn)
            return NULL;
        if(!strcmp(text, key)) {
            if(expect_type) {
                et = eplist_type(xn);
                if(et != expect_type)
                    return NULL;
            }
            return xn;
        }
        xn = mxmlGetNextSibling(xn);
    }
    return NULL;
}

epelem_t eplist_array_first(epelem_t ee)
{
    int et = eplist_type(ee);
    mxml_node_t *xn = ee;
    const char *type;
    if(et != EPLIST_ARRAY)
        return NULL;
    xn = mxmlGetFirstChild(xn);
    while(xn) {
        type = mxmlGetElement(xn);
        if(type)
            break;
        xn = mxmlGetNextSibling(xn);
    }
    return xn;
}

static epelem_t eplist_deref(epelem_t ee)
{
    mxml_node_t *xn;
    if(!ee)
        return NULL;
    xn = mxmlGetUserData(ee);
    if(xn)
        return xn;
    return ee;
}

const char *eplist_get_string(epelem_t ee)
{
    int et = eplist_type(ee);
    if(et != EPLIST_STRING)
        return NULL;
    return mxmlGetOpaque(eplist_deref(ee));
}

long long eplist_get_integer(epelem_t ee)
{
    int et = eplist_type(ee);
    const char *text;
    if(et != EPLIST_INTEGER)
        return -1ll;
    text = mxmlGetOpaque(eplist_deref(ee));
    if(!text)
        return -1ll;
    return strtoull(text, NULL, 0);
}

int eplist_get_bool(epelem_t ee)
{
    const char *type;
    if(!ee)
        return -1;
    type = mxmlGetElement(ee);
    if(!type)
        return -1;
    if(!strcmp(type, "false"))
        return 0;
    if(!strcmp(type, "true"))
        return 1;
    return -1;
}

static const unsigned char eplist_b64[] = {
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x3e, 0xff, 0xff, 0xff, 0x3f,
    0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3a, 0x3b, 0x3c, 0x3d, 0xff, 0xff, 0xff, 0x40, 0xff, 0xff,
    0xff, 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e,
    0x0f, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f, 0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28,
    0x29, 0x2a, 0x2b, 0x2c, 0x2d, 0x2e, 0x2f, 0x30, 0x31, 0x32, 0x33, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };

void *eplist_get_data(epelem_t ee, unsigned long *psize)
{
    int et = eplist_type(ee);
    unsigned i, ch, b;
    unsigned long size, nc = 0, np = 0;
    const unsigned char *text;
    unsigned char *out;
    if(et != EPLIST_DATA)
        return NULL;
    text = (const unsigned char *)mxmlGetOpaque(eplist_deref(ee));
    if(!text)
        return NULL;
    for(i=0; text[i]; i++) {
        ch = eplist_b64[text[i]];
        if(ch < 64) {
            if(np)
                return NULL;
            nc ++;
        }
        if(ch == 64)
            np ++;
    }
    if((nc + np) & 3)
        return NULL;
    size = (nc * 6) >> 3;
    out = malloc(size + 1);
    if(!out)
        return NULL;
    nc = np = 0;
    b = 0;
    for(i=0; text[i]; i++) {
        ch = eplist_b64[text[i]];
        if(ch < 64) {
            nc += 6;
            b |= ch << (32 - nc);
            if(nc >= 8) {
                out[np ++] = b >> 24;
                b <<= 8;
                nc -= 8;
            }
        }
    }
    out[np] = 0;
    if(psize)
        *psize = size;
    return out;
}
