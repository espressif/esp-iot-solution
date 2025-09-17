/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once
#include "sdkconfig.h"

#ifndef ESP_LV_ENABLE_HW_JPEG
#if defined(CONFIG_SOC_JPEG_DECODE_SUPPORTED) && CONFIG_SOC_JPEG_DECODE_SUPPORTED
#define ESP_LV_ENABLE_HW_JPEG 1
#else
#define ESP_LV_ENABLE_HW_JPEG 0
#endif
#endif

#ifndef ESP_LV_ENABLE_PJPG
#if ESP_LV_ENABLE_HW_JPEG
#define ESP_LV_ENABLE_PJPG 1
#else
#define ESP_LV_ENABLE_PJPG 0
#endif
#endif
