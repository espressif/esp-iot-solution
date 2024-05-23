/*
 * SPDX-FileCopyrightText: 2020-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "tusb.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Device descriptor generated from Kconfig
 *
 * This descriptor is used by default.
 * The user can provide their own device descriptor via tinyusb_driver_install() call
 */
extern const tusb_desc_device_t descriptor_dev_default;

#if (TUD_OPT_HIGH_SPEED)
/**
 * @brief Qualifier Device descriptor generated from Kconfig
 *
 * This descriptor is used by default.
 * The user can provide their own descriptor via tinyusb_driver_install() call
 */
extern const tusb_desc_device_qualifier_t descriptor_qualifier_default;
#endif // TUD_OPT_HIGH_SPEED

/**
 * @brief Array of string descriptors generated from Kconfig
 *
 * This descriptor is used by default.
 * The user can provide their own descriptor via tinyusb_driver_install() call
 */
extern const char *descriptor_str_default[];

/**
 * @brief FullSpeed configuration descriptor generated from Kconfig
 * This descriptor is used by default.
 * The user can provide their own FullSpeed configuration descriptor via tinyusb_driver_install() call
 */
extern const uint8_t descriptor_fs_cfg_default[];

#if (TUD_OPT_HIGH_SPEED)
/**
 * @brief HighSpeed Configuration descriptor generated from Kconfig
 *
 * This descriptor is used by default.
 * The user can provide their own HighSpeed configuration descriptor via tinyusb_driver_install() call
 */
extern const uint8_t descriptor_hs_cfg_default[];
#endif // TUD_OPT_HIGH_SPEED

uint8_t tusb_get_mac_string_id(void);

#ifdef __cplusplus
}
#endif
