/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief ESP LCD touch input multiplexer
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "driver/gpio.h"
#include "esp_err.h"
#include "esp_lcd_touch.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Virtual touch frame
 */
typedef struct {
    uint8_t count; /*!< Number of valid touch points */
    esp_lcd_touch_point_data_t points[CONFIG_ESP_LCD_TOUCH_MAX_POINTS]; /*!< Touch points in the wrapped driver's logical coordinate space */
} esp_lcd_touch_mux_frame_t;

/**
 * @brief Touch multiplexer configuration
 */
typedef struct {
    uint8_t queue_depth; /*!< Maximum number of queued virtual touch frames */
    bool own_physical_handle; /*!< Delete the wrapped physical touch handle when the multiplexer is deleted */
} esp_lcd_touch_mux_config_t;

/**
 * @brief Touch multiplexer runtime statistics
 */
typedef struct {
    uint32_t injected_frames; /*!< Number of virtual frames queued */
    uint32_t consumed_frames; /*!< Number of virtual frames consumed */
    uint32_t software_interrupts; /*!< Number of GPTimer wake alarms scheduled */
    uint32_t injection_failures; /*!< Number of virtual frame enqueue failures */
    uint32_t forced_releases; /*!< Number of release frames forced by timeout or cancellation */
    uint8_t queue_high_watermark; /*!< Maximum number of frames observed in the queue */
    bool virtual_active; /*!< Whether a virtual touch session is active */
} esp_lcd_touch_mux_stats_t;

/**
 * @brief Default touch multiplexer configuration
 */
#define ESP_LCD_TOUCH_MUX_CONFIG_DEFAULT() { \
    .queue_depth = 32,                     \
    .own_physical_handle = false,          \
}

/**
 * @brief Create a touch multiplexer around a physical touch driver
 *
 * @note The physical touch handle must not have an interrupt callback registered. Without a callback on the mux handle, virtual frames are consumed by polling.
 *
 * @param[in] physical Physical touch handle to wrap
 * @param[in] config Touch multiplexer configuration
 * @param[out] out_touch Touch multiplexer handle
 * @return
 *          - ESP_OK on success
 *          - ESP_ERR_INVALID_ARG if an argument or configuration value is invalid
 *          - ESP_ERR_INVALID_STATE if the physical touch interrupt is already owned
 *          - ESP_ERR_NO_MEM if memory allocation fails
 */
esp_err_t esp_lcd_touch_mux_new(esp_lcd_touch_handle_t physical,
                                const esp_lcd_touch_mux_config_t *config,
                                esp_lcd_touch_handle_t *out_touch);

/**
 * @brief Begin an exclusive virtual touch session
 *
 * @param[in] touch Touch multiplexer handle
 * @param[in] timeout_ms Producer lock timeout in milliseconds, or UINT32_MAX to wait indefinitely
 * @return
 *          - ESP_OK on success
 *          - ESP_ERR_INVALID_ARG if the handle is invalid
 *          - ESP_ERR_INVALID_STATE if another input source is active
 *          - ESP_ERR_TIMEOUT if the producer lock cannot be acquired
 */
esp_err_t esp_lcd_touch_mux_begin(esp_lcd_touch_handle_t touch, uint32_t timeout_ms);

/**
 * @brief Queue a virtual touch frame
 *
 * The frame is copied into the queue before this function returns.
 *
 * @param[in] touch Touch multiplexer handle
 * @param[in] frame Complete virtual touch frame
 * @param[in] timeout_ms Queue and producer lock timeout in milliseconds, or UINT32_MAX to wait indefinitely
 * @return
 *          - ESP_OK on success
 *          - ESP_ERR_INVALID_ARG if the handle or frame is invalid
 *          - ESP_ERR_INVALID_STATE if no virtual touch session is active
 *          - ESP_ERR_TIMEOUT if the lock or queue operation times out
 *          - ESP_ERR_xxx if triggering the software interrupt fails
 */
esp_err_t esp_lcd_touch_mux_inject(esp_lcd_touch_handle_t touch,
                                   const esp_lcd_touch_mux_frame_t *frame,
                                   uint32_t timeout_ms);

/**
 * @brief End a virtual touch session
 *
 * A release frame is queued and the session ends after the frame is consumed.
 *
 * @param[in] touch Touch multiplexer handle
 * @param[in] timeout_ms Queue and producer lock timeout in milliseconds, or UINT32_MAX to wait indefinitely
 * @return
 *          - ESP_OK on success
 *          - ESP_ERR_INVALID_ARG if the handle is invalid
 *          - ESP_ERR_INVALID_STATE if no virtual touch session is active
 *          - ESP_ERR_TIMEOUT if the producer lock cannot be acquired
 *          - ESP_ERR_xxx if queuing the release or triggering the software interrupt fails
 */
esp_err_t esp_lcd_touch_mux_end(esp_lcd_touch_handle_t touch, uint32_t timeout_ms);

/**
 * @brief Cancel a virtual touch session
 *
 * Pending motion frames are discarded and a release frame is forced when a session is active.
 *
 * @param[in] touch Touch multiplexer handle
 * @return
 *          - ESP_OK on success or if no virtual touch session is active
 *          - ESP_ERR_INVALID_ARG if the handle is invalid
 *          - ESP_ERR_TIMEOUT if the producer or state lock cannot be acquired
 *          - ESP_ERR_xxx if queuing the release or triggering the software interrupt fails
 */
esp_err_t esp_lcd_touch_mux_cancel(esp_lcd_touch_handle_t touch);

/**
 * @brief Get touch multiplexer runtime statistics
 *
 * @param[in] touch Touch multiplexer handle
 * @param[out] out_stats Runtime statistics
 * @return
 *          - ESP_OK on success
 *          - ESP_ERR_INVALID_ARG if the handle or output pointer is invalid
 *          - ESP_ERR_TIMEOUT if the state lock cannot be acquired
 */
esp_err_t esp_lcd_touch_mux_get_stats(esp_lcd_touch_handle_t touch,
                                      esp_lcd_touch_mux_stats_t *out_stats);

#ifdef __cplusplus
}
#endif
