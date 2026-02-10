/*
 * SPDX-FileCopyrightText: 2022-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file esp_eaf_dec.h
 * @brief ESP EAF (Emote Animation Format) Decoder
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
#include <stdbool.h>
#include "esp_err.h"

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
} esp_eaf_frame_table_entry_t;
#pragma pack()

/**
 * @brief Frame entry with memory and table information
 */
typedef struct {
    const char *frame_mem;
    const esp_eaf_frame_table_entry_t *table;
} esp_eaf_frame_entry_t;

/**
 * @brief EAF format context structure
 */
typedef struct {
    esp_eaf_frame_entry_t *entries;
    int total_frames;
} esp_eaf_format_ctx_t;

/**********************
 *  PUBLIC TYPES
 **********************/

/**
 * @brief Color type for EAF (RGB565)
 */
typedef union {
    uint16_t full;                  /**< Full 16-bit color value */
} esp_eaf_color_t;

/**
 * @brief EAF format type enumeration
 */
typedef enum {
    ESP_EAF_FORMAT_VALID = 0,       /*!< Valid EAF format with split BMP data */
    ESP_EAF_FORMAT_REDIRECT = 1,    /*!< Redirect format pointing to another file */
    ESP_EAF_FORMAT_INVALID = 2      /*!< Invalid or unsupported format */
} esp_eaf_format_t;

/**
 * @brief EAF encoding type enumeration
 */
typedef enum {
    ESP_EAF_ENCODING_RLE = 0,           /*!< Run-Length Encoding */
    ESP_EAF_ENCODING_HUFFMAN = 1,       /*!< Huffman encoding with RLE */
    ESP_EAF_ENCODING_JPEG = 2,          /*!< JPEG encoding */
    ESP_EAF_ENCODING_HUFFMAN_DIRECT = 3, /*!< Direct Huffman encoding without RLE */
    ESP_EAF_ENCODING_MAX                /*!< Maximum number of encoding types */
} esp_eaf_encoding_t;

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
} esp_eaf_header_t;

/**
 * @brief Huffman tree node structure
 */
typedef struct esp_eaf_huffman_node {
    uint8_t is_leaf;                        /*!< Whether this node is a leaf node */
    uint8_t symbol;                         /*!< Symbol value for leaf nodes */
    struct esp_eaf_huffman_node* left;      /*!< Left child node */
    struct esp_eaf_huffman_node* right;     /*!< Right child node */
} esp_eaf_huffman_node_t;

/**
 * @brief EAF format parser handle
 */
typedef void *esp_eaf_format_handle_t;

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
esp_eaf_format_t esp_eaf_header_parse(const uint8_t *file_data, size_t file_size, esp_eaf_header_t *header);

/**
 * @brief Free resources allocated for EAF header
 * @param header Pointer to the header structure
 */
void esp_eaf_free_header(esp_eaf_header_t *header);

/**
 * @brief Calculate block offsets from header information
 * @param header Pointer to the header structure
 * @param offsets Array to store calculated offsets
 */
void esp_eaf_calculate_offsets(const esp_eaf_header_t *header, uint32_t *offsets);

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
esp_eaf_color_t esp_eaf_palette_get_color(const esp_eaf_header_t *header, uint8_t color_index, bool swap_bytes);

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
typedef esp_err_t (*esp_eaf_block_decoder_t)(const uint8_t *input_data, size_t input_size,
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
esp_err_t esp_eaf_rle_decode(const uint8_t *input_data, size_t input_size,
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
esp_err_t esp_eaf_huffman_decode(const uint8_t *input_data, size_t input_size,
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
esp_err_t esp_eaf_jpeg_decode(const uint8_t *input_data, size_t input_size,
                              uint8_t *output_buffer, size_t *out_size,
                              bool swap_color);

/**
 * @brief Initialize hardware JPEG decoder (if available)
 *
 * On chips with hardware JPEG decoder support (e.g., ESP32-P4), this function
 * initializes the hardware decoder engine. On other chips, this is a no-op.
 *
 * @return ESP_OK on success, or error code on failure
 */
esp_err_t esp_eaf_hw_jpeg_init(void);

/**
 * @brief Deinitialize hardware JPEG decoder
 *
 * Releases resources allocated by esp_eaf_hw_jpeg_init().
 *
 * @return ESP_OK on success
 */
esp_err_t esp_eaf_hw_jpeg_deinit(void);

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
} esp_eaf_palette_cache_t;

/**
 * @brief Decode a block of EAF data
 * @param header EAF header information
 * @param frame_data Pointer to the frame data
 * @param block_index Index of the block to decode
 * @param decode_buffer Buffer to store decoded RGB565 data (block start)
 * @param alpha_buffer Optional pointer to the start of the alpha plane (NULL if not used)
 * @param swap_color Whether to swap color bytes
 * @param cache Optional palette cache for frame-level caching (NULL to use block-level cache)
 * @return ESP_OK on success, ESP_FAIL on failure
 */
esp_err_t esp_eaf_block_decode(const esp_eaf_header_t *header, const uint8_t *frame_data,
                               int block_index, uint8_t *decode_buffer, uint8_t *alpha_buffer,
                               bool swap_color, esp_eaf_palette_cache_t *cache);

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
esp_err_t esp_eaf_format_init(const uint8_t *data, size_t data_len, esp_eaf_format_handle_t *ret_parser);

/**
 * @brief Deinitialize EAF format parser
 * @param handle Parser handle
 * @return ESP_OK on success, ESP_FAIL on failure
 */
esp_err_t esp_eaf_format_deinit(esp_eaf_format_handle_t handle);

/**
 * @brief Get total number of frames in EAF file
 * @param handle Parser handle
 * @return Total number of frames
 */
int esp_eaf_format_get_total_frames(esp_eaf_format_handle_t handle);

/**
 * @brief Get frame data at specified index
 * @param handle Parser handle
 * @param index Frame index
 * @return Pointer to frame data, NULL on failure
 */
const uint8_t *esp_eaf_format_get_frame_data(esp_eaf_format_handle_t handle, int index);

/**
 * @brief Get frame size at specified index
 * @param handle Parser handle
 * @param index Frame index
 * @return Frame size in bytes, -1 on failure
 */
int esp_eaf_format_get_frame_size(esp_eaf_format_handle_t handle, int index);

#ifdef __cplusplus
}
#endif
