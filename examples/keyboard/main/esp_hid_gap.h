/*
 * SPDX-FileCopyrightText: 2021-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */

#ifndef _ESP_HID_GAP_H_
#define _ESP_HID_GAP_H_

#include "esp_err.h"
#include "esp_log.h"
#include "esp_bt.h"
#include "esp_hid_common.h"

#if CONFIG_BT_NIMBLE_ENABLED
/* NimBLE: GAP is provided by host/ble_gap.h (included from esp_hid_gap.c). */
#elif CONFIG_BT_BLE_ENABLED
#include "esp_bt_defs.h"
#include "esp_bt_main.h"
#include "esp_gap_ble_api.h"
#include "esp_gatt_defs.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize BLE controller and host (Bluedroid or NimBLE per menuconfig).
 *
 * @return esp_err_t
 *      - ESP_OK: success
 */
esp_err_t esp_hid_gap_init(void);

/**
 * @brief Deinitialize BLE controller and host.
 *
 * @return esp_err_t
 *      - ESP_OK: success
 */
esp_err_t esp_hid_gap_deinit(void);

/**
 * @brief Configure BLE advertising and security (appearance + device name).
 *
 * @param appearance HID BLE Appearances
 * @param device_name device name
 * @return esp_err_t
 */
esp_err_t esp_hid_ble_gap_adv_init(uint16_t appearance, const char *device_name);

/**
 * @brief Start BLE connectable advertising.
 *
 * @return esp_err_t
 *     - ESP_OK: success
 */
esp_err_t esp_hid_ble_gap_adv_start(void);

#ifdef __cplusplus
}
#endif

#endif /* _ESP_HID_GAP_H_ */
