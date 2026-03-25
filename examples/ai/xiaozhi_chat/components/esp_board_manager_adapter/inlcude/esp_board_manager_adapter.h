/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

#include "esp_err.h"

#define ESP_BOARD_MANAGER_ADAPTER_CONFIG_DEFAULT() {  \
    .enable_lvgl          = false,                    \
    .enable_lcd_backlight = false,                    \
    .enable_lcd           = false,                    \
    .enable_touch         = false,                    \
    .enable_audio         = true,                     \
    .enable_sdcard        = true,                     \
}

typedef struct {
    bool enable_lvgl;
    bool enable_lcd;
    bool enable_lcd_backlight;
    bool enable_touch;
    bool enable_audio;
    bool enable_sdcard;
} esp_board_manager_adapter_config_t;

/**
 * @brief  Board Manager Adapter information structure
 *
 *         This structure contains handles and configuration information for various
 *         board manager adapter devices and peripherals.
 */
typedef struct {
    void  *play_dev;           /*!< Audio playback device handle (esp_codec_dev_handle_t)， only valid when `enable_audio` is true */
    void  *rec_dev;            /*!< Audio recording device handle (esp_codec_dev_handle_t)， only valid when `enable_audio` is true */
    void  *lcd_panel;          /*!< LCD panel handle (esp_lcd_panel_handle_t) ， only valid when `enable_lcd` is true */
    void  *brightness_handle;  /*!< LCD backlight brightness control handle (periph_ledc_handle_t *)， only valid when `enable_lcd_backlight` is true */
    void  *sdcard_handle;      /*!< SD card filesystem handle (void *)， only valid when `enable_sdcard` is true */
    int    sample_rate;        /*!< Board audio sample rate in Hz， only valid when `enable_audio` is true  */
    int    sample_bits;        /*!< Board audio sample bits per sample， only valid when `enable_audio` is true */
    int    channels;           /*!< Board audio channel count， only valid when `enable_audio` is true */
    char   mic_layout[8];      /*!< Board microphone layout array (8 elements)， only valid when `enable_audio` is true */
} esp_board_manager_adapter_info_t;

/**
 * @brief Initialize Board Manager Adapter
 *
 *         This function calls esp_board_manager interfaces to get device handles
 *         and fills them into the info structure.
 *         It also initializes the LVGL port if the configuration is enabled.
 *
 * @param[in] config  Pointer to the Board Manager Adapter configuration
 * @param[out] info  Pointer to store the Board Manager Adapter information
 *
 * @return
 *      - ESP_OK: Success
 *      - ESP_ERR_INVALID_ARG: Invalid arguments
 *      - Others: Error codes from board manager adapter
 */
esp_err_t esp_board_manager_adapter_init(const esp_board_manager_adapter_config_t *config, esp_board_manager_adapter_info_t *info);

/*
 * @brief Deinitialize Board Manager Adapter
 *
 *       This function deinitializes the Board Manager Adapter and frees the resources.
 *
 * @return
 *      - ESP_OK: Success
 *      - Others: Error codes from board manager adapter
*/
esp_err_t esp_board_manager_adapter_deinit(void);

/**
 * @brief Set LCD backlight brightness percentage
 *
 *         This function sets the LCD backlight brightness percentage.
 *
 * @param[in] brightness_percent  Brightness percentage (0-100)
 *
 * @return ESP_OK on success, appropriate error code on failure
 */
esp_err_t esp_board_manager_adapter_set_lcd_backlight(int brightness_percent);

/**
 * @brief Get LCD resolution
 *
 *       This function gets the LCD resolution.
 *
 * @param[out] width  Width
 * @param[out] height  Height
 */
esp_err_t esp_board_manager_adapter_get_lcd_resolution(int *width, int *height);

#ifdef __cplusplus
}
#endif  /* __cplusplus */
