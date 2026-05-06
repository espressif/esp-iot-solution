/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "esp_err.h"
#include "esp_lcd_touch.h"
#include "lvgl.h"
#include "esp_lv_adapter_display.h"

#if CONFIG_ESP_LVGL_ADAPTER_ENABLE_BUTTON
#include "iot_button.h"
#endif

#if CONFIG_ESP_LVGL_ADAPTER_ENABLE_KNOB
#include "iot_knob.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

/*****************************************************************************
 *                          Touch Input Structures                           *
 *****************************************************************************/

/**
 * @brief Touch event callback collection
 *
 * All members are optional; set to NULL to disable. A single @c user_ctx is shared by all
 * callbacks and is set via @c esp_lv_adapter_touch_config_t or
 * @c esp_lv_adapter_set_touch_callbacks.
 */
typedef struct {
    /**
     * @brief Called from the touch interrupt handler (ISR context).
     *
     * Useful for latency-sensitive notification that a touch event occurred. The adapter will
     * still read touch data and feed it to LVGL as normal; this callback runs in parallel.
     *
     * @note Must comply with ISR restrictions: placed in IRAM (@c IRAM_ATTR), no blocking calls,
     *       no heap allocation.
     * @note The touch driver data buffer has not been read yet at this point.
     *
     * @param tp        ESP LCD touch handle
     * @param user_ctx  User context
     */
    void (*on_interrupt)(esp_lcd_touch_handle_t tp, void *user_ctx);

    /**
     * @brief Replace the default @c esp_lcd_touch_read_data + @c esp_lcd_touch_get_data calls.
     *
     * When set, the adapter skips both hardware read calls and invokes this callback instead to
     * obtain raw touch points. All subsequent processing (coordinate scaling, gesture recognition,
     * @c lv_indev_data_t fill) continues unchanged.
     *
     * Only invoked when new data should be read (IRQ mode: after semaphore is taken; polling
     * mode: every LVGL indev scan).
     *
     * @param tp        ESP LCD touch handle
     * @param points    Output buffer for raw touch points (unscaled hardware coordinates)
     * @param count     Output: number of valid entries written to @p points
     * @param max_count Capacity of @p points buffer
     * @param user_ctx  User context
     * @return ESP_OK on success (adapter proceeds with normal data processing)
     *         any other value is treated as a read error (adapter applies released state)
     */
    esp_err_t (*custom_touch_read)(esp_lcd_touch_handle_t tp,
                                   esp_lcd_touch_point_data_t *points, uint8_t *count,
                                   uint8_t max_count, void *user_ctx);

    void *user_ctx; /*!< User context passed to all callbacks */
} esp_lv_adapter_touch_callbacks_t;

/**
 * @brief Touch input device configuration structure
 */
typedef struct {
    lv_display_t *disp;                 /*!< Associated LVGL display */
    esp_lcd_touch_handle_t handle;      /*!< ESP LCD touch handle */
    struct {
        float x;                        /*!< Horizontal scale factor */
        float y;                        /*!< Vertical scale factor */
    } scale;                            /*!< Touch coordinate scaling */
    esp_lv_adapter_touch_callbacks_t callbacks; /*!< Optional event callbacks (all fields may be NULL) */
} esp_lv_adapter_touch_config_t;

/**
 * @brief Default touch device configuration macro
 *
 * @param display LVGL display handle
 * @param touch_handle ESP LCD touch handle
 */
#define ESP_LV_ADAPTER_TOUCH_DEFAULT_CONFIG(display, touch_handle) { \
    .disp = display,                                            \
    .handle = touch_handle,                                     \
    .scale = {                                                  \
        .x = 1.0f,                                              \
        .y = 1.0f,                                              \
    },                                                          \
}

/**
 * @brief Register a touch input device to LVGL
 *
 * Registers a touch device with LVGL and associates it with a display.
 *
 * @param[in] config Pointer to touch input device configuration
 *
 * @return
 *      - Pointer to LVGL input device object on success
 *      - NULL on failure
 */
lv_indev_t *esp_lv_adapter_register_touch(const esp_lv_adapter_touch_config_t *config);

/**
 * @brief Unregister a touch input device from LVGL
 *
 * Unregisters and deletes a previously registered touch device.
 *
 * @param[in] touch Pointer to LVGL touch input device
 *
 * @return
 *      - ESP_OK: Success
 *      - ESP_ERR_INVALID_ARG: Invalid input device pointer
 */
esp_err_t esp_lv_adapter_unregister_touch(lv_indev_t *touch);

/**
 * @brief Update touch event callbacks after registration
 *
 * Replaces any previously registered callbacks atomically. Pass NULL to clear all callbacks.
 *
 * @param[in] touch  LVGL touch input device handle returned by @c esp_lv_adapter_register_touch
 * @param[in] cbs    Callback collection (NULL to clear)
 *
 * @return
 *      - ESP_OK: Success
 *      - ESP_ERR_INVALID_ARG: Invalid touch handle
 *      - ESP_ERR_INVALID_STATE: Touch context not found
 */
esp_err_t esp_lv_adapter_set_touch_callbacks(lv_indev_t *touch,
                                             const esp_lv_adapter_touch_callbacks_t *cbs);

#if CONFIG_ESP_LVGL_ADAPTER_ENABLE_BUTTON
/*****************************************************************************
 *                       Button Input Structures                             *
 *****************************************************************************/

/**
 * @brief Button configuration or handle type
 *
 * For button component v3: Use const button_config_t*
 * For button component v4+: Use button_handle_t directly
 */
typedef button_handle_t esp_lv_adapter_button_config_or_handle_t;

/**
 * @brief Navigation buttons configuration structure
 */
typedef struct {
    lv_display_t *disp;                               /*!< Associated LVGL display */
    esp_lv_adapter_button_config_or_handle_t button_prev;   /*!< Previous button config/handle */
    esp_lv_adapter_button_config_or_handle_t button_next;   /*!< Next button config/handle */
    esp_lv_adapter_button_config_or_handle_t button_enter;  /*!< Enter button config/handle */
} esp_lv_adapter_nav_buttons_config_t;

/**
 * @brief Register navigation buttons to LVGL
 *
 * Registers three navigation buttons (previous, next, enter) with LVGL
 * for menu navigation and control.
 *
 * @param[in] config Pointer to navigation buttons configuration
 *
 * @return
 *      - Pointer to LVGL input device object on success
 *      - NULL on failure
 */
lv_indev_t *esp_lv_adapter_register_navigation_buttons(const esp_lv_adapter_nav_buttons_config_t *config);

/**
 * @brief Unregister navigation buttons from LVGL
 *
 * Unregisters and deletes previously registered navigation buttons.
 *
 * @param[in] buttons Pointer to LVGL navigation buttons input device
 *
 * @return
 *      - ESP_OK: Success
 *      - ESP_ERR_INVALID_ARG: Invalid input device pointer
 */
esp_err_t esp_lv_adapter_unregister_navigation_buttons(lv_indev_t *buttons);

#endif // CONFIG_ESP_LVGL_ADAPTER_ENABLE_BUTTON

#if CONFIG_ESP_LVGL_ADAPTER_ENABLE_KNOB
/*****************************************************************************
 *                       Encoder Input Structures                            *
 *****************************************************************************/

/**
 * @brief Encoder input device configuration structure
 */
typedef struct {
    lv_display_t *disp;                               /*!< Associated LVGL display */
    const knob_config_t *encoder_a_b;                 /*!< Encoder A/B phase configuration */
    esp_lv_adapter_button_config_or_handle_t encoder_enter; /*!< Encoder button config/handle */
} esp_lv_adapter_encoder_config_t;

/**
 * @brief Register an encoder input device to LVGL
 *
 * Registers a rotary encoder with optional button with LVGL.
 * The encoder can be used for menu navigation and value adjustment.
 *
 * @param[in] config Pointer to encoder configuration
 *
 * @return
 *      - Pointer to LVGL input device object on success
 *      - NULL on failure
 */
lv_indev_t *esp_lv_adapter_register_encoder(const esp_lv_adapter_encoder_config_t *config);

/**
 * @brief Unregister an encoder input device from LVGL
 *
 * Unregisters and deletes a previously registered encoder device.
 *
 * @param[in] encoder Pointer to LVGL encoder input device
 *
 * @return
 *      - ESP_OK: Success
 *      - ESP_ERR_INVALID_ARG: Invalid input device pointer
 */
esp_err_t esp_lv_adapter_unregister_encoder(lv_indev_t *encoder);

#endif // CONFIG_ESP_LVGL_ADAPTER_ENABLE_KNOB

#ifdef __cplusplus
}
#endif
