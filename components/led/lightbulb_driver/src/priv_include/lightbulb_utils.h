/*
 * SPDX-FileCopyrightText: 2022-2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#ifndef MAX
#define MAX(a, b)                                   (((a) > (b)) ? (a) : (b))
#endif
#ifndef MIN
#define MIN(a, b)                                   (((a) < (b)) ? (a) : (b))
#endif

#if CONFIG_LIGHTBULB_CHECK_DEFAULT_LEVEL_3

#define LIGHTBULB_CHECK(a, str, action, ...)                                                \
    if (unlikely(!(a))) {                                                                   \
        ESP_LOGE(TAG, "%s:%d (%s): " str, __FILE__, __LINE__, __FUNCTION__, ##__VA_ARGS__); \
        action;                                                                             \
    }

#elif CONFIG_LIGHTBULB_CHECK_DEFAULT_LEVEL_2

#define LIGHTBULB_CHECK(a, str, action, ...)                                  \
    if (unlikely(!(a))) {                                                     \
        ESP_LOGE(TAG, "%s(%d): " str, __FUNCTION__, __LINE__, ##__VA_ARGS__); \
        action;                                                               \
    }

#else

#define LIGHTBULB_CHECK(a, str, action, ...)                                \
    if (unlikely(!(a))) {                                                   \
        ESP_LOGE(TAG, "Line %d returns an error.", __LINE__);               \
        action;                                                             \
    }

#endif
