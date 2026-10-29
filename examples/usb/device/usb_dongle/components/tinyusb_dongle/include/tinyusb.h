/*
 * SPDX-FileCopyrightText: 2020-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdbool.h>
#include "esp_err.h"
#include "tusb.h"
#include "tinyusb_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief GPIO mapping for an external USB PHY
 *
 * Set unused signals to -1.
 */
typedef struct {
    int vp_io_num;
    int vm_io_num;
    int rcv_io_num;
    int suspend_n_io_num;
    int oen_io_num;
    int vpo_io_num;
    int vmo_io_num;
    int fs_edge_sel_io_num;
} tinyusb_ext_phy_io_config_t;

/**
 * @brief Configuration structure of the TinyUSB core
 *
 * USB specification mandates self-powered devices to monitor USB VBUS to detect connection/disconnection events.
 * If you want to use this feature, connected VBUS to any free GPIO through a voltage divider or voltage comparator.
 * The voltage divider output should be (0.75 * Vdd) if VBUS is 4.4V (lowest valid voltage at device port).
 * The comparator thresholds should be set with hysteresis: 4.35V (falling edge) and 4.75V (raising edge).
 */
typedef struct {
    union {
        const tusb_desc_device_t *device_descriptor; /*!< Pointer to a device descriptor. If set to NULL, the TinyUSB device will use a default device descriptor whose values are set in Kconfig */
        const tusb_desc_device_t *descriptor  __attribute__((deprecated)); /*!< Alias to `device_descriptor` for backward compatibility */
    };
    const char **string_descriptor;            /*!< Pointer to array of string descriptors. If set to NULL, TinyUSB device will use a default string descriptors whose values are set in Kconfig */
    int string_descriptor_count;               /*!< Number of descriptors in above array */
    bool external_phy;                         /*!< Should USB use an external PHY */
    const tinyusb_ext_phy_io_config_t *external_phy_io; /*!< GPIO mapping. Required when external_phy is true */
    union {
        struct {
            const uint8_t *configuration_descriptor;            /*!< Pointer to a configuration descriptor. If set to NULL, TinyUSB device will use a default configuration descriptor whose values are set in Kconfig */
        };
#if (TUD_OPT_HIGH_SPEED)
        struct {
            const uint8_t *fs_configuration_descriptor;         /*!< Pointer to a FullSpeed configuration descriptor. If set to NULL, TinyUSB device will use a default configuration descriptor whose values are set in Kconfig */
        };
    };
    const uint8_t *hs_configuration_descriptor;                 /*!< Pointer to a HighSpeed configuration descriptor. If set to NULL, TinyUSB device will use a default configuration descriptor whose values are set in Kconfig */
    const tusb_desc_device_qualifier_t *qualifier_descriptor;   /*!< Pointer to a qualifier descriptor */
#else
    };
#endif // TUD_OPT_HIGH_SPEED
    bool self_powered;                         /*!< This is a self-powered USB device. USB VBUS must be monitored. */
    int vbus_monitor_io;                       /*!< GPIO for VBUS monitoring. Ignored if not self_powered. */
} tinyusb_config_t;

/**
 * @brief This is an all-in-one helper function, including:
 * 1. USB device driver initialization
 * 2. Descriptors preparation
 * 3. TinyUSB stack initialization
 * 4. Creates and start a task to handle usb events
 *
 * @note Don't change Custom descriptor, but if it has to be done,
 *       Suggest to define as follows in order to match the Interface Association Descriptor (IAD):
 *       bDeviceClass = TUSB_CLASS_MISC,
 *       bDeviceSubClass = MISC_SUBCLASS_COMMON,
 *
 * @param config tinyusb stack specific configuration
 * @retval ESP_ERR_INVALID_ARG Install driver and tinyusb stack failed because of invalid argument
 * @retval ESP_FAIL Install driver and tinyusb stack failed because of internal error
 * @retval ESP_OK Install driver and tinyusb stack successfully
 */
esp_err_t tinyusb_driver_install(const tinyusb_config_t *config);

esp_err_t tinyusb_driver_uninstall(void);

#ifdef __cplusplus
}
#endif
