/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdbool.h>

#include "esp_display_present.h"
#include "esp_display_present_blit.h"
#include "esp_display_present_drawbuf.h"
#include "esp_display_present_target.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

/**
 * Asynchronous tile → framebuffer copy shared by PARTITION and TE compose.
 * Enabled only when the drawbuf pool has two or more buffers: the producer
 * renders the next band while a worker task swaps (optional) and blits the
 * previous one. The producer task alone submits and waits; the worker only
 * gives a slot back after its blit.
 */
typedef struct {
    bool enabled;
    const esp_display_present_drawbuf_pool_t *pool;
    TaskHandle_t task;
    QueueHandle_t jobs;
    SemaphoreHandle_t idle[ESP_DISPLAY_PRESENT_DRAWBUF_MAX_BUFFERS];
    StaticSemaphore_t idle_storage[ESP_DISPLAY_PRESENT_DRAWBUF_MAX_BUFFERS];
    bool busy[ESP_DISPLAY_PRESENT_DRAWBUF_MAX_BUFFERS];
    /** First worker failure since the last drain; read after a slot give. */
    esp_err_t error;
} present_async_copy_t;

/** Prepare @p copy for @p pool; stays disabled unless @p allow and count >= 2. */
esp_err_t present_async_copy_init(present_async_copy_t *copy,
                                  const esp_display_present_drawbuf_pool_t *pool,
                                  bool allow);

/** Drain pending work and release worker resources. */
void present_async_copy_deinit(present_async_copy_t *copy);

/** Wait until drawbuf @p slot is no longer read or written by the worker. */
void present_async_copy_wait_slot(present_async_copy_t *copy, uint8_t slot);

/**
 * Queue @p region for the worker. With @p swap_rgb565 the worker first swaps
 * the tile in place to panel byte order. ESP_ERR_NOT_SUPPORTED means the
 * caller must drain and copy inline.
 */
esp_err_t present_async_copy_submit(present_async_copy_t *copy,
                                    esp_display_present_target_t *target,
                                    const esp_display_present_blit_fb_t *dst,
                                    const esp_display_presenter_region_t *region,
                                    bool swap_rgb565);

/** Wait for every queued blit and return the first worker error. */
esp_err_t present_async_copy_drain(present_async_copy_t *copy);
