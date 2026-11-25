/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 * SPDX-License-Identifier: Apache-2.0
 */
/** @file bounded.c
 *  @brief Shared error-bounded 4x4 tile decoder for EAF and EAF4 payload profiles.
 */
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#include "codec_internal.h"
#include "bounded.h"

#include "eaf_port.h"

typedef struct {
    esp_eaf_container_t container;
    unsigned pixel_bytes;
    uint16_t width;
    uint16_t height;
    uint16_t y_offset;
    const uint8_t *modes;
    const uint8_t *src;
    const uint8_t *end;
    uint8_t *output;
    uint8_t *owned_payload;
    eaf_decoder_t *decoder;
    bool swap_rgb565;
} bounded_view_t;

static uint16_t read_le16(const uint8_t *p)
{
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

static uint32_t read_le32(const uint8_t *p)
{
    return p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static uint32_t bgra_word(uint8_t b, uint8_t g, uint8_t r, uint8_t a)
{
#if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
    return ((uint32_t)b << 24) | ((uint32_t)g << 16) | ((uint32_t)r << 8) | a;
#else
    return b | ((uint32_t)g << 8) | ((uint32_t)r << 16) | ((uint32_t)a << 24);
#endif
}

static uint32_t read_color(const bounded_view_t *view, const uint8_t *p)
{
    return view->pixel_bytes == 2 ? read_le16(p) : bgra_word(p[0], p[1], p[2], p[3]);
}

static void interpolate_palette(const bounded_view_t *view, uint32_t *palette)
{
    if (view->pixel_bytes == 2) {
        uint16_t c0 = (uint16_t)palette[0];
        uint16_t c3 = (uint16_t)palette[3];
        int r0 = c0 >> 11, g0 = (c0 >> 5) & 63, b0 = c0 & 31;
        int r3 = c3 >> 11, g3 = (c3 >> 5) & 63, b3 = c3 & 31;
        palette[1] = (uint16_t)((((2 * r0 + r3) / 3) << 11) |
                                (((2 * g0 + g3) / 3) << 5) | ((2 * b0 + b3) / 3));
        palette[2] = (uint16_t)((((r0 + 2 * r3) / 3) << 11) |
                                (((g0 + 2 * g3) / 3) << 5) | ((b0 + 2 * b3) / 3));
        return;
    }

    const uint8_t *c0 = (const uint8_t *)&palette[0];
    const uint8_t *c3 = (const uint8_t *)&palette[3];
    palette[1] = bgra_word((uint8_t)((2U * c0[0] + c3[0]) / 3U),
                           (uint8_t)((2U * c0[1] + c3[1]) / 3U),
                           (uint8_t)((2U * c0[2] + c3[2]) / 3U),
                           (uint8_t)((2U * c0[3] + c3[3]) / 3U));
    palette[2] = bgra_word((uint8_t)((c0[0] + 2U * c3[0]) / 3U),
                           (uint8_t)((c0[1] + 2U * c3[1]) / 3U),
                           (uint8_t)((c0[2] + 2U * c3[2]) / 3U),
                           (uint8_t)((c0[3] + 2U * c3[3]) / 3U));
}

static void store_color(const bounded_view_t *view, size_t index, uint32_t color)
{
    if (view->pixel_bytes == 4) {
        ((uint32_t *)view->output)[index] = color;
        return;
    }
    uint16_t rgb565 = (uint16_t)color;
    ((uint16_t *)view->output)[index] = view->swap_rgb565 ? __builtin_bswap16(rgb565) : rgb565;
}

static int prepare_view(const eaf_codec_request_t *request, bounded_view_t *view)
{
    *view = (bounded_view_t) {
        .container = request->container
    };
    if (request->container == ESP_EAF_CONTAINER_EAF4) {
        bool is_565 = (request->mode == EAF4_FRAME_TILE_BOUNDED_RGB565);
        bool is_8888 = (request->mode == EAF4_FRAME_TILE_BOUNDED_BGRA8888);
        if ((!is_565 && !is_8888) || !request->frame || !request->scratch ||
                !request->frame_width || !request->frame_height || request->frame_width % 4 ||
                request->frame_height % 4) {
            return -1;
        }
        size_t tiles = (size_t)request->frame_width * request->frame_height / 16;
        if (request->decoded_size < tiles) {
            return -1;
        }
        view->pixel_bytes = is_565 ? 2 : 4;
        view->width = request->frame_width;
        view->height = request->frame_height;
        view->modes = request->scratch;
        view->src = request->scratch + tiles;
        view->end = request->scratch + request->decoded_size;
        view->output = request->frame;
        return 0;
    }

    if (request->container != ESP_EAF_CONTAINER_EAF || !request->decoder || !request->header ||
            !request->block_data || !request->decode_buffer || request->block_len < 5 ||
            request->width <= 0 || request->block_height <= 0 || request->width % 4 ||
            request->block_height % 4 ||
            eaf_decoder_get_output_format(request->decoder) != ESP_EAF_OUT_RGB565) {
        return -1;
    }
    size_t tiles = (size_t)(request->width / 4) * (request->block_height / 4);
    size_t raw_len = read_le32(request->block_data + 1);
    if (raw_len < tiles || raw_len > tiles * 33) {
        return -1;
    }
    uint8_t *raw = eaf_port_alloc_internal(raw_len);
    if (!raw) {
        raw = eaf_alloc(raw_len);
    }
    if (!raw) {
        return -1;
    }
    if (eaf_port_inflate_raw(raw, raw_len, request->block_data + 5,
                             request->block_len - 5) != raw_len) {
        free(raw);
        return -1;
    }
    view->pixel_bytes = 2;
    view->width = (uint16_t)request->width;
    view->height = (uint16_t)request->block_height;
    view->y_offset = (uint16_t)(request->block_index * request->header->block_height);
    view->modes = raw;
    view->src = raw + tiles;
    view->end = raw + raw_len;
    view->output = request->decode_buffer;
    view->owned_payload = raw;
    view->decoder = request->decoder;
    view->swap_rgb565 = request->swap_color;
    return 0;
}

int eaf_bounded_decode(const eaf_codec_request_t *request)
{
    if (!request) {
        return -1;
    }
    bounded_view_t view;
    if (prepare_view(request, &view) != 0) {
        return -1;
    }

    const size_t tiles = (size_t)(view.width / 4) * (view.height / 4);
    const unsigned sizes_565[] = {0, 2, 6, 8, 32};
    const unsigned sizes_8888[] = {0, 4, 10, 12, 64};
    const unsigned *sizes = view.pixel_bytes == 2 ? sizes_565 : sizes_8888;
    int result = 0;
    uint16_t min_x = view.width, min_y = view.height, max_x = 0, max_y = 0;

    for (size_t tile = 0; tile < tiles; tile++) {
        unsigned mode = view.modes[tile];
        if (mode > BOUNDED_BLOCK_LITERAL || (size_t)(view.end - view.src) < sizes[mode]) {
            result = -1;
            break;
        }
        if (mode == BOUNDED_BLOCK_SKIP) {
            continue;
        }

        unsigned tx = (unsigned)(tile % (view.width / 4)) * 4;
        unsigned ty = (unsigned)(tile / (view.width / 4)) * 4;
        if (tx < min_x) {
            min_x = (uint16_t)tx;
        }
        if (ty < min_y) {
            min_y = (uint16_t)ty;
        }
        if (tx + 3 > max_x) {
            max_x = (uint16_t)(tx + 3);
        }
        if (ty + 3 > max_y) {
            max_y = (uint16_t)(ty + 3);
        }
        uint32_t palette[4] = {0};
        uint32_t mask = 0;
        if (mode < BOUNDED_BLOCK_LITERAL) {
            palette[0] = read_color(&view, view.src);
            if (mode == BOUNDED_BLOCK_PALETTE2) {
                palette[1] = read_color(&view, view.src + view.pixel_bytes);
                mask = read_le16(view.src + 2 * view.pixel_bytes);
            } else if (mode == BOUNDED_BLOCK_PALETTE4) {
                palette[3] = read_color(&view, view.src + view.pixel_bytes);
                mask = read_le32(view.src + 2 * view.pixel_bytes);
                interpolate_palette(&view, palette);
            }
        }
        for (unsigned pixel = 0; pixel < 16; pixel++) {
            uint32_t color;
            if (mode == BOUNDED_BLOCK_LITERAL) {
                color = read_color(&view, view.src + pixel * view.pixel_bytes);
            } else {
                unsigned index = mode == BOUNDED_BLOCK_SOLID ? 0 : mode == BOUNDED_BLOCK_PALETTE2 ? ((mask >> pixel) & 1U)
                                 : ((mask >> (pixel * 2U)) & 3U);
                color = palette[index];
            }
            size_t output_index = (size_t)(ty + pixel / 4) * view.width + tx + pixel % 4;
            store_color(&view, output_index, color);
        }
        view.src += sizes[mode];
        if (view.container == ESP_EAF_CONTAINER_EAF) {
            eaf_decoder_mark_dirty(view.decoder, (uint16_t)tx, (uint16_t)(view.y_offset + ty),
                                   (uint16_t)(tx + 3), (uint16_t)(view.y_offset + ty + 3));
        }
    }
    if (result == 0 && view.src != view.end) {
        result = -1;
    }
    if (result == 0 && request->eaf4_dirty) {
        if (min_x <= max_x && min_y <= max_y) {
            *request->eaf4_dirty = (eaf4_dirty_rect_t) {
                .x1 = min_x, .y1 = min_y, .x2 = max_x, .y2 = max_y, .valid = true
            };
        } else {
            *request->eaf4_dirty = (eaf4_dirty_rect_t) {
                0
            };
        }
    }
    free(view.owned_payload);
    return result;
}
