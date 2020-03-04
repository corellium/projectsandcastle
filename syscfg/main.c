// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) 2020 Corellium LLC
 */

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <syscfg.h>

int main(int argc, char *argv[])
{
    uint8_t *buf, ch;
    unsigned long len, explen = 0, i;
    char fmt = 's', sep = 0, *fa;
    uint64_t ubuf;

    if(argc < 3 || argc > 4) {
        fprintf(stderr, "usage: syscfg <syscfg.bin> <entry> [<format>]\n"
                        "       <syscfg.bin> is usually /dev/nvme0n3\n"
                        "       <entry> is 4-characted entry ID\n"
                        "       <format> is [<type>][<len>]\n"
                        "         <type> is 's' for string, 'z' for zero-terminated string,\n"
                        "                   'u' for unsigned decimal, 'x' for hex\n"
                        "                   (optionally followed by separator character)\n"
                        "         <len> is expected number of bytes (otherwise prints all)\n");
        return 1;
    }

    buf = syscfg_get(argv[1], argv[2], &len);
    if(!buf) {
        fprintf(stderr, "SysCfg item '%s' not found.\n", argv[2]);
        return 1;
    }

    if(argc >= 4) {
        fa = argv[3];
        if(fa[0] > '9') {
            fmt = fa[0];
            fa ++;
            if(fa[0] && (fa[0] < '0' || fa[0] > '9')) {
                sep = fa[0];
                fa ++;
            }
        }
        if(fa[0])
            explen = strtoul(fa, NULL, 0);
    }

    if(!explen)
        explen = len;

    if(fmt == 'u') {
        if(len > 8)
            len = 8;
        ubuf = 0;
        memcpy(&ubuf, buf, len);
        printf("%llu", (unsigned long long)ubuf);
        return 0;
    }

    for(i=0; i<explen; i++) {
        ch = (i < len) ? buf[i] : 0;
        switch(fmt) {
        case 's':
            putchar(ch);
            break;
        case 'z':
            if(ch == '\0')
                return 0;
            putchar(ch);
            break;
        case 'x':
            printf("%02x", ch);
            if(sep && i < explen - 1)
                putchar(sep);
            break;
        default:
            fprintf(stderr, "unknown format character '%c'.\n", fmt);
            return 1;
        }
    }

    return 0;
}
