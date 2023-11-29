/*
 * SPDX-FileCopyrightText: 2023-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "esp_log.h"

#define BLDC_CHECK(a, str, ret, ...) if(!(a)) { \
        ESP_LOGE(TAG,"%s:%d (%s):" str, __FILE__, __LINE__, __FUNCTION__, ##__VA_ARGS__); \
        return (ret); \
    }

#define BLDC_CHECK_ABORT(a, str, ...) if(!(a)) { \
        ESP_LOGE(TAG,"%s:%d (%s):" str, __FILE__, __LINE__, __FUNCTION__, ##__VA_ARGS__); \
        abort(); \
    }

#define BLDC_CHECK_RETURN_VOID(a, str, ...) if(!(a)) { \
        ESP_LOGE(TAG,"%s:%d (%s):" str, __FILE__, __LINE__, __FUNCTION__, ##__VA_ARGS__); \
        return; \
    }

#define BLDC_CHECK_CONTINUE(a, str, ...) if(!(a)) { \
        ESP_LOGE(TAG,"%s:%d (%s):" str, __FILE__, __LINE__, __FUNCTION__, ##__VA_ARGS__); \
    }

#define BLDC_CHECK_GOTO(a, str, label, ...) if(!(a)) { \
        ESP_LOGE(TAG,"%s:%d (%s):" str, __FILE__, __LINE__, __FUNCTION__, ##__VA_ARGS__); \
        goto label; \
    }

#ifdef __cplusplus
}
#endif
