/* SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"
#include "esp_ble_conn_mgr.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#define BLE_LINK_GATE_MAX_CONNECTIONS 4
#define BLE_LINK_GATE_WAIT_MS 10000

typedef struct {
    uint16_t conn_handle;
    bool occupied;
    bool connected;
    bool mtu_done;
    bool params_done;
    bool phy_done;
    bool timed_out;
} ble_link_gate_slot_t;

typedef struct {
    SemaphoreHandle_t mutex;
    ble_link_gate_slot_t slots[BLE_LINK_GATE_MAX_CONNECTIONS];
} ble_link_gate_t;

esp_err_t ble_link_gate_init(ble_link_gate_t *gate);
void ble_link_gate_begin(ble_link_gate_t *gate, uint16_t conn_handle);
void ble_link_gate_end(ble_link_gate_t *gate, uint16_t conn_handle);
void ble_link_gate_on_event(ble_link_gate_t *gate, int32_t id,
                            const esp_ble_conn_event_data_t *event);
bool ble_link_gate_is_ready(ble_link_gate_t *gate, uint16_t conn_handle, uint16_t min_mtu);
bool ble_link_gate_wait(ble_link_gate_t *gate, uint16_t conn_handle, uint16_t min_mtu,
                        uint32_t timeout_ms);
