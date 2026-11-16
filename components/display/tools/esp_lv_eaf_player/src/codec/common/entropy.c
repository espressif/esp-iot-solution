/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
/* SPDX-License-Identifier: Apache-2.0 */
#include <stdlib.h>
#include <string.h>
#include "codec_internal.h"
#include "eaf_port.h"
static const char *TAG = "eaf_entropy";
/* Huffman Tree Helper Functions */
static eaf_huffman_node_t* huffman_node_create()
{
    eaf_huffman_node_t* node = (eaf_huffman_node_t*)calloc(1, sizeof(eaf_huffman_node_t));
    return node;
}

static void huffman_tree_free(eaf_huffman_node_t* node)
{
    if (!node) {
        return;
    }
    huffman_tree_free(node->left);
    huffman_tree_free(node->right);
    free(node);
}

static esp_err_t huffman_decode_data(const uint8_t* encoded_data, size_t encoded_len,
                                     const uint8_t* dict_data, size_t dict_len,
                                     uint8_t* decoded_data, size_t* decoded_len)
{
    if (!encoded_data || !dict_data || !decoded_data || !decoded_len ||
            encoded_len == 0 || dict_len < 1 || dict_data[0] > 7) {
        return ESP_ERR_INVALID_SIZE;
    }
    size_t capacity = *decoded_len;

    /* Get padding bits from dictionary */
    uint8_t padding_bits = dict_data[0];
    size_t dict_pos = 1;

    /* Reconstruct Huffman Tree */
    eaf_huffman_node_t* root = huffman_node_create();
    if (!root) {
        ESP_LOGE(TAG, "Failed to allocate root node");
        return ESP_ERR_NO_MEM;
    }
    eaf_huffman_node_t* current_node = NULL;

    while (dict_pos < dict_len) {
        if (dict_pos + 1 >= dict_len) {
            ESP_LOGE(TAG, "Dictionary truncated at position %zu", dict_pos);
            huffman_tree_free(root);
            return ESP_FAIL;
        }
        uint8_t symbol = dict_data[dict_pos++];
        uint8_t code_len = dict_data[dict_pos++];

        if (code_len == 0 || code_len > 64) {
            ESP_LOGE(TAG, "Invalid code length: %d", code_len);
            huffman_tree_free(root);
            return ESP_FAIL;
        }

        size_t code_byte_len = (code_len + 7) / 8;
        if (dict_pos + code_byte_len > dict_len) {
            ESP_LOGE(TAG, "Dictionary overflow at position %zu", dict_pos);
            huffman_tree_free(root);
            return ESP_FAIL;
        }

        uint64_t code = 0;
        for (size_t i = 0; i < code_byte_len; ++i) {
            code = (code << 8) | dict_data[dict_pos++];
        }

        /* Insert symbol into tree */
        current_node = root;
        for (int bit_pos = code_len - 1; bit_pos >= 0; --bit_pos) {
            if (current_node->is_leaf) {
                huffman_tree_free(root);
                return ESP_ERR_INVALID_SIZE;
            }
            int bit_val = (code >> bit_pos) & 1;
            if (bit_val == 0) {
                if (!current_node->left) {
                    current_node->left = huffman_node_create();
                    if (!current_node->left) {
                        ESP_LOGE(TAG, "Failed to allocate Huffman node");
                        huffman_tree_free(root);
                        return ESP_ERR_NO_MEM;
                    }
                }
                current_node = current_node->left;
            } else {
                if (!current_node->right) {
                    current_node->right = huffman_node_create();
                    if (!current_node->right) {
                        ESP_LOGE(TAG, "Failed to allocate Huffman node");
                        huffman_tree_free(root);
                        return ESP_ERR_NO_MEM;
                    }
                }
                current_node = current_node->right;
            }
        }
        if (current_node->is_leaf || current_node->left || current_node->right) {
            huffman_tree_free(root); return ESP_ERR_INVALID_SIZE;
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
            huffman_tree_free(root);
            return ESP_FAIL;
        }

        if (current_node->is_leaf) {
            if (decoded_pos >= capacity) {
                huffman_tree_free(root);
                return ESP_ERR_INVALID_SIZE;
            }
            decoded_data[decoded_pos++] = current_node->symbol;
            current_node = root;
        }
    }

    if (current_node != root) {
        huffman_tree_free(root);
        return ESP_ERR_INVALID_SIZE;
    }
    *decoded_len = decoded_pos;
    huffman_tree_free(root);
    return ESP_OK;
}
esp_err_t eaf_rle_decode(const uint8_t *input_data, size_t input_size,
                         uint8_t *output_buffer, size_t *out_size,
                         bool swap_color)
{
    (void)swap_color;
    if (!input_data || !output_buffer || !out_size) {
        return ESP_ERR_INVALID_ARG;
    }
    if (input_size % 2) {
        return ESP_ERR_INVALID_SIZE;
    }

    size_t in_pos = 0;
    size_t out_pos = 0;

    while (in_pos < input_size) {
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

esp_err_t eaf_huffman_decode(const uint8_t *input_data, size_t input_size,
                             uint8_t *output_buffer, size_t *out_size,
                             bool swap_color)
{
    (void)swap_color; /* Unused parameter */
    if (!out_size) {
        return ESP_ERR_INVALID_ARG;
    }
    size_t decoded_size = *out_size;

    if (!input_data || input_size < 3 || !output_buffer) {
        ESP_LOGE(TAG, "Invalid parameters: input_data=%p, input_size=%zu, output_buffer=%p",
                 input_data, input_size, output_buffer);
        return ESP_FAIL;
    }

    uint16_t dict_size = (input_data[1] << 8) | input_data[0];
    if (input_size < 2U + dict_size) {
        ESP_LOGE(TAG, "Compressed data too short for dictionary");
        return ESP_FAIL;
    }

    size_t encoded_size = input_size - 2 - dict_size;
    /* Original EAF single-color Huffman dictionary: padding, symbol, zero code bits.
     * The block geometry supplies the output count; there is no encoded bitstream. */
    if (encoded_size == 0 && dict_size == 3 && input_data[2] == 0 && input_data[4] == 0) {
        memset(output_buffer, input_data[3], *out_size);
        return ESP_OK;
    }
    if (encoded_size == 0) {
        ESP_LOGE(TAG, "No data to decode");
        return ESP_FAIL;
    }

    esp_err_t ret = huffman_decode_data(input_data + 2 + dict_size, encoded_size,
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
