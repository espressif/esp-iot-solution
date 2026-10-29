/*
 * SPDX-FileCopyrightText: 2024-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <stdint.h>
#include "esp_err.h"
#include "usb/usb_host.h"

#ifdef __cplusplus
extern "C" {
#endif

// Use these macros to match any VID or PID
#define USB_DEVICE_VENDOR_ANY   0
#define USB_DEVICE_PRODUCT_ANY  0

/**
 * @brief Match a USB device against a set of fields
 */
typedef enum {
    USB_DEVICE_ID_MATCH_VENDOR   = 0x0001,
    USB_DEVICE_ID_MATCH_PRODUCT  = 0x0002,
    USB_DEVICE_ID_MATCH_DEV_BCD   = 0x0004,
    USB_DEVICE_ID_MATCH_DEV_CLASS    = 0x0010,
    USB_DEVICE_ID_MATCH_DEV_SUBCLASS = 0x0020,
    USB_DEVICE_ID_MATCH_DEV_PROTOCOL = 0x0040,
    USB_DEVICE_ID_MATCH_INT_CLASS    = 0x0080,
    USB_DEVICE_ID_MATCH_INT_SUBCLASS = 0x0100,
    USB_DEVICE_ID_MATCH_INT_PROTOCOL = 0x0200,
    USB_DEVICE_ID_MATCH_INT_NUMBER   = 0x0400,

    USB_DEVICE_ID_MATCH_ALL  = 0xFFFF,
    USB_DEVICE_ID_MATCH_DEV  = 0x007F,  /*!< Match device descriptor */
    USB_DEVICE_ID_MATCH_INTF = 0x0780, /*!< Match interface descriptor */
    USB_DEVICE_ID_MATCH_VID_PID = 0x0003, /*!< Match vendor and product ID */
} usb_dev_match_flags_t;

/**
 * @brief Match a USB device against a set of criteria
 */
typedef struct {
    usb_dev_match_flags_t match_flags; /*!< which fields to match against? */

    uint16_t idVendor; /*!< Vendor ID */
    uint16_t idProduct; /*!< Product ID */
    uint16_t bcdDevice; /*!< Device release number in binary-coded decimal (BCD) */

    uint8_t bDeviceClass; /*!< Device class */
    uint8_t bDeviceSubClass; /*!< Device subclass */
    uint8_t bDeviceProtocol; /*!< Device protocol */

    uint8_t bInterfaceClass; /*!< Interface class */
    uint8_t bInterfaceSubClass; /*!< Interface subclass */
    uint8_t bInterfaceProtocol; /*!< Interface protocol */
    uint8_t bInterfaceNumber; /*!< Interface number */
} usb_device_match_id_t;

// Use this to match any device
extern const usb_device_match_id_t  _g_esp_usb_device_match_id_any[];
#define ESP_USB_DEVICE_MATCH_ID_ANY _g_esp_usb_device_match_id_any

/**
 * @brief Match a USB device against a set of criteria
 *
 * This function checks if the provided USB device descriptor matches the specified criteria in the match ID.
 * It compares vendor ID, product ID, device class, subclass, protocol, and other fields based on the match flags.
 *
 * @param[in] dev_desc Pointer to the USB device descriptor to match against
 * @param[in] id Pointer to the USB device match ID containing the criteria for matching
 *
 * @return
 *     - 1 if the device matches the criteria
 *     - 0 if it does not match
 */
int usbh_match_device(const usb_device_desc_t *dev_desc, const usb_device_match_id_t *id);

/**
 * @brief Match a USB interface against a set of criteria
 *
 * This function checks if the provided USB interface descriptor matches the specified criteria in the match ID.
 * It compares interface class, subclass, protocol, and other fields based on the match flags.
 *
 * @param[in] intf_desc Pointer to the USB interface descriptor to match against
 * @param[in] id Pointer to the USB device match ID containing the criteria for matching
 *
 * @return
 *     - 1 if the interface matches the criteria
 *     - 0 if it does not match
 */
int usbh_match_interface(const usb_intf_desc_t *intf_desc, const usb_device_match_id_t *id);

/**
 * @brief Match a USB device against a set of criteria
 *
 * This function checks if the provided USB device descriptor and configuration descriptor match the specified criteria in the match ID.
 * It compares vendor ID, product ID, device class, subclass, protocol, details based on the match flags.
 *
 * @param[in] dev_desc Pointer to the USB device descriptor to match against
 * @param[in] id_list Pointer to list of USB device match ID containing the criteria for matching
 *
 * @return
 *     - 1 if both device match the criteria
 *     - 0 if they do not match
 */
int usbh_match_device_from_id_list(const usb_device_desc_t *dev_desc, const usb_device_match_id_t *id_list);

/**
 * @brief Match a USB interface in a configuration against a set of criteria
 *
 * This function checks if any interface in the provided USB configuration descriptor matches the specified criteria in the match ID.
 * It iterates through all interfaces in the configuration and compares their details based on the match flags.
 *
 * @param[in] config_desc Pointer to the USB configuration descriptor to search for matching interfaces
 * @param[in] id Pointer to the USB device match ID containing the criteria for matching
 * @param[out] matched_intf_num Pointer to an integer to store the number of matched interfaces, set NULL if not needed
 *
 * @return
 *     - 1 if any interface matches the criteria
 *     - 0 if no interface matches
 */
int usbh_match_interface_in_configuration(const usb_config_desc_t *config_desc, const usb_device_match_id_t *id, int *matched_intf_num);

/**
 * @brief Match a USB device and interface against a set of criteria
 *
 * This function checks if the provided USB device descriptor and configuration descriptor match the specified criteria in the match ID.
 * It compares vendor ID, product ID, device class, subclass, protocol, and interface details based on the match flags.
 *
 * @param[in] dev_desc Pointer to the USB device descriptor to match against
 * @param[in] config_desc Pointer to the USB configuration descriptor to match against
 * @param[in] id_list Pointer to list of USB device match ID containing the criteria for matching
 * @param[out] matched_intf_num Pointer to an integer to store the number of matched interfaces, set NULL if not needed
 *
 * @return
 *     - 1 if both device and interface match the criteria
 *     - 0 if they do not match
 */
int usbh_match_id_from_list(const usb_device_desc_t *dev_desc, const usb_config_desc_t *config_desc, const usb_device_match_id_t *id_list, int *matched_intf_num);

#ifdef __cplusplus
}
#endif
