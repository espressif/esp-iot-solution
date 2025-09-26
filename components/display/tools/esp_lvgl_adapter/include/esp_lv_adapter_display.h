/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdbool.h>
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_io.h"
#include "lvgl.h"

#ifndef LVGL_VERSION_MAJOR
#ifdef LV_VERSION_MAJOR
#define LVGL_VERSION_MAJOR LV_VERSION_MAJOR
#else
#error "LVGL_VERSION_MAJOR not provided by LVGL headers"
#endif
#endif

#if LVGL_VERSION_MAJOR < 9
typedef lv_disp_t lv_display_t;
#endif

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief LCD panel interface type enumeration
 */
typedef enum {
    ESP_LV_ADAPTER_PANEL_IF_RGB       = 0,    /*!< RGB interface */
    ESP_LV_ADAPTER_PANEL_IF_MIPI_DSI  = 1,    /*!< MIPI DSI interface */
    ESP_LV_ADAPTER_PANEL_IF_OTHER     = 2,    /*!< Other interface (SPI, I2C, etc.) */
} esp_lv_adapter_panel_interface_t;

/**
 * @brief Display rotation angle enumeration
 */
typedef enum {
    ESP_LV_ADAPTER_ROTATE_0   = 0,        /*!< No rotation */
    ESP_LV_ADAPTER_ROTATE_90  = 90,       /*!< Rotate 90 degrees clockwise */
    ESP_LV_ADAPTER_ROTATE_180 = 180,      /*!< Rotate 180 degrees */
    ESP_LV_ADAPTER_ROTATE_270 = 270,      /*!< Rotate 270 degrees clockwise */
} esp_lv_adapter_rotation_t;

/**
 * @brief Display tearing effect avoidance mode enumeration
 */
typedef enum {
    ESP_LV_ADAPTER_TEAR_AVOID_MODE_NONE           = 0,    /*!< No tearing avoidance (single buffer) */
    ESP_LV_ADAPTER_TEAR_AVOID_MODE_DOUBLE_FULL    = 1,    /*!< Double buffering with full frame buffers */
    ESP_LV_ADAPTER_TEAR_AVOID_MODE_TRIPLE_FULL    = 2,    /*!< Triple buffering with full frame buffers */
    ESP_LV_ADAPTER_TEAR_AVOID_MODE_DOUBLE_DIRECT  = 3,    /*!< Double buffering with direct mode */
    ESP_LV_ADAPTER_TEAR_AVOID_MODE_TRIPLE_PARTIAL = 4,    /*!< Triple buffering with partial refresh */
} esp_lv_adapter_tear_avoid_mode_t;

#define ESP_LV_ADAPTER_TEAR_AVOID_MODE_DEFAULT_MIPI_DSI     ESP_LV_ADAPTER_TEAR_AVOID_MODE_TRIPLE_PARTIAL
#define ESP_LV_ADAPTER_TEAR_AVOID_MODE_DEFAULT_RGB          ESP_LV_ADAPTER_TEAR_AVOID_MODE_TRIPLE_PARTIAL
#define ESP_LV_ADAPTER_TEAR_AVOID_MODE_DEFAULT              ESP_LV_ADAPTER_TEAR_AVOID_MODE_NONE

/**
 * @brief Display profile configuration structure
 *
 * Contains display characteristics and buffer configuration
 */
typedef struct {
    esp_lv_adapter_panel_interface_t interface;   /*!< Panel interface type */
    esp_lv_adapter_rotation_t rotation;           /*!< Display rotation angle */
    uint16_t hor_res;                       /*!< Horizontal resolution in pixels */
    uint16_t ver_res;                       /*!< Vertical resolution in pixels */
    uint16_t buffer_height;                 /*!< Draw buffer height in pixels */
    bool use_psram;                         /*!< Use PSRAM for buffers if available */
    bool enable_ppa_accel;                  /*!< Enable PPA hardware acceleration */
    bool require_double_buffer;             /*!< Require double buffering */
} esp_lv_adapter_display_profile_t;

/**
 * @brief Default display profile configuration macro
 *
 * @param _hor_res Horizontal resolution in pixels
 * @param _ver_res Vertical resolution in pixels
 * @param _rotation Display rotation angle
 */

/* Base macro with common default values */
#define ESP_LV_ADAPTER_DISPLAY_PROFILE_BASE_CONFIG(_hor_res, _ver_res, _rotation)         \
    .rotation              = (_rotation),                                            \
    .hor_res               = (_hor_res),                                             \
    .ver_res               = (_ver_res),

/* MIPI DSI interface default configuration */
#define ESP_LV_ADAPTER_DISPLAY_PROFILE_MIPI_DEFAULT_CONFIG(_hor_res, _ver_res, _rotation) \
    .interface             = ESP_LV_ADAPTER_PANEL_IF_MIPI_DSI,                            \
    ESP_LV_ADAPTER_DISPLAY_PROFILE_BASE_CONFIG(_hor_res, _ver_res, _rotation)             \
    .buffer_height         = 50,                                                    \
    .use_psram             = false,                                                 \
    .enable_ppa_accel      = false,                                                 \
    .require_double_buffer = false

/* RGB interface default configuration */
#define ESP_LV_ADAPTER_DISPLAY_PROFILE_RGB_DEFAULT_CONFIG(_hor_res, _ver_res, _rotation)  \
    .interface             = ESP_LV_ADAPTER_PANEL_IF_RGB,                                 \
    ESP_LV_ADAPTER_DISPLAY_PROFILE_BASE_CONFIG(_hor_res, _ver_res, _rotation)             \
    .buffer_height         = 50,                                                    \
    .use_psram             = false,                                                 \
    .enable_ppa_accel      = false,                                                 \
    .require_double_buffer = false

/* SPI/I2C and other interfaces default configuration with PSRAM*/
#define ESP_LV_ADAPTER_DISPLAY_PROFILE_SPI_WITH_PSRAM_DEFAULT_CONFIG(_hor_res, _ver_res, _rotation)  \
    .interface             = ESP_LV_ADAPTER_PANEL_IF_OTHER,                               \
    ESP_LV_ADAPTER_DISPLAY_PROFILE_BASE_CONFIG(_hor_res, _ver_res, _rotation)             \
    .buffer_height         = _ver_res,                                              \
    .use_psram             = true,                                                  \
    .enable_ppa_accel      = false,                                                 \
    .require_double_buffer = true

/* SPI/I2C and other interfaces default configuration without PSRAM*/
#define ESP_LV_ADAPTER_DISPLAY_PROFILE_SPI_WITHOUT_PSRAM_DEFAULT_CONFIG(_hor_res, _ver_res, _rotation) \
    .interface             = ESP_LV_ADAPTER_PANEL_IF_OTHER,                                       \
    ESP_LV_ADAPTER_DISPLAY_PROFILE_BASE_CONFIG(_hor_res, _ver_res, _rotation)                     \
    .buffer_height         = 10,                                                            \
    .use_psram             = false,                                                         \
    .enable_ppa_accel      = false,                                                         \
    .require_double_buffer = false

/**
 * @brief Display configuration structure
 *
 * Complete configuration for registering a display with the LVGL adapter
 */
typedef struct {
    esp_lcd_panel_handle_t panel;           /*!< ESP LCD panel handle */
    esp_lcd_panel_io_handle_t panel_io;     /*!< ESP LCD panel IO handle (can be NULL) */
    esp_lv_adapter_display_profile_t profile;     /*!< Display profile configuration */
    esp_lv_adapter_tear_avoid_mode_t tear_avoid_mode;      /*!< Tearing effect avoidance mode */
} esp_lv_adapter_display_config_t;

/**
 * @brief Default display configuration macro
 *
 * @param _panel ESP LCD panel handle
 * @param _panel_io ESP LCD panel IO handle
 * @param _hor_res Horizontal resolution in pixels
 * @param _ver_res Vertical resolution in pixels
 * @param _rotation Display rotation angle
 */
#define ESP_LV_ADAPTER_DISPLAY_MIPI_DEFAULT_CONFIG(_panel, _panel_io, _hor_res, _ver_res, _rotation) {             \
    .panel             = _panel,                                                                             \
    .panel_io          = _panel_io,                                                                          \
    .profile           = {                                                                                   \
        ESP_LV_ADAPTER_DISPLAY_PROFILE_MIPI_DEFAULT_CONFIG(_hor_res, _ver_res, _rotation),                         \
    },                                                                                                       \
    .tear_avoid_mode   = ESP_LV_ADAPTER_TEAR_AVOID_MODE_DEFAULT_MIPI_DSI,                                          \
}

#define ESP_LV_ADAPTER_DISPLAY_RGB_DEFAULT_CONFIG(_panel, _panel_io, _hor_res, _ver_res, _rotation) {              \
    .panel             = _panel,                                                                             \
    .panel_io          = _panel_io,                                                                          \
    .profile           = {                                                                                   \
        ESP_LV_ADAPTER_DISPLAY_PROFILE_RGB_DEFAULT_CONFIG(_hor_res, _ver_res, _rotation),                          \
    },                                                                                                       \
    .tear_avoid_mode   = ESP_LV_ADAPTER_TEAR_AVOID_MODE_DEFAULT_RGB,                                               \
}

#define ESP_LV_ADAPTER_DISPLAY_SPI_WITH_PSRAM_DEFAULT_CONFIG(_panel, _panel_io, _hor_res, _ver_res, _rotation) {   \
    .panel             = _panel,                                                                             \
    .panel_io          = _panel_io,                                                                          \
    .profile           = {                                                                                   \
        ESP_LV_ADAPTER_DISPLAY_PROFILE_SPI_WITH_PSRAM_DEFAULT_CONFIG(_hor_res, _ver_res, _rotation),               \
    },                                                                                                       \
    .tear_avoid_mode   = ESP_LV_ADAPTER_TEAR_AVOID_MODE_DEFAULT,                                                   \
}

#define ESP_LV_ADAPTER_DISPLAY_SPI_WITHOUT_PSRAM_DEFAULT_CONFIG(_panel, _panel_io, _hor_res, _ver_res, _rotation) {    \
    .panel             = _panel,                                                                                 \
    .panel_io          = _panel_io,                                                                              \
    .profile           = {                                                                                       \
        ESP_LV_ADAPTER_DISPLAY_PROFILE_SPI_WITHOUT_PSRAM_DEFAULT_CONFIG(_hor_res, _ver_res, _rotation),                \
    },                                                                                                           \
    .tear_avoid_mode   = ESP_LV_ADAPTER_TEAR_AVOID_MODE_DEFAULT,                                                       \
}

/**
 * @brief Calculate required frame buffer count based on rotation and tearing mode
 *
 * This function calculates how many frame buffers are needed for the LCD panel
 * based on the rotation angle and tearing avoidance mode. This is useful when
 * initializing hardware (RGB/MIPI DSI) that requires specifying the number of
 * frame buffers before registering the display.
 *
 * Buffer count logic:
 * - Rotation 90° or 270°: Always requires 3 buffers (for rotation processing)
 * - Triple buffering modes: Requires 3 buffers
 * - Double buffering modes: Requires 2 buffers
 * - Single buffer mode (TEAR_AVOID_MODE_NONE): Returns 1 (minimum required by RGB/MIPI DSI hardware)
 *
 * @param[in] tear_avoid_mode Tearing effect avoidance mode
 * @param[in] rotation Display rotation angle
 *
 * @return Required frame buffer count (1, 2, or 3)
 *
 */
uint8_t esp_lv_adapter_get_required_frame_buffer_count(esp_lv_adapter_tear_avoid_mode_t tear_avoid_mode,
                                                       esp_lv_adapter_rotation_t rotation);

/**
 * @brief Register a display with the LVGL adapter
 *
 * Creates and configures a new LVGL display object using the provided ESP LCD panel.
 * Allocates necessary buffers and sets up rendering callbacks.
 *
 * @param[in] config Pointer to display configuration structure
 *
 * @return
 *      - Pointer to LVGL display object on success
 *      - NULL on failure (adapter not initialized or invalid config)
 */
lv_display_t *esp_lv_adapter_register_display(const esp_lv_adapter_display_config_t *config);

/**
 * @brief Unregister a display from the LVGL adapter
 *
 * Removes a previously registered display and releases associated resources.
 * Only frees resources that were allocated by the adapter. Frame buffers
 * obtained from hardware (RGB/MIPI DSI panels) are not freed.
 *
 * @note This function must be called after all UI objects associated with
 *       the display have been deleted to avoid accessing freed memory.
 *
 * @param[in] disp Pointer to LVGL display object
 *
 * @return
 *      - ESP_OK: Success
 *      - ESP_ERR_INVALID_ARG: Invalid display pointer
 *      - ESP_ERR_INVALID_STATE: Adapter not initialized
 *      - ESP_ERR_NOT_FOUND: Display not found in registered list
 */
esp_err_t esp_lv_adapter_unregister_display(lv_display_t *disp);

#ifdef __cplusplus
}
#endif
