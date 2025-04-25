/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief ESP LVGL port USB HID
 */

#pragma once

#include "esp_err.h"
#include "lvgl.h"

#if __has_include ("usb/hid_host.h")
#define ESP_LVGL_PORT_USB_HOST_HID_COMPONENT 1
#endif

#if LVGL_VERSION_MAJOR == 8
#include "esp_lvgl_port_compatibility.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

#ifdef ESP_LVGL_PORT_USB_HOST_HID_COMPONENT
/**
 * @brief Configuration of the mouse input
 */
typedef struct {
    lv_display_t *disp;        /*!< LVGL display handle (returned from lvgl_port_add_disp) */
    uint8_t sensitivity;    /*!< Mouse sensitivity (cannot be zero) */
    lv_obj_t *cursor_img;   /*!< Mouse cursor image, if NULL then used default */
} lvgl_port_hid_mouse_cfg_t;

/**
 * @brief Configuration of the keyboard input
 */
typedef struct {
    lv_display_t *disp;        /*!< LVGL display handle (returned from lvgl_port_add_disp) */
} lvgl_port_hid_keyboard_cfg_t;

/**
 * @brief Add USB HID mouse as an input device
 *
 * @note The USB host must be initialized before. Use `usb_host_install` for host initialization.
 *
 * @param mouse_cfg mouse configuration structure
 * @return Pointer to LVGL buttons input device or NULL when error occurred
 */
lv_indev_t *lvgl_port_add_usb_hid_mouse_input(const lvgl_port_hid_mouse_cfg_t *mouse_cfg);

/**
 * @brief Add USB HID keyboard as an input device
 *
 * @note The USB host must be initialized before. Use `usb_host_install` for host initialization.
 *
 * @param keyboard_cfg keyboard configuration structure
 * @return Pointer to LVGL buttons input device or NULL when error occurred
 */
lv_indev_t *lvgl_port_add_usb_hid_keyboard_input(const lvgl_port_hid_keyboard_cfg_t *keyboard_cfg);

/**
 * @brief Remove selected USB HID from input devices
 *
 * @note Free all memory used for this input device. When removed all HID devices, the HID task will be freed.
 *
 * @return
 *      - ESP_OK                    on success
 */
esp_err_t lvgl_port_remove_usb_hid_input(lv_indev_t *hid);
#endif

#ifdef __cplusplus
}
#endif
