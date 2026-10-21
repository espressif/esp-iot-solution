/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <assert.h>
#include <stddef.h>
#include <string.h>
#include "private/elf_symbol.h"

static int example(int n)
{
    return n * 2;
}

static const struct esp_elfsym exports[] = {
    ESP_ELFSYM_EXPORT_NAMED("custom_alias", example),
    ESP_ELFSYM_EXPORT(example),
    ESP_ELFSYM_END,
};

int main(void)
{
    assert(strcmp(exports[0].name, "custom_alias") == 0);
    assert(strcmp(exports[1].name, "example") == 0);
    assert(exports[0].sym == exports[1].sym);
    assert(((int (*)(int))exports[0].sym)(2) == 4);
    return 0;
}
