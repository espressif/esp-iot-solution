/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "esl_image_codec.h"

#include <inttypes.h>
#include <stdio.h>
#include "esp_log.h"

void esl_image_hex_dump(const char *tag, uint8_t image_index, uint32_t offset,
                        const uint8_t *data, size_t len, size_t bytes_per_line)
{
    if (data == NULL || len == 0 || bytes_per_line == 0) {
        return;
    }

    char line[128];
    size_t pos = 0;

    for (size_t i = 0; i < len; i += bytes_per_line) {
        size_t chunk = len - i;
        if (chunk > bytes_per_line) {
            chunk = bytes_per_line;
        }

        pos = snprintf(line, sizeof(line), "img=%u off=%" PRIu32 "+%zu:",
                       image_index, offset + (uint32_t)i, chunk);
        for (size_t j = 0; j < chunk && pos < sizeof(line) - 4; j++) {
            pos += snprintf(line + pos, sizeof(line) - pos, " %02x", data[i + j]);
        }
        ESP_LOGI(tag, "%s", line);
    }
}
