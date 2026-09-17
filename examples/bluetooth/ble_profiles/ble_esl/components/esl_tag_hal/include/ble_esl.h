/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Tag-side compatibility for backported product HAL (image snapshot / LED).
 */

#pragma once

#include <stddef.h>
#include "ble_esl_common.h"
#include "esp_eslp_tag.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef esp_ble_eslp_led_control_t ble_esl_led_control_evt_param_t;
typedef esp_ble_eslp_led_control_t esl_led_control_params_t;

typedef struct {
    ble_esl_address_t esl_address;
    ble_esl_key_material_t ap_sync_key;
    ble_esl_key_material_t resp_key;
    uint8_t peer_addr_type;
    uint8_t peer_addr[6];
} ble_esl_persisted_tag_t;

esp_err_t ble_esl_image_snapshot(uint8_t image_index, uint8_t *out, uint32_t out_cap, size_t *out_len);
esp_err_t ble_esl_image_restore(uint8_t image_index, const uint8_t *data, uint32_t len);
void ble_esl_image_invalidate_all(void);
esp_err_t ble_esl_restore_persisted_tag(const ble_esl_persisted_tag_t *info);
esp_err_t ble_esl_export_persisted_tag(ble_esl_persisted_tag_t *info);

#ifdef __cplusplus
}
#endif
