/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include <stdlib.h>
#include "esp_xiaozhi_payload.h"
#include "esp_log.h"

static const char *TAG = "esp_xiaozhi_payload";

void esp_xiaozhi_payload_clear(esp_xiaozhi_payload_t *r)
{
    if (r->buf) {
        free(r->buf);
        r->buf = NULL;
    }
    r->len = 0;
    r->cap = 0;
}

esp_err_t esp_xiaozhi_payload_append(esp_xiaozhi_payload_t *r, const void *data, int data_len, int chunk_offset, int total_len, size_t max_payload)
{
    if (!data || data_len <= 0 || chunk_offset < 0) {
        return ESP_ERR_INVALID_ARG;
    }
    if (chunk_offset > (int)max_payload || data_len > (int)max_payload) {
        ESP_LOGE(TAG, "Invalid chunk parameters");
        return ESP_ERR_INVALID_SIZE;
    }
    if ((size_t)chunk_offset + (size_t)data_len > max_payload) {
        ESP_LOGE(TAG, "Reassembly exceeds max size");
        return ESP_ERR_INVALID_SIZE;
    }
    size_t need = (size_t)(chunk_offset + data_len);

    if (r->buf == NULL) {
        size_t alloc = need;
        if (total_len > 0 && (size_t)total_len <= max_payload) {
            alloc = (size_t)total_len;
        }
        if (alloc > max_payload) {
            alloc = max_payload;
        }
        r->buf = (uint8_t *)calloc(1, alloc);
        if (!r->buf) {
            ESP_LOGE(TAG, "Failed to allocate reassembly buffer");
            return ESP_ERR_NO_MEM;
        }
        r->cap = alloc;
    }

    if (need > r->cap) {
        size_t new_cap = need;
        if (new_cap > max_payload) {
            ESP_LOGE(TAG, "Reassembly buffer exceeds max");
            return ESP_ERR_INVALID_SIZE;
        }
        uint8_t *n = (uint8_t *)realloc(r->buf, new_cap);
        if (!n) {
            return ESP_ERR_NO_MEM;
        }
        r->buf = n;
        r->cap = new_cap;
    }

    memcpy(r->buf + chunk_offset, data, (size_t)data_len);
    if (need > r->len) {
        r->len = need;
    }
    return ESP_OK;
}
