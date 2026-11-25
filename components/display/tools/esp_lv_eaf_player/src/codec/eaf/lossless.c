/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
/* SPDX-License-Identifier: Apache-2.0 */
/* Lossless codec payload used by the EAF container. */
#include "codec_internal.h"
#include "eaf_port.h"

#include <stdlib.h>

static inline uint16_t read_le16(const uint8_t *p)
{
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

static inline uint32_t read_le32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

#define EAF_LOSSLESS_INT_SCRATCH (64U * 1024U)
#define EAF_LOSSLESS_INT_RESERVE (32U * 1024U)

#define EAF_JOINT_SYM_ZERO   40U
#define EAF_JOINT_SYM_ESCAPE 255U

static uint16_t s_joint_rgb_delta[81];
static uint8_t s_joint_a_delta[81];
static eaf_once_t s_lut_once = EAF_ONCE_INIT;

static void eaf_joint_lut_init(void)
{
    for (int s = 0; s < 81; s++) {
        int dr = s / 27 - 1, dg = (s / 9) % 3 - 1, db = (s / 3) % 3 - 1, da = s % 3 - 1;
        s_joint_rgb_delta[s] = (uint16_t)(((dr & 31) << 11) | ((dg & 63) << 5) | (db & 31));
        s_joint_a_delta[s] = (uint8_t)da;
    }
}

static inline uint16_t eaf_rgb565_add_per_channel(uint16_t v, uint16_t d)
{
    return (uint16_t)((((uint32_t)v & 0xF800U) + (d & 0xF800U)) & 0xF800U) |
           (uint16_t)((((uint32_t)v & 0x07E0U) + (d & 0x07E0U)) & 0x07E0U) |
           (uint16_t)((((uint32_t)v & 0x001FU) + (d & 0x001FU)) & 0x001FU);
}

static inline uint32_t eaf_rgb565x2_add_per_channel(uint32_t w, uint32_t d)
{
    return (((w & 0xF800F800U) + (d & 0xF800F800U)) & 0xF800F800U) |
           (((w & 0x07E007E0U) + (d & 0x07E007E0U)) & 0x07E007E0U) |
           (((w & 0x001F001FU) + (d & 0x001F001FU)) & 0x001F001FU);
}

static esp_err_t eaf_lossless_restore_joint(const uint8_t *mask, size_t mask_bytes, int total_tiles, int tiles_x,
                                            int width, const uint8_t *sym, const uint8_t *esc, uint32_t n_esc,
                                            uint16_t *fb16, uint8_t *alpha_dst, bool swap_color,
                                            uint16_t *min_x, uint16_t *min_y, uint16_t *max_x, uint16_t *max_y)
{
    eaf_port_call_once(&s_lut_once, eaf_joint_lut_init);
    const uint8_t *esc_lo = esc;
    const uint8_t *esc_hi = esc + n_esc;
    const uint8_t *esc_a = esc + 2 * (size_t)n_esc;
    uint32_t esc_i = 0;
    for (size_t byte_idx = 0; byte_idx < mask_bytes; byte_idx++) {
        uint8_t mbyte = mask[byte_idx];
        while (mbyte != 0) {
            int bit = __builtin_ctz((unsigned)mbyte);
            mbyte &= (uint8_t)(mbyte - 1);
            int t = (int)(byte_idx << 3) + bit;
            if (t >= total_tiles) {
                break;
            }
            uint16_t ty = (t / tiles_x) * 4;
            uint16_t tx = (t % tiles_x) * 4;
            if (tx < *min_x) {
                *min_x = tx;
            }
            if (ty < *min_y) {
                *min_y = ty;
            }
            if (tx + 3 > *max_x) {
                *max_x = tx + 3;
            }
            if (ty + 3 > *max_y) {
                *max_y = ty + 3;
            }

            for (int r = 0; r < 4; r++, sym += 4) {
                if (read_le32(sym) == EAF_JOINT_SYM_ZERO * 0x01010101U) {
                    continue;
                }
                size_t row_off = (size_t)(ty + r) * width + tx;
                uint16_t *dst_c = fb16 + row_off;
                uint8_t *dst_a = alpha_dst ? (alpha_dst + row_off) : NULL;
                uint32_t s4 = read_le32(sym);
                /* Any byte >= 81 (escape or invalid) takes the per-pixel path */
                bool slow = ((((s4 & 0x7F7F7F7FU) + 0x2F2F2F2FU) | s4) & 0x80808080U) != 0;
                if (!slow && !swap_color) {
                    uint8_t s0 = (uint8_t)s4, s1 = (uint8_t)(s4 >> 8), s2 = (uint8_t)(s4 >> 16), s3 = (uint8_t)(s4 >> 24);
                    uint32_t *c32 = (uint32_t *)dst_c;
                    c32[0] = eaf_rgb565x2_add_per_channel(c32[0], (uint32_t)s_joint_rgb_delta[s0] |
                                                          ((uint32_t)s_joint_rgb_delta[s1] << 16));
                    c32[1] = eaf_rgb565x2_add_per_channel(c32[1], (uint32_t)s_joint_rgb_delta[s2] |
                                                          ((uint32_t)s_joint_rgb_delta[s3] << 16));
                    if (dst_a) {
                        uint32_t d = (uint32_t)s_joint_a_delta[s0] | ((uint32_t)s_joint_a_delta[s1] << 8) |
                                     ((uint32_t)s_joint_a_delta[s2] << 16) | ((uint32_t)s_joint_a_delta[s3] << 24);
                        uint32_t a = *(uint32_t *)dst_a;
                        *(uint32_t *)dst_a = ((a & 0x7F7F7F7FU) + (d & 0x7F7F7F7FU)) ^ ((a ^ d) & 0x80808080U);
                    }
                    continue;
                }
                for (int k = 0; k < 4; k++) {
                    uint8_t s = sym[k];
                    if (s == EAF_JOINT_SYM_ZERO) {
                        continue;
                    }
                    if (s == EAF_JOINT_SYM_ESCAPE) {
                        if (esc_i >= n_esc) {
                            return ESP_ERR_INVALID_SIZE;
                        }
                        uint16_t v = (uint16_t)esc_lo[esc_i] | ((uint16_t)esc_hi[esc_i] << 8);
                        dst_c[k] = swap_color ? __builtin_bswap16(v) : v;
                        if (dst_a) {
                            dst_a[k] = esc_a[esc_i];
                        }
                        esc_i++;
                    } else if (s < 81) {
                        uint16_t v = swap_color ? __builtin_bswap16(dst_c[k]) : dst_c[k];
                        v = eaf_rgb565_add_per_channel(v, s_joint_rgb_delta[s]);
                        dst_c[k] = swap_color ? __builtin_bswap16(v) : v;
                        if (dst_a) {
                            dst_a[k] = (uint8_t)(dst_a[k] + s_joint_a_delta[s]);
                        }
                    } else {
                        return ESP_ERR_INVALID_SIZE;
                    }
                }
            }
        }
    }
    return esc_i == n_esc ? ESP_OK : ESP_ERR_INVALID_SIZE;
}

esp_err_t eaf_lossless_decode(const eaf_codec_request_t *r)
{
    if (!r || !r->decoder ||
            eaf_decoder_get_output_format(r->decoder) != ESP_EAF_OUT_RGB565) {
        return ESP_ERR_NOT_SUPPORTED;
    }
    eaf_decoder_t *decoder = r->decoder;
    eaf_lossless_state_t *state = eaf_decoder_state(decoder, ESP_EAF_CODEC_LOSSLESS);
    if (!state) {
        return ESP_ERR_NO_MEM;
    }
    const eaf_header_t *header = r->header;
    const uint8_t *block_data = r->block_data;
    size_t block_len = r->block_len;
    int block_index = r->block_index;
    int width = r->width;
    int block_height = r->block_height;
    uint8_t *decode_buffer = r->decode_buffer;
    uint8_t *alpha_buffer = r->alpha_buffer;
    bool swap_color = r->swap_color;
    /* Codec 0x04 LOSSLESS: bit-exact 4x4 Temporal-XOR RGB565 + A8
     * Header after encoding_type(1B):
     *   uint8_t flags (bit 0: temporal XOR/delta from previous frame; 0 = keyframe overwrite)
     *   uint32_t raw_len
     *   uint16_t n_changed (number of changed 4x4 tiles)
     * Followed by raw deflate payload of size (block_len - 8).
     * Decompressed layout:
     *   mask[(total_4x4 + 7)/8] (1 bit per 4x4 tile: 1 = changed, 0 = unchanged)
     *   c_low[n_changed * 16]   (low byte of RGB565 or XOR delta)
     *   c_high[n_changed * 16]  (high byte of RGB565 or XOR delta)
     *   a_plane[n_changed * 16] (A8 byte or uint8 additive delta)
     *
     * flags == 3 (inter frame, joint-symbol layout) extends the header:
     *   uint32_t n_esc      (pixels stored as absolute values)
     *   uint32_t sym_comp   (size of the first deflate stream)
     * Followed by two raw deflate streams:
     *   1) mask[(total_4x4 + 7)/8] + sym[n_changed * 16]
     *      sym 0..80 = (dr+1)*27 + (dg+1)*9 + (db+1)*3 + (da+1), each delta in -1..1
     *      per channel (R5/G6/B5 wrap within the channel, A8 wraps mod 256);
     *      sym 255 = escape, pixel is taken from the escape stream
     *   2) esc_low[n_esc] + esc_high[n_esc] + esc_a[n_esc] (absolute RGB565 + A8)
     */
    if (block_len < 8 || width % 4 || block_height % 4) {
        return ESP_ERR_INVALID_SIZE;
    }
    uint8_t flags = block_data[1];
    if (flags != 0 && flags != 1 && flags != 3) {
        return ESP_ERR_INVALID_SIZE;
    }
    bool joint = (flags & 2) != 0;
    uint32_t raw_len = read_le32(block_data + 2);
    uint16_t n_changed = read_le16(block_data + 6);
    int tiles_x = width / 4;
    int tiles_y = block_height / 4;
    int total_tiles = tiles_x * tiles_y;
    size_t mask_bytes = (size_t)(total_tiles + 7) / 8;
    size_t hdr_len = 8;
    uint32_t n_esc = 0;
    uint32_t sym_comp = 0;
    if (joint) {
        if (block_len < 16) {
            return ESP_ERR_INVALID_SIZE;
        }
        n_esc = read_le32(block_data + 8);
        sym_comp = read_le32(block_data + 12);
        hdr_len = 16;
        if (sym_comp > block_len - hdr_len || n_esc > (uint32_t)n_changed * 16) {
            return ESP_ERR_INVALID_SIZE;
        }
    }
    size_t expected = mask_bytes + (size_t)n_changed * (joint ? 16 : 48);
    if (expected != raw_len || n_changed > total_tiles) {
        return ESP_ERR_INVALID_SIZE;
    }
    size_t scratch_need = (size_t)raw_len + (size_t)n_esc * 3;
    uint8_t *raw_buf = NULL;
    /* Use caller-reserved internal SRAM scratch if it fits; otherwise use PSRAM */
    if (scratch_need + 4 <= state->internal_capacity) {
        raw_buf = state->internal_scratch;
    }
    if (!raw_buf) {
        if (state->capacity < scratch_need + 4) {
            free(state->scratch);
            state->capacity = 0;
            state->scratch = eaf_alloc(scratch_need + 4);
            if (!state->scratch) {
                return ESP_ERR_NO_MEM;
            }
            state->capacity = scratch_need + 4;
        }
        raw_buf = state->scratch;
    }
    int64_t t_inf0 = eaf_port_now_us();
    size_t sym_in_len = joint ? sym_comp : block_len - hdr_len;
    size_t decomp_sz = eaf_port_inflate_raw(raw_buf, raw_len, block_data + hdr_len, sym_in_len);
    if (decomp_sz != raw_len) {
        return ESP_FAIL;
    }
    if (n_esc) {
        size_t esc_len = (size_t)n_esc * 3;
        decomp_sz = eaf_port_inflate_raw(raw_buf + raw_len, esc_len,
                                         block_data + hdr_len + sym_comp,
                                         block_len - hdr_len - sym_comp);
        if (decomp_sz != esc_len) {
            return ESP_FAIL;
        }
    }
    int64_t t_rest0 = eaf_port_now_us();
    state->stats.inflate_us += (uint64_t)(t_rest0 - t_inf0);
    const uint8_t *mask = raw_buf;
    unsigned actual_changed = 0;
    for (int t = 0; t < total_tiles; t++) {
        if (mask[t >> 3] & (1u << (t & 7))) {
            actual_changed++;
        }
    }
    if (actual_changed != n_changed) {
        return ESP_ERR_INVALID_SIZE;
    }
    const uint8_t *c_low = mask + mask_bytes;
    const uint8_t *c_high = c_low + (size_t)n_changed * 16;
    const uint8_t *a_src = c_high + (size_t)n_changed * 16;

    uint16_t *fb16 = (uint16_t *)decode_buffer;
    size_t blk_px_off = (size_t)block_index * header->block_height * width;
    uint8_t *alpha_dst = alpha_buffer ? (alpha_buffer + blk_px_off) : NULL;
    const uint16_t *fused_bg = state->fused_bg_rgb565 ? (state->fused_bg_rgb565 + blk_px_off) : NULL;
    uint16_t *fused_comp = state->fused_comp_rgb565 ? (state->fused_comp_rgb565 + blk_px_off) : NULL;
    uint16_t min_x = width, min_y = header->height, max_x = 0, max_y = 0;

    /* Pass 1: Fast sequential restore of RGB565 + A8 across changed 4x4 tiles */
    if (joint) {
        esp_err_t jerr = eaf_lossless_restore_joint(mask, mask_bytes, total_tiles, tiles_x, width,
                                                    mask + mask_bytes, raw_buf + raw_len, n_esc,
                                                    fb16, alpha_dst, swap_color,
                                                    &min_x, &min_y, &max_x, &max_y);
        if (jerr != ESP_OK) {
            return jerr;
        }
    } else {
        const uint8_t *c_low_p = c_low;
        const uint8_t *c_high_p = c_high;
        const uint8_t *a_src_p = a_src;
        for (size_t byte_idx = 0; byte_idx < mask_bytes; byte_idx++) {
            uint8_t mbyte = mask[byte_idx];
            if (mbyte == 0) {
                continue;
            }
            while (mbyte != 0) {
                int bit = __builtin_ctz((unsigned)mbyte);
                mbyte &= (uint8_t)(mbyte - 1);
                int t = (int)(byte_idx << 3) + bit;
                if (t >= total_tiles) {
                    break;
                }
                uint16_t ty = (t / tiles_x) * 4;
                uint16_t tx = (t % tiles_x) * 4;
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

                for (int r = 0; r < 4; r++) {
                    size_t row_off = (size_t)(ty + r) * width + tx;
                    uint16_t *dst_c = fb16 + row_off;
                    uint8_t *dst_a = alpha_dst ? (alpha_dst + row_off) : NULL;
                    int p = r * 4;
                    uint32_t lo32 = read_le32(c_low_p + p);
                    uint32_t hi32 = read_le32(c_high_p + p);
                    uint32_t w0, w1;
                    if (!swap_color) {
                        w0 = (lo32 & 0x00FFU) | ((hi32 & 0x00FFU) << 8) |
                             ((lo32 & 0xFF00U) << 8) | ((hi32 & 0xFF00U) << 16);
                        w1 = ((lo32 >> 16) & 0x00FFU) | (((hi32 >> 16) & 0x00FFU) << 8) |
                             (((lo32 >> 16) & 0xFF00U) << 8) | (((hi32 >> 16) & 0xFF00U) << 16);
                    } else {
                        w0 = ((lo32 & 0x00FFU) << 8) | (hi32 & 0x00FFU) |
                             ((lo32 & 0xFF00U) << 16) | ((hi32 & 0xFF00U) << 8);
                        w1 = (((lo32 >> 16) & 0x00FFU) << 8) | ((hi32 >> 16) & 0x00FFU) |
                             (((lo32 >> 16) & 0xFF00U) << 16) | (((hi32 >> 16) & 0xFF00U) << 8);
                    }
                    uint32_t *dst_c32 = (uint32_t *)dst_c;
                    if (flags & 1) {
                        dst_c32[0] ^= w0;
                        dst_c32[1] ^= w1;
                        if (dst_a) {
                            dst_a[0] = (uint8_t)(dst_a[0] + a_src_p[p + 0]);
                            dst_a[1] = (uint8_t)(dst_a[1] + a_src_p[p + 1]);
                            dst_a[2] = (uint8_t)(dst_a[2] + a_src_p[p + 2]);
                            dst_a[3] = (uint8_t)(dst_a[3] + a_src_p[p + 3]);
                        }
                    } else {
                        dst_c32[0] = w0;
                        dst_c32[1] = w1;
                        if (dst_a) {
                            *(uint32_t *)dst_a = read_le32(a_src_p + p);
                        }
                    }
                }
                c_low_p += 16;
                c_high_p += 16;
                a_src_p += 16;
            }
        }
    }

    /* Pass 2: Per-stripe cache-hot fused blend for changed 4x4 tiles */
    if (fused_bg && fused_comp && alpha_dst) {
        for (size_t byte_idx = 0; byte_idx < mask_bytes; byte_idx++) {
            uint8_t mbyte = mask[byte_idx];
            if (mbyte == 0) {
                continue;
            }
            while (mbyte != 0) {
                int bit = __builtin_ctz((unsigned)mbyte);
                mbyte &= (uint8_t)(mbyte - 1);
                int t = (int)(byte_idx << 3) + bit;
                if (t >= total_tiles) {
                    break;
                }
                uint16_t ty = (t / tiles_x) * 4;
                uint16_t tx = (t % tiles_x) * 4;
                for (int r = 0; r < 4; r++) {
                    size_t row_off = (size_t)(ty + r) * width + tx;
                    const uint32_t *fg32 = (const uint32_t *)(fb16 + row_off);
                    uint32_t a32 = *(const uint32_t *)(alpha_dst + row_off);
                    uint32_t *comp32 = (uint32_t *)(fused_comp + row_off);
                    if (a32 == 0xFFFFFFFFU) {
                        comp32[0] = fg32[0];
                        comp32[1] = fg32[1];
                    } else if (a32 == 0U) {
                        const uint32_t *bg32 = (const uint32_t *)(fused_bg + row_off);
                        comp32[0] = bg32[0];
                        comp32[1] = bg32[1];
                    } else {
                        const uint16_t *fg_row = fb16 + row_off;
                        const uint8_t *a_row = alpha_dst + row_off;
                        const uint16_t *bg_row = fused_bg + row_off;
                        uint16_t *comp_row = fused_comp + row_off;
                        for (int k = 0; k < 4; k++) {
                            uint8_t a = a_row[k];
                            uint16_t bg_px = bg_row[k];
                            uint16_t fg_c = fg_row[k];
                            if (a == 0) {
                                comp_row[k] = bg_px;
                            } else if (a == 255 || fg_c == bg_px) {
                                comp_row[k] = fg_c;
                            } else {
                                uint32_t mix5 = ((uint32_t)a + 4U) >> 3;
                                uint32_t bg_p = ((uint32_t)bg_px | ((uint32_t)bg_px << 16)) & 0x07E0F81FU;
                                uint32_t fg_p = ((uint32_t)fg_c | ((uint32_t)fg_c << 16)) & 0x07E0F81FU;
                                uint32_t res = ((((fg_p - bg_p) * mix5) >> 5) + bg_p) & 0x07E0F81FU;
                                comp_row[k] = (uint16_t)((res >> 16) | res);
                            }
                        }
                    }
                }
            }
        }
    }
    if (max_x >= min_x) {
        min_y += block_index * header->block_height;
        max_y += block_index * header->block_height;
        if (min_x < decoder->dirty.x1) {
            decoder->dirty.x1 = min_x;
        }
        if (min_y < decoder->dirty.y1) {
            decoder->dirty.y1 = min_y;
        }
        if (max_x > decoder->dirty.x2) {
            decoder->dirty.x2 = max_x;
        }
        if (max_y > decoder->dirty.y2) {
            decoder->dirty.y2 = max_y;
        }
        decoder->dirty.valid = true;

        eaf_decoder_emit_tile_dirty_bands(decoder, mask, tiles_x, tiles_y,
                                          (uint16_t)(block_index * header->block_height));
    }
    state->stats.restore_us += (uint64_t)(eaf_port_now_us() - t_rest0);
    state->stats.frames++;
    return ESP_OK;
}

void eaf_lossless_destroy(void *opaque)
{
    eaf_lossless_state_t *state = opaque;
    free(state->scratch);
    free(state->internal_scratch);
}
