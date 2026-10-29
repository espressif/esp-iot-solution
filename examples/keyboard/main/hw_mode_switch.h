/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include "esp_err.h"
#include "btn_progress.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*hw_mode_switch_cb_t)(btn_report_type_t new_mode, void *ctx);

esp_err_t hw_mode_switch_gpio_init(void);

esp_err_t hw_mode_switch_init(hw_mode_switch_cb_t on_mode_changed, void *ctx);

btn_report_type_t hw_mode_switch_get_report_type(void);

#ifdef __cplusplus
}
#endif
