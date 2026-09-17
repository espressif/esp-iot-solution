/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "ble_esl.h"

esp_err_t ble_esl_image_snapshot(uint8_t image_index, uint8_t *out, uint32_t out_cap, size_t *out_len)
{
    uint32_t len32 = 0;
    esp_err_t ret = esp_ble_eslp_image_snapshot(image_index, out, out_cap, &len32);
    if (out_len != NULL) {
        *out_len = (size_t)len32;
    }
    return ret;
}

esp_err_t ble_esl_image_restore(uint8_t image_index, const uint8_t *data, uint32_t len)
{
    return esp_ble_eslp_image_restore(image_index, data, len);
}

void ble_esl_image_invalidate_all(void)
{
    esp_ble_eslp_image_invalidate_all();
}

esp_err_t ble_esl_restore_persisted_tag(const ble_esl_persisted_tag_t *info)
{
    if (info == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    return esp_ble_eslp_restore_association(&info->esl_address, &info->ap_sync_key,
                                            &info->resp_key, info->peer_addr_type,
                                            info->peer_addr);
}

esp_err_t ble_esl_export_persisted_tag(ble_esl_persisted_tag_t *info)
{
    if (info == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    return esp_ble_eslp_export_association(&info->esl_address, &info->ap_sync_key,
                                           &info->resp_key, &info->peer_addr_type,
                                           info->peer_addr);
}
