/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
/* Codec registry shared by all EAF containers. */
/* SPDX-License-Identifier: Apache-2.0 */
#include "codec_internal.h"

static const eaf_codec_ops_t s_codecs[] = {
    {
        ESP_EAF_CONTAINER_EAF4, EAF4_FRAME_FULL, "FULL",
        eaf4_lossless_decode, 0, NULL, NULL
    },
    {
        ESP_EAF_CONTAINER_EAF4, EAF4_FRAME_XOR_BGRA8888, "XOR_BGRA8888",
        eaf4_lossless_decode, 0, NULL, NULL
    },
    {
        ESP_EAF_CONTAINER_EAF4, EAF4_FRAME_XOR_PLANAR_BGRA8888, "XOR_PLANAR_BGRA8888",
        eaf4_lossless_decode, 0, NULL, NULL
    },
    {
        ESP_EAF_CONTAINER_EAF4, EAF4_FRAME_TILE_XOR_PLANAR_BGRA8888, "TILE_XOR_PLANAR_BGRA8888",
        eaf4_lossless_decode, 0, NULL, NULL
    },
    {
        ESP_EAF_CONTAINER_EAF4, EAF4_FRAME_TILE_XOR_PACKED_BGRA8888, "TILE_XOR_PACKED_BGRA8888",
        eaf4_lossless_decode, 0, NULL, NULL
    },
    {
        ESP_EAF_CONTAINER_EAF4, EAF4_FRAME_TILE_JOINT_DELTA_BGRA8888, "TILE_JOINT_DELTA_BGRA8888",
        eaf4_lossless_decode, 0, NULL, NULL
    },
    {
        ESP_EAF_CONTAINER_EAF4, EAF4_FRAME_TILE_BOUNDED_BGRA8888, "TILE_BOUNDED_BGRA8888",
        eaf_bounded_decode, 0, NULL, NULL
    },
    {
        ESP_EAF_CONTAINER_EAF4, EAF4_FRAME_TILE_DELTA_RGB565, "TILE_DELTA_RGB565",
        eaf4_lossless_decode, 0, NULL, NULL
    },
    {
        ESP_EAF_CONTAINER_EAF4, EAF4_FRAME_TILE_JOINT_DELTA_RGB565, "TILE_JOINT_DELTA_RGB565",
        eaf4_lossless_decode, 0, NULL, NULL
    },
    {
        ESP_EAF_CONTAINER_EAF4, EAF4_FRAME_TILE_BOUNDED_RGB565, "TILE_BOUNDED_RGB565",
        eaf_bounded_decode, 0, NULL, NULL
    },
    {
        ESP_EAF_CONTAINER_EAF4, EAF4_FRAME_HSUB_BGRA8888, "HSUB_BGRA8888",
        eaf4_lossless_decode, 0, NULL, NULL
    },
    {
        ESP_EAF_CONTAINER_EAF4, EAF4_FRAME_TILE_SUB_PACKED_BGRA8888, "TILE_SUB_PACKED_BGRA8888",
        eaf4_lossless_decode, 0, NULL, NULL
    },
    {
        ESP_EAF_CONTAINER_EAF, ESP_EAF_CODEC_RLE, "RLE",
        eaf_rle_block_decode, 0, NULL, NULL
    },
    {
        ESP_EAF_CONTAINER_EAF, ESP_EAF_CODEC_HUFFMAN_RLE, "HUFFMAN_RLE",
        eaf_huffman_rle_block_decode, 0, NULL, NULL
    },
    {
        ESP_EAF_CONTAINER_EAF, ESP_EAF_CODEC_JPEG, "JPEG",
        eaf_jpeg_block_decode, sizeof(eaf_jpeg_state_t), eaf_jpeg_reset, NULL
    },
    {
        ESP_EAF_CONTAINER_EAF, ESP_EAF_CODEC_HUFFMAN, "HUFFMAN",
        eaf_huffman_block_decode, 0, NULL, NULL
    },
    {
        ESP_EAF_CONTAINER_EAF, ESP_EAF_CODEC_LOSSLESS, "LOSSLESS",
        eaf_lossless_decode, sizeof(eaf_lossless_state_t), NULL, eaf_lossless_destroy
    },
    {
        ESP_EAF_CONTAINER_EAF, ESP_EAF_CODEC_BOUNDED, "BOUNDED",
        eaf_bounded_decode, 0, NULL, NULL
    },
    {
        ESP_EAF_CONTAINER_EAF, ESP_EAF_CODEC_JPEG_DELTA, "JPEG_DELTA",
        eaf_jpeg_delta_decode, 0, NULL, NULL
    },
};

const eaf_codec_ops_t *eaf_codec_find(esp_eaf_container_t container, uint8_t mode)
{
    for (size_t i = 0; i < sizeof(s_codecs) / sizeof(s_codecs[0]); i++) {
        if (s_codecs[i].container == container && s_codecs[i].mode == mode) {
            return &s_codecs[i];
        }
    }
    return NULL;
}

const char *esp_eaf_codec_name(esp_eaf_codec_id_t codec_id)
{
    if (codec_id < ESP_EAF_CODEC_RLE || codec_id >= ESP_EAF_CODEC_MAX) {
        return "UNKNOWN";
    }
    const eaf_codec_ops_t *ops = eaf_codec_find(ESP_EAF_CONTAINER_EAF, (uint8_t)codec_id);
    return ops ? ops->name : "UNKNOWN";
}

const char *eaf4_frame_mode_name(eaf4_frame_mode_t frame_mode)
{
    if (frame_mode < EAF4_FRAME_FULL || frame_mode >= EAF4_FRAME_MAX) {
        return "UNKNOWN";
    }
    const eaf_codec_ops_t *ops = eaf_codec_find(ESP_EAF_CONTAINER_EAF4, (uint8_t)frame_mode);
    return ops ? ops->name : "UNKNOWN";
}
