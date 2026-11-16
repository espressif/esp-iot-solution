/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
/* EAF quadrant JPEG-delta codec. */
/* SPDX-License-Identifier: Apache-2.0 */
#include "codec_internal.h"
#include "eaf_port.h"

#include <stdlib.h>
#include <string.h>

esp_err_t eaf_jpeg_delta_decode(const eaf_codec_request_t *r)
{
    if (!r || !r->decoder ||
            eaf_decoder_get_output_format(r->decoder) != ESP_EAF_OUT_RGB565) {
        return ESP_ERR_NOT_SUPPORTED;
    }
    eaf_decoder_t *decoder = r->decoder;
    eaf_jpeg_state_t *state = eaf_decoder_state(decoder, ESP_EAF_CODEC_JPEG);
    if (!state) {
        return ESP_ERR_NO_MEM;
    }
    const eaf_header_t *header = r->header;
    const uint8_t *block_data = r->block_data;
    size_t block_len = r->block_len;
    int width = r->width;
    int block_height = r->block_height;
    uint8_t *decode_buffer = r->decode_buffer;
    bool swap_color = r->swap_color;
    esp_err_t decode_result = ESP_FAIL;
    /* Quadtree 8x8/16x16 Multi-Res Abbreviated JPEG:
     * - 29 bytes: 225-bit outer 16x16 tile mask
     * - (changed_16 + 1)/2 bytes: 4-bit nibble per changed 16x16 tile:
     *     nibble == 0    : Smooth 16x16 tile (1 8x8 block upscaled 2x2 to 16x16)
     *     nibble in 1..15: 4-bit quadrant mask (each set bit is 1 sharp 8x8 quadrant)
     * - Abbreviated JPEG scan of 1D horizontal strip (width=16 * ceil(total_8x8 / 4), height=16)
     */
    int tiles_x = width / 16;
    int tiles_y = block_height / 16;
    int total_tiles = tiles_x * tiles_y;
    int mask_bytes = (total_tiles + 7) / 8;
    if (block_len < 1U + (size_t)mask_bytes) {
        return ESP_FAIL;
    }
    const uint8_t *mask = block_data + 1;
    int changed_16 = 0;
    uint16_t min_x = width, min_y = header->height, max_x = 0, max_y = 0;
    for (int t = 0; t < total_tiles; t++) {
        if (mask[t >> 3] & (1 << (t & 7))) {
            changed_16++;
            uint16_t ty = (t / tiles_x) * 16;
            uint16_t tx = (t % tiles_x) * 16;
            if (tx < min_x) {
                min_x = tx;
            }
            if (ty < min_y) {
                min_y = ty;
            }
            if (tx + 15 > max_x) {
                max_x = tx + 15;
            }
            if (ty + 15 > max_y) {
                max_y = ty + 15;
            }
        }
    }
    if (changed_16 == 0) {
        return ESP_OK;
    }
    decoder->dirty.x1 = min_x;
    decoder->dirty.y1 = min_y;
    decoder->dirty.x2 = max_x;
    decoder->dirty.y2 = max_y;
    decoder->dirty.valid = true;

    int nibble_bytes = (changed_16 + 1) / 2;
    if (block_len < 1U + (size_t)mask_bytes + (size_t)nibble_bytes || state->hdr_len == 0 || state->sof0_offset < 0) {
        return ESP_FAIL;
    }
    const uint8_t *nibbles = block_data + 1 + mask_bytes;
    int total_8x8 = 0;
    for (int c = 0; c < changed_16; c++) {
        uint8_t nib = (c & 1) ? ((nibbles[c >> 1] >> 4) & 0x0F) : (nibbles[c >> 1] & 0x0F);
        if (nib == 0) {
            /* This revision supports native-resolution quadrants only. */
            return ESP_ERR_NOT_SUPPORTED;
        } else {
            total_8x8 += __builtin_popcount(nib);
        }
    }
    int mcus = (total_8x8 + 3) / 4;
    /* The canonical encoder pads strip MCUs for hardware JPEG output alignment.
     * Rebuild exactly that width; entropy data cannot be decoded at a shorter width. */
    if ((size_t)state->sof0_offset + 11 >= state->hdr_len) {
        return ESP_ERR_INVALID_SIZE;
    }
    int mcu_alignment = state->hdr[state->sof0_offset + 11] == 0x11 ? 3 : 2;
    mcus = ((mcus + mcu_alignment - 1) / mcu_alignment) * mcu_alignment;
    if (mcus > 65535 / 16) {
        return ESP_ERR_INVALID_SIZE;
    }
    int atlas_w = mcus * 16;
    int atlas_h = 16;
    size_t atlas_size = (size_t)atlas_w * atlas_h * 2;
    size_t alloc_sz = 0;
    uint8_t *atlas_buf = (uint8_t *)eaf_alloc_aligned(atlas_size, 0, &alloc_sz);
    if (!atlas_buf) {
        return ESP_ERR_NO_MEM;
    }

    const uint8_t *scan_ptr = block_data + 1 + mask_bytes + nibble_bytes;
    size_t scan_len = block_len - 1 - mask_bytes - nibble_bytes;
    size_t full_len = state->hdr_len + scan_len + 2;
    uint8_t *rebuilt_jpg = (uint8_t *)eaf_alloc(full_len);
    if (!rebuilt_jpg) {
        free(atlas_buf);
        return ESP_ERR_NO_MEM;
    }
    memcpy(rebuilt_jpg, state->hdr, state->hdr_len);
    rebuilt_jpg[state->sof0_offset + 5] = (atlas_h >> 8) & 0xFF;
    rebuilt_jpg[state->sof0_offset + 6] = atlas_h & 0xFF;
    rebuilt_jpg[state->sof0_offset + 7] = (atlas_w >> 8) & 0xFF;
    rebuilt_jpg[state->sof0_offset + 8] = atlas_w & 0xFF;
    memcpy(rebuilt_jpg + state->hdr_len, scan_ptr, scan_len);
    rebuilt_jpg[state->hdr_len + scan_len] = 0xFF;
    rebuilt_jpg[state->hdr_len + scan_len + 1] = 0xD9;

    size_t dec_sz = alloc_sz;
    decode_result = eaf_jpeg_decode(rebuilt_jpg, full_len, atlas_buf, &dec_sz, swap_color);
    free(rebuilt_jpg);

    if (decode_result == ESP_OK && dec_sz != atlas_size) {
        decode_result = ESP_ERR_INVALID_SIZE;
    }
    if (decode_result == ESP_OK) {
        uint16_t *fb16 = (uint16_t *)decode_buffer;
        const uint16_t *at16 = (const uint16_t *)atlas_buf;
        int idx8 = 0;
        int c = 0;
        for (int t = 0; t < total_tiles; t++) {
            if (!(mask[t >> 3] & (1 << (t & 7)))) {
                continue;
            }
            int ty = (t / tiles_x) * 16;
            int tx = (t % tiles_x) * 16;
            uint8_t nib = (c & 1) ? ((nibbles[c >> 1] >> 4) & 0x0F) : (nibbles[c >> 1] & 0x0F);
            c++;

            /* Sharp 1:1 8x8 quadrants only (no 2x2 boxy upscale!) */
            for (int q = 0; q < 4; q++) {
                if (!(nib & (1 << q))) {
                    continue;
                }
                int mcu_idx = idx8 >> 2;
                int q_sub = idx8 & 3;
                int ay = (q_sub >> 1) * 8;
                int ax = mcu_idx * 16 + (q_sub & 1) * 8;
                idx8++;
                int qy = ty + (q >> 1) * 8;
                int qx = tx + (q & 1) * 8;
                for (int r = 0; r < 8; r++) {
                    memcpy(fb16 + (qy + r) * width + qx, at16 + (ay + r) * atlas_w + ax, 8 * 2);
                }
            }
        }
    }
    free(atlas_buf);
    return decode_result;
}
