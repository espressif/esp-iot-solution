/*
 * SPDX-FileCopyrightText: 2019-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include <esp_timer.h>
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
#ifdef CONFIG_EXAMPLE_BLE_HRS_ROLE
#include "esp_dis.h"
#endif
#include "esp_hrs.h"

static const char *TAG = "app_main";

#define APP_HRS_SET_VAL(a)      (esp_random() % a)

#define APP_HRS_FLAGS           2
#define APP_HRS_MAX_U08         0xFF
#define APP_HRS_MAX_U16         0xFFFF

static void app_ble_hrs_init(void)
{
#ifdef CONFIG_EXAMPLE_BLE_HRS_ROLE
    esp_ble_dis_init();

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
    esp_ble_set_dis_model_number(CONFIG_BLE_DIS_MODEL);
    esp_ble_set_dis_serial_number(mac_str);
    esp_ble_set_dis_firmware_revision(esp_get_idf_version());
    esp_ble_set_dis_hardware_revision(esp_get_idf_version());
    esp_ble_set_dis_software_revision(esp_get_idf_version());
    esp_ble_set_dis_manufacturer_name(CONFIG_BLE_DIS_MANUF);
    esp_ble_set_dis_system_id(CONFIG_BLE_DIS_SYSTEM_ID);
    esp_ble_set_dis_pnp_id(&pnp_id);
#endif
    esp_ble_hrs_init();
    esp_ble_hrs_set_location(BLE_HRS_CHR_BODY_SENSOR_LOC_HAND);
}

static void heartrate_timer_cb(void *arg)
{
    esp_ble_hrs_hrm_t hrm = {
        .flags = {
            .format = APP_HRS_SET_VAL(APP_HRS_FLAGS),
            .interval = APP_HRS_SET_VAL(APP_HRS_FLAGS),
            .supported = APP_HRS_SET_VAL(APP_HRS_FLAGS),
            .energy = APP_HRS_SET_VAL(APP_HRS_FLAGS),
            .detected = APP_HRS_SET_VAL(APP_HRS_FLAGS),
        },
        .energy_val = APP_HRS_SET_VAL(APP_HRS_MAX_U16),
    };

    if (hrm.flags.format) {
        hrm.heartrate.u16 = APP_HRS_SET_VAL(APP_HRS_MAX_U16);
    } else {
        hrm.heartrate.u8 = APP_HRS_SET_VAL(APP_HRS_MAX_U08);
    }

    esp_fill_random(hrm.interval_buf, 2 * APP_HRS_SET_VAL(BLE_HRS_CHR_MERSUREMENT_RR_INTERVAL_MAX_NUM));

    ESP_LOGI(TAG, "Heart Rate Value %d", hrm.flags.format ? hrm.heartrate.u16 : hrm.heartrate.u8);

    if (!hrm.flags.supported && hrm.flags.detected) {
        ESP_LOGI(TAG, "Skin Contact should be detected on Sensor Contact Feature Supported");
    } else {
        ESP_LOGI(TAG, "Sensor Contact Feature %s Supported", hrm.flags.supported ? "is" : "isn't");
        ESP_LOGI(TAG, "Skin Contact %s Detected", hrm.flags.detected ? "is" : "isn't");
    }

    if (hrm.flags.energy) {
        ESP_LOGI(TAG, "The Energy Expended feature is supported and accumulated %dkJ", hrm.energy_val);
    }

    if (hrm.flags.interval) {
        ESP_LOGI(TAG, "The multiple time between two R-Wave detections");
        ESP_LOG_BUFFER_HEX_LEVEL(TAG, hrm.interval_buf, sizeof(hrm.interval_buf), ESP_LOG_INFO);
    }
    esp_ble_hrs_set_hrm(&hrm);
}

static void app_ble_conn_event_handler(void *handler_args, esp_event_base_t base, int32_t id, void *event_data)
{
    if (base != BLE_CONN_MGR_EVENTS) {
        return;
    }

    static esp_timer_handle_t heartrate_timer = NULL;
    esp_timer_create_args_t heartrate_timer_conf = {
        .callback = heartrate_timer_cb,
        .arg = NULL,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "heartrate"
    };

    switch (id) {
    case ESP_BLE_CONN_EVENT_CONNECTED:
        ESP_LOGI(TAG, "ESP_BLE_CONN_EVENT_CONNECTED");
        if (!heartrate_timer) {
            esp_timer_create(&heartrate_timer_conf, &heartrate_timer);
            esp_timer_start_periodic(heartrate_timer, 10000000);
        }
        break;
    case ESP_BLE_CONN_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "ESP_BLE_CONN_EVENT_DISCONNECTED");
        if (heartrate_timer) {
            esp_timer_stop(heartrate_timer);
            esp_timer_delete(heartrate_timer);
            heartrate_timer = NULL;
        }
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
    app_ble_hrs_init();
    if (esp_ble_conn_start() != ESP_OK) {
        esp_ble_conn_stop();
        esp_ble_conn_deinit();
        esp_event_handler_unregister(BLE_CONN_MGR_EVENTS, ESP_EVENT_ANY_ID, app_ble_conn_event_handler);
    }
}
