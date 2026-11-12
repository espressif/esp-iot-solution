/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once
#include "esp_err.h"
#include <stdint.h>
#include <stdbool.h>
#define ESP_COLOR_FOURCC_RGB16 0
#define ESP_COLOR_FOURCC_BGR24 1
typedef void *async_memcpy_dma2d_handle_t;
typedef struct {
    int unused;
} async_memcpy_dma2d_event_data_t;
typedef bool (*async_memcpy_dma2d_isr_cb_t)(async_memcpy_dma2d_handle_t, async_memcpy_dma2d_event_data_t *, void *);
typedef struct {
    unsigned backlog;
} async_memcpy_dma2d_config_t;
typedef struct {
    const void *src_buffer;
    uint32_t src_stride, src_height, src_x, src_y;
    void *dst_buffer;
    uint32_t dst_stride, dst_height, dst_x, dst_y, copy_width, copy_height;
    int pixel_format;
} async_memcpy_dma2d_trans_desc_t;
esp_err_t esp_async_memcpy_install_dma2d(const async_memcpy_dma2d_config_t *, async_memcpy_dma2d_handle_t *);
esp_err_t esp_async_memcpy_uninstall_dma2d(async_memcpy_dma2d_handle_t);
esp_err_t esp_async_memcpy_dma2d(async_memcpy_dma2d_handle_t, const async_memcpy_dma2d_trans_desc_t *, async_memcpy_dma2d_isr_cb_t, void *);
