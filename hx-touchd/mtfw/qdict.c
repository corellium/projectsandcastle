// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) 2017-20 Corellium LLC
 */

#define _POSIX_C_SOURCE 200809L

#include <string.h>
#include <stdlib.h>

#include "qdict.h"

typedef struct qelem_s {
    unsigned depth;
    struct qelem_s *left, *right, *parent, **up;
    char *str;
} qelem;

struct qdict_s {
    int size;
    qelem *root;
};

qdict *qdict_new(int size)
{
    qdict *dict = calloc(1, sizeof(qdict));
    if(!dict)
        return dict;
    dict->size = size;
    return dict;
}

static int qdict_delta(qelem *elem)
{
    int res = 0;
    if(elem->left)
        res -= elem->left->depth + 1;
    if(elem->right)
        res += elem->right->depth + 1;
    return res;
}

static void qdict_update(qelem *elem)
{
    elem->depth = 0;
    if(elem->left)
        elem->depth = elem->left->depth + 1;
    if(elem->right && elem->depth < elem->right->depth + 1)
        elem->depth = elem->right->depth + 1;
}

static void qdict_pivot(qelem *root, qelem *pivot)
{
    pivot->parent = root->parent;
    pivot->up = root->up;
    *(pivot->up) = pivot;

    if(pivot == root->left) {
        root->left = pivot->right;
        if(root->left) {
            root->left->parent = root;
            root->left->up = &(root->left);
        }

        root->parent = pivot;
        root->up = &(pivot->right);
        pivot->right = root;
    } else {
        root->right = pivot->left;
        if(root->right) {
            root->right->parent = root;
            root->right->up = &(root->right);
        }

        root->parent = pivot;
        root->up = &(pivot->left);
        pivot->left = root;
    }

    qdict_update(root);
    qdict_update(pivot);
}

static void qdict_rebalance(qelem *elem)
{
    int delta;

    qdict_update(elem);

    for(; elem; elem=elem->parent) {
        delta = qdict_delta(elem);
        if(delta < -1 || delta > 1) {
            if(delta < -2 || delta > 2)
                return;

            if(delta < -1) {
                if(qdict_delta(elem->left) > 0)
                    qdict_pivot(elem->left, elem->left->right);
                qdict_pivot(elem, elem->left);
            } else {
                if(qdict_delta(elem->right) < 0)
                    qdict_pivot(elem->right, elem->right->left);
                qdict_pivot(elem, elem->right);
            }
            elem = elem->parent;
        }

        if(!elem || !elem->parent)
            break;

        qdict_update(elem->parent);
    }
}

void *qdict_find(qdict *_dict, const char *str, unsigned mode)
{
    qdict *dict = _dict;
    qelem **pelem = &(dict->root), *parent = NULL;
    int cmp;

    while(*pelem) {
        cmp = strcmp(str, (*pelem)->str);
        if(!cmp) {
            if(!(mode & QDICT_FIND))
                return NULL;
            return (*pelem) + 1;
        }

        parent = *pelem;
        if(cmp < 0)
            pelem = &(*pelem)->left;
        else
            pelem = &(*pelem)->right;
    }

    if(!(mode & QDICT_ADD))
        return NULL;

    (*pelem) = calloc(1, sizeof(qelem) + dict->size);
    if(!*pelem)
        return NULL;

    (*pelem)->str = strdup(str);
    (*pelem)->up = pelem;
    (*pelem)->parent = parent;

    parent = *pelem;
    qdict_rebalance(*pelem);

    return parent + 1;
}

static void qdict_iter_recurse(qelem *elem, void(*func)(void *param, const char *str, void *elem), void *param)
{
    if(elem->left)
        qdict_iter_recurse(elem->left, func, param);
    func(param, elem->str, elem + 1);
    if(elem->right)
        qdict_iter_recurse(elem->right, func, param);
}

void qdict_iter(qdict *_dict, void(*func)(void *param, const char *str, void *elem), void *param)
{
    qdict *dict = _dict;
    if(dict->root)
        qdict_iter_recurse(dict->root, func, param);
}

const char *qdict_str(void *_elem)
{
    qelem *elem = _elem;
    return elem[-1].str;
}

static void qdict_free_recurse(qelem *elem)
{
    if(elem->left)
        qdict_free_recurse(elem->left);
    if(elem->right)
        qdict_free_recurse(elem->right);
    free(elem->str);
    free(elem);
}

void qdict_free(qdict *_dict)
{
    qdict *dict = _dict;
    if(dict->root)
        qdict_free_recurse(dict->root);
    free(dict);
}
