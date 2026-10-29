/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "esp_err.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

/**
 * @brief  Initialize display UI using board manager's display
 *
 * @note   This function should be called after esp_board_manager_adapter_init() with enable_lvgl=true
 *
 * @return
 *       - ESP_OK   On success
 *       - ESP_ERR_INVALID_STATE   Board manager display not available
 */
esp_err_t esp_xiaozhi_chat_display_init(void);

/**
 * @brief  Get display width in pixels
 *
 * @param[out] width  Pointer to width in pixels
 *
 * @return
 *       - ESP_OK   On success
 *       - ESP_ERR_INVALID_ARG   If width is NULL
 */
esp_err_t esp_xiaozhi_chat_display_get_width(int *width);

/**
 * @brief  Get display height in pixels
 *
 * @param[out] height  Pointer to height in pixels
 *
 * @return
 *       - ESP_OK   On success
 *       - ESP_ERR_INVALID_ARG   If height is NULL
 */
esp_err_t esp_xiaozhi_chat_display_get_height(int *height);

/**
 * @brief  Update LCD status text
 *
 * @param[in] status  Status text to display
 *
 * @return
 *       - ESP_OK   On success
 *       - ESP_ERR_INVALID_STATE   If display not initialized
 */
esp_err_t esp_xiaozhi_chat_display_set_status(const char *status);

/**
 * @brief  Update LCD subtitle text (same as status in current UI)
 *
 * @param[in] subtitle  Subtitle text to display
 *
 * @return
 *       - ESP_OK   On success
 *       - ESP_ERR_INVALID_STATE   If display not initialized
 */
esp_err_t esp_xiaozhi_chat_display_set_subtitle(const char *subtitle);

/**
 * @brief  Update LCD volume display / backlight
 *
 * @param[in] volume  Volume value (0-100), used for backlight when supported
 *
 * @return
 *       - ESP_OK   On success
 *       - ESP_ERR_INVALID_STATE   If display not initialized
 */
esp_err_t esp_xiaozhi_chat_display_set_volume(int volume);

/**
 * @brief  Update LCD backlight brightness
 *
 * @param[in] brightness  Brightness value (0-100)
 *
 * @return
 *       - ESP_OK   On success
 *       - ESP_ERR_INVALID_STATE   If display not initialized
 */
esp_err_t esp_xiaozhi_chat_display_set_brightness(int brightness);

/**
 * @brief  Show notification on display
 *
 * @param[in] notification   Notification text to display
 * @param[in] duration_ms    Duration in milliseconds (default: 3000)
 *
 * @return
 *       - ESP_OK   On success
 *       - ESP_ERR_INVALID_STATE   If display not initialized
 */
esp_err_t esp_xiaozhi_chat_display_set_notification(const char *notification, int duration_ms);

/**
 * @brief  Set emotion icon on display
 *
 * @param[in] emotion  Emotion name (e.g., "happy", "sad", "neutral")
 *
 * @return
 *       - ESP_OK   On success
 *       - ESP_ERR_INVALID_STATE   If display not initialized
 */
esp_err_t esp_xiaozhi_chat_display_set_emotion(const char *emotion);

/**
 * @brief  Set chat message on display
 *
 * @param[in] role     Message role: "user", "assistant", or "system"
 * @param[in] content  Message content
 *
 * @return
 *       - ESP_OK   On success
 *       - ESP_ERR_INVALID_STATE   If display not initialized
 */
esp_err_t esp_xiaozhi_chat_display_set_chat_message(const char *role, const char *content);

/**
 * @brief  Set icon on display
 *
 * @param[in] icon  Icon text or symbol
 *
 * @return
 *       - ESP_OK   On success
 *       - ESP_ERR_INVALID_STATE   If display not initialized
 */
esp_err_t esp_xiaozhi_chat_display_set_icon(const char *icon);

/**
 * @brief  Set image (preview image or other image) on display
 *
 * @param[in] image  Pointer to LVGL image descriptor (lv_img_dsc_t* or lv_image_dsc_t*, can be NULL to hide)
 *
 * @return
 *       - ESP_OK   On success
 *       - ESP_ERR_INVALID_STATE   If display not initialized
 */
esp_err_t esp_xiaozhi_chat_display_set_image(const void *image);

/**
 * @brief  Set display theme
 *
 * @param[in] theme_name  Theme name: "light" or "dark"
 *
 * @return
 *       - ESP_OK   On success
 *       - ESP_ERR_INVALID_STATE   If display not initialized
 */
esp_err_t esp_xiaozhi_chat_display_set_theme(const char *theme_name);

#ifdef __cplusplus
}
#endif  /* __cplusplus */
