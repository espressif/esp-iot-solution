/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/* This executable deliberately never references the compiled registry. */
#include "esp_elf.h"

static int host_export(void)
{
    return 42;
}

const struct esp_elfsym g_esp_libc_elfsyms[] = {
    ESP_ELFSYM_EXPORT(host_export),
    ESP_ELFSYM_END
};

uintptr_t esp_elf_find_symbol(const char *name)
{
    (void)name;
    return 0;
}

int main(void)
{
    return 0;
}
