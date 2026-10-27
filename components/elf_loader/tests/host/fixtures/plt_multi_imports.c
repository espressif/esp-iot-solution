/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/* Build as an Xtensa shared object: four separate imports create several PLT
 * entries and the .got.loc PC-relative literal pool missing in #686. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

__attribute__((visibility("default"))) int plugin_entry(const char *str)
{
    char *copy = malloc(32);
    if (!copy) {
        return -1;
    }
    size_t length = strlen(str);
    printf("%s %u\n", str, (unsigned)length);
    free(copy);
    return (int)length;
}
