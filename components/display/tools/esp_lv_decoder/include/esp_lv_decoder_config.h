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

/* Output ARGB8888 for transparent PJPG on RGB888 displays so alpha blending
 * runs on the PPA hardware (vs planar RGB565A8 + CPU blend).
 * Selected by CONFIG_ESP_LV_DECODER_PJPG_HW_ALPHA_BLEND; override in build flags.
 * Only used by the LVGL v9 decoder. */
#ifndef ESP_LV_PJPG_PREFER_ARGB8888
#if defined(CONFIG_ESP_LV_DECODER_PJPG_HW_ALPHA_BLEND)
#define ESP_LV_PJPG_PREFER_ARGB8888 1
#else
#define ESP_LV_PJPG_PREFER_ARGB8888 0
#endif
#endif
