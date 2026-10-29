/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "esp_lv_decoder_config.h"

#if ESP_LV_ENABLE_PJPG

#include <stdbool.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include "esp_heap_caps.h"
#include "esp_jpeg_dec.h"
#include "esp_lv_decoder_pjpg.h"

static uint16_t pjpg_read_le16(const uint8_t *p)
{
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

static uint32_t pjpg_read_le32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

esp_err_t esp_lv_pjpg_parse_header(const uint8_t *data, size_t size, pjpg_info_t *info)
{
    if (!data || !info || size < PJPG_HEADER_SIZE) {
        return ESP_ERR_INVALID_ARG;
    }
    if (memcmp(data, PJPG_MAGIC, PJPG_MAGIC_LEN) != 0 || data[7] != 1) {
        return ESP_ERR_NOT_SUPPORTED;
    }
    info->width = pjpg_read_le16(data + 8);
    info->height = pjpg_read_le16(data + 10);
    info->rgb_jpeg_size = pjpg_read_le32(data + 12);
    info->alpha_jpeg_size = pjpg_read_le32(data + 16);
    if (!info->width || !info->height || !info->rgb_jpeg_size ||
            (uint64_t)info->width * info->height > SIZE_MAX / 4 ||
            info->rgb_jpeg_size > INT_MAX || info->alpha_jpeg_size > INT_MAX) {
        return ESP_ERR_INVALID_SIZE;
    }
    return ESP_OK;
}

esp_err_t esp_lv_pjpg_validate_data(const uint8_t *data, size_t size, pjpg_info_t *info)
{
    esp_err_t ret = esp_lv_pjpg_parse_header(data, size, info);
    if (ret != ESP_OK) {
        return ret;
    }
    size_t payload_size = size - PJPG_HEADER_SIZE;
    if (info->rgb_jpeg_size > payload_size || info->alpha_jpeg_size > payload_size - info->rgb_jpeg_size) {
        return ESP_ERR_INVALID_SIZE;
    }
    return ESP_OK;
}

typedef struct {
    pjpg_info_t info;
    uint8_t *output;
    size_t stride;
    pjpg_output_format_t format;
    uint8_t *scratch;
    size_t scratch_size;
} pjpg_sw_session_t;

static void *pjpg_scratch_alloc(size_t size)
{
#if defined(CONFIG_SPIRAM) && CONFIG_SPIRAM
    void *buf = heap_caps_aligned_alloc(16, size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (buf) {
        return buf;
    }
#endif
    return heap_caps_aligned_alloc(16, size, MALLOC_CAP_DEFAULT);
}

static void pjpg_copy_rows(pjpg_sw_session_t *session, bool alpha, const uint8_t *source,
                           uint32_t first_y, uint32_t rows)
{
    const uint32_t width = session->info.width;
    const bool argb = session->format == PJPG_ARGB8888;
    const size_t source_stride = (size_t)width * ((alpha || argb) ? 3 : 2);
    for (uint32_t y = 0; y < rows; y++) {
        const uint8_t *src = source + y * source_stride;
        uint8_t *dst = session->output + (first_y + y) * session->stride;
        if (alpha) {
            if (argb) {
                for (uint32_t x = 0; x < width; x++) {
                    dst[x * 4 + 3] = src[x * 3];
                }
            } else {
                dst = session->output + session->stride * session->info.height +
                      (first_y + y) * (session->stride / 2);
                for (uint32_t x = 0; x < width; x++) {
                    dst[x] = src[x * 3];
                }
            }
        } else if (argb) {
            for (uint32_t x = 0; x < width; x++) {
                dst[x * 4] = src[x * 3 + 2];
                dst[x * 4 + 1] = src[x * 3 + 1];
                dst[x * 4 + 2] = src[x * 3];
                dst[x * 4 + 3] = 255;
            }
        } else {
            memcpy(dst, src, (size_t)width * 2);
        }
    }
}

static esp_err_t pjpg_decode_plane(pjpg_sw_session_t *session, const uint8_t *data, uint32_t size, bool alpha)
{
    const bool rgb888 = alpha || session->format == PJPG_ARGB8888;
    const bool block_mode = session->info.width % 8 == 0 && session->info.height % 8 == 0;
    jpeg_dec_config_t config = {
        .output_type = rgb888 ? JPEG_PIXEL_FORMAT_RGB888 :
        (session->format == PJPG_RGB565_BE ? JPEG_PIXEL_FORMAT_RGB565_BE : JPEG_PIXEL_FORMAT_RGB565_LE),
        .rotate = JPEG_ROTATE_0D,
        .block_enable = block_mode,
    };
    jpeg_dec_handle_t handle = NULL;
    jpeg_dec_io_t io = { .inbuf = (uint8_t *)data, .inbuf_len = (int)size };
    jpeg_dec_header_info_t header = {0};
    esp_err_t ret = ESP_FAIL;
    int buffer_size = 0;
    int process_count = 0;

    if (jpeg_dec_open(&config, &handle) != JPEG_ERR_OK || !handle) {
        goto cleanup;
    }
    if (jpeg_dec_parse_header(handle, &io, &header) != JPEG_ERR_OK ||
            header.width != session->info.width || header.height != session->info.height) {
        ret = ESP_ERR_INVALID_SIZE;
        goto cleanup;
    }
    if (jpeg_dec_get_outbuf_len(handle, &buffer_size) != JPEG_ERR_OK || buffer_size <= 0 ||
            jpeg_dec_get_process_count(handle, &process_count) != JPEG_ERR_OK ||
            process_count <= 0 || process_count > session->info.height) {
        goto cleanup;
    }
    if (session->scratch_size < (size_t)buffer_size) {
        free(session->scratch);
        session->scratch = pjpg_scratch_alloc(buffer_size);
        session->scratch_size = session->scratch ? buffer_size : 0;
        if (!session->scratch) {
            ret = ESP_ERR_NO_MEM;
            goto cleanup;
        }
    }
    io.outbuf = session->scratch;
    const size_t row_bytes = (size_t)header.width * (rgb888 ? 3 : 2);
    uint32_t output_y = 0;
    for (int i = 0; i < process_count; i++) {
        if (jpeg_dec_process(handle, &io) != JPEG_ERR_OK) {
            goto cleanup;
        }
        if (io.out_size <= 0 || (size_t)io.out_size > session->scratch_size || io.out_size % row_bytes != 0) {
            ret = ESP_ERR_INVALID_SIZE;
            goto cleanup;
        }
        uint32_t rows = io.out_size / row_bytes;
        const uint32_t remaining = header.height - output_y;
        if (!remaining || (rows > remaining && (!block_mode || i != process_count - 1))) {
            ret = ESP_ERR_INVALID_SIZE;
            goto cleanup;
        }
        if (rows > remaining) {
            rows = remaining;
        }
        pjpg_copy_rows(session, alpha, io.outbuf, output_y, rows);
        output_y += rows;
    }
    ret = output_y == header.height ? ESP_OK : ESP_ERR_INVALID_SIZE;

cleanup:
    if (handle) {
        jpeg_dec_close(handle);
    }
    return ret;
}

esp_err_t esp_lv_pjpg_decode_sw(const uint8_t *data, size_t size, uint8_t *output,
                                size_t output_size, size_t stride, pjpg_output_format_t format)
{
    pjpg_sw_session_t session = { .output = output, .stride = stride, .format = format };
    esp_err_t ret = esp_lv_pjpg_validate_data(data, size, &session.info);
    if (ret != ESP_OK) {
        return ret;
    }
    if (!output || (format != PJPG_RGB565_LE && format != PJPG_RGB565_BE && format != PJPG_ARGB8888)) {
        return ESP_ERR_INVALID_ARG;
    }
    const bool argb = format == PJPG_ARGB8888;
    if (stride < (size_t)session.info.width * (argb ? 4 : 2) ||
            (!argb && stride % 2 != 0) || stride > SIZE_MAX / session.info.height) {
        return ESP_ERR_INVALID_SIZE;
    }
    size_t required = stride * session.info.height;
    if (!argb) {
        if (required > SIZE_MAX - required / 2) {
            return ESP_ERR_INVALID_SIZE;
        }
        required += required / 2;
    }
    if (output_size < required) {
        return ESP_ERR_INVALID_SIZE;
    }

    const uint8_t *rgb = data + PJPG_HEADER_SIZE;
    ret = pjpg_decode_plane(&session, rgb, session.info.rgb_jpeg_size, false);
    if (ret == ESP_OK && session.info.alpha_jpeg_size) {
        ret = pjpg_decode_plane(&session, rgb + session.info.rgb_jpeg_size, session.info.alpha_jpeg_size, true);
    } else if (ret == ESP_OK && !argb) {
        uint8_t *alpha = output + stride * session.info.height;
        for (uint32_t y = 0; y < session.info.height; y++) {
            memset(alpha + y * (stride / 2), 255, session.info.width);
        }
    }
    free(session.scratch);
    return ret;
}

#endif /* ESP_LV_ENABLE_PJPG */
