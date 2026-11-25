/*
 * SPDX-FileCopyrightText: 2025-2026 Espressif Systems (Shanghai) CO LTD
 * SPDX-License-Identifier: Apache-2.0
 */
/* Common adapters for legacy EAF codec payloads. */
#include <stdlib.h>
#include <string.h>

#include "codec_internal.h"
#include "eaf_port.h"

static const char *TAG = "eaf_compat_codec";

static uint16_t rgb565_from_bgr(const uint8_t *bgr, bool swap_color)
{
    uint16_t value = ((bgr[2] & 0xF8) << 8) | ((bgr[1] & 0xFC) << 3) |
                     ((bgr[0] & 0xF8) >> 3);
    return swap_color ? __builtin_bswap16(value) : value;
}

static esp_err_t huffman_rle_decode(const uint8_t *input_data, size_t input_size,
                                    uint8_t *output_buffer, size_t *out_size,
                                    bool swap_color)
{
    if (!out_size || *out_size == 0) {
        return ESP_ERR_INVALID_SIZE;
    }
    if (*out_size > SIZE_MAX / 2) {
        return ESP_ERR_INVALID_SIZE;
    }
    uint8_t *huffman_buffer = eaf_alloc(*out_size * 2);
    if (!huffman_buffer) {
        return ESP_ERR_NO_MEM;
    }
    size_t huffman_size = *out_size * 2;
    esp_err_t err = eaf_huffman_decode(input_data, input_size, huffman_buffer,
                                       &huffman_size, swap_color);
    if (err == ESP_OK) {
        err = eaf_rle_decode(huffman_buffer, huffman_size, output_buffer,
                             out_size, swap_color);
    }
    free(huffman_buffer);
    return err;
}

static esp_err_t decode_legacy_block(const eaf_codec_request_t *request,
                                     eaf_block_decoder_t block_decoder,
                                     bool is_jpeg)
{
    if (!request || !request->decoder || !request->header || !request->block_data ||
            request->block_len < 1 || !request->decode_buffer || !block_decoder ||
            request->block_height <= 0) {
        return ESP_ERR_INVALID_ARG;
    }
    esp_eaf_output_format_t out_fmt = eaf_decoder_get_output_format(request->decoder);
    if (out_fmt != ESP_EAF_OUT_RGB565 && out_fmt != ESP_EAF_OUT_BGRA8888) {
        return ESP_ERR_NOT_SUPPORTED;
    }
    if (is_jpeg && out_fmt != ESP_EAF_OUT_RGB565) {
        return ESP_ERR_NOT_SUPPORTED;
    }
    if (!is_jpeg && request->header->bit_depth == 4 &&
            out_fmt != ESP_EAF_OUT_RGB565) {
        return ESP_ERR_NOT_SUPPORTED;
    }
    const eaf_header_t *header = request->header;
    eaf_decoder_t *decoder = request->decoder;
    size_t pixel_count = (size_t)request->width * request->block_height;
    uint8_t *alpha_block = NULL;
    if (request->alpha_buffer) {
        size_t alpha_offset = (size_t)request->block_index * header->block_height * request->width;
        alpha_block = request->alpha_buffer + alpha_offset;
    }

    size_t decode_capacity;
    if (is_jpeg) {
        decode_capacity = pixel_count * 2;
    } else if (header->bit_depth == 24) {
        decode_capacity = pixel_count * 3;
    } else if (header->bit_depth == 8) {
        decode_capacity = pixel_count;
    } else if (header->bit_depth == 4) {
        decode_capacity = (pixel_count + 1) / 2;
    } else {
        return ESP_ERR_INVALID_ARG;
    }

    uint8_t *work_buffer = NULL;
    uint8_t *decode_target = request->decode_buffer;
    if (!is_jpeg) {
        work_buffer = eaf_alloc(decode_capacity);
        if (!work_buffer) {
            return ESP_ERR_NO_MEM;
        }
        decode_target = work_buffer;
    }

    if (is_jpeg && request->block_index == 0) {
        eaf_decoder_cache_jpeg_tables(decoder, request->block_data + 1,
                                      request->block_len - 1);
    }
    uint16_t y0 = (uint16_t)(request->block_index * header->block_height);
    uint16_t y1 = (uint16_t)(y0 + request->block_height - 1);
    eaf_decoder_mark_dirty(decoder, 0, y0, (uint16_t)(request->width - 1), y1);

    size_t out_size = decode_capacity;
    esp_err_t err = block_decoder(request->block_data + 1, request->block_len - 1,
                                  decode_target, &out_size, request->swap_color);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to decode block %d", request->block_index);
        free(work_buffer);
        return err;
    }
    if (is_jpeg) {
        if (out_size != pixel_count * 2) {
            return ESP_ERR_INVALID_SIZE;
        }
        if (alpha_block) {
            memset(alpha_block, 0xFF, pixel_count);
        }
        return ESP_OK;
    }

    uint16_t *dest = (uint16_t *)request->decode_buffer;
    uint32_t *dest32 = (uint32_t *)request->decode_buffer;
    bool out_8888 = out_fmt == ESP_EAF_OUT_BGRA8888;
    size_t dest_index = 0;
    if (header->bit_depth == 24) {
        if (out_size < pixel_count * 3) {
            free(work_buffer);
            return ESP_ERR_INVALID_SIZE;
        }
        for (size_t i = 0; i < pixel_count; i++) {
            const uint8_t *bgr = &work_buffer[i * 3];
            if (out_8888) {
                dest32[dest_index++] = (uint32_t)bgr[0] | ((uint32_t)bgr[1] << 8) |
                                       ((uint32_t)bgr[2] << 16) | 0xFF000000U;
            } else {
                dest[dest_index++] = rgb565_from_bgr(bgr, request->swap_color);
            }
            if (alpha_block) {
                alpha_block[i] = 0xFF;
            }
        }
    } else if (header->bit_depth == 8) {
        if (!header->palette || out_size < pixel_count) {
            free(work_buffer);
            return ESP_ERR_INVALID_SIZE;
        }
        if (out_8888) {
            const uint32_t *palette = (const uint32_t *)header->palette;
            for (size_t i = 0; i < pixel_count; i++) {
                dest32[i] = palette[work_buffer[i]];
            }
        } else {
            for (size_t i = 0; i < pixel_count; i++) {
                uint8_t index = work_buffer[i];
                uint16_t color;
                uint8_t alpha;
                if (request->palette_cache && request->palette_cache->color[index] != 0xFFFFFFFF) {
                    color = (uint16_t)request->palette_cache->color[index];
                    alpha = request->palette_cache->alpha[index];
                } else {
                    const uint8_t *p = &header->palette[index * 4];
                    alpha = p[3];
                    uint16_t rgb = ((p[2] & 0xF8) << 8) | ((p[1] & 0xFC) << 3) |
                                   ((p[0] & 0xF8) >> 3);
                    color = request->swap_color ? __builtin_bswap16(rgb) : rgb;
                    if (request->palette_cache) {
                        request->palette_cache->color[index] = color;
                        request->palette_cache->alpha[index] = alpha;
                    }
                }
                if (alpha) {
                    dest[dest_index] = color;
                }
                dest_index++;
                if (alpha_block) {
                    alpha_block[i] = alpha;
                }
            }
        }
    } else {
        if (!header->palette) {
            free(work_buffer);
            return ESP_ERR_INVALID_STATE;
        }
        if (out_size != (pixel_count + 1) / 2) {
            free(work_buffer);
            return ESP_ERR_INVALID_SIZE;
        }
        size_t pixel = 0;
        for (size_t i = 0; i < out_size && pixel < pixel_count; i++) {
            uint8_t indexes[2] = {(uint8_t)(work_buffer[i] >> 4),
                                  (uint8_t)(work_buffer[i] & 0x0F)
                                 };
            for (unsigned nibble = 0; nibble < 2 && pixel < pixel_count; nibble++) {
                uint8_t index = indexes[nibble];
                uint16_t color;
                uint8_t alpha;
                if (request->palette_cache && request->palette_cache->color[index] != 0xFFFFFFFF) {
                    color = (uint16_t)request->palette_cache->color[index];
                    alpha = request->palette_cache->alpha[index];
                } else {
                    const uint8_t *p = &header->palette[index * 4];
                    alpha = p[3];
                    uint16_t rgb = ((p[2] & 0xF8) << 8) | ((p[1] & 0xFC) << 3) |
                                   ((p[0] & 0xF8) >> 3);
                    color = request->swap_color ? __builtin_bswap16(rgb) : rgb;
                    if (request->palette_cache) {
                        request->palette_cache->color[index] = color;
                        request->palette_cache->alpha[index] = alpha;
                    }
                }
                if (alpha) {
                    dest[pixel] = color;
                }
                if (alpha_block) {
                    alpha_block[pixel] = alpha;
                }
                pixel++;
            }
        }
    }
    free(work_buffer);
    return ESP_OK;
}

esp_err_t eaf_rle_block_decode(const eaf_codec_request_t *request)
{
    return decode_legacy_block(request, eaf_rle_decode, false);
}

esp_err_t eaf_huffman_rle_block_decode(const eaf_codec_request_t *request)
{
    return decode_legacy_block(request, huffman_rle_decode, false);
}

esp_err_t eaf_jpeg_block_decode(const eaf_codec_request_t *request)
{
    return decode_legacy_block(request, eaf_jpeg_decode, true);
}

esp_err_t eaf_huffman_block_decode(const eaf_codec_request_t *request)
{
    return decode_legacy_block(request, eaf_huffman_decode, false);
}
