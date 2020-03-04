// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) 2020 Corellium LLC
 */

#include <stdio.h>
#include <stdlib.h>

#include "mtfw.h"

static const char *type_names[] = { "-", "WRITE", "WRITE_ACK", "WAIT_IRQ" };

int main(void)
{
    mtfw_item_t *mtfw, *iter;
    unsigned i;

    mtfw = mtfw_load_firmware("C1F5D,2", "../D10.mtprops", "../syscfg.bin");

    for(iter=mtfw; iter; iter=iter->next) {
        printf("%-10s", type_names[iter->type]);
        if(iter->type == MTFW_WRITE || iter->type == MTFW_WRITE_ACK) {
            printf("%6d ", iter->size);
            for(i=0; i<iter->size && i<48; i++)
                printf(" %02x", iter->data[i]);
            if(iter->size > i)
                printf(" ...");
        }
        printf("\n");
    }

    return 0;
}
