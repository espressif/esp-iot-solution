/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stddef.h>
#include <stdint.h>
#include "esp_err.h"
#include "ble_esl.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t esl_tag_hal_init(void);
esp_err_t esl_tag_display_show(uint8_t display_index, uint8_t image_index);
esp_err_t esl_tag_display_refresh(uint8_t display_index);
esp_err_t esl_tag_image_write(uint8_t image_index, uint32_t offset, const uint8_t *data, uint32_t len);
esp_err_t esl_tag_led_apply(uint8_t led_index, const esl_led_control_params_t *params);
esp_err_t esl_tag_sensor_read(uint8_t sensor_index, uint8_t *buf, size_t buf_size, size_t *out_len);

/* Association persistence (see esl_tag_storage.h). */
esp_err_t esl_tag_storage_init(void);

#ifdef __cplusplus
}
#endif
