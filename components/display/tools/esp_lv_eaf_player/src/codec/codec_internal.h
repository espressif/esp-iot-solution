/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
/* Internal codec ABI shared by container readers and codec implementations. */
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "esp_eaf_codec.h"

#include "eaf_internal.h"
#include "esp_lv_eaf_player_config.h"
#include "zlib.h"

typedef struct {
    uint8_t *scratch;
    size_t capacity;
    uint8_t *internal_scratch;
    size_t internal_capacity;
    eaf_lossless_stats_t stats;
    const uint16_t *fused_bg_rgb565;
    uint16_t *fused_comp_rgb565;
} eaf_lossless_state_t;

typedef struct {
    uint8_t hdr[640];
    size_t hdr_len;
    int sof0_offset;
} eaf_jpeg_state_t;

struct eaf_decoder {
    eaf_dirty_rect_t dirty;
    eaf_dirty_rect_t dirty_bands[ESP_EAF_MAX_DIRTY_BANDS + 1];
    uint8_t dirty_band_count;
    void *states[ESP_EAF_CODEC_MAX];
    esp_eaf_output_format_t out_fmt;
};

typedef struct {
    esp_eaf_container_t container;
    eaf_decoder_t *decoder;
    const eaf_header_t *header;
    const uint8_t *block_data;
    size_t block_len;
    int block_index;
    int width;
    int block_height;
    uint8_t *decode_buffer;
    uint8_t *alpha_buffer;
    bool swap_color;
    eaf_palette_cache_t *palette_cache;
    uint8_t mode;
    uint8_t pixel_format; /*!< eaf4_pixfmt_t for unified EAF4 container */
    uint8_t *frame;
    const uint8_t *scratch;
    uint32_t decoded_size;
    uint32_t frame_bytes;
    uint16_t frame_width;
    uint16_t frame_height;
    eaf4_dirty_rect_t *eaf4_dirty;
} eaf_codec_request_t;

typedef int (*eaf_codec_decode_cb_t)(const eaf_codec_request_t *request);

typedef struct {
    esp_eaf_container_t container;
    uint8_t mode;
    const char *name;
    eaf_codec_decode_cb_t decode;
    size_t state_size;
    void (*reset)(void *state);
    void (*destroy)(void *state);
} eaf_codec_ops_t;

const eaf_codec_ops_t *eaf_codec_find(esp_eaf_container_t container, uint8_t mode);

int eaf4_lossless_decode(const eaf_codec_request_t *request);
int eaf_bounded_decode(const eaf_codec_request_t *request);

esp_err_t eaf_rle_block_decode(const eaf_codec_request_t *request);
esp_err_t eaf_huffman_rle_block_decode(const eaf_codec_request_t *request);
esp_err_t eaf_jpeg_block_decode(const eaf_codec_request_t *request);
esp_err_t eaf_huffman_block_decode(const eaf_codec_request_t *request);
esp_err_t eaf_lossless_decode(const eaf_codec_request_t *request);
esp_err_t eaf_jpeg_delta_decode(const eaf_codec_request_t *request);

void eaf_decoder_emit_tile_dirty_bands(eaf_decoder_t *decoder, const uint8_t *mask,
                                       int tiles_x, int tiles_y, uint16_t y_offset);
esp_eaf_output_format_t eaf_decoder_get_output_format(const eaf_decoder_t *decoder);
void eaf_decoder_mark_dirty(eaf_decoder_t *decoder, uint16_t x1, uint16_t y1,
                            uint16_t x2, uint16_t y2);
void eaf_decoder_cache_jpeg_tables(eaf_decoder_t *decoder,
                                   const uint8_t *jpeg, size_t size);

/* State is allocated lazily for a canonical EAF codec ID, never for a bytecode alias. */
void *eaf_decoder_state(eaf_decoder_t *decoder, esp_eaf_codec_id_t codec_id);
void eaf_lossless_destroy(void *state);
void eaf_jpeg_reset(void *state);
