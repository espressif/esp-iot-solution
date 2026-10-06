/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    void *fbcpy_handle;
    void *dma2d_mutex;
    void *dma2d_done_sem;
} esp_display_present_hw_resource_t;

esp_display_present_hw_resource_t *esp_display_present_hw_resource_acquire(int max_pending_trans_num);

esp_display_present_hw_resource_t *esp_display_present_hw_resource_peek(void);

/** Release lazily-created shared DMA2D resources after all users stop. */
esp_err_t esp_display_present_hw_resource_release(void);

esp_err_t esp_display_present_dma2d_copy_sync(void *trans_desc, uint32_t timeout_ms);

#ifdef __cplusplus
}
#endif
