// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) 2020 Corellium LLC
 */

#ifndef _EPLIST_H
#define _EPLIST_H

typedef struct eplist_s *eplist_t;
typedef void *epelem_t;

#define EPLIST_LOAD_FILE        1
#define EPLIST_LOAD_STRING      2

eplist_t eplist_load(int srctype, void *src);
void eplist_free(eplist_t epl);

#define EPLIST_ARRAY            1
#define EPLIST_DICT             2
#define EPLIST_STRING           3
#define EPLIST_INTEGER          4
#define EPLIST_BOOL             5
#define EPLIST_DATA             6

int eplist_type(epelem_t ee);
epelem_t eplist_root(eplist_t epl);
epelem_t eplist_next(epelem_t ee);
epelem_t eplist_dict_find(epelem_t ee, const char *key, int expect_type);
epelem_t eplist_array_first(epelem_t ee);
const char *eplist_get_string(epelem_t ee);
long long eplist_get_integer(epelem_t ee);
int eplist_get_bool(epelem_t ee);
void *eplist_get_data(epelem_t ee, unsigned long *size);

#endif
