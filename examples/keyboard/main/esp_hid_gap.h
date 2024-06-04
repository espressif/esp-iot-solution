/*
 * SPDX-FileCopyrightText: 2021-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */

#ifndef _ESP_HID_GAP_H_
#define _ESP_HID_GAP_H_

#include "esp_err.h"
#include "esp_log.h"

#include "esp_bt.h"
#include "esp_bt_defs.h"
#include "esp_bt_main.h"
#include "esp_gap_bt_api.h"
#include "esp_hid_common.h"
#include "esp_gattc_api.h"
#include "esp_gatt_defs.h"
#include "esp_gap_ble_api.h"

#ifdef __cplusplus
extern "C" {
#endif
/**
 * @brief Init BLE gap
 *
 * @return esp_err_t
 *      - ESP_OK: success
 */
esp_err_t esp_hid_gap_init(void);

/**
 * @brief Deinit BLE gap
 *
 * @return esp_err_t
 *      - ESP_OK: success
 */
esp_err_t esp_hid_gap_deinit(void);

/**
 * @brief Init ble advertising
 *
 * @param appearance HID BLE Appearances
 * @param device_name device name
 * @return esp_err_t
 */
esp_err_t esp_hid_ble_gap_adv_init(uint16_t appearance, const char *device_name);

/**
 * @brief Start ble advertising
 *
 * @return esp_err_t
 *     - ESP_OK: success
 */
esp_err_t esp_hid_ble_gap_adv_start(void);

#ifdef __cplusplus
}
#endif

#endif /* _ESP_HIDH_GAP_H_ */
