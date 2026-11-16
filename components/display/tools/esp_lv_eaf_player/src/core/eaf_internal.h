/*
 * SPDX-FileCopyrightText: 2022-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file eaf_internal.h
 * @brief Private types shared by the EAF container, decoder runtime and codecs
 *
 * This module provides decoding functionality for EAF format files, including:
 * - File format parsing and validation
 * - Frame data extraction and management
 * - Multiple encoding format support (RLE, Huffman, JPEG)
 * - Color palette handling
 *
 * All public APIs use `esp_eaf_` prefix to avoid naming conflicts.
 */

#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "esp_eaf_status.h"
#include "esp_eaf_codec.h"

#ifdef __cplusplus
extern "C" {
#endif

/**********************
 *  FILE FORMAT DEFINITIONS
 **********************/

/*
 * EAF File Format Structure:
 *
 * Offset  Size    Description
 * 0       1       Magic number (0x89)
 * 1       3       Format string ("EAF" or "AAF")
 * 4       4       Total number of frames
 * 8       4       Checksum of table + data
 * 12      4       Length of table + data
 * 16      N       Frame table (N = total_frames * 8)
 * 16+N    M       Frame data (M = sum of all frame sizes)
 */

/* Magic numbers and identifiers */
#define ESP_EAF_MAGIC_HEAD          0x5A5A
#define ESP_EAF_MAGIC_LEN           2
#define ESP_EAF_FORMAT_MAGIC        0x89
#define ESP_EAF_FORMAT_STR          "EAF"
#define ESP_AAF_FORMAT_STR          "AAF"

/* File structure offsets */
#define ESP_EAF_FORMAT_OFFSET       0
#define ESP_EAF_STR_OFFSET          1
#define ESP_EAF_NUM_OFFSET          4
#define ESP_EAF_CHECKSUM_OFFSET     8
#define ESP_EAF_TABLE_LEN           12
#define ESP_EAF_TABLE_OFFSET        16

/**********************
 *  INTERNAL STRUCTURES
 **********************/

/**
 * @brief Frame table entry structure
 */
#pragma pack(1)
typedef struct {
    uint32_t frame_size;          /*!< Size of the frame */
    uint32_t frame_offset;        /*!< Offset of the frame */
} eaf_frame_table_entry_t;
#pragma pack()

/**
 * @brief Frame entry with memory and table information
 */
typedef struct {
    const char *frame_mem;
    const eaf_frame_table_entry_t *table;
} eaf_frame_entry_t;

/**
 * @brief EAF format context structure
 */
typedef struct {
    eaf_frame_entry_t *entries;
    int total_frames;
} eaf_format_ctx_t;

/**********************
 *  PUBLIC TYPES
 **********************/

/**
 * @brief Color type for EAF (RGB565)
 */
typedef union {
    uint16_t full;                  /**< Full 16-bit color value */
} eaf_color_t;

/**
 * @brief EAF format type enumeration
 */
typedef enum {
    ESP_EAF_FORMAT_VALID = 0,       /*!< Valid EAF format with split BMP data */
    ESP_EAF_FORMAT_REDIRECT = 1,    /*!< Redirect format pointing to another file */
    ESP_EAF_FORMAT_INVALID = 2      /*!< Invalid or unsupported format */
} eaf_format_t;

/**
 * @brief EAF image header structure
 */
typedef struct {
    char format[3];        /*!< Format identifier (e.g., "_S") */
    char version[6];       /*!< Version string */
    uint8_t bit_depth;     /*!< Bit depth (4, 8, or 24) */
    uint16_t width;        /*!< Image width in pixels */
    uint16_t height;       /*!< Image height in pixels */
    uint16_t blocks;       /*!< Number of blocks */
    uint16_t block_height; /*!< Height of each block */
    uint32_t *block_len;   /*!< Data length of each block */
    uint16_t data_offset;  /*!< Offset to data segment */
    uint8_t *palette;      /*!< Color palette (dynamically allocated) */
    int num_colors;        /*!< Number of colors in palette */
} eaf_header_t;

/**
 * @brief Huffman tree node structure
 */
typedef struct eaf_huffman_node {
    uint8_t is_leaf;                        /*!< Whether this node is a leaf node */
    uint8_t symbol;                         /*!< Symbol value for leaf nodes */
    struct eaf_huffman_node* left;      /*!< Left child node */
    struct eaf_huffman_node* right;     /*!< Right child node */
} eaf_huffman_node_t;

/**
 * @brief EAF format parser handle
 */
typedef void *eaf_format_handle_t;
typedef struct eaf_decoder eaf_decoder_t;
typedef struct {
    uint64_t inflate_us;
    uint64_t restore_us;
    uint32_t frames;
} eaf_lossless_stats_t;
eaf_decoder_t *eaf_decoder_create(void);
void eaf_decoder_reset(eaf_decoder_t *decoder);
void eaf_decoder_destroy(eaf_decoder_t *decoder);
void eaf_decoder_get_lossless_stats(const eaf_decoder_t *decoder, eaf_lossless_stats_t *stats);
/* Internal-RAM scratch for codec 0x04 lossless blocks that fit; larger blocks fall back to PSRAM.
 * Returns the reserved size (0 if the allocation failed). */
size_t eaf_decoder_reserve_internal_scratch(eaf_decoder_t *decoder, size_t size);
void eaf_decoder_set_fused_blend(eaf_decoder_t *decoder, const uint16_t *bg_rgb565, uint16_t *comp_rgb565);
void eaf_decoder_set_output_format(eaf_decoder_t *decoder, esp_eaf_output_format_t out_fmt);

/**********************
 *  HEADER OPERATIONS
 **********************/

/**
 * @brief Parse the header of an EAF file
 * @param file_data Pointer to the image file data
 * @param file_size Size of the image file data
 * @param header Pointer to store the parsed header information
 * @return Image format type (VALID, REDIRECT, or INVALID)
 */
eaf_format_t eaf_header_parse(const uint8_t *file_data, size_t file_size, eaf_header_t *header);

/**
 * @brief Free resources allocated for EAF header
 * @param header Pointer to the header structure
 */
void eaf_free_header(eaf_header_t *header);

/**
 * @brief Calculate block offsets from header information
 * @param header Pointer to the header structure
 * @param offsets Array to store calculated offsets
 */
void eaf_calculate_offsets(const eaf_header_t *header, uint32_t *offsets);

/**********************
 *  COLOR OPERATIONS
 **********************/

/**
 * @brief Get color from palette at specified index
 * @param header Pointer to the header structure containing palette
 * @param color_index Index in the palette
 * @param swap_bytes Whether to swap color bytes
 * @return Color value in RGB565 format
 */
eaf_color_t eaf_palette_get_color(const eaf_header_t *header, uint8_t color_index, bool swap_bytes);

/**********************
 *  COMPRESSION OPERATIONS
 **********************/

/**
 * @brief Function pointer type for block decoders
 * @param input_data Input compressed data
 * @param input_size Size of input data
 * @param output_buffer Output buffer for decompressed data
 * @param out_size Size of output buffer
 * @param swap_color Whether to swap color bytes (only used by JPEG decoder)
 * @return ESP_OK on success, ESP_FAIL on failure
 */
typedef esp_err_t (*eaf_block_decoder_t)(const uint8_t *input_data, size_t input_size,
                                         uint8_t *output_buffer, size_t *out_size,
                                         bool swap_color);

/**
 * @brief Decode RLE compressed data
 * @param input_data Input compressed data
 * @param input_size Size of compressed data
 * @param output_buffer Output buffer for decompressed data
 * @param out_size Size of output buffer
 * @param swap_color Whether to swap color bytes (unused for RLE)
 * @return ESP_OK on success, ESP_FAIL on failure
 */
esp_err_t eaf_rle_decode(const uint8_t *input_data, size_t input_size,
                         uint8_t *output_buffer, size_t *out_size,
                         bool swap_color);

/**
 * @brief Decode Huffman compressed data
 * @param input_data Input compressed data
 * @param input_size Size of input data
 * @param output_buffer Output buffer for decompressed data
 * @param out_size Size of output buffer
 * @param swap_color Whether to swap color bytes (unused for Huffman)
 * @return ESP_OK on success, ESP_FAIL on failure
 */
esp_err_t eaf_huffman_decode(const uint8_t *input_data, size_t input_size,
                             uint8_t *output_buffer, size_t *out_size,
                             bool swap_color);

/**
 * @brief Decode JPEG compressed data
 * @param input_data Input JPEG data
 * @param input_size Size of input data
 * @param output_buffer Output buffer for decoded data
 * @param out_size Size of output buffer
 * @param swap_color Whether to swap color bytes
 * @return ESP_OK on success, ESP_FAIL on failure
 */
esp_err_t eaf_jpeg_decode(const uint8_t *input_data, size_t input_size,
                          uint8_t *output_buffer, size_t *out_size,
                          bool swap_color);

/**********************
 *  FRAME OPERATIONS
 **********************/

/**
 * @brief Palette cache entry (0xFFFFFFFF = not cached)
 */
typedef struct {
    uint32_t color[256];   /*!< RGB565 color cache (0xFFFFFFFF = not cached) */
    uint8_t alpha[256];    /*!< Alpha value cache */
    bool initialized;      /*!< Whether cache has been initialized */
} eaf_palette_cache_t;

/**
 * @brief Dirty bounding box of the last decoded frame (for LVGL partial invalidation)
 */
typedef struct {
    uint16_t x1;
    uint16_t y1;
    uint16_t x2;
    uint16_t y2;
    bool valid;
} eaf_dirty_rect_t;

/* Keep below the display's LV_INV_BUF_SIZE (32 by default) minus the other widgets' invalidations,
 * otherwise LVGL falls back to a full-screen refresh. */
#define ESP_EAF_MAX_DIRTY_BANDS 24

uint8_t eaf_get_dirty_bands_ctx(const eaf_decoder_t *decoder, eaf_dirty_rect_t *bands, uint8_t max_bands);

/* One context per stream; callers must serialize access to each context. */
esp_err_t eaf_block_decode_ctx(eaf_decoder_t *decoder, const eaf_header_t *header, const uint8_t *frame_data,
                               int block_index, uint8_t *decode_buffer, uint8_t *alpha_buffer,
                               bool swap_color, eaf_palette_cache_t *cache);
void eaf_get_dirty_rect_ctx(const eaf_decoder_t *decoder, eaf_dirty_rect_t *rect);

/**********************
 *  FORMAT OPERATIONS
 **********************/

/**
 * @brief Initialize EAF format parser
 * @param data Pointer to EAF file data
 * @param data_len Length of EAF file data
 * @param ret_parser Pointer to store the parser handle
 * @return ESP_OK on success, ESP_FAIL on failure
 */
esp_err_t eaf_format_init(const uint8_t *data, size_t data_len, eaf_format_handle_t *ret_parser);

/**
 * @brief Deinitialize EAF format parser
 * @param handle Parser handle
 * @return ESP_OK on success, ESP_FAIL on failure
 */
esp_err_t eaf_format_deinit(eaf_format_handle_t handle);

/**
 * @brief Get total number of frames in EAF file
 * @param handle Parser handle
 * @return Total number of frames
 */
int eaf_format_get_total_frames(eaf_format_handle_t handle);

/**
 * @brief Get frame data at specified index
 * @param handle Parser handle
 * @param index Frame index
 * @return Pointer to frame data, NULL on failure
 */
const uint8_t *eaf_format_get_frame_data(eaf_format_handle_t handle, int index);

/**
 * @brief Get frame size at specified index
 * @param handle Parser handle
 * @param index Frame index
 * @return Frame size in bytes, -1 on failure
 */
int eaf_format_get_frame_size(eaf_format_handle_t handle, int index);

#ifdef __cplusplus
}
#endif
