/*
 * SPDX-FileCopyrightText: 2019-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>

#include "esp_log.h"
#include "nvs_flash.h"

#include "esp_idf_version.h"
#if (ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(4, 4, 0))
#include "esp_random.h"
#include "esp_mac.h"
#else
#include "esp_system.h"
#endif

#include "esp_ble_conn_mgr.h"
#include "esp_dis.h"

static const char *TAG = "app_main";

static void app_ble_dis_init(void)
{
    esp_ble_dis_init();
#ifdef CONFIG_EXAMPLE_BLE_DIS_INFO
    esp_ble_dis_pnp_t pnp_id = {
        .src = CONFIG_BLE_DIS_PNP_VID_SRC,
        .vid = CONFIG_BLE_DIS_PNP_VID,
        .pid = CONFIG_BLE_DIS_PNP_PID,
        .ver = CONFIG_BLE_DIS_PNP_VER
    };
    static char mac_str[19] = {0};
    uint8_t mac[6];
    ESP_ERROR_CHECK(esp_read_mac((uint8_t *)mac, ESP_MAC_BT));
    sprintf(mac_str, MACSTR, MAC2STR(mac));
    esp_ble_dis_set_model_number(CONFIG_BLE_DIS_MODEL);
    esp_ble_dis_set_serial_number(mac_str);
    esp_ble_dis_set_firmware_revision(esp_get_idf_version());
    esp_ble_dis_set_hardware_revision(esp_get_idf_version());
    esp_ble_dis_set_software_revision(esp_get_idf_version());
    esp_ble_dis_set_manufacturer_name(CONFIG_BLE_DIS_MANUF);
    esp_ble_dis_set_system_id(CONFIG_BLE_DIS_SYSTEM_ID);
    esp_ble_dis_set_pnp_id(&pnp_id);
#endif
}

static void app_ble_conn_event_handler(void *handler_args, esp_event_base_t base, int32_t id, void *event_data)
{
    if (base != BLE_CONN_MGR_EVENTS) {
        return;
    }

    esp_ble_dis_pnp_t pnp_id;
    const char *model_number = NULL;
    const char *serial_number = NULL;
    const char *firmware_revision = NULL;
    const char *hardware_revision = NULL;
    const char *software_revision = NULL;
    const char *manufacturer = NULL;
    const char *system_id = NULL;
    switch (id) {
    case ESP_BLE_CONN_EVENT_CONNECTED:
        ESP_LOGI(TAG, "ESP_BLE_CONN_EVENT_CONNECTED");
        esp_ble_dis_get_pnp_id(&pnp_id);
        esp_ble_dis_get_model_number(&model_number);
        esp_ble_dis_get_serial_number(&serial_number);
        esp_ble_dis_get_firmware_revision(&firmware_revision);
        esp_ble_dis_get_hardware_revision(&hardware_revision);
        esp_ble_dis_get_software_revision(&software_revision);
        esp_ble_dis_get_manufacturer_name(&manufacturer);
        esp_ble_dis_get_system_id(&system_id);
        ESP_LOGI(TAG, "Model name %s", model_number);
        ESP_LOGI(TAG, "Serial Number %s", serial_number);
        ESP_LOGI(TAG, "Firmware revision %s", firmware_revision);
        ESP_LOGI(TAG, "Hardware revision %s", hardware_revision);
        ESP_LOGI(TAG, "Software revision %s", software_revision);
        ESP_LOGI(TAG, "Manufacturer name %s", manufacturer);
        ESP_LOGI(TAG, "System ID %s", system_id);
        ESP_LOGI(TAG, "PnP ID Vendor ID Source 0x%0x, Vendor ID 0x%02x, Product ID 0x%02x, Product Version 0x%02x",
                 pnp_id.src, pnp_id.vid, pnp_id.pid, pnp_id.ver);
        break;
    case ESP_BLE_CONN_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "ESP_BLE_CONN_EVENT_DISCONNECTED");
        break;
    default:
        break;
    }
}

void
app_main(void)
{
    esp_ble_conn_config_t config = {
        .device_name = CONFIG_EXAMPLE_BLE_ADV_NAME,
        .broadcast_data = CONFIG_EXAMPLE_BLE_SUB_ADV
    };

    esp_err_t ret;

    // Initialize NVS
    ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    esp_event_loop_create_default();
    esp_event_handler_register(BLE_CONN_MGR_EVENTS, ESP_EVENT_ANY_ID, app_ble_conn_event_handler, NULL);

    esp_ble_conn_init(&config);
    app_ble_dis_init();
    if (esp_ble_conn_start() != ESP_OK) {
        esp_ble_conn_stop();
        esp_ble_conn_deinit();
        esp_event_handler_unregister(BLE_CONN_MGR_EVENTS, ESP_EVENT_ANY_ID, app_ble_conn_event_handler);
    }
}
