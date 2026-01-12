/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdbool.h>
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_io.h"
#include "driver/gpio.h"
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
 * @brief Monochrome buffer layout
 */
typedef enum {
    ESP_LV_ADAPTER_MONO_LAYOUT_NONE = 0,  /*!< Not monochrome */
    ESP_LV_ADAPTER_MONO_LAYOUT_HTILED,    /*!< I1 horizontal tiled layout (LVGL native) */
    ESP_LV_ADAPTER_MONO_LAYOUT_VTILED,    /*!< I1 vertical tiled layout (page mode) */
} esp_lv_adapter_mono_layout_t;

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
    ESP_LV_ADAPTER_TEAR_AVOID_MODE_TE_SYNC        = 5,    /*!< TE GPIO based tear avoidance for SPI/I80/QSPI */
} esp_lv_adapter_tear_avoid_mode_t;

#define ESP_LV_ADAPTER_TEAR_AVOID_MODE_DEFAULT_MIPI_DSI     ESP_LV_ADAPTER_TEAR_AVOID_MODE_TRIPLE_PARTIAL
#define ESP_LV_ADAPTER_TEAR_AVOID_MODE_DEFAULT_RGB          ESP_LV_ADAPTER_TEAR_AVOID_MODE_TRIPLE_PARTIAL
#define ESP_LV_ADAPTER_TEAR_AVOID_MODE_DEFAULT              ESP_LV_ADAPTER_TEAR_AVOID_MODE_NONE

/**
 * @brief TE (Tearing Effect) timing defaults for 60Hz panels
 */
#define ESP_LV_ADAPTER_TE_TVDL_DEFAULT_MS            13  /*!< Default panel refresh window (Tvdl) for 60Hz */
#define ESP_LV_ADAPTER_TE_TVDH_DEFAULT_MS            1   /*!< Default panel idle window (Tvdh) for 60Hz */
#define ESP_LV_ADAPTER_TE_WINDOW_PERCENT_DEFAULT     66  /*!< Default TE transmission window percentage */
#define ESP_LV_ADAPTER_TE_WINDOW_MARGIN_PERCENT       5  /*!< Extra window percentage used when expanding TE timing */
#define ESP_LV_ADAPTER_TE_DATA_LINES_DEFAULT          1  /*!< Default data lines when TE config omits it */
#define ESP_LV_ADAPTER_TE_BITS_PER_PIXEL_DEFAULT     16  /*!< Default bits-per-pixel when TE config omits it */

/**
 * @brief TE (Tearing Effect) synchronization configuration
 *
 * This structure configures GPIO-based TE synchronization for SPI/I2C/I80 panels.
 * TE sync uses ISR + semaphore mechanism (no dedicated task required).
 *
 * @note Set gpio_num to -1 to disable TE synchronization
 * @note Set time_tvdl_ms/time_tvdh_ms to 0 to use defaults
 * @note Set refresh_window_percent to 0 to use default (66%)
 */
typedef struct {
    int gpio_num;                   /*!< TE GPIO number (-1 to disable) */
    uint32_t time_tvdl_ms;          /*!< Panel refresh window (Tvdl), 0 for default (13ms) */
    uint32_t time_tvdh_ms;          /*!< Panel idle window (Tvdh), 0 for default (1ms) */
    uint32_t bus_freq_hz;           /*!< LCD bus frequency (Hz) for auto-calculation */
    uint8_t data_lines;             /*!< Bus data lines (e.g., 4 for QSPI) for auto-calculation */
    uint8_t bits_per_pixel;         /*!< Bits per pixel for auto-calculation */
    gpio_int_type_t intr_type;      /*!< TE interrupt edge (GPIO_INTR_DISABLE for auto-detect) */
    uint8_t refresh_window_percent; /*!< TE period percentage for transmission, 0 for default (66%) */
} esp_lv_adapter_te_sync_config_t;

/**
 * @brief TE synchronization disabled configuration
 */
#define ESP_LV_ADAPTER_TE_SYNC_DISABLED()  \
    (esp_lv_adapter_te_sync_config_t){ .gpio_num = -1 }

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
    esp_lv_adapter_mono_layout_t mono_layout;      /*!< Monochrome layout selection */
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
    .rotation              = (_rotation),                                                 \
    .hor_res               = (_hor_res),                                                  \
    .ver_res               = (_ver_res),

/* Common helpers to build display profiles */
#define ESP_LV_ADAPTER_DISPLAY_PROFILE_MIPI_DEFAULT_CONFIG(_hor_res, _ver_res, _rotation) \
    .interface             = ESP_LV_ADAPTER_PANEL_IF_MIPI_DSI,                            \
    ESP_LV_ADAPTER_DISPLAY_PROFILE_BASE_CONFIG(_hor_res, _ver_res, _rotation)             \
    .buffer_height         = 50,                                                          \
    .use_psram             = false,                                                       \
    .enable_ppa_accel      = false,                                                       \
    .require_double_buffer = false,                                                       \
    .mono_layout           = ESP_LV_ADAPTER_MONO_LAYOUT_NONE

/* RGB interface default configuration */
#define ESP_LV_ADAPTER_DISPLAY_PROFILE_RGB_DEFAULT_CONFIG(_hor_res, _ver_res, _rotation)  \
    .interface             = ESP_LV_ADAPTER_PANEL_IF_RGB,                                 \
    ESP_LV_ADAPTER_DISPLAY_PROFILE_BASE_CONFIG(_hor_res, _ver_res, _rotation)             \
    .buffer_height         = 50,                                                          \
    .use_psram             = false,                                                       \
    .enable_ppa_accel      = false,                                                       \
    .require_double_buffer = false,                                                       \
    .mono_layout           = ESP_LV_ADAPTER_MONO_LAYOUT_NONE

/* SPI/I2C and other interfaces default configuration with PSRAM*/
#define ESP_LV_ADAPTER_DISPLAY_PROFILE_SPI_WITH_PSRAM_BASE_CONFIG(_hor_res, _ver_res, _rotation)     \
    .interface             = ESP_LV_ADAPTER_PANEL_IF_OTHER,                                          \
    ESP_LV_ADAPTER_DISPLAY_PROFILE_BASE_CONFIG(_hor_res, _ver_res, _rotation)                        \
    .buffer_height         = _ver_res,                                                               \
    .use_psram             = true,                                                                   \
    .enable_ppa_accel      = false,                                                                  \
    .mono_layout           = ESP_LV_ADAPTER_MONO_LAYOUT_NONE

#define ESP_LV_ADAPTER_DISPLAY_PROFILE_SPI_WITH_PSRAM_DEFAULT_CONFIG(_hor_res, _ver_res, _rotation)    \
    ESP_LV_ADAPTER_DISPLAY_PROFILE_SPI_WITH_PSRAM_BASE_CONFIG(_hor_res, _ver_res, _rotation),          \
    .require_double_buffer = true

#define ESP_LV_ADAPTER_DISPLAY_PROFILE_SPI_WITH_PSRAM_TE_DEFAULT_CONFIG(_hor_res, _ver_res, _rotation) \
    ESP_LV_ADAPTER_DISPLAY_PROFILE_SPI_WITH_PSRAM_BASE_CONFIG(_hor_res, _ver_res, _rotation),          \
    .require_double_buffer = false

/* SPI/I2C and other interfaces default configuration without PSRAM*/
#define ESP_LV_ADAPTER_DISPLAY_PROFILE_SPI_WITHOUT_PSRAM_DEFAULT_CONFIG(_hor_res, _ver_res, _rotation) \
    .interface             = ESP_LV_ADAPTER_PANEL_IF_OTHER,                                            \
    ESP_LV_ADAPTER_DISPLAY_PROFILE_BASE_CONFIG(_hor_res, _ver_res, _rotation)                          \
    .buffer_height         = 10,                                                                       \
    .use_psram             = false,                                                                    \
    .enable_ppa_accel      = false,                                                                    \
    .require_double_buffer = false,                                                                    \
    .mono_layout           = ESP_LV_ADAPTER_MONO_LAYOUT_NONE

#define ESP_LV_ADAPTER_DISPLAY_PROFILE_MONO_DEFAULT_CONFIG(_hor_res, _ver_res, _rotation, _mono_layout) \
    .interface             = ESP_LV_ADAPTER_PANEL_IF_OTHER,                                            \
    ESP_LV_ADAPTER_DISPLAY_PROFILE_BASE_CONFIG(_hor_res, _ver_res, _rotation)                          \
    .buffer_height         = (_ver_res),                                                               \
    .use_psram             = false,                                                                    \
    .enable_ppa_accel      = false,                                                                    \
    .require_double_buffer = false,                                                                    \
    .mono_layout           = (_mono_layout)

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
    esp_lv_adapter_te_sync_config_t te_sync;               /*!< TE synchronization configuration */
} esp_lv_adapter_display_config_t;

#define ESP_LV_ADAPTER_DISPLAY_CONFIG(_panel, _panel_io, _profile_cfg, _tear_mode, _te_sync_cfg) \
    ((esp_lv_adapter_display_config_t){                                                          \
        .panel           = (_panel),                                                             \
        .panel_io        = (_panel_io),                                                          \
        .profile         = { _profile_cfg },                                                     \
        .tear_avoid_mode = (_tear_mode),                                                         \
        .te_sync         = (_te_sync_cfg),                                                       \
    })

#define ESP_LV_ADAPTER_DISPLAY_MIPI_DEFAULT_CONFIG(_panel, _panel_io, _hor_res, _ver_res, _rotation)                 \
    ESP_LV_ADAPTER_DISPLAY_CONFIG(_panel, _panel_io,                                                                 \
                                  ESP_LV_ADAPTER_DISPLAY_PROFILE_MIPI_DEFAULT_CONFIG(_hor_res, _ver_res, _rotation), \
                                  ESP_LV_ADAPTER_TEAR_AVOID_MODE_DEFAULT_MIPI_DSI,                                   \
                                  ESP_LV_ADAPTER_TE_SYNC_DISABLED())

#define ESP_LV_ADAPTER_DISPLAY_RGB_DEFAULT_CONFIG(_panel, _panel_io, _hor_res, _ver_res, _rotation)                  \
    ESP_LV_ADAPTER_DISPLAY_CONFIG(_panel, _panel_io,                                                                 \
                                  ESP_LV_ADAPTER_DISPLAY_PROFILE_RGB_DEFAULT_CONFIG(_hor_res, _ver_res, _rotation),  \
                                  ESP_LV_ADAPTER_TEAR_AVOID_MODE_DEFAULT_RGB,                                        \
                                  ESP_LV_ADAPTER_TE_SYNC_DISABLED())

#define ESP_LV_ADAPTER_DISPLAY_SPI_WITH_PSRAM_DEFAULT_CONFIG(_panel, _panel_io, _hor_res, _ver_res, _rotation)                 \
    ESP_LV_ADAPTER_DISPLAY_CONFIG(_panel, _panel_io,                                                                           \
                                  ESP_LV_ADAPTER_DISPLAY_PROFILE_SPI_WITH_PSRAM_DEFAULT_CONFIG(_hor_res, _ver_res, _rotation), \
                                  ESP_LV_ADAPTER_TEAR_AVOID_MODE_DEFAULT,                                                      \
                                  ESP_LV_ADAPTER_TE_SYNC_DISABLED())

#define ESP_LV_ADAPTER_DISPLAY_SPI_WITHOUT_PSRAM_DEFAULT_CONFIG(_panel, _panel_io, _hor_res, _ver_res, _rotation)                 \
    ESP_LV_ADAPTER_DISPLAY_CONFIG(_panel, _panel_io,                                                                              \
                                  ESP_LV_ADAPTER_DISPLAY_PROFILE_SPI_WITHOUT_PSRAM_DEFAULT_CONFIG(_hor_res, _ver_res, _rotation), \
                                  ESP_LV_ADAPTER_TEAR_AVOID_MODE_DEFAULT,                                                         \
                                  ESP_LV_ADAPTER_TE_SYNC_DISABLED())

#define ESP_LV_ADAPTER_DISPLAY_SPI_MONO_DEFAULT_CONFIG(_panel, _panel_io, _hor_res, _ver_res, _rotation, _mono_layout)                 \
    ESP_LV_ADAPTER_DISPLAY_CONFIG(_panel, _panel_io,                                                                                 \
                                  ESP_LV_ADAPTER_DISPLAY_PROFILE_SPI_MONO_DEFAULT_CONFIG(_hor_res, _ver_res, _rotation, _mono_layout), \
                                  ESP_LV_ADAPTER_TEAR_AVOID_MODE_DEFAULT,                                                            \
                                  ESP_LV_ADAPTER_TE_SYNC_DISABLED())

/**
 * @brief SPI with PSRAM and TE synchronization default configuration
 *
 * Simplified macro that creates a complete display configuration with TE sync enabled
 */
#define ESP_LV_ADAPTER_DISPLAY_SPI_WITH_PSRAM_TE_DEFAULT_CONFIG(_panel, _panel_io, _hor_res, _ver_res, _rotation, _te_gpio, _bus_freq, _data_lines, _bpp) \
    ESP_LV_ADAPTER_DISPLAY_CONFIG(_panel, _panel_io,                                                                             \
                                  ESP_LV_ADAPTER_DISPLAY_PROFILE_SPI_WITH_PSRAM_TE_DEFAULT_CONFIG(_hor_res, _ver_res, _rotation),\
                                  ESP_LV_ADAPTER_TEAR_AVOID_MODE_TE_SYNC,                                                        \
                                  ((esp_lv_adapter_te_sync_config_t){                                                            \
                                      .gpio_num = (_te_gpio),                                                                    \
                                      .time_tvdl_ms = ESP_LV_ADAPTER_TE_TVDL_DEFAULT_MS,                                         \
                                      .time_tvdh_ms = ESP_LV_ADAPTER_TE_TVDH_DEFAULT_MS,                                         \
                                      .bus_freq_hz = (_bus_freq),                                                                \
                                      .data_lines = (_data_lines),                                                               \
                                      .bits_per_pixel = (_bpp),                                                                  \
                                      .intr_type = GPIO_INTR_DISABLE,                                                            \
                                      .refresh_window_percent = ESP_LV_ADAPTER_TE_WINDOW_PERCENT_DEFAULT,                        \
                                  }))

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

/**
 * @brief Set area rounding callback for display
 *
 * Some displays require specific alignment for drawing areas (e.g., width/height must be
 * multiple of 8). This callback allows rounding the area before flushing to hardware.
 *
 * @param[in] disp Pointer to LVGL display object
 * @param[in] rounder_cb Callback function to round the area (NULL to disable)
 * @param[in] user_data User data passed to callback
 *
 * @return
 *      - ESP_OK: Success
 *      - ESP_ERR_INVALID_ARG: Invalid display pointer
 *      - ESP_ERR_INVALID_STATE: Adapter not initialized
 *      - ESP_ERR_NOT_FOUND: Display not found in registered list
 */
esp_err_t esp_lv_adapter_set_area_rounder_cb(lv_display_t *disp,
                                             void (*rounder_cb)(lv_area_t *area, void *user_data),
                                             void *user_data);

#ifdef __cplusplus
}
#endif
