/*
 * SPDX-FileCopyrightText: 2017-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "sdkconfig.h"

#if CONFIG_GATTS_ENABLE
#include "esp_hidd_gatts.h"
#else
typedef int esp_gatt_conn_reason_t;
#endif /* CONFIG_GATTS_ENABLE */

#ifdef __cplusplus
}
#endif
