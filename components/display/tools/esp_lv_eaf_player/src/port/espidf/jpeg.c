/*
 * SPDX-FileCopyrightText: 2025-2026 Espressif Systems (Shanghai) CO LTD
 * SPDX-License-Identifier: Apache-2.0
 */
/** @file jpeg.c
 *  @brief ESP-IDF software/hardware JPEG backend.
 */
#include <inttypes.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <sys/lock.h>

#include "esp_cache.h"
#include "esp_check.h"
#include "esp_heap_caps.h"
#include "esp_memory_utils.h"
#include "esp_log.h"
#include "esp_private/esp_cache_private.h"

#include "eaf_internal.h"
#include "codec_internal.h"
#include "esp_lv_eaf_player_config.h"

#if ESP_LV_EAF_ENABLE_SW_JPEG
#include "esp_jpeg_common.h"
#include "esp_jpeg_dec.h"
#endif

static const char *TAG = "eaf_codec_support";

#include "eaf_port.h"
/* Hardware JPEG decoder support detection */
#if ESP_LV_EAF_ENABLE_HW_JPEG
#define ESP_EAF_HW_JPEG_ENABLED 1
#include "driver/jpeg_decode.h"
#else
#define ESP_EAF_HW_JPEG_ENABLED 0
#endif

#if ESP_EAF_HW_JPEG_ENABLED
static jpeg_decoder_handle_t s_hw_jpeg_handle;
static _lock_t s_hw_jpeg_lock;
static size_t s_hw_jpeg_users;

static inline size_t get_psram_cache_align(void)
{
    size_t cache_align = 0;
    esp_cache_get_alignment(0, &cache_align);
#if CONFIG_SPIRAM_USE_MALLOC || CONFIG_SPIRAM
    size_t external_align = 0;
    esp_cache_get_alignment(MALLOC_CAP_SPIRAM, &external_align);
    if (external_align > cache_align) {
        cache_align = external_align;
    }
#endif
    if (cache_align == 0) {
        cache_align = 4;
    }
    return cache_align;
}
#endif
/**********************
 *  HARDWARE JPEG FUNCTIONS
 **********************/

#if ESP_EAF_HW_JPEG_ENABLED
/**
 * @brief Initialize hardware JPEG decoder
 */
static esp_err_t hw_jpeg_init_locked(void)
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

/* All accesses to the engine and user count hold s_hw_jpeg_lock. */
void eaf_jpeg_backend_retain(void)
{
    _lock_acquire(&s_hw_jpeg_lock);
    s_hw_jpeg_users++;
    _lock_release(&s_hw_jpeg_lock);
}

void eaf_jpeg_backend_release(void)
{
    _lock_acquire(&s_hw_jpeg_lock);
    if (s_hw_jpeg_users && --s_hw_jpeg_users == 0 && s_hw_jpeg_handle) {
        jpeg_del_decoder_engine(s_hw_jpeg_handle);
        s_hw_jpeg_handle = NULL;
    }
    _lock_release(&s_hw_jpeg_lock);
}

/**
 * @brief Decode JPEG using hardware decoder
 */
static esp_err_t jpeg_decode_hardware(const uint8_t *jpeg_data, size_t jpeg_size,
                                      uint8_t *decode_buffer, size_t *out_size,
                                      bool swap_color)
{
    if (s_hw_jpeg_handle == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    /* Query JPEG picture info and enforce S31 / P4 2D-DMA horizontal macroblock alignment (hb) */
    uint32_t w = 0, h = 0, hb = 0;
    jpeg_decode_picture_info_t pic_info = {0};
    if (jpeg_decoder_get_info(jpeg_data, jpeg_size, &pic_info) != ESP_OK) {
        return ESP_FAIL;
    }
    w = pic_info.width;
    h = pic_info.height;
    uint32_t rgb888_hb = 0;
    if (pic_info.sample_method == JPEG_DOWN_SAMPLING_YUV444) {
        hb = 32;
        rgb888_hb = 40;
    } else if (pic_info.sample_method == JPEG_DOWN_SAMPLING_YUV422) {
        hb = 64;
        rgb888_hb = 32;
    } else if (pic_info.sample_method == JPEG_DOWN_SAMPLING_YUV420) {
        hb = 48;
        rgb888_hb = 32;
    }
    bool convert_rgb888 = hb && w % hb != 0 && rgb888_hb && w % rgb888_hb == 0;
    if (!hb || (!convert_rgb888 && w % hb != 0) || h % 16 != 0) {
        return ESP_ERR_NOT_SUPPORTED;
    }

    /* Validate output buffer size */
    size_t required_size = w * h * 2;  /* RGB565 = 2 bytes per pixel */
    if (*out_size < required_size) {
        ESP_LOGE(TAG, "Output buffer too small for HW decode: need %zu, got %zu", required_size, *out_size);
        return ESP_ERR_INVALID_SIZE;
    }

    /* Check buffer alignment for hardware JPEG decoder */
    size_t cache_align = get_psram_cache_align();
    uintptr_t buffer_addr = (uintptr_t)decode_buffer;
    if ((buffer_addr % cache_align) != 0) {
        ESP_LOGD(TAG, "Decode buffer address not aligned: %p (required: %zu)", decode_buffer, cache_align);
        return ESP_ERR_NOT_SUPPORTED;  /* Fall back to software */
    }
    if ((required_size % cache_align) != 0) {
        ESP_LOGD(TAG, "Decode buffer size not aligned: %zu (required: %zu)", required_size, cache_align);
        return ESP_ERR_NOT_SUPPORTED;  /* Fall back to software */
    }

    size_t hw_capacity = required_size;
    uint8_t *rgb888 = NULL;
    if (convert_rgb888) {
        rgb888 = eaf_alloc_aligned((size_t)w * h * 3, 0, &hw_capacity);
        if (!rgb888) {
            return ESP_ERR_NO_MEM;
        }
    }

    /* Configure hardware decoder */
    jpeg_decode_cfg_t hw_cfg = {
        .output_format = convert_rgb888 ? JPEG_DECODE_OUT_FORMAT_RGB888 : JPEG_DECODE_OUT_FORMAT_RGB565,
        .rgb_order = JPEG_DEC_RGB_ELEMENT_ORDER_BGR,
    };

    /* Flash-mapped streams are CPU-readable but cannot feed JPEG DMA. */
    uint8_t *input_copy = NULL;
    if (esp_ptr_in_drom(jpeg_data)) {
        input_copy = eaf_alloc(jpeg_size);
        if (!input_copy) {
            free(rgb888);
            return ESP_ERR_NO_MEM;
        }
        memcpy(input_copy, jpeg_data, jpeg_size);
        jpeg_data = input_copy;
    }

    /* Perform hardware decode */
    uint32_t output_size = 0;
    esp_err_t ret = jpeg_decoder_process(
                        s_hw_jpeg_handle,
                        &hw_cfg,
                        (uint8_t *)jpeg_data,
                        jpeg_size,
                        rgb888 ? rgb888 : decode_buffer,
                        hw_capacity,
                        &output_size
                    );

    free(input_copy);
    if (ret == ESP_OK && rgb888) {
        uint16_t *pixels = (uint16_t *)decode_buffer;
        for (size_t i = 0; i < (size_t)w * h; i++) {
            uint8_t *color = rgb888 + 3 * i;
            pixels[i] = ((uint16_t)(color[2] >> 3) << 11) |
                        ((uint16_t)(color[1] >> 2) << 5) | (color[0] >> 3);
        }
    }
    free(rgb888);
    if (ret == ESP_OK && swap_color) {
        uint16_t *pixels = (uint16_t *)decode_buffer;
        for (size_t i = 0; i < (size_t)w * h; i++) {
            pixels[i] = (uint16_t)((pixels[i] << 8) | (pixels[i] >> 8));
        }
    }
    if (ret == ESP_OK) {
        *out_size = required_size;
        ESP_LOGD(TAG, "Hardware JPEG decode: %"PRIu32"x%"PRIu32" -> %"PRIu32" bytes", w, h, output_size);
    }

    return ret;
}
#else
void eaf_jpeg_backend_retain(void) {}
void eaf_jpeg_backend_release(void) {}
#endif

esp_err_t eaf_jpeg_decode(const uint8_t *jpeg_data, size_t jpeg_size,
                          uint8_t *decode_buffer, size_t *out_size, bool swap_color)
{
    if (!jpeg_data || !jpeg_size || !decode_buffer || !out_size) {
        return ESP_ERR_INVALID_ARG;
    }
#if ESP_EAF_HW_JPEG_ENABLED
    _lock_acquire(&s_hw_jpeg_lock);
    esp_err_t hw_ret = hw_jpeg_init_locked();
    if (hw_ret == ESP_OK) {
        hw_ret = jpeg_decode_hardware(jpeg_data, jpeg_size, decode_buffer, out_size, swap_color);
    }
    _lock_release(&s_hw_jpeg_lock);
    if (hw_ret == ESP_OK) {
        return ESP_OK;
    }
#if !ESP_LV_EAF_ENABLE_SW_JPEG
    return hw_ret;
#endif
    ESP_LOGD(TAG, "Hardware JPEG unavailable (0x%x), falling back to software", hw_ret);
#endif

#if ESP_LV_EAF_ENABLE_SW_JPEG
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

    jpeg_dec_io_t *jpeg_io = eaf_alloc(sizeof(jpeg_dec_io_t));
    jpeg_dec_header_info_t *out_info = eaf_alloc(sizeof(jpeg_dec_header_info_t));
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

        /* S3's SIMD software decoder requires 16-byte output alignment. A
         * strip atlas or a block offset may be less aligned than the frame. */
        uint8_t *aligned_output = NULL;
        if ((uintptr_t)decode_buffer & 15U) {
            aligned_output = eaf_alloc_aligned(required_size, 16, NULL);
            if (!aligned_output) {
                free(jpeg_io);
                free(out_info);
                jpeg_dec_close(jpeg_dec);
                return ESP_ERR_NO_MEM;
            }
        }
        jpeg_io->outbuf = aligned_output ? aligned_output : decode_buffer;
        ret = jpeg_dec_process(jpeg_dec, jpeg_io);
        if (ret == JPEG_ERR_OK && aligned_output) {
            memcpy(decode_buffer, aligned_output, required_size);
        }
        free(aligned_output);
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
    ESP_LOGW(TAG, "No enabled JPEG decoder can handle this block");
    ESP_LOGW(TAG, "Enable CONFIG_ESP_LV_EAF_ENABLE_SW_JPEG in menuconfig to decode JPEG blocks");
    return ESP_ERR_NOT_SUPPORTED;
#endif
}
