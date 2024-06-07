/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdint.h>

#include "esp_lcd_panel_vendor.h"
#include "driver/jpeg_encode.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef bool (esp_lcd_jpeg_buf_finish_cb_t)(void *user_ctx);

/**
 * @brief LCD panel vendor configuration.
 *
 */
typedef struct {
    uint16_t h_res;                                 /*!< Horizontal resolution of the display */
    uint16_t v_res;                                 /*!< Vertical resolution of the display */
    uint8_t bits_per_pixel;                         /*!< Color depth, in bpp */
    uint8_t fb_rgb_nums;                            /*!< Number of RGB components in the frame buffer */
    size_t fb_uvc_jpeg_size;                        /*!< Size of the UVC JPEG frame buffer */
    int uvc_device_index;                           /*!< Index of the UVC device */
    struct {
        jpeg_down_sampling_type_t sub_sample;       /*!< Sub-sampling type for JPEG encoding */
        uint8_t quality;                            /*!< JPEG encoding quality */
        uint8_t task_priority;                      /*!< Priority of the JPEG encoding task */
        int task_core_id;                           /*!< Core ID for the JPEG encoding task */
    } jpeg_encode_config;                           /*!< Configuration for JPEG encoding */
    void *user_ctx;                                 /*!< User context */
    void (*on_uvc_start)(void *user_ctx);           /*!< Callback function for UVC start event */
    void (*on_uvc_stop)(void *user_ctx);            /*!< Callback function for UVC stop event */
    void (*on_display_trans_done)(void *user_ctx);  /*!< Callback function for display transfer completion event */
} usb_display_vendor_config_t;                      /*!< Configuration structure for USB display vendor */

/**
 * @brief Create LCD panel for USB display
 *
 * @param[in]  panel_dev_config General panel device configuration (`vendor_config` is necessary)
 * @param[out] ret_panel Returned LCD panel handle
 * @return
 *      - ESP_OK: Success
 *      - Otherwise: Fail
 */
esp_err_t esp_lcd_new_panel_usb_display(usb_display_vendor_config_t *vendor_config, esp_lcd_panel_handle_t *ret_panel);

/**
 * @brief Get the address of the frame buffer(s) that allocated by the driver
 *
 * @param[in] handle  LCD panel handle, returned from `esp_lcd_new_panel_usb_display()`
 * @param[in] fb_num Number of frame buffer(s) to get. This value must be the same as the number of the following parameters.
 * @param[out] fb0   Returned address of the frame buffer 0
 * @param[out] ...   List of other frame buffer addresses
 * @return
 *      - ESP_OK:              Success
 *      - ESP_ERR_INVALID_ARG: Fail
 */
esp_err_t esp_lcd_usb_display_get_frame_buffer(esp_lcd_panel_handle_t handle, uint32_t fb_num, void **fb0, ...);

/**
 * @brief Register LCD panel event callbacks
 *
 * @param[in] panel LCD panel handle, returned from `esp_lcd_new_panel_usb_display()`
 * @param[in] callbacks Group of callback functions
 * @param[in] user_ctx User data, which will be passed to the callback functions directly
 * @return
 *      - ESP_OK: Set event callbacks successfully
 *      - ESP_ERR_INVALID_ARG: Set event callbacks failed because of invalid argument
 *      - ESP_FAIL: Set event callbacks failed because of other error
 */
esp_err_t esp_lcd_usb_display_register_event_callbacks(esp_lcd_panel_handle_t panel, esp_lcd_jpeg_buf_finish_cb_t *callbacks, void *user_ctx);

/**
 * @brief USB Display configuration structure
 *
 * @param[in] hres Horizontal resolution of the display
 * @param[in] vres Vertical resolution of the display
 * @param[in] px_format Color depth, in bpp
 * @param[in] panel_handle LCD panel handle
 *
 */
#define DEFAULT_USB_DISPLAY_VENDOR_CONFIG(hres, vres, px_format, panel_handle)          \
    {                                                                                   \
        .h_res = hres,                                                                  \
        .v_res = vres,                                                                  \
        .bits_per_pixel = px_format,                                                    \
        .fb_rgb_nums = 1,                                                               \
        .fb_uvc_jpeg_size = hres * vres * px_format / 8,                                \
        .uvc_device_index = 0,                                                          \
        .jpeg_encode_config = {                                                         \
            .sub_sample = JPEG_DOWN_SAMPLING_YUV420,                                    \
            .quality = 80,                                                              \
            .task_priority = 4,                                                         \
            .task_core_id = 1,                                                          \
        },                                                                              \
        .user_ctx = panel_handle,                                                       \
    }

#ifdef __cplusplus
}
#endif
