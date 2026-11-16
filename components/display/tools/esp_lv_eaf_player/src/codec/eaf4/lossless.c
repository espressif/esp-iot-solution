/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 * SPDX-License-Identifier: Apache-2.0
 */
/* Lossless EAF4 submodes (BGRA8888, RGB565, RGB565A8) with LUT + SWAR acceleration. */
#include <stdbool.h>
#include <string.h>

#include "codec_internal.h"
#include "eaf_port.h"

#define EAF4_JOINT_SYM_ZERO   40U
#define EAF4_JOINT_SYM_ESCAPE 255U

static uint16_t s_joint565_rgb_delta[81];
static uint8_t  s_joint565_a_delta[81];
static int8_t   s_joint8888_delta[81][4];
static uint32_t s_joint8888_packed[81];
static eaf_once_t s_lut_once = EAF_ONCE_INIT;

static uint32_t read_le32(const uint8_t *p)
{
    return p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static uint32_t channel_word(uint8_t b, uint8_t g, uint8_t r, uint8_t a)
{
#if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
    return ((uint32_t)b << 24) | ((uint32_t)g << 16) | ((uint32_t)r << 8) | a;
#else
    return b | ((uint32_t)g << 8) | ((uint32_t)r << 16) | ((uint32_t)a << 24);
#endif
}

static void eaf4_joint_lut_init(void)
{
    for (int s = 0; s < 81; s++) {
        int d0 = s / 27 - 1;
        int d1 = (s / 9) % 3 - 1;
        int d2 = (s / 3) % 3 - 1;
        int d3 = s % 3 - 1;
        s_joint565_rgb_delta[s] = (uint16_t)(((d0 & 31) << 11) | ((d1 & 63) << 5) | (d2 & 31));
        s_joint565_a_delta[s]   = (uint8_t)d3;
        s_joint8888_delta[s][0] = (int8_t)d0;
        s_joint8888_delta[s][1] = (int8_t)d1;
        s_joint8888_delta[s][2] = (int8_t)d2;
        s_joint8888_delta[s][3] = (int8_t)d3;
        s_joint8888_packed[s]   = channel_word((uint8_t)d0, (uint8_t)d1, (uint8_t)d2, (uint8_t)d3);
    }
}

static inline uint16_t eaf4_rgb565_add_per_channel(uint16_t v, uint16_t d)
{
    return (uint16_t)((((uint32_t)v & 0xF800U) + (d & 0xF800U)) & 0xF800U) |
           (uint16_t)((((uint32_t)v & 0x07E0U) + (d & 0x07E0U)) & 0x07E0U) |
           (uint16_t)((((uint32_t)v & 0x001FU) + (d & 0x001FU)) & 0x001FU);
}

static inline uint32_t eaf4_rgb565x2_add_per_channel(uint32_t w, uint32_t d)
{
    return (((w & 0xF800F800U) + (d & 0xF800F800U)) & 0xF800F800U) |
           (((w & 0x07E007E0U) + (d & 0x07E007E0U)) & 0x07E007E0U) |
           (((w & 0x001F001FU) + (d & 0x001F001FU)) & 0x001F001FU);
}

static inline uint32_t eaf4_u8x4_add_wrap(uint32_t a, uint32_t d)
{
    return ((a & 0x7F7F7F7FU) + (d & 0x7F7F7F7FU)) ^ ((a ^ d) & 0x80808080U);
}

static inline void set_dirty(const eaf_codec_request_t *request,
                             uint16_t min_x, uint16_t min_y, uint16_t max_x, uint16_t max_y)
{
    if (!request->eaf4_dirty) {
        return;
    }
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

static uint32_t count_mask_bits(const uint8_t *mask, uint32_t mask_size, uint32_t tiles)
{
    uint32_t changed = 0;
    uint32_t full_bytes = tiles >> 3;
    for (uint32_t b = 0; b < full_bytes; b++) {
        changed += (uint32_t)__builtin_popcount((unsigned)mask[b]);
    }
    if (full_bytes < mask_size) {
        uint8_t rem = (uint8_t)(tiles & 7U);
        uint8_t tail = (uint8_t)(mask[full_bytes] & ((1U << rem) - 1U));
        changed += (uint32_t)__builtin_popcount((unsigned)tail);
    }
    return changed;
}

static int reconstruct_565a8_tiles(const eaf_codec_request_t *request)
{
    uint32_t width = request->frame_width;
    uint32_t height = request->frame_height;
    uint32_t pixels = width * height;
    bool has_alpha = (request->frame_bytes == pixels * 3);
    if (!has_alpha && request->frame_bytes != pixels * 2) {
        return -1;
    }

    uint32_t tiles = pixels / 16;
    uint32_t columns = width / 4;
    uint32_t mask_size = (tiles + 7) / 8;
    const uint8_t *scratch = request->scratch;
    if (request->decoded_size < mask_size) {
        return -1;
    }

    uint32_t changed = count_mask_bits(scratch, mask_size, tiles);
    uint32_t tile_payload = has_alpha ? 48U : 32U;
    if (request->decoded_size != mask_size + changed * tile_payload) {
        return -1;
    }
    if (changed == 0) {
        set_dirty(request, 1, 1, 0, 0);
        return 0;
    }

    const uint8_t *c_low_p = scratch + mask_size;
    const uint8_t *c_high_p = c_low_p + changed * 16;
    const uint8_t *a_src_p = has_alpha ? (c_high_p + changed * 16) : NULL;
    uint16_t *fb16 = (uint16_t *)request->frame;
    uint8_t *alpha_dst = has_alpha ? (request->frame + pixels * 2) : NULL;

    uint16_t min_x = (uint16_t)width, min_y = (uint16_t)height, max_x = 0, max_y = 0;
    for (uint32_t byte_idx = 0; byte_idx < mask_size; byte_idx++) {
        uint8_t mbyte = scratch[byte_idx];
        while (mbyte != 0) {
            int bit = __builtin_ctz((unsigned)mbyte);
            mbyte &= (uint8_t)(mbyte - 1);
            uint32_t tile = (byte_idx << 3) + (uint32_t)bit;
            if (tile >= tiles) {
                break;
            }
            uint16_t ty = (uint16_t)((tile / columns) * 4);
            uint16_t tx = (uint16_t)((tile % columns) * 4);
            if (tx < min_x) {
                min_x = tx;
            }
            if (ty < min_y) {
                min_y = ty;
            }
            if (tx + 3 > max_x) {
                max_x = tx + 3;
            }
            if (ty + 3 > max_y) {
                max_y = ty + 3;
            }

            for (unsigned r = 0; r < 4; r++) {
                uint32_t row_off = (uint32_t)(ty + r) * width + tx;
                unsigned p = r * 4;
                uint32_t lo32 = read_le32(c_low_p + p);
                uint32_t hi32 = read_le32(c_high_p + p);
                uint32_t w0 = (lo32 & 0x00FFU) | ((hi32 & 0x00FFU) << 8) |
                              ((lo32 & 0xFF00U) << 8) | ((hi32 & 0xFF00U) << 16);
                uint32_t w1 = ((lo32 >> 16) & 0x00FFU) | (((hi32 >> 16) & 0x00FFU) << 8) |
                              (((lo32 >> 16) & 0xFF00U) << 8) | (((hi32 >> 16) & 0xFF00U) << 16);
                uint32_t *dst_c32 = (uint32_t *)(fb16 + row_off);
                dst_c32[0] ^= w0;
                dst_c32[1] ^= w1;
                if (alpha_dst && a_src_p) {
                    uint32_t *dst_a32 = (uint32_t *)(alpha_dst + row_off);
                    *dst_a32 = eaf4_u8x4_add_wrap(*dst_a32, read_le32(a_src_p + p));
                }
            }
            c_low_p += 16;
            c_high_p += 16;
            if (a_src_p) {
                a_src_p += 16;
            }
        }
    }
    set_dirty(request, min_x, min_y, max_x, max_y);
    return 0;
}

static int reconstruct_565a8_joint(const eaf_codec_request_t *request)
{
    eaf_port_call_once(&s_lut_once, eaf4_joint_lut_init);

    uint32_t width = request->frame_width;
    uint32_t height = request->frame_height;
    uint32_t pixels = width * height;
    bool has_alpha = (request->frame_bytes == pixels * 3);
    if (!has_alpha && request->frame_bytes != pixels * 2) {
        return -1;
    }

    uint32_t tiles = pixels / 16;
    uint32_t columns = width / 4;
    uint32_t mask_size = (tiles + 7) / 8;
    uint32_t decoded = request->decoded_size;
    const uint8_t *scratch = request->scratch;
    if (decoded < mask_size) {
        return -1;
    }

    uint32_t changed = count_mask_bits(scratch, mask_size, tiles);
    if (changed == 0) {
        set_dirty(request, 1, 1, 0, 0);
        return decoded == mask_size ? 0 : -1;
    }

    uint32_t symbol_count = changed * 16;
    if (decoded < mask_size + symbol_count) {
        return -1;
    }
    const uint8_t *sym = scratch + mask_size;
    uint32_t escaped = 0;
    for (uint32_t i = 0; i < symbol_count; i++) {
        if (sym[i] == EAF4_JOINT_SYM_ESCAPE) {
            escaped++;
        }
    }
    uint32_t esc_bytes_per_px = has_alpha ? 3U : 2U;
    if (decoded != mask_size + symbol_count + escaped * esc_bytes_per_px) {
        return -1;
    }

    const uint8_t *esc_lo = sym + symbol_count;
    const uint8_t *esc_hi = esc_lo + escaped;
    const uint8_t *esc_a = has_alpha ? (esc_hi + escaped) : NULL;
    uint16_t *fb16 = (uint16_t *)request->frame;
    uint8_t *alpha_dst = has_alpha ? (request->frame + pixels * 2) : NULL;

    uint32_t esc_i = 0;
    uint16_t min_x = (uint16_t)width, min_y = (uint16_t)height, max_x = 0, max_y = 0;
    for (uint32_t byte_idx = 0; byte_idx < mask_size; byte_idx++) {
        uint8_t mbyte = scratch[byte_idx];
        while (mbyte != 0) {
            int bit = __builtin_ctz((unsigned)mbyte);
            mbyte &= (uint8_t)(mbyte - 1);
            uint32_t tile = (byte_idx << 3) + (uint32_t)bit;
            if (tile >= tiles) {
                break;
            }
            uint16_t ty = (uint16_t)((tile / columns) * 4);
            uint16_t tx = (uint16_t)((tile % columns) * 4);
            if (tx < min_x) {
                min_x = tx;
            }
            if (ty < min_y) {
                min_y = ty;
            }
            if (tx + 3 > max_x) {
                max_x = tx + 3;
            }
            if (ty + 3 > max_y) {
                max_y = ty + 3;
            }

            for (unsigned r = 0; r < 4; r++, sym += 4) {
                uint32_t s4 = read_le32(sym);
                if (s4 == EAF4_JOINT_SYM_ZERO * 0x01010101U) {
                    continue;
                }
                uint32_t row_off = (uint32_t)(ty + r) * width + tx;
                uint16_t *dst_c = fb16 + row_off;
                uint8_t *dst_a = alpha_dst ? (alpha_dst + row_off) : NULL;
                bool slow = ((((s4 & 0x7F7F7F7FU) + 0x2F2F2F2FU) | s4) & 0x80808080U) != 0;
                if (!slow) {
                    uint8_t s0 = (uint8_t)s4, s1 = (uint8_t)(s4 >> 8), s2 = (uint8_t)(s4 >> 16), s3 = (uint8_t)(s4 >> 24);
                    uint32_t *c32 = (uint32_t *)dst_c;
                    c32[0] = eaf4_rgb565x2_add_per_channel(c32[0], (uint32_t)s_joint565_rgb_delta[s0] |
                                                           ((uint32_t)s_joint565_rgb_delta[s1] << 16));
                    c32[1] = eaf4_rgb565x2_add_per_channel(c32[1], (uint32_t)s_joint565_rgb_delta[s2] |
                                                           ((uint32_t)s_joint565_rgb_delta[s3] << 16));
                    if (dst_a) {
                        uint32_t d = (uint32_t)s_joint565_a_delta[s0] | ((uint32_t)s_joint565_a_delta[s1] << 8) |
                                     ((uint32_t)s_joint565_a_delta[s2] << 16) | ((uint32_t)s_joint565_a_delta[s3] << 24);
                        *(uint32_t *)dst_a = eaf4_u8x4_add_wrap(*(uint32_t *)dst_a, d);
                    }
                    continue;
                }
                for (unsigned k = 0; k < 4; k++) {
                    uint8_t s = sym[k];
                    if (s == EAF4_JOINT_SYM_ZERO) {
                        continue;
                    }
                    if (s == EAF4_JOINT_SYM_ESCAPE) {
                        if (esc_i >= escaped) {
                            return -1;
                        }
                        dst_c[k] = (uint16_t)esc_lo[esc_i] | ((uint16_t)esc_hi[esc_i] << 8);
                        if (dst_a && esc_a) {
                            dst_a[k] = esc_a[esc_i];
                        }
                        esc_i++;
                    } else if (s < 81) {
                        dst_c[k] = eaf4_rgb565_add_per_channel(dst_c[k], s_joint565_rgb_delta[s]);
                        if (dst_a) {
                            dst_a[k] = (uint8_t)(dst_a[k] + s_joint565_a_delta[s]);
                        }
                    } else {
                        return -1;
                    }
                }
            }
        }
    }
    set_dirty(request, min_x, min_y, max_x, max_y);
    return esc_i == escaped ? 0 : -1;
}

static int reconstruct_joint_delta(const eaf_codec_request_t *request)
{
    eaf_port_call_once(&s_lut_once, eaf4_joint_lut_init);

    uint32_t width = request->frame_width;
    uint32_t height = request->frame_height;
    uint32_t tiles = (request->frame_bytes / 4) / 16;
    uint32_t columns = width / 4;
    uint32_t mask_size = (tiles + 7) / 8;
    uint32_t decoded = request->decoded_size;
    const uint8_t *scratch = request->scratch;
    if (decoded < mask_size) {
        return -1;
    }

    uint32_t changed = count_mask_bits(scratch, mask_size, tiles);
    if (changed == 0) {
        set_dirty(request, 1, 1, 0, 0);
        return decoded == mask_size ? 0 : -1;
    }

    uint32_t symbol_count = changed * 16;
    if (decoded < mask_size + symbol_count) {
        return -1;
    }
    const uint8_t *sym = scratch + mask_size;
    uint32_t escaped = 0;
    for (uint32_t i = 0; i < symbol_count; i++) {
        if (sym[i] == EAF4_JOINT_SYM_ESCAPE) {
            escaped++;
        }
    }
    if (decoded != mask_size + symbol_count + escaped * 4) {
        return -1;
    }

    const uint8_t *escape = sym + symbol_count;
    uint32_t escape_index = 0;
    uint32_t *fb32 = (uint32_t *)request->frame;
    uint16_t min_x = (uint16_t)width, min_y = (uint16_t)height, max_x = 0, max_y = 0;

    for (uint32_t byte_idx = 0; byte_idx < mask_size; byte_idx++) {
        uint8_t mbyte = scratch[byte_idx];
        while (mbyte != 0) {
            int bit = __builtin_ctz((unsigned)mbyte);
            mbyte &= (uint8_t)(mbyte - 1);
            uint32_t tile = (byte_idx << 3) + (uint32_t)bit;
            if (tile >= tiles) {
                break;
            }
            uint16_t y = (uint16_t)((tile / columns) * 4);
            uint16_t x = (uint16_t)((tile % columns) * 4);
            if (x < min_x) {
                min_x = x;
            }
            if (y < min_y) {
                min_y = y;
            }
            if (x + 3 > max_x) {
                max_x = x + 3;
            }
            if (y + 3 > max_y) {
                max_y = y + 3;
            }

            for (unsigned r = 0; r < 4; r++, sym += 4) {
                uint32_t s4 = read_le32(sym);
                if (s4 == EAF4_JOINT_SYM_ZERO * 0x01010101U) {
                    continue;
                }
                uint32_t *dst32 = fb32 + ((uint32_t)(y + r) * width + x);
                bool slow = ((((s4 & 0x7F7F7F7FU) + 0x2F2F2F2FU) | s4) & 0x80808080U) != 0;
                if (!slow) {
                    dst32[0] = eaf4_u8x4_add_wrap(dst32[0], s_joint8888_packed[(uint8_t)s4]);
                    dst32[1] = eaf4_u8x4_add_wrap(dst32[1], s_joint8888_packed[(uint8_t)(s4 >> 8)]);
                    dst32[2] = eaf4_u8x4_add_wrap(dst32[2], s_joint8888_packed[(uint8_t)(s4 >> 16)]);
                    dst32[3] = eaf4_u8x4_add_wrap(dst32[3], s_joint8888_packed[(uint8_t)(s4 >> 24)]);
                    continue;
                }
                for (unsigned k = 0; k < 4; k++) {
                    uint8_t symbol = sym[k];
                    if (symbol == EAF4_JOINT_SYM_ZERO) {
                        continue;
                    }
                    if (symbol == EAF4_JOINT_SYM_ESCAPE) {
                        uint8_t *dst = (uint8_t *)&dst32[k];
                        dst[0] = escape[0 * escaped + escape_index];
                        dst[1] = escape[1 * escaped + escape_index];
                        dst[2] = escape[2 * escaped + escape_index];
                        dst[3] = escape[3 * escaped + escape_index];
                        escape_index++;
                    } else if (symbol < 81) {
                        dst32[k] = eaf4_u8x4_add_wrap(dst32[k], s_joint8888_packed[symbol]);
                    } else {
                        return -1;
                    }
                }
            }
        }
    }
    set_dirty(request, min_x, min_y, max_x, max_y);
    return 0;
}

static int reconstruct_tiles(const eaf_codec_request_t *request)
{
    uint32_t width = request->frame_width;
    uint32_t height = request->frame_height;
    uint32_t count = request->frame_bytes / 4;
    uint32_t tiles = count / 16;
    uint32_t mask_size = (tiles + 7) / 8;
    const uint8_t *scratch = request->scratch;
    uint32_t changed = count_mask_bits(scratch, mask_size, tiles);
    if (mask_size + changed * 64 != request->decoded_size) {
        return -1;
    }
    if (changed == 0) {
        set_dirty(request, 1, 1, 0, 0);
        return 0;
    }

    uint32_t ordinal = 0;
    uint32_t columns = width / 4;
    uint32_t *output = (uint32_t *)request->frame;
    const uint8_t *payload = scratch + mask_size;
    uint16_t min_x = (uint16_t)width, min_y = (uint16_t)height, max_x = 0, max_y = 0;

    for (uint32_t byte_idx = 0; byte_idx < mask_size; byte_idx++) {
        uint8_t mbyte = scratch[byte_idx];
        while (mbyte != 0) {
            int bit = __builtin_ctz((unsigned)mbyte);
            mbyte &= (uint8_t)(mbyte - 1);
            uint32_t tile = (byte_idx << 3) + (uint32_t)bit;
            if (tile >= tiles) {
                break;
            }
            uint16_t y = (uint16_t)((tile / columns) * 4);
            uint16_t x = (uint16_t)((tile % columns) * 4);
            if (x < min_x) {
                min_x = x;
            }
            if (y < min_y) {
                min_y = y;
            }
            if (x + 3 > max_x) {
                max_x = x + 3;
            }
            if (y + 3 > max_y) {
                max_y = y + 3;
            }

            if (request->mode == EAF4_FRAME_TILE_XOR_PACKED_BGRA8888) {
                const uint8_t *tile_src = payload + ordinal * 64;
                for (unsigned row = 0; row < 4; row++) {
                    uint32_t *dst = output + (uint32_t)(y + row) * width + x;
                    const uint8_t *src = tile_src + row * 16;
                    dst[0] ^= read_le32(src);
                    dst[1] ^= read_le32(src + 4);
                    dst[2] ^= read_le32(src + 8);
                    dst[3] ^= read_le32(src + 12);
                }
            } else if (request->mode == EAF4_FRAME_TILE_SUB_PACKED_BGRA8888) {
                const uint8_t *tile_src = payload + ordinal * 64;
                for (unsigned row = 0; row < 4; row++) {
                    uint32_t *dst = output + (uint32_t)(y + row) * width + x;
                    const uint8_t *src = tile_src + row * 16;
                    dst[0] = eaf4_u8x4_add_wrap(dst[0], read_le32(src));
                    dst[1] = eaf4_u8x4_add_wrap(dst[1], read_le32(src + 4));
                    dst[2] = eaf4_u8x4_add_wrap(dst[2], read_le32(src + 8));
                    dst[3] = eaf4_u8x4_add_wrap(dst[3], read_le32(src + 12));
                }
            } else {
                const uint8_t *b = payload + ordinal * 16;
                const uint8_t *g = b + changed * 16;
                const uint8_t *r = g + changed * 16;
                const uint8_t *a = r + changed * 16;
                for (unsigned row = 0; row < 4; row++) {
                    uint32_t *dst = output + (uint32_t)(y + row) * width + x;
                    for (unsigned col = 0; col < 4; col++) {
                        unsigned pixel = row * 4 + col;
                        dst[col] ^= channel_word(b[pixel], g[pixel], r[pixel], a[pixel]);
                    }
                }
            }
            ordinal++;
        }
    }
    set_dirty(request, min_x, min_y, max_x, max_y);
    return 0;
}

int eaf4_lossless_decode(const eaf_codec_request_t *request)
{
    if (!request || !request->frame || !request->scratch || !request->frame_width ||
            !request->frame_height || request->frame_bytes == 0) {
        return -1;
    }
    uint32_t count = request->frame_bytes / 4;
    switch (request->mode) {
    case EAF4_FRAME_FULL:
        if (request->decoded_size != request->frame_bytes) {
            return -1;
        }
        set_dirty(request, 0, 0, request->frame_width - 1, request->frame_height - 1);
        return 0;
    case EAF4_FRAME_HSUB_BGRA8888: {
        if (request->decoded_size != request->frame_bytes) {
            return -1;
        }
        uint32_t width = request->frame_width;
        uint32_t height = request->frame_height;
        uint32_t *output = (uint32_t *)request->frame;
        if (request->scratch != request->frame) {
            memcpy(output, request->scratch, request->frame_bytes);
        }
        for (uint32_t y = 0; y < height; y++) {
            uint32_t *row = output + y * width;
            uint32_t acc = row[0];
            for (uint32_t x = 1; x < width; x++) {
                acc = eaf4_u8x4_add_wrap(acc, row[x]);
                row[x] = acc;
            }
        }
        set_dirty(request, 0, 0, request->frame_width - 1, request->frame_height - 1);
        return 0;
    }
    case EAF4_FRAME_XOR_BGRA8888: {
        if (request->decoded_size != request->frame_bytes) {
            return -1;
        }
        uint32_t *output = (uint32_t *)request->frame;
        const uint32_t *difference = (const uint32_t *)request->scratch;
        for (uint32_t i = 0; i < count; i++) {
            output[i] ^= difference[i];
        }
        set_dirty(request, 0, 0, request->frame_width - 1, request->frame_height - 1);
        return 0;
    }
    case EAF4_FRAME_XOR_PLANAR_BGRA8888: {
        if (request->decoded_size != request->frame_bytes) {
            return -1;
        }
        uint32_t *output = (uint32_t *)request->frame;
        const uint8_t *b = request->scratch;
        const uint8_t *g = b + count;
        const uint8_t *r = g + count;
        const uint8_t *a = r + count;
        for (uint32_t i = 0; i < count; i++) {
            output[i] ^= channel_word(b[i], g[i], r[i], a[i]);
        }
        set_dirty(request, 0, 0, request->frame_width - 1, request->frame_height - 1);
        return 0;
    }
    case EAF4_FRAME_TILE_XOR_PLANAR_BGRA8888:
    case EAF4_FRAME_TILE_XOR_PACKED_BGRA8888:
    case EAF4_FRAME_TILE_SUB_PACKED_BGRA8888:
        return reconstruct_tiles(request);
    case EAF4_FRAME_TILE_JOINT_DELTA_BGRA8888:
        return reconstruct_joint_delta(request);
    case EAF4_FRAME_TILE_DELTA_RGB565:
        return reconstruct_565a8_tiles(request);
    case EAF4_FRAME_TILE_JOINT_DELTA_RGB565:
        return reconstruct_565a8_joint(request);
    default:
        return -1;
    }
}
