/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "esp_err.h"
#include "esp_esl.h"
#include "esp_ots.h"

#ifdef __cplusplus
extern "C"
{
#endif

/* ESL image Object ID base: Object ID = 0x000000000100 + Image_Index */
#define BLE_ESLP_OTS_OBJECT_ID_BASE     BLE_ESL_OBJECT_ID_BASE

static inline void eslp_otp_image_index_to_id(uint8_t image_index, esp_ble_ots_id_t *out_id)
{
    uint64_t value = BLE_ESLP_OTS_OBJECT_ID_BASE + image_index;
    if (!out_id) {
        return;
    }
    for (int i = 0; i < 6; i++) {
        out_id->id[i] = (uint8_t)((value >> (8 * i)) & 0xFF);
    }
}

static inline int eslp_otp_id_to_image_index(const esp_ble_ots_id_t *id)
{
    if (!id) {
        return -1;
    }
    uint64_t value = 0;
    for (int i = 0; i < 6; i++) {
        value |= ((uint64_t)id->id[i]) << (8 * i);
    }
    if (value < BLE_ESLP_OTS_OBJECT_ID_BASE) {
        return -1;
    }
    value -= BLE_ESLP_OTS_OBJECT_ID_BASE;
    if (value > 0xFF) {
        return -1;
    }
    return (int)value;
}

#ifdef CONFIG_BLE_ESL_OTS_SUPPORT
esp_err_t eslp_otp_tag_init(void);
esp_err_t eslp_otp_tag_deinit(void);
/**
 * @brief Clear all stored ESL image object contents
 */
void eslp_otp_tag_clear_images(void);
#endif

#ifdef CONFIG_BLE_ESL_PROFILE_AP
esp_err_t eslp_otp_ap_init(void);
esp_err_t eslp_otp_ap_deinit(void);
#endif

#ifdef __cplusplus
}
#endif
