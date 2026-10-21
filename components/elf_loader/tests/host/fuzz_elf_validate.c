/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/* Optional ASan/UBSan smoke test: mutate real ELF fixtures without executing them. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "private/elf_validate.h"

static uint32_t random_word(void)
{
    static uint32_t state = 0x9e3779b9u;
    state ^= state << 13;
    state ^= state >> 17;
    state ^= state << 5;
    return state;
}

int main(int argc, char **argv)
{
    if (argc < 2) {
        return 2;
    }

    for (int i = 1; i < argc; ++i) {
        FILE *file = fopen(argv[i], "rb");
        if (!file || fseek(file, 0, SEEK_END) != 0) {
            return 2;
        }

        long length = ftell(file);
        if (length <= 0 || length > 65536 || fseek(file, 0, SEEK_SET) != 0) {
            fclose(file);
            return 2;
        }

        size_t size = (size_t)length;
        uint8_t *original = malloc(size);
        uint8_t *mutated = malloc(size);
        if (!original || !mutated || fread(original, 1, size, file) != size) {
            fclose(file);
            free(original);
            free(mutated);
            return 2;
        }
        fclose(file);

        if (esp_elf_validate(original, size) != 0) {
            fprintf(stderr, "Unexpectedly invalid fixture: %s\n", argv[i]);
            free(original);
            free(mutated);
            return 1;
        }

        for (unsigned iteration = 0; iteration < 4096; ++iteration) {
            memcpy(mutated, original, size);
            unsigned flips = (random_word() % 8) + 1;
            for (unsigned n = 0; n < flips; ++n) {
                size_t offset = random_word() % size;
                mutated[offset] ^= (uint8_t)(1u << (random_word() % 8));
            }
            (void)esp_elf_validate(mutated, size);
        }

        printf("Checked %u deterministic mutations of %s\n", 4096, argv[i]);
        free(original);
        free(mutated);
    }

    return 0;
}
