/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "sdkconfig.h"

/* Allow project to override encoder/decoder toggles before including this header */

/* Read from Kconfig if available, otherwise default to disabled */
#ifndef ESP_LV_EAF_ENABLE_SW_JPEG
#if defined(CONFIG_ESP_LV_EAF_ENABLE_SW_JPEG)
#define ESP_LV_EAF_ENABLE_SW_JPEG CONFIG_ESP_LV_EAF_ENABLE_SW_JPEG
#else
#define ESP_LV_EAF_ENABLE_SW_JPEG 0
#endif
#endif

#ifndef ESP_LV_EAF_ENABLE_HW_JPEG
#if defined(CONFIG_SOC_JPEG_DECODE_SUPPORTED) && CONFIG_SOC_JPEG_DECODE_SUPPORTED
#define ESP_LV_EAF_ENABLE_HW_JPEG 1
#else
#define ESP_LV_EAF_ENABLE_HW_JPEG 0
#endif
#endif

#if ESP_LV_EAF_ENABLE_HW_JPEG && !ESP_LV_EAF_ENABLE_SW_JPEG
#error "Hardware JPEG decoding requires ESP_LV_EAF_ENABLE_SW_JPEG"
#endif
