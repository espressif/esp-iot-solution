/*
 * SPDX-FileCopyrightText: 2025-2026 Espressif Systems (Shanghai) CO LTD
 * SPDX-License-Identifier: Apache-2.0
 */
/** @file container.c
 *  @brief Legacy EAF/AAF container validation, frame lookup and codec dispatch.
 */
#include <limits.h>
#include <stdlib.h>
#include <string.h>

#include "eaf_internal.h"
#include "eaf_port.h"
#include "codec_internal.h"

static const char *TAG = "eaf_container";

static uint16_t read_le16(const uint8_t *p)
{
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

static uint32_t read_le32(const uint8_t *p)
{
    return (uint32_t)read_le16(p) | ((uint32_t)read_le16(p + 2) << 16);
}

static uint32_t format_calc_checksum(const uint8_t *data, uint32_t length)
{
    uint32_t checksum = 0;
    for (uint32_t i = 0; i < length; i++) {
        checksum += data[i];
    }
    return checksum;
}
eaf_format_t eaf_header_parse(const uint8_t *file_data, size_t file_size, eaf_header_t *header)
{
    if (!header) {
        return ESP_EAF_FORMAT_INVALID;
    }
    memset(header, 0, sizeof(*header));
    if (!file_data || file_size < 3) {
        return ESP_EAF_FORMAT_INVALID;
    }
    memcpy(header->format, file_data, 2);
    if (memcmp(file_data, "_R", 2) == 0) {
        size_t len = file_data[2];
        if (len > file_size - 3) {
            return ESP_EAF_FORMAT_INVALID;
        }
        header->palette = eaf_alloc(len + 1);
        if (!header->palette) {
            return ESP_EAF_FORMAT_INVALID;
        }
        memcpy(header->palette, file_data + 3, len);
        header->palette[len] = 0;
        header->num_colors = len + 1;
        return ESP_EAF_FORMAT_REDIRECT;
    }
    if (memcmp(file_data, "_S", 2) != 0 || file_size < 18) {
        return ESP_EAF_FORMAT_INVALID;
    }
    memcpy(header->version, file_data + 3, 6);
    header->bit_depth = file_data[9];
    if (header->bit_depth != 4 && header->bit_depth != 8 && header->bit_depth != 24) {
        return ESP_EAF_FORMAT_INVALID;
    }
    header->width = read_le16(file_data + 10);
    header->height = read_le16(file_data + 12);
    header->blocks = read_le16(file_data + 14);
    header->block_height = read_le16(file_data + 16);
    if (!header->width || !header->height || !header->blocks || !header->block_height ||
            header->blocks != (header->height + header->block_height - 1) / header->block_height) {
        return ESP_EAF_FORMAT_INVALID;
    }
    header->num_colors = header->bit_depth == 24 ? 0 : 1 << header->bit_depth;
    size_t offset = 18 + (size_t)header->blocks * 4 + (size_t)header->num_colors * 4;
    if (offset > file_size || offset > UINT16_MAX) {
        return ESP_EAF_FORMAT_INVALID;
    }
    size_t remaining = file_size - offset;
    for (int i = 0; i < header->blocks; i++) {
        uint32_t len = read_le32(file_data + 18 + i * 4);
        if (len > remaining) {
            return ESP_EAF_FORMAT_INVALID;
        }
        remaining -= len;
    }
    header->block_len = eaf_alloc(header->blocks * sizeof(uint32_t));
    if (!header->block_len) {
        return ESP_EAF_FORMAT_INVALID;
    }
    for (int i = 0; i < header->blocks; i++) {
        header->block_len[i] = read_le32(file_data + 18 + i * 4);
    }
    if (header->num_colors) {
        header->palette = eaf_alloc(header->num_colors * 4);
        if (!header->palette) {
            eaf_free_header(header);
            return ESP_EAF_FORMAT_INVALID;
        }
        memcpy(header->palette, file_data + 18 + header->blocks * 4, header->num_colors * 4);
    }
    header->data_offset = offset;
    return ESP_EAF_FORMAT_VALID;
}

void eaf_free_header(eaf_header_t *header)
{
    if (header->block_len != NULL) {
        free(header->block_len);
        header->block_len = NULL;
    }
    if (header->palette != NULL) {
        free(header->palette);
        header->palette = NULL;
    }
}

void eaf_calculate_offsets(const eaf_header_t *header, uint32_t *offsets)
{
    offsets[0] = header->data_offset;
    for (int i = 1; i < header->blocks; i++) {
        offsets[i] = offsets[i - 1] + header->block_len[i - 1];
    }
}

/**********************
 *  PALETTE FUNCTIONS
 **********************/

eaf_color_t eaf_palette_get_color(const eaf_header_t *header, uint8_t color_index, bool swap_bytes)
{
    const uint8_t *color_data = &header->palette[color_index * 4];
    /* RGB888: R=color[2], G=color[1], B=color[0] */
    /* RGB565: R: (color[2] & 0xF8) << 8, G: (color[1] & 0xFC) << 3, B: (color[0] & 0xF8) >> 3 */
    eaf_color_t result;
    uint16_t rgb565_value = swap_bytes ? __builtin_bswap16(((color_data[2] & 0xF8) << 8) | ((color_data[1] & 0xFC) << 3) | ((color_data[0] & 0xF8) >> 3)) : \
                            ((color_data[2] & 0xF8) << 8) | ((color_data[1] & 0xFC) << 3) | ((color_data[0] & 0xF8) >> 3);
    result.full = rgb565_value;
    return result;
}

/**********************
 *  DECODING FUNCTIONS
 **********************/

static int get_block_rows(const eaf_header_t *header, int block_index)
{
    int start_row = block_index * header->block_height;
    int remaining = header->height - start_row;
    if (remaining <= 0) {
        return 0;
    }
    return remaining > header->block_height ? header->block_height : remaining;
}

esp_err_t eaf_block_decode_ctx(eaf_decoder_t *decoder,
                               const eaf_header_t *header,
                               const uint8_t *frame_data, int block_index,
                               uint8_t *decode_buffer, uint8_t *alpha_buffer,
                               bool swap_color, eaf_palette_cache_t *cache)
{
    if (!decoder || !header || !frame_data || !decode_buffer || !header->block_len ||
            block_index < 0 || block_index >= header->blocks || !header->width || !header->height) {
        return ESP_ERR_INVALID_ARG;
    }
    size_t offset = header->data_offset;
    for (int i = 0; i < block_index; i++) {
        offset += header->block_len[i];
    }
    const uint8_t *block_data = frame_data + offset;
    size_t block_len = header->block_len[block_index];
    int block_height = get_block_rows(header, block_index);
    if (block_index == 0) {
        decoder->dirty = (eaf_dirty_rect_t) {
            .x1 = header->width,
            .y1 = header->height,
        };
        decoder->dirty_band_count = 0;
    }
    if (block_len == 0 || block_height <= 0) {
        return ESP_OK;
    }

    uint8_t mode = block_data[0];

    if (mode == ESP_EAF_CODEC_JPEG_DELTA &&
            (header->width % 16 || block_height % 16 || header->blocks != 1)) {
        return ESP_ERR_INVALID_SIZE;
    }
    const eaf_codec_ops_t *codec = eaf_codec_find(ESP_EAF_CONTAINER_EAF, mode);
    if (!codec) {
        ESP_LOGE(TAG, "Unknown EAF codec_id 0x%02X", mode);
        return ESP_ERR_NOT_SUPPORTED;
    }
    eaf_codec_request_t request = {
        .container = ESP_EAF_CONTAINER_EAF,
        .decoder = decoder,
        .header = header,
        .block_data = block_data,
        .block_len = block_len,
        .block_index = block_index,
        .width = header->width,
        .block_height = block_height,
        .decode_buffer = decode_buffer,
        .alpha_buffer = alpha_buffer,
        .swap_color = swap_color,
        .palette_cache = cache,
        .mode = mode,
    };
    return codec->decode(&request);
}
esp_err_t eaf_format_init(const uint8_t *data, size_t data_len, eaf_format_handle_t *ret_parser)
{
    esp_err_t ret = ESP_OK;
    eaf_frame_entry_t *entries = NULL;
    eaf_format_ctx_t *parser = NULL;

    ESP_GOTO_ON_FALSE(data != NULL && ret_parser != NULL, ESP_ERR_INVALID_ARG, err, TAG, "Invalid input");
    ESP_GOTO_ON_FALSE(data_len >= ESP_EAF_TABLE_OFFSET, ESP_ERR_INVALID_SIZE, err, TAG, "File too small");

    parser = (eaf_format_ctx_t *)calloc(1, sizeof(eaf_format_ctx_t));
    ESP_GOTO_ON_FALSE(parser, ESP_ERR_NO_MEM, err, TAG, "No memory for parser handle");

    /* Check file format magic number: 0x89 */
    ESP_GOTO_ON_FALSE(data[ESP_EAF_FORMAT_OFFSET] == ESP_EAF_FORMAT_MAGIC, ESP_ERR_INVALID_CRC, err, TAG, "Bad file format magic");

    /* Check for EAF/AAF format string */
    const char *format_str = (const char *)(data + ESP_EAF_STR_OFFSET);
    bool is_valid = (memcmp(format_str, ESP_EAF_FORMAT_STR, 3) == 0) || (memcmp(format_str, ESP_AAF_FORMAT_STR, 3) == 0);
    ESP_GOTO_ON_FALSE(is_valid, ESP_ERR_INVALID_CRC, err, TAG, "Bad file format string (expected EAF or AAF)");

    int total_frames = (int)read_le32(data + ESP_EAF_NUM_OFFSET);
    uint32_t stored_chk = read_le32(data + ESP_EAF_CHECKSUM_OFFSET);
    uint32_t stored_len = read_le32(data + ESP_EAF_TABLE_LEN);

    ESP_GOTO_ON_FALSE(total_frames > 0, ESP_ERR_INVALID_SIZE, err, TAG, "Invalid frame count %d", total_frames);
    ESP_GOTO_ON_FALSE((size_t)total_frames <= (SIZE_MAX / sizeof(eaf_frame_table_entry_t)), ESP_ERR_INVALID_SIZE, err, TAG, "Frame count too large");

    const size_t payload_available = data_len - ESP_EAF_TABLE_OFFSET;
    const size_t table_bytes = (size_t)total_frames * sizeof(eaf_frame_table_entry_t);
    ESP_GOTO_ON_FALSE(table_bytes <= payload_available, ESP_ERR_INVALID_SIZE, err, TAG, "Frame table exceeds file size");

    const size_t stored_len_sz = stored_len;
    ESP_GOTO_ON_FALSE(stored_len_sz <= payload_available, ESP_ERR_INVALID_SIZE, err, TAG, "Stored length beyond file");
    ESP_GOTO_ON_FALSE(stored_len_sz >= table_bytes, ESP_ERR_INVALID_SIZE, err, TAG, "Stored length smaller than table");
    ESP_GOTO_ON_FALSE((size_t)ESP_EAF_TABLE_OFFSET + stored_len_sz <= data_len, ESP_ERR_INVALID_SIZE, err, TAG, "Stored data exceeds file");

    const uint32_t calculated_chk = format_calc_checksum((uint8_t *)(data + ESP_EAF_TABLE_OFFSET), stored_len);
    ESP_GOTO_ON_FALSE(calculated_chk == stored_chk, ESP_ERR_INVALID_CRC, err, TAG, "Bad full checksum");

    entries = (eaf_frame_entry_t *)eaf_alloc(sizeof(eaf_frame_entry_t) * total_frames);
    ESP_GOTO_ON_FALSE(entries, ESP_ERR_NO_MEM, err, TAG, "No memory for frame entries");

    eaf_frame_table_entry_t *table = (eaf_frame_table_entry_t *)(data + ESP_EAF_TABLE_OFFSET);
    const size_t frame_region_len = stored_len_sz - table_bytes;
    const size_t frame_region_offset = ESP_EAF_TABLE_OFFSET + table_bytes;
    const uint8_t *frame_region_base = data + frame_region_offset;

    int animation_frames = total_frames;
    for (int i = 0; i < total_frames; i++) {
        size_t frame_size = table[i].frame_size;
        size_t frame_offset = table[i].frame_offset;

        ESP_GOTO_ON_FALSE(frame_size >= ESP_EAF_MAGIC_LEN, ESP_ERR_INVALID_SIZE, err, TAG, "Frame %d size too small", i);
        ESP_GOTO_ON_FALSE(frame_offset <= frame_region_len, ESP_ERR_INVALID_SIZE, err, TAG, "Frame %d offset out of range", i);
        ESP_GOTO_ON_FALSE(frame_size <= (frame_region_len - frame_offset), ESP_ERR_INVALID_SIZE, err, TAG, "Frame %d size out of range", i);

        const uint8_t *frame_mem = frame_region_base + frame_offset;
        uint16_t magic = 0;
        memcpy(&magic, frame_mem, sizeof(magic));
        ESP_GOTO_ON_FALSE(magic == ESP_EAF_MAGIC_HEAD, ESP_ERR_INVALID_CRC, err, TAG, "Bad file magic header");

        /* Original EAF assets may end with a _C configuration record. It is
         * included in the file table/checksum but is not a displayable frame. */
        if (frame_size >= ESP_EAF_MAGIC_LEN + 2 &&
                memcmp(frame_mem + ESP_EAF_MAGIC_LEN, "_C", 2) == 0) {
            ESP_GOTO_ON_FALSE(i == total_frames - 1 && i > 0,
                              ESP_ERR_INVALID_SIZE, err, TAG, "Misplaced configuration record");
            animation_frames--;
        }
        (entries + i)->table = (table + i);
        (entries + i)->frame_mem = (const char *)frame_mem;
    }

    parser->entries = entries;
    parser->total_frames = animation_frames;

    *ret_parser = (eaf_format_handle_t)parser;

    return ESP_OK;

err:
    if (entries) {
        free(entries);
    }
    if (parser) {
        free(parser);
    }
    if (ret_parser) {
        *ret_parser = NULL;
    }

    return ret;
}

esp_err_t eaf_format_deinit(eaf_format_handle_t handle)
{
    if (handle == NULL) {
        ESP_LOGW(TAG, "Handle is invalid");
        return ESP_ERR_INVALID_ARG;
    }

    eaf_format_ctx_t *parser = (eaf_format_ctx_t *)(handle);
    if (parser) {
        if (parser->entries) {
            free(parser->entries);
        }
        free(parser);
    }
    return ESP_OK;
}

int eaf_format_get_total_frames(eaf_format_handle_t handle)
{
    if (handle == NULL) {
        ESP_LOGE(TAG, "Handle is invalid");
        return -1;
    }

    eaf_format_ctx_t *parser = (eaf_format_ctx_t *)(handle);
    return parser->total_frames;
}

const uint8_t *eaf_format_get_frame_data(eaf_format_handle_t handle, int index)
{
    if (handle == NULL) {
        ESP_LOGE(TAG, "Handle is invalid");
        return NULL;
    }

    eaf_format_ctx_t *parser = (eaf_format_ctx_t *)(handle);

    if (index >= 0 && parser->total_frames > index) {
        return (const uint8_t *)((parser->entries + index)->frame_mem + ESP_EAF_MAGIC_LEN);
    } else {
        ESP_LOGE(TAG, "Invalid index: %d. Maximum index is %d", index, parser->total_frames);
        return NULL;
    }
}

int eaf_format_get_frame_size(eaf_format_handle_t handle, int index)
{
    if (handle == NULL) {
        ESP_LOGE(TAG, "Handle is invalid");
        return -1;
    }

    eaf_format_ctx_t *parser = (eaf_format_ctx_t *)(handle);

    if (index >= 0 && parser->total_frames > index) {
        return ((parser->entries + index)->table->frame_size - ESP_EAF_MAGIC_LEN);
    } else {
        ESP_LOGE(TAG, "Invalid index: %d. Maximum index is %d", index, parser->total_frames);
        return -1;
    }
}
