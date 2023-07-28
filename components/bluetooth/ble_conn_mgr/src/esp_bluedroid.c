/*
 * SPDX-FileCopyrightText: 2019-2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <esp_gatt_common_api.h>
#include <esp_gap_bt_api.h>
#include <esp_bt.h>
#include <esp_gap_ble_api.h>
#include <esp_gatts_api.h>
#include <esp_bt.h>
#include <esp_bt_main.h>

#include "esp_ble_conn_mgr.h"

/* Event source task related definitions */
ESP_EVENT_DEFINE_BASE(BLE_CONN_MGR_EVENTS);

// Todo: Support BLE connection managemanet use bluedroid host
esp_err_t esp_ble_conn_init(esp_ble_conn_config_t *config)
{
    return ESP_OK;
}

esp_err_t esp_ble_conn_deinit(void)
{
    return ESP_OK;
}

esp_err_t esp_ble_conn_start(void)
{
    return ESP_OK;
}

esp_err_t esp_ble_conn_stop(void)
{
    return ESP_OK;
}

esp_err_t esp_ble_conn_set_mtu(uint16_t mtu)
{
    return ESP_OK;
}

esp_err_t esp_ble_conn_connect(void)
{
    return ESP_OK;
}

esp_err_t esp_ble_conn_disconnect(void)
{
    return ESP_OK;
}

esp_err_t esp_ble_conn_notify(const esp_ble_conn_data_t *inbuff)
{
    return ESP_OK;
}

esp_err_t esp_ble_conn_read(esp_ble_conn_data_t *inbuff)
{
    return ESP_OK;
}

esp_err_t esp_ble_conn_write(const esp_ble_conn_data_t *inbuff)
{
    return ESP_OK;
}

esp_err_t esp_ble_conn_add_svc(const esp_ble_conn_svc_t *svc)
{
    return ESP_OK;
}

esp_err_t esp_ble_conn_remove_svc(const esp_ble_conn_svc_t *svc)
{
    return ESP_OK;
}
