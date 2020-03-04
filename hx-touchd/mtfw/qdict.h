// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) 2017-20 Corellium LLC
 */

#ifndef _QDICT_H
#define _QDICT_H

typedef struct qdict_s qdict;

#define QDICT_ADD       1
#define QDICT_FIND      2
#define QDICT_ANY       3

qdict *qdict_new(int size);
void *qdict_find(qdict *dict, const char *str, unsigned mode);
void qdict_iter(qdict *dict, void(*func)(void *param, const char *str, void *elem), void *param);
const char *qdict_str(void *elem);
void qdict_free(qdict *dict);

#endif
