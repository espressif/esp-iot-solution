/* SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 * SPDX-License-Identifier: Apache-2.0
 */
#include "ble_link_gate.h"

#include <string.h>

#include "freertos/task.h"
#include "esp_log.h"
#include "host/ble_att.h"
#include "host/ble_gap.h"
#include "host/ble_l2cap.h"
#include "soc/soc_caps.h"

#if defined(SOC_BLE_50_SUPPORTED) && SOC_BLE_50_SUPPORTED
#define BLE_LINK_GATE_WAIT_FOR_PHY 1
#else
#define BLE_LINK_GATE_WAIT_FOR_PHY 0
#endif

static const char *TAG = "ble_link_gate";

static ble_link_gate_slot_t *find_slot(ble_link_gate_t *gate, uint16_t conn_handle)
{
    for (size_t i = 0; i < BLE_LINK_GATE_MAX_CONNECTIONS; ++i) {
        if (gate->slots[i].occupied && gate->slots[i].conn_handle == conn_handle) {
            return &gate->slots[i];
        }
    }
    return NULL;
}

esp_err_t ble_link_gate_init(ble_link_gate_t *gate)
{
    if (!gate) {
        return ESP_ERR_INVALID_ARG;
    }
    memset(gate, 0, sizeof(*gate));
    gate->mutex = xSemaphoreCreateMutex();
    return gate->mutex ? ESP_OK : ESP_ERR_NO_MEM;
}

void ble_link_gate_begin(ble_link_gate_t *gate, uint16_t conn_handle)
{
    if (!gate || !gate->mutex) {
        return;
    }
    xSemaphoreTake(gate->mutex, portMAX_DELAY);
    ble_link_gate_slot_t *slot = find_slot(gate, conn_handle);
    if (!slot) {
        for (size_t i = 0; i < BLE_LINK_GATE_MAX_CONNECTIONS; ++i) {
            if (!gate->slots[i].occupied) {
                slot = &gate->slots[i];
                break;
            }
        }
    }
    if (slot) {
        if (!slot->connected) {
            bool mtu_done = slot->mtu_done;
            *slot = (ble_link_gate_slot_t) {
                .conn_handle = conn_handle,
                .occupied = true,
                .mtu_done = mtu_done,
            };
        }
        slot->connected = true;
    }
    xSemaphoreGive(gate->mutex);
}

void ble_link_gate_end(ble_link_gate_t *gate, uint16_t conn_handle)
{
    if (!gate || !gate->mutex) {
        return;
    }
    xSemaphoreTake(gate->mutex, portMAX_DELAY);
    ble_link_gate_slot_t *slot = find_slot(gate, conn_handle);
    if (slot) {
        memset(slot, 0, sizeof(*slot));
    }
    xSemaphoreGive(gate->mutex);
}

void ble_link_gate_on_event(ble_link_gate_t *gate, int32_t id,
                            const esp_ble_conn_event_data_t *event)
{
    uint16_t conn_handle;
    if (!gate || !gate->mutex || !event) {
        return;
    }
    switch (id) {
    case ESP_BLE_CONN_EVENT_MTU:
        conn_handle = event->mtu_update.conn_handle;
        break;
    case ESP_BLE_CONN_EVENT_CONN_PARAM_UPDATE:
        conn_handle = event->conn_param_update.conn_handle;
        break;
    case ESP_BLE_CONN_EVENT_PHY_UPDATE_COMPLETE:
        conn_handle = event->phy_update.conn_handle;
        break;
    default:
        return;
    }
    xSemaphoreTake(gate->mutex, portMAX_DELAY);
    ble_link_gate_slot_t *slot = find_slot(gate, conn_handle);
    if (!slot && id == ESP_BLE_CONN_EVENT_MTU) {
        struct ble_gap_conn_desc desc;
        if (ble_gap_conn_find(conn_handle, &desc) == 0) {
            for (size_t i = 0; i < BLE_LINK_GATE_MAX_CONNECTIONS; ++i) {
                if (!gate->slots[i].occupied) {
                    slot = &gate->slots[i];
                    *slot = (ble_link_gate_slot_t) {
                        .conn_handle = conn_handle,
                        .occupied = true,
                    };
                    break;
                }
            }
        }
    }
    int status = 0;
    if (slot) {
        switch (id) {
        case ESP_BLE_CONN_EVENT_MTU:
            if (event->mtu_update.channel_id == BLE_L2CAP_CID_ATT) {
                slot->mtu_done = true;
            }
            break;
        case ESP_BLE_CONN_EVENT_CONN_PARAM_UPDATE:
            slot->params_done = true;
            status = event->conn_param_update.status;
            break;
        case ESP_BLE_CONN_EVENT_PHY_UPDATE_COMPLETE:
            slot->phy_done = true;
            status = event->phy_update.status;
            break;
        default:
            break;
        }
    }
    xSemaphoreGive(gate->mutex);
    if (status != 0) {
        ESP_LOGW(TAG, "%s completed conn=%u status=%d (0x%x); using current link settings",
                 id == ESP_BLE_CONN_EVENT_PHY_UPDATE_COMPLETE ? "PHY update" : "Connection parameter update",
                 (unsigned)conn_handle, status, (unsigned)status);
    }
}

bool ble_link_gate_is_ready(ble_link_gate_t *gate, uint16_t conn_handle, uint16_t min_mtu)
{
    bool ready = false;
    bool timed_out = false;
    struct ble_gap_conn_desc desc;
    if (!gate || !gate->mutex) {
        return false;
    }
    xSemaphoreTake(gate->mutex, portMAX_DELAY);
    ble_link_gate_slot_t *slot = find_slot(gate, conn_handle);
    timed_out = slot && slot->timed_out;
    ready = slot && slot->connected &&
            (timed_out || (slot->mtu_done && slot->params_done &&
                           (!BLE_LINK_GATE_WAIT_FOR_PHY || slot->phy_done)));
    xSemaphoreGive(gate->mutex);
    return ready && ble_gap_conn_find(conn_handle, &desc) == 0 &&
           (timed_out || ble_att_mtu(conn_handle) >= min_mtu);
}

bool ble_link_gate_wait(ble_link_gate_t *gate, uint16_t conn_handle, uint16_t min_mtu,
                        uint32_t timeout_ms)
{
    TickType_t deadline = xTaskGetTickCount() + pdMS_TO_TICKS(timeout_ms);
    while (true) {
        if (ble_link_gate_is_ready(gate, conn_handle, min_mtu)) {
            return true;
        }
        xSemaphoreTake(gate->mutex, portMAX_DELAY);
        ble_link_gate_slot_t *slot = find_slot(gate, conn_handle);
        bool disconnected = !slot || !slot->connected;
        bool mtu_done = slot && slot->mtu_done;
        bool params_done = slot && slot->params_done;
        bool phy_done = slot && slot->phy_done;
        bool timed_out = (int32_t)(xTaskGetTickCount() - deadline) >= 0;
        if (slot && slot->connected && timed_out) {
            slot->timed_out = true;
        }
        xSemaphoreGive(gate->mutex);
        if (disconnected) {
            ESP_LOGW(TAG, "Link gate failed conn=%u connected=%u mtu=%u params=%u phy=%u",
                     (unsigned)conn_handle, (unsigned)!disconnected,
                     (unsigned)mtu_done, (unsigned)params_done, (unsigned)phy_done);
            return false;
        }
        if (timed_out) {
            if (!mtu_done) {
                ESP_LOGW(TAG, "MTU negotiation did not complete conn=%u", (unsigned)conn_handle);
            }
            if (!params_done) {
                ESP_LOGW(TAG, "Connection parameter negotiation did not complete conn=%u",
                         (unsigned)conn_handle);
            }
            if (BLE_LINK_GATE_WAIT_FOR_PHY && !phy_done) {
                ESP_LOGW(TAG, "PHY negotiation did not complete conn=%u", (unsigned)conn_handle);
            }
            if (mtu_done && ble_att_mtu(conn_handle) < min_mtu) {
                ESP_LOGW(TAG, "ATT MTU is below the requested minimum conn=%u mtu=%u required=%u",
                         (unsigned)conn_handle, (unsigned)ble_att_mtu(conn_handle),
                         (unsigned)min_mtu);
            }
            ESP_LOGW(TAG, "Link negotiation timed out conn=%u; continuing with current settings",
                     (unsigned)conn_handle);
            return true;
        }
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}
