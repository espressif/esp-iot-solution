/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
/**
 * @file esp_eaf_dec.c
 * @brief ESP EAF (Emote Animation Format) decoder implementation
 */

#include <string.h>
#include <stdlib.h>
#include <limits.h>
#include <inttypes.h>
#include "esp_err.h"
#include "esp_log.h"
#include "esp_check.h"
#include "esp_heap_caps.h"
#include "esp_cache.h"
#include "esp_private/esp_cache_private.h"
#include "esp_eaf_dec.h"
#include "esp_lv_eaf_player_config.h"

#if ESP_LV_EAF_ENABLE_SW_JPEG
#include "esp_jpeg_dec.h"
#include "esp_jpeg_common.h"
#endif

/* Hardware JPEG decoder support detection */
#if ESP_LV_EAF_ENABLE_HW_JPEG
#define ESP_EAF_HW_JPEG_ENABLED 1
#include "driver/jpeg_decode.h"
#else
#define ESP_EAF_HW_JPEG_ENABLED 0
#endif

static const char *TAG = "esp_eaf_dec";

/**********************
 *  STATIC VARIABLES
 **********************/
static esp_eaf_block_decoder_t s_esp_eaf_decoder[ESP_EAF_ENCODING_MAX] = {0};

#if ESP_EAF_HW_JPEG_ENABLED
static jpeg_decoder_handle_t s_hw_jpeg_handle = NULL;
#endif

/**********************
 *  STATIC HELPER FUNCTIONS
 **********************/

/**
 * @brief Align size up to alignment boundary
 */
#define ESP_EAF_ALIGN_UP(num, align) (((num) + ((align) - 1)) & ~((align) - 1))

/**
 * @brief Get cache alignment for PSRAM
 */
static inline size_t esp_eaf_get_psram_cache_align(void)
{
    size_t cache_align = 0;
#if CONFIG_SPIRAM_USE_MALLOC || CONFIG_SPIRAM
    esp_cache_get_alignment(MALLOC_CAP_SPIRAM, &cache_align);
#endif
    /* Fallback to 4-byte alignment if cache alignment not available */
    if (cache_align == 0) {
        cache_align = 4;
    }
    return cache_align;
}

/**
 * @brief Allocate memory with PSRAM priority
 *
 * Tries to allocate from PSRAM first, falls back to internal RAM if unavailable.
 * This function prioritizes PSRAM usage to save SRAM.
 */
static inline void *esp_eaf_malloc_prefer_psram(size_t size)
{
    void *ptr = NULL;

    /* Try PSRAM first if available */
#if CONFIG_SPIRAM_USE_MALLOC || CONFIG_SPIRAM
    ptr = heap_caps_malloc(size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
#endif

    /* Fallback to default memory (internal RAM) */
    if (ptr == NULL) {
        ptr = malloc(size);
    }

    return ptr;
}

/**
 * @brief Allocate aligned memory with PSRAM priority
 *
 * Tries to allocate aligned memory from PSRAM first, falls back to internal RAM if unavailable.
 * This is required for hardware JPEG decoder output buffers.
 *
 * @param size Size to allocate
 * @param alignment Alignment requirement (0 means use cache line alignment)
 * @param allocated_size Pointer to store actual allocated size (may be larger due to alignment)
 * @return Pointer to allocated memory, or NULL on failure
 */
static inline void *esp_eaf_malloc_aligned_prefer_psram(size_t size, size_t alignment, size_t *allocated_size)
{
    void *ptr = NULL;
    size_t aligned_size = size;

    /* Use cache alignment if not specified */
    if (alignment == 0) {
        alignment = esp_eaf_get_psram_cache_align();
    }

    /* Align size up to alignment boundary */
    aligned_size = ESP_EAF_ALIGN_UP(size, alignment);

    /* Try PSRAM first if available */
#if CONFIG_SPIRAM_USE_MALLOC || CONFIG_SPIRAM
    ptr = heap_caps_aligned_alloc(alignment, aligned_size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
#endif

    /* Fallback to default memory (internal RAM) with alignment */
    if (ptr == NULL) {
        ptr = heap_caps_aligned_alloc(alignment, aligned_size, MALLOC_CAP_DEFAULT);
    }

    if (allocated_size != NULL) {
        *allocated_size = (ptr != NULL) ? aligned_size : 0;
    }

    return ptr;
}

/**
 * @brief Allocate and zero-initialize memory with PSRAM priority
 *
 * Tries to allocate from PSRAM first, falls back to internal RAM if unavailable.
 */
static inline void *esp_eaf_calloc_prefer_psram(size_t num, size_t size)
{
    void *ptr = NULL;

    /* Try PSRAM first if available */
#if CONFIG_SPIRAM_USE_MALLOC || CONFIG_SPIRAM
    size_t total_size = num * size;
    ptr = heap_caps_malloc(total_size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (ptr != NULL) {
        memset(ptr, 0, total_size);
    }
#endif

    /* Fallback to default memory (internal RAM) */
    if (ptr == NULL) {
        ptr = calloc(num, size);
    }

    return ptr;
}

/**
 * @brief Allocate and zero-initialize aligned memory with PSRAM priority
 *
 * Tries to allocate aligned memory from PSRAM first, falls back to internal RAM if unavailable.
 * This is required for hardware JPEG decoder output buffers.
 *
 * @param num Number of elements
 * @param size Size of each element
 * @param alignment Alignment requirement (0 means use cache line alignment)
 * @param allocated_size Pointer to store actual allocated size (may be larger due to alignment)
 * @return Pointer to allocated memory, or NULL on failure
 */
static inline void *esp_eaf_calloc_aligned_prefer_psram(size_t num, size_t size, size_t alignment, size_t *allocated_size)
{
    size_t total_size = num * size;
    void *ptr = esp_eaf_malloc_aligned_prefer_psram(total_size, alignment, allocated_size);
    if (ptr != NULL) {
        memset(ptr, 0, *allocated_size);
    }
    return ptr;
}

/**
 * @brief Calculate checksum for EAF data
 */
static uint32_t esp_eaf_format_calc_checksum(const uint8_t *data, uint32_t length)
{
    uint32_t checksum = 0;
    for (uint32_t i = 0; i < length; i++) {
        checksum += data[i];
    }
    return checksum;
}

/* Huffman Tree Helper Functions */
static esp_eaf_huffman_node_t* esp_eaf_huffman_node_create()
{
    esp_eaf_huffman_node_t* node = (esp_eaf_huffman_node_t*)esp_eaf_calloc_prefer_psram(1, sizeof(esp_eaf_huffman_node_t));
    return node;
}

static void esp_eaf_huffman_tree_free(esp_eaf_huffman_node_t* node)
{
    if (!node) {
        return;
    }
    esp_eaf_huffman_tree_free(node->left);
    esp_eaf_huffman_tree_free(node->right);
    free(node);
}

static esp_err_t esp_eaf_huffman_decode_data(const uint8_t* encoded_data, size_t encoded_len,
                                             const uint8_t* dict_data, size_t dict_len,
                                             uint8_t* decoded_data, size_t* decoded_len)
{
    if (!encoded_data || !dict_data || encoded_len == 0 || dict_len == 0) {
        *decoded_len = 0;
        return ESP_OK;
    }

    /* Get padding bits from dictionary */
    uint8_t padding_bits = dict_data[0];
    size_t dict_pos = 1;

    /* Reconstruct Huffman Tree */
    esp_eaf_huffman_node_t* root = esp_eaf_huffman_node_create();
    if (!root) {
        ESP_LOGE(TAG, "Failed to allocate root node");
        return ESP_ERR_NO_MEM;
    }
    esp_eaf_huffman_node_t* current_node = NULL;

    while (dict_pos < dict_len) {
        if (dict_pos + 1 >= dict_len) {
            ESP_LOGE(TAG, "Dictionary truncated at position %zu", dict_pos);
            esp_eaf_huffman_tree_free(root);
            return ESP_FAIL;
        }
        uint8_t symbol = dict_data[dict_pos++];
        uint8_t code_len = dict_data[dict_pos++];

        if (code_len == 0 || code_len > 64) {
            ESP_LOGE(TAG, "Invalid code length: %d", code_len);
            esp_eaf_huffman_tree_free(root);
            return ESP_FAIL;
        }

        size_t code_byte_len = (code_len + 7) / 8;
        if (dict_pos + code_byte_len > dict_len) {
            ESP_LOGE(TAG, "Dictionary overflow at position %zu", dict_pos);
            esp_eaf_huffman_tree_free(root);
            return ESP_FAIL;
        }

        uint64_t code = 0;
        for (size_t i = 0; i < code_byte_len; ++i) {
            code = (code << 8) | dict_data[dict_pos++];
        }

        /* Insert symbol into tree */
        current_node = root;
        for (int bit_pos = code_len - 1; bit_pos >= 0; --bit_pos) {
            int bit_val = (code >> bit_pos) & 1;
            if (bit_val == 0) {
                if (!current_node->left) {
                    current_node->left = esp_eaf_huffman_node_create();
                    if (!current_node->left) {
                        ESP_LOGE(TAG, "Failed to allocate Huffman node");
                        esp_eaf_huffman_tree_free(root);
                        return ESP_ERR_NO_MEM;
                    }
                }
                current_node = current_node->left;
            } else {
                if (!current_node->right) {
                    current_node->right = esp_eaf_huffman_node_create();
                    if (!current_node->right) {
                        ESP_LOGE(TAG, "Failed to allocate Huffman node");
                        esp_eaf_huffman_tree_free(root);
                        return ESP_ERR_NO_MEM;
                    }
                }
                current_node = current_node->right;
            }
        }
        current_node->is_leaf = 1;
        current_node->symbol = symbol;
    }

    /* Calculate total bits to decode */
    size_t total_bits = encoded_len * 8;
    if (padding_bits > 0) {
        total_bits -= padding_bits;
    }

    current_node = root;
    size_t decoded_pos = 0;

    /* Process each bit in the encoded data */
    for (size_t bit_index = 0; bit_index < total_bits; bit_index++) {
        size_t byte_idx = bit_index / 8;
        int bit_offset = 7 - (bit_index % 8);  /* Most significant bit first */
        int bit_val = (encoded_data[byte_idx] >> bit_offset) & 1;

        if (bit_val == 0) {
            current_node = current_node->left;
        } else {
            current_node = current_node->right;
        }

        if (current_node == NULL) {
            ESP_LOGE(TAG, "Invalid path in Huffman tree at bit %zu", bit_index);
            esp_eaf_huffman_tree_free(root);
            return ESP_FAIL;
        }

        if (current_node->is_leaf) {
            decoded_data[decoded_pos++] = current_node->symbol;
            current_node = root;
        }
    }

    *decoded_len = decoded_pos;
    esp_eaf_huffman_tree_free(root);
    return ESP_OK;
}

/**********************
 *  HEADER FUNCTIONS
 **********************/

esp_eaf_format_t esp_eaf_header_parse(const uint8_t *file_data, size_t file_size, esp_eaf_header_t *header)
{
    memset(header, 0, sizeof(esp_eaf_header_t));

    memcpy(header->format, file_data, 2);
    header->format[2] = '\0';

    if (strncmp(header->format, "_S", 2) == 0) {
        memcpy(header->version, file_data + 3, 6);

        header->bit_depth = file_data[9];

        if (header->bit_depth != 4 && header->bit_depth != 8 && header->bit_depth != 24) {
            ESP_LOGE(TAG, "Invalid bit depth: %d", header->bit_depth);
            return ESP_EAF_FORMAT_INVALID;
        }

        header->width = *(uint16_t *)(file_data + 10);
        header->height = *(uint16_t *)(file_data + 12);
        header->blocks = *(uint16_t *)(file_data + 14);
        header->block_height = *(uint16_t *)(file_data + 16);

        header->block_len = (uint32_t *)esp_eaf_malloc_prefer_psram(header->blocks * sizeof(uint32_t));
        if (header->block_len == NULL) {
            ESP_LOGE(TAG, "Failed to allocate memory for block lengths");
            return ESP_EAF_FORMAT_INVALID;
        }

        for (int i = 0; i < header->blocks; i++) {
            header->block_len[i] = *(uint32_t *)(file_data + 18 + i * 4);
        }

        header->num_colors = 1 << header->bit_depth;

        if (header->bit_depth == 24) {
            header->num_colors = 0;
            header->palette = NULL;
        } else {
            header->palette = (uint8_t *)esp_eaf_malloc_prefer_psram(header->num_colors * 4);
            if (header->palette == NULL) {
                ESP_LOGE(TAG, "Failed to allocate memory for palette");
                free(header->block_len);
                header->block_len = NULL;
                return ESP_EAF_FORMAT_INVALID;
            }

            memcpy(header->palette, file_data + 18 + header->blocks * 4, header->num_colors * 4);
        }
        header->data_offset = 18 + header->blocks * 4 + header->num_colors * 4;
        return ESP_EAF_FORMAT_VALID;

    } else if (strncmp(header->format, "_R", 2) == 0) {
        uint8_t file_length = *(uint8_t *)(file_data + 2);

        header->palette = (uint8_t *)esp_eaf_malloc_prefer_psram(file_length + 1);
        if (header->palette == NULL) {
            ESP_LOGE(TAG, "Failed to allocate memory for redirect filename");
            return ESP_EAF_FORMAT_INVALID;
        }

        memcpy(header->palette, file_data + 3, file_length);
        header->palette[file_length] = '\0';
        header->num_colors = file_length + 1;

        return ESP_EAF_FORMAT_REDIRECT;
    } else if (strncmp(header->format, "_C", 2) == 0) {
        return ESP_EAF_FORMAT_INVALID;
    } else {
        ESP_LOGE(TAG, "Invalid format: %s", header->format);
        return ESP_EAF_FORMAT_INVALID;
    }
}

void esp_eaf_free_header(esp_eaf_header_t *header)
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

void esp_eaf_calculate_offsets(const esp_eaf_header_t *header, uint32_t *offsets)
{
    offsets[0] = header->data_offset;
    for (int i = 1; i < header->blocks; i++) {
        offsets[i] = offsets[i - 1] + header->block_len[i - 1];
    }
}

/**********************
 *  PALETTE FUNCTIONS
 **********************/

esp_eaf_color_t esp_eaf_palette_get_color(const esp_eaf_header_t *header, uint8_t color_index, bool swap_bytes)
{
    const uint8_t *color_data = &header->palette[color_index * 4];
    /* RGB888: R=color[2], G=color[1], B=color[0] */
    /* RGB565: R: (color[2] & 0xF8) << 8, G: (color[1] & 0xFC) << 3, B: (color[0] & 0xF8) >> 3 */
    esp_eaf_color_t result;
    uint16_t rgb565_value = swap_bytes ? __builtin_bswap16(((color_data[2] & 0xF8) << 8) | ((color_data[1] & 0xFC) << 3) | ((color_data[0] & 0xF8) >> 3)) : \
                            ((color_data[2] & 0xF8) << 8) | ((color_data[1] & 0xFC) << 3) | ((color_data[0] & 0xF8) >> 3);
    result.full = rgb565_value;
    return result;
}

/**********************
 *  DECODING FUNCTIONS
 **********************/

/**
 * @brief Decode Huffman + RLE compressed data
 */
static esp_err_t esp_eaf_huffman_rle_decode(const uint8_t *input_data, size_t input_size,
                                            uint8_t *output_buffer, size_t *out_size,
                                            bool swap_color)
{
    if (out_size == NULL || *out_size == 0) {
        ESP_LOGE(TAG, "Output size is invalid");
        return ESP_FAIL;
    }

    uint8_t *huffman_buffer = esp_eaf_malloc_prefer_psram(*out_size);
    if (huffman_buffer == NULL) {
        ESP_LOGE(TAG, "Failed to allocate memory for Huffman buffer");
        return ESP_FAIL;
    }

    size_t huffman_out_size = *out_size;
    esp_err_t ret = esp_eaf_huffman_decode(input_data, input_size, huffman_buffer, &huffman_out_size, swap_color);
    if (ret == ESP_OK) {
        ret = esp_eaf_rle_decode(huffman_buffer, huffman_out_size, output_buffer, out_size, swap_color);
    }

    free(huffman_buffer);
    return ret;
}

/**
 * @brief Register a decoder for a specific encoding type
 */
static esp_err_t esp_eaf_register_decoder(esp_eaf_encoding_t type, esp_eaf_block_decoder_t decoder)
{
    if (type >= ESP_EAF_ENCODING_MAX) {
        ESP_LOGE(TAG, "Invalid encoding type: %d", type);
        return ESP_ERR_INVALID_ARG;
    }

    if (s_esp_eaf_decoder[type] != NULL) {
        ESP_LOGW(TAG, "Decoder already registered for type: %d", type);
    }

    s_esp_eaf_decoder[type] = decoder;
    return ESP_OK;
}

/**
 * @brief Initialize all decoders
 */
static esp_err_t esp_eaf_init_decoders(void)
{
    esp_err_t ret = esp_eaf_register_decoder(ESP_EAF_ENCODING_RLE, esp_eaf_rle_decode);
    if (ret != ESP_OK) {
        return ret;
    }

    ret = esp_eaf_register_decoder(ESP_EAF_ENCODING_HUFFMAN, esp_eaf_huffman_rle_decode);
    if (ret != ESP_OK) {
        return ret;
    }

    ret = esp_eaf_register_decoder(ESP_EAF_ENCODING_JPEG, esp_eaf_jpeg_decode);
    if (ret != ESP_OK) {
        return ret;
    }

    ret = esp_eaf_register_decoder(ESP_EAF_ENCODING_HUFFMAN_DIRECT, esp_eaf_huffman_decode);
    if (ret != ESP_OK) {
        return ret;
    }

    /* Initialize hardware JPEG decoder if available */
    ret = esp_eaf_hw_jpeg_init();
    return ret;
}

/**
 * @brief Convert BGR to RGB565
 */
static inline uint16_t esp_eaf_rgb565_from_bgr(const uint8_t *bgr, bool swap_color)
{
    uint16_t value = ((bgr[2] & 0xF8) << 8) | ((bgr[1] & 0xFC) << 3) | ((bgr[0] & 0xF8) >> 3);
    return swap_color ? __builtin_bswap16(value) : value;
}

/**
 * @brief Get the number of rows in a block
 */
static inline int esp_eaf_get_block_rows(const esp_eaf_header_t *header, int block_index)
{
    int start_row = block_index * header->block_height;
    int remaining = header->height - start_row;
    if (remaining <= 0) {
        return 0;
    }
    return remaining > header->block_height ? header->block_height : remaining;
}

esp_err_t esp_eaf_block_decode(const esp_eaf_header_t *header, const uint8_t *frame_data,
                               int block_index, uint8_t *decode_buffer, uint8_t *alpha_buffer,
                               bool swap_color, esp_eaf_palette_cache_t *cache)
{
    /* Calculate block offsets */
    uint32_t *offsets = (uint32_t *)esp_eaf_malloc_prefer_psram(header->blocks * sizeof(uint32_t));
    if (offsets == NULL) {
        ESP_LOGE(TAG, "Failed to allocate memory for block offsets");
        return ESP_FAIL;
    }

    esp_eaf_calculate_offsets(header, offsets);

    /* Get block data */
    const uint8_t *block_data = frame_data + offsets[block_index];
    int block_len = header->block_len[block_index];
    uint8_t encoding_type = block_data[0];
    int width = header->width;
    int block_height = esp_eaf_get_block_rows(header, block_index);

    esp_err_t decode_result = ESP_FAIL;

    if (encoding_type >= sizeof(s_esp_eaf_decoder) / sizeof(s_esp_eaf_decoder[0])) {
        ESP_LOGE(TAG, "Unknown encoding type: 0x%02X", encoding_type);
        free(offsets);
        return ESP_FAIL;
    }

    esp_eaf_block_decoder_t decoder = s_esp_eaf_decoder[encoding_type];
    if (!decoder) {
        ESP_LOGE(TAG, "No decoder for encoding type: 0x%02X", encoding_type);
        free(offsets);
        return ESP_FAIL;
    }

    if (block_height <= 0) {
        free(offsets);
        return ESP_OK;
    }

    size_t pixel_count = (size_t)width * block_height;
    uint8_t *alpha_block = NULL;
    if (alpha_buffer != NULL) {
        size_t alpha_offset = (size_t)block_index * header->block_height * width;
        alpha_block = alpha_buffer + alpha_offset;
    }
    size_t decode_capacity = 0;
    bool is_jpeg = (encoding_type == ESP_EAF_ENCODING_JPEG);

    if (is_jpeg) {
        decode_capacity = pixel_count * 2;
    } else {
        switch (header->bit_depth) {
        case 24:
            decode_capacity = pixel_count * 3;
            break;
        case 8:
            decode_capacity = pixel_count;
            break;
        case 4:
            decode_capacity = (pixel_count + 1) / 2;
            break;
        default:
            ESP_LOGE(TAG, "Unsupported bit depth: %d", header->bit_depth);
            free(offsets);
            return ESP_ERR_INVALID_ARG;
        }
    }

    uint8_t *work_buffer = NULL;
    uint8_t *decode_target = decode_buffer;

    if (!is_jpeg) {
        work_buffer = (uint8_t *)esp_eaf_malloc_prefer_psram(decode_capacity);
        if (!work_buffer) {
            ESP_LOGE(TAG, "Failed to allocate working buffer");
            free(offsets);
            return ESP_ERR_NO_MEM;
        }
        decode_target = work_buffer;
    }

    size_t out_size = decode_capacity;
    decode_result = decoder(block_data + 1, block_len - 1, decode_target, &out_size, swap_color);

    if (decode_result != ESP_OK) {
        ESP_LOGE(TAG, "Failed to decode block %d", block_index);
        free(offsets);
        if (work_buffer) {
            free(work_buffer);
        }
        return ESP_FAIL;
    }

    if (is_jpeg) {
        if (alpha_block != NULL) {
            memset(alpha_block, 0xFF, pixel_count);
        }
        free(offsets);
        return ESP_OK;
    }

    uint16_t *dest = (uint16_t *)decode_buffer;
    size_t dest_index = 0;

    switch (header->bit_depth) {
    case 24: {
        if (out_size < pixel_count * 3) {
            ESP_LOGE(TAG, "Insufficient data for 24-bit block %d", block_index);
            free(offsets);
            free(work_buffer);
            return ESP_FAIL;
        }
        for (size_t i = 0; i < pixel_count; i++) {
            const uint8_t *bgr = &work_buffer[i * 3];
            dest[dest_index++] = esp_eaf_rgb565_from_bgr(bgr, swap_color);
            if (alpha_block != NULL) {
                alpha_block[i] = 0xFF;
            }
        }
        break;
    }
    case 8: {
        if (header->palette == NULL) {
            ESP_LOGE(TAG, "Palette missing for 8-bit block");
            free(offsets);
            free(work_buffer);
            return ESP_FAIL;
        }
        if (out_size < pixel_count) {
            ESP_LOGE(TAG, "Insufficient data for 8-bit block %d", block_index);
            free(offsets);
            free(work_buffer);
            return ESP_FAIL;
        }
        for (size_t i = 0; i < pixel_count; i++) {
            uint8_t ci = work_buffer[i];
            uint16_t color;
            uint8_t a;

            if (cache && cache->color[ci] != 0xFFFFFFFF) {
                /* Cache hit */
                color = (uint16_t)cache->color[ci];
                a = cache->alpha[ci];
            } else {
                /* Cache miss - compute color */
                const uint8_t *p = &header->palette[ci * 4];
                a = p[3];
                uint16_t rgb = ((p[2] & 0xF8) << 8) | ((p[1] & 0xFC) << 3) | ((p[0] & 0xF8) >> 3);
                color = swap_color ? __builtin_bswap16(rgb) : rgb;
                if (cache) {
                    cache->color[ci] = color;
                    cache->alpha[ci] = a;
                }
            }

            if (a) {
                dest[dest_index++] = color;
            } else {
                dest_index++;
            }
            if (alpha_block != NULL) {
                alpha_block[i] = a;
            }
        }
        break;
    }
    case 4: {
        if (header->palette == NULL) {
            ESP_LOGE(TAG, "Palette missing for 4-bit block");
            free(offsets);
            free(work_buffer);
            return ESP_FAIL;
        }
        size_t px = 0;
        for (size_t i = 0; i < out_size && px < pixel_count; i++) {
            uint8_t hi = (work_buffer[i] >> 4) & 0x0F;
            uint16_t color_hi;
            uint8_t a_hi;

            if (cache && cache->color[hi] != 0xFFFFFFFF) {
                color_hi = (uint16_t)cache->color[hi];
                a_hi = cache->alpha[hi];
            } else {
                const uint8_t *p = &header->palette[hi * 4];
                a_hi = p[3];
                uint16_t rgb = ((p[2] & 0xF8) << 8) | ((p[1] & 0xFC) << 3) | ((p[0] & 0xF8) >> 3);
                color_hi = swap_color ? __builtin_bswap16(rgb) : rgb;
                if (cache) {
                    cache->color[hi] = color_hi;
                    cache->alpha[hi] = a_hi;
                }
            }

            if (a_hi) {
                dest[px++] = color_hi;
            } else {
                px++;
            }
            if (alpha_block != NULL) {
                alpha_block[px - 1] = a_hi;
            }
            if (px >= pixel_count) {
                break;
            }

            uint8_t lo = work_buffer[i] & 0x0F;
            uint16_t color_lo;
            uint8_t a_lo;

            if (cache && cache->color[lo] != 0xFFFFFFFF) {
                color_lo = (uint16_t)cache->color[lo];
                a_lo = cache->alpha[lo];
            } else {
                const uint8_t *p = &header->palette[lo * 4];
                a_lo = p[3];
                uint16_t rgb = ((p[2] & 0xF8) << 8) | ((p[1] & 0xFC) << 3) | ((p[0] & 0xF8) >> 3);
                color_lo = swap_color ? __builtin_bswap16(rgb) : rgb;
                if (cache) {
                    cache->color[lo] = color_lo;
                    cache->alpha[lo] = a_lo;
                }
            }

            if (a_lo) {
                dest[px++] = color_lo;
            } else {
                px++;
            }
            if (alpha_block != NULL) {
                alpha_block[px - 1] = a_lo;
            }
        }
        break;
    }
    default:
        break;
    }

    free(offsets);
    free(work_buffer);
    return ESP_OK;
}

esp_err_t esp_eaf_rle_decode(const uint8_t *input_data, size_t input_size,
                             uint8_t *output_buffer, size_t *out_size,
                             bool swap_color)
{
    (void)swap_color; /* Unused parameter */

    size_t in_pos = 0;
    size_t out_pos = 0;

    while (in_pos + 1 <= input_size) {
        uint8_t repeat_count = input_data[in_pos++];
        uint8_t repeat_value = input_data[in_pos++];

        if (out_pos + repeat_count > *out_size) {
            ESP_LOGE(TAG, "Decompressed buffer overflow: %zu > %zu", out_pos + repeat_count, *out_size);
            return ESP_FAIL;
        }

        for (uint8_t i = 0; i < repeat_count; i++) {
            output_buffer[out_pos++] = repeat_value;
        }
    }

    *out_size = out_pos;
    return ESP_OK;
}

/**********************
 *  HARDWARE JPEG FUNCTIONS
 **********************/

#if ESP_EAF_HW_JPEG_ENABLED
/**
 * @brief Initialize hardware JPEG decoder
 */
esp_err_t esp_eaf_hw_jpeg_init(void)
{
    if (s_hw_jpeg_handle != NULL) {
        return ESP_OK;  /* Already initialized */
    }

    jpeg_decode_engine_cfg_t decode_eng_cfg = {
        .timeout_ms = 40,
    };

    esp_err_t ret = jpeg_new_decoder_engine(&decode_eng_cfg, &s_hw_jpeg_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create HW JPEG decoder engine: %s", esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGI(TAG, "Hardware JPEG decoder initialized");
    return ESP_OK;
}

/**
 * @brief Deinitialize hardware JPEG decoder
 */
esp_err_t esp_eaf_hw_jpeg_deinit(void)
{
    if (s_hw_jpeg_handle != NULL) {
        jpeg_del_decoder_engine(s_hw_jpeg_handle);
        s_hw_jpeg_handle = NULL;
        ESP_LOGI(TAG, "Hardware JPEG decoder deinitialized");
    }
    return ESP_OK;
}

/**
 * @brief Decode JPEG using hardware decoder
 */
static esp_err_t esp_eaf_jpeg_decode_hardware(const uint8_t *jpeg_data, size_t jpeg_size,
                                              uint8_t *decode_buffer, size_t *out_size,
                                              bool swap_color)
{
    if (s_hw_jpeg_handle == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    /* First parse header to get dimensions using software decoder */
    uint32_t w, h;
    jpeg_dec_config_t sw_config = {
        .output_type = JPEG_PIXEL_FORMAT_RGB565_LE,
        .rotate = JPEG_ROTATE_0D,
    };

    jpeg_dec_handle_t sw_dec;
    if (jpeg_dec_open(&sw_config, &sw_dec) != ESP_OK) {
        return ESP_FAIL;
    }

    jpeg_dec_io_t *jpeg_io = esp_eaf_malloc_prefer_psram(sizeof(jpeg_dec_io_t));
    jpeg_dec_header_info_t *header_info = esp_eaf_malloc_prefer_psram(sizeof(jpeg_dec_header_info_t));
    if (!jpeg_io || !header_info) {
        free(jpeg_io);
        free(header_info);
        jpeg_dec_close(sw_dec);
        return ESP_ERR_NO_MEM;
    }

    jpeg_io->inbuf = (unsigned char *)jpeg_data;
    jpeg_io->inbuf_len = jpeg_size;

    if (jpeg_dec_parse_header(sw_dec, jpeg_io, header_info) != JPEG_ERR_OK) {
        free(jpeg_io);
        free(header_info);
        jpeg_dec_close(sw_dec);
        return ESP_FAIL;
    }

    w = header_info->width;
    h = header_info->height;

    free(jpeg_io);
    free(header_info);
    jpeg_dec_close(sw_dec);

    /* Check if dimensions support hardware decoding (16-pixel alignment required) */
    if ((w % 16 != 0) || (h % 16 != 0)) {
        return ESP_ERR_NOT_SUPPORTED;  /* Fall back to software */
    }

    /* Validate output buffer size */
    size_t required_size = w * h * 2;  /* RGB565 = 2 bytes per pixel */
    if (*out_size < required_size) {
        ESP_LOGE(TAG, "Output buffer too small for HW decode: need %zu, got %zu", required_size, *out_size);
        return ESP_ERR_INVALID_SIZE;
    }

    /* Check buffer alignment for hardware JPEG decoder */
    size_t cache_align = esp_eaf_get_psram_cache_align();
    uintptr_t buffer_addr = (uintptr_t)decode_buffer;
    if ((buffer_addr % cache_align) != 0) {
        ESP_LOGD(TAG, "Decode buffer address not aligned: %p (required: %zu)", decode_buffer, cache_align);
        return ESP_ERR_NOT_SUPPORTED;  /* Fall back to software */
    }
    if ((required_size % cache_align) != 0) {
        ESP_LOGD(TAG, "Decode buffer size not aligned: %zu (required: %zu)", required_size, cache_align);
        return ESP_ERR_NOT_SUPPORTED;  /* Fall back to software */
    }

    /* Configure hardware decoder */
    jpeg_decode_cfg_t hw_cfg = {
        .output_format = JPEG_DECODE_OUT_FORMAT_RGB565,
        .rgb_order = swap_color ? JPEG_DEC_RGB_ELEMENT_ORDER_RGB : JPEG_DEC_RGB_ELEMENT_ORDER_BGR,
    };

    /* Perform hardware decode */
    uint32_t output_size = 0;
    esp_err_t ret = jpeg_decoder_process(
                        s_hw_jpeg_handle,
                        &hw_cfg,
                        (uint8_t *)jpeg_data,
                        jpeg_size,
                        decode_buffer,
                        required_size,
                        &output_size
                    );

    if (ret == ESP_OK) {
        *out_size = required_size;
        ESP_LOGD(TAG, "Hardware JPEG decode: %"PRIu32"x%"PRIu32" -> %"PRIu32" bytes", w, h, output_size);
    }

    return ret;
}
#else
esp_err_t esp_eaf_hw_jpeg_init(void)
{
    ESP_LOGD(TAG, "Hardware JPEG decoder not available on this chip");
    return ESP_OK;
}

esp_err_t esp_eaf_hw_jpeg_deinit(void)
{
    return ESP_OK;
}
#endif

esp_err_t esp_eaf_jpeg_decode(const uint8_t *jpeg_data, size_t jpeg_size,
                              uint8_t *decode_buffer, size_t *out_size, bool swap_color)
{
#if ESP_LV_EAF_ENABLE_SW_JPEG

#if ESP_EAF_HW_JPEG_ENABLED
    /* Try hardware decoding first if available */
    if (s_hw_jpeg_handle != NULL) {
        esp_err_t hw_ret = esp_eaf_jpeg_decode_hardware(jpeg_data, jpeg_size,
                                                        decode_buffer, out_size, swap_color);
        if (hw_ret == ESP_OK) {
            return ESP_OK;
        }
        /* If hardware decode fails, fall through to software */
        ESP_LOGD(TAG, "Hardware JPEG decode failed (0x%x), falling back to software", hw_ret);
    }
#endif

    /* Software decoding fallback */
    uint32_t w, h;
    jpeg_dec_config_t config = {
        .output_type = swap_color ? JPEG_PIXEL_FORMAT_RGB565_BE : JPEG_PIXEL_FORMAT_RGB565_LE,
        .rotate = JPEG_ROTATE_0D,
    };

    jpeg_dec_handle_t jpeg_dec;
    if (jpeg_dec_open(&config, &jpeg_dec) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open JPEG decoder");
        return ESP_FAIL;
    }

    jpeg_dec_io_t *jpeg_io = esp_eaf_malloc_prefer_psram(sizeof(jpeg_dec_io_t));
    jpeg_dec_header_info_t *out_info = esp_eaf_malloc_prefer_psram(sizeof(jpeg_dec_header_info_t));
    if (!jpeg_io || !out_info) {
        if (jpeg_io) {
            free(jpeg_io);
        }
        if (out_info) {
            free(out_info);
        }
        jpeg_dec_close(jpeg_dec);
        ESP_LOGE(TAG, "Failed to allocate memory for JPEG decoder");
        return ESP_FAIL;
    }

    jpeg_io->inbuf = (unsigned char *)jpeg_data;
    jpeg_io->inbuf_len = jpeg_size;

    jpeg_error_t ret = jpeg_dec_parse_header(jpeg_dec, jpeg_io, out_info);
    if (ret == JPEG_ERR_OK) {
        w = out_info->width;
        h = out_info->height;

        size_t required_size = w * h * 2; /* RGB565 = 2 bytes per pixel */
        if (*out_size < required_size) {
            ESP_LOGE(TAG, "Output buffer too small: need %zu, got %zu", required_size, *out_size);
            free(jpeg_io);
            free(out_info);
            jpeg_dec_close(jpeg_dec);
            return ESP_ERR_INVALID_SIZE;
        }

        jpeg_io->outbuf = decode_buffer;
        ret = jpeg_dec_process(jpeg_dec, jpeg_io);
        if (ret != JPEG_ERR_OK) {
            free(jpeg_io);
            free(out_info);
            jpeg_dec_close(jpeg_dec);
            ESP_LOGE(TAG, "Failed to decode JPEG: %d", ret);
            return ESP_FAIL;
        }
        *out_size = required_size;
    } else {
        free(jpeg_io);
        free(out_info);
        jpeg_dec_close(jpeg_dec);
        ESP_LOGE(TAG, "Failed to parse JPEG header");
        return ESP_FAIL;
    }

    free(jpeg_io);
    free(out_info);
    jpeg_dec_close(jpeg_dec);
    return ESP_OK;
#else
    ESP_LOGW(TAG, "JPEG block detected but software JPEG decoder is DISABLED");
    ESP_LOGW(TAG, "Enable CONFIG_ESP_LV_EAF_ENABLE_SW_JPEG in menuconfig to decode JPEG blocks");
    return ESP_ERR_NOT_SUPPORTED;
#endif
}

esp_err_t esp_eaf_huffman_decode(const uint8_t *input_data, size_t input_size,
                                 uint8_t *output_buffer, size_t *out_size,
                                 bool swap_color)
{
    (void)swap_color; /* Unused parameter */
    size_t decoded_size = *out_size;

    if (!input_data || input_size < 3 || !output_buffer) {
        ESP_LOGE(TAG, "Invalid parameters: input_data=%p, input_size=%zu, output_buffer=%p",
                 input_data, input_size, output_buffer);
        return ESP_FAIL;
    }

    uint16_t dict_size = (input_data[1] << 8) | input_data[0];
    if (input_size < 2 + dict_size) {
        ESP_LOGE(TAG, "Compressed data too short for dictionary");
        return ESP_FAIL;
    }

    size_t encoded_size = input_size - 2 - dict_size;
    if (encoded_size == 0) {
        ESP_LOGE(TAG, "No data to decode");
        return ESP_FAIL;
    }

    esp_err_t ret = esp_eaf_huffman_decode_data(input_data + 2 + dict_size, encoded_size,
                                                input_data + 2, dict_size,
                                                output_buffer, &decoded_size);

    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Huffman decoding failed: %d", ret);
        return ESP_FAIL;
    }

    if (decoded_size > *out_size) {
        ESP_LOGE(TAG, "Decoded data too large: %zu > %zu", decoded_size, *out_size);
        return ESP_FAIL;
    }
    *out_size = decoded_size;

    return ESP_OK;
}

/**********************
 *  FORMAT FUNCTIONS
 **********************/

esp_err_t esp_eaf_format_init(const uint8_t *data, size_t data_len, esp_eaf_format_handle_t *ret_parser)
{
    static bool decoders_initialized = false;

    if (!decoders_initialized) {
        esp_err_t ret = esp_eaf_init_decoders();
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to initialize decoders");
            return ret;
        }
        decoders_initialized = true;
    }

    esp_err_t ret = ESP_OK;
    esp_eaf_frame_entry_t *entries = NULL;
    esp_eaf_format_ctx_t *parser = NULL;

    ESP_GOTO_ON_FALSE(data != NULL && ret_parser != NULL, ESP_ERR_INVALID_ARG, err, TAG, "Invalid input");
    ESP_GOTO_ON_FALSE(data_len >= ESP_EAF_TABLE_OFFSET, ESP_ERR_INVALID_SIZE, err, TAG, "File too small");

    parser = (esp_eaf_format_ctx_t *)esp_eaf_calloc_prefer_psram(1, sizeof(esp_eaf_format_ctx_t));
    ESP_GOTO_ON_FALSE(parser, ESP_ERR_NO_MEM, err, TAG, "No memory for parser handle");

    /* Check file format magic number: 0x89 */
    ESP_GOTO_ON_FALSE(data[ESP_EAF_FORMAT_OFFSET] == ESP_EAF_FORMAT_MAGIC, ESP_ERR_INVALID_CRC, err, TAG, "Bad file format magic");

    /* Check for EAF/AAF format string */
    const char *format_str = (const char *)(data + ESP_EAF_STR_OFFSET);
    bool is_valid = (memcmp(format_str, ESP_EAF_FORMAT_STR, 3) == 0) || (memcmp(format_str, ESP_AAF_FORMAT_STR, 3) == 0);
    ESP_GOTO_ON_FALSE(is_valid, ESP_ERR_INVALID_CRC, err, TAG, "Bad file format string (expected EAF or AAF)");

    int total_frames = *(int *)(data + ESP_EAF_NUM_OFFSET);
    uint32_t stored_chk = *(uint32_t *)(data + ESP_EAF_CHECKSUM_OFFSET);
    uint32_t stored_len = *(uint32_t *)(data + ESP_EAF_TABLE_LEN);

    ESP_GOTO_ON_FALSE(total_frames > 0, ESP_ERR_INVALID_SIZE, err, TAG, "Invalid frame count %d", total_frames);
    ESP_GOTO_ON_FALSE((size_t)total_frames <= (SIZE_MAX / sizeof(esp_eaf_frame_table_entry_t)), ESP_ERR_INVALID_SIZE, err, TAG, "Frame count too large");

    const size_t payload_available = data_len - ESP_EAF_TABLE_OFFSET;
    const size_t table_bytes = (size_t)total_frames * sizeof(esp_eaf_frame_table_entry_t);
    ESP_GOTO_ON_FALSE(table_bytes <= payload_available, ESP_ERR_INVALID_SIZE, err, TAG, "Frame table exceeds file size");

    const size_t stored_len_sz = stored_len;
    ESP_GOTO_ON_FALSE(stored_len_sz <= payload_available, ESP_ERR_INVALID_SIZE, err, TAG, "Stored length beyond file");
    ESP_GOTO_ON_FALSE(stored_len_sz >= table_bytes, ESP_ERR_INVALID_SIZE, err, TAG, "Stored length smaller than table");
    ESP_GOTO_ON_FALSE((size_t)ESP_EAF_TABLE_OFFSET + stored_len_sz <= data_len, ESP_ERR_INVALID_SIZE, err, TAG, "Stored data exceeds file");

    const uint32_t calculated_chk = esp_eaf_format_calc_checksum((uint8_t *)(data + ESP_EAF_TABLE_OFFSET), stored_len);
    ESP_GOTO_ON_FALSE(calculated_chk == stored_chk, ESP_ERR_INVALID_CRC, err, TAG, "Bad full checksum");

    entries = (esp_eaf_frame_entry_t *)esp_eaf_malloc_prefer_psram(sizeof(esp_eaf_frame_entry_t) * total_frames);
    ESP_GOTO_ON_FALSE(entries, ESP_ERR_NO_MEM, err, TAG, "No memory for frame entries");

    esp_eaf_frame_table_entry_t *table = (esp_eaf_frame_table_entry_t *)(data + ESP_EAF_TABLE_OFFSET);
    const size_t frame_region_len = stored_len_sz - table_bytes;
    const size_t frame_region_offset = ESP_EAF_TABLE_OFFSET + table_bytes;
    const uint8_t *frame_region_base = data + frame_region_offset;

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

        (entries + i)->table = (table + i);
        (entries + i)->frame_mem = (const char *)frame_mem;
    }

    parser->entries = entries;
    parser->total_frames = total_frames;

    *ret_parser = (esp_eaf_format_handle_t)parser;

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

esp_err_t esp_eaf_format_deinit(esp_eaf_format_handle_t handle)
{
    if (handle == NULL) {
        ESP_LOGW(TAG, "Handle is invalid");
        return ESP_ERR_INVALID_ARG;
    }

    esp_eaf_format_ctx_t *parser = (esp_eaf_format_ctx_t *)(handle);
    if (parser) {
        if (parser->entries) {
            free(parser->entries);
        }
        free(parser);
    }
    return ESP_OK;
}

int esp_eaf_format_get_total_frames(esp_eaf_format_handle_t handle)
{
    if (handle == NULL) {
        ESP_LOGE(TAG, "Handle is invalid");
        return -1;
    }

    esp_eaf_format_ctx_t *parser = (esp_eaf_format_ctx_t *)(handle);
    return parser->total_frames;
}

const uint8_t *esp_eaf_format_get_frame_data(esp_eaf_format_handle_t handle, int index)
{
    if (handle == NULL) {
        ESP_LOGE(TAG, "Handle is invalid");
        return NULL;
    }

    esp_eaf_format_ctx_t *parser = (esp_eaf_format_ctx_t *)(handle);

    if (parser->total_frames > index) {
        return (const uint8_t *)((parser->entries + index)->frame_mem + ESP_EAF_MAGIC_LEN);
    } else {
        ESP_LOGE(TAG, "Invalid index: %d. Maximum index is %d", index, parser->total_frames);
        return NULL;
    }
}

int esp_eaf_format_get_frame_size(esp_eaf_format_handle_t handle, int index)
{
    if (handle == NULL) {
        ESP_LOGE(TAG, "Handle is invalid");
        return -1;
    }

    esp_eaf_format_ctx_t *parser = (esp_eaf_format_ctx_t *)(handle);

    if (parser->total_frames > index) {
        return ((parser->entries + index)->table->frame_size - ESP_EAF_MAGIC_LEN);
    } else {
        ESP_LOGE(TAG, "Invalid index: %d. Maximum index is %d", index, parser->total_frames);
        return -1;
    }
}
