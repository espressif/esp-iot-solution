/*
 * SPDX-FileCopyrightText: 2019-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include <time.h>

#include <esp_timer.h>
#include "esp_log.h"
#include "nvs_flash.h"

#include "esp_system.h"
#if (ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(4, 4, 0))
#include "esp_random.h"
#include "esp_mac.h"
#endif

#include "esp_ble_conn_mgr.h"
#ifdef CONFIG_EXAMPLE_BLE_THERMOMETER_ROLE
#include "esp_dis.h"
#endif
#include "esp_hts.h"

static const char *TAG = "app_main";

#if (ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0))
#define UINTSTR "%ld"
#else
#define UINTSTR "%d"
#endif

#define APP_HTS_SET_VAL(a)      (esp_random() % a)

#define APP_HTS_FLAGS           2
#define APP_HTS_MAX_U16         0xFFFF

static esp_timer_handle_t s_ble_hts_timer = NULL;

static void app_ble_hts_temp(void)
{
    time_t gnow;
    time(&gnow);
    struct tm *tm = localtime(&gnow);

    esp_ble_hts_temp_t measurement_temp = {
        .flags = {
            .temperature_unit = APP_HTS_SET_VAL(APP_HTS_FLAGS),
            .time_stamp = APP_HTS_SET_VAL(APP_HTS_FLAGS),
            .temperature_type = APP_HTS_SET_VAL(APP_HTS_FLAGS),
        },
        .timestamp = {
            .year = tm->tm_year + 1900,
            .month = tm->tm_mon + 1,
            .day = tm->tm_mday,
            .hours = tm->tm_hour,
            .minutes = tm->tm_min,
            .seconds = tm->tm_sec,
        },
        .location = APP_HTS_SET_VAL(BLE_HTS_CHR_TEMPERATURE_TYPE_MAX),
    };

    if (measurement_temp.flags.time_stamp) {
        ESP_LOGI(TAG, "%02d年%d月%d日  %d:%d:%d",
                 measurement_temp.timestamp.year,  measurement_temp.timestamp.month,   measurement_temp.timestamp.day,
                 measurement_temp.timestamp.hours, measurement_temp.timestamp.minutes, measurement_temp.timestamp.seconds);
    }

    if (measurement_temp.flags.temperature_unit) {
        measurement_temp.temperature.celsius = APP_HTS_SET_VAL(APP_HTS_MAX_U16);
        ESP_LOGI(TAG, "Measurement Temperature "UINTSTR"°F", measurement_temp.temperature.celsius);
    } else {
        measurement_temp.temperature.fahrenheit = APP_HTS_SET_VAL(APP_HTS_MAX_U16);
        ESP_LOGI(TAG, "Measurement Temperature "UINTSTR"°C", measurement_temp.temperature.fahrenheit);
    }

    if (measurement_temp.flags.temperature_type) {
        ESP_LOGI(TAG, "Temperature Type %d", measurement_temp.location);
    }

    esp_ble_hts_set_measurement_temp(&measurement_temp);

    esp_ble_hts_temp_t intermediate_temp = {
        .flags = {
            .temperature_unit = APP_HTS_SET_VAL(APP_HTS_FLAGS),
            .time_stamp = APP_HTS_SET_VAL(APP_HTS_FLAGS),
            .temperature_type = APP_HTS_SET_VAL(APP_HTS_FLAGS),
        },
        .timestamp = {
            .year = tm->tm_year + 1900,
            .month = tm->tm_mon + 1,
            .day = tm->tm_mday,
            .hours = tm->tm_hour,
            .minutes = tm->tm_min,
            .seconds = tm->tm_sec,
        },
        .location = APP_HTS_SET_VAL(BLE_HTS_CHR_TEMPERATURE_TYPE_MAX),
    };

    if (intermediate_temp.flags.time_stamp) {
        ESP_LOGI(TAG, "%02d年%d月%d日  %d:%d:%d",
                 intermediate_temp.timestamp.year,  intermediate_temp.timestamp.month,   intermediate_temp.timestamp.day,
                 intermediate_temp.timestamp.hours, intermediate_temp.timestamp.minutes, intermediate_temp.timestamp.seconds);
    }

    if (intermediate_temp.flags.temperature_unit) {
        intermediate_temp.temperature.celsius = APP_HTS_SET_VAL(APP_HTS_MAX_U16);
        ESP_LOGI(TAG, "Intermediate Temperature "UINTSTR"°F", intermediate_temp.temperature.celsius);
    } else {
        intermediate_temp.temperature.fahrenheit = APP_HTS_SET_VAL(APP_HTS_MAX_U16);
        ESP_LOGI(TAG, "Intermediate Temperature "UINTSTR"°C", intermediate_temp.temperature.fahrenheit);
    }

    if (intermediate_temp.flags.temperature_type) {
        ESP_LOGI(TAG, "Temperature Type %d", intermediate_temp.location);
    }

    esp_ble_hts_set_intermediate_temp(&intermediate_temp);
}

static void app_ble_hts_event_handler(void *handler_args, esp_event_base_t base, int32_t id, void *event_data)
{
    if (base != BLE_HTS_EVENTS) {
        return;
    }

    uint16_t *interval_val = (uint16_t *)event_data;
    switch (id) {
    case BLE_HTS_CHR_UUID16_MEASUREMENT_INTERVAL:
        app_ble_hts_temp();
        esp_timer_stop(s_ble_hts_timer);
        if ((*interval_val)) {
            esp_timer_start_periodic(s_ble_hts_timer, (*interval_val) * 1000000);
        }
        break;
    default:
        break;
    }
}

static void app_ble_hts_init(void)
{
#ifdef CONFIG_EXAMPLE_BLE_THERMOMETER_ROLE
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
    esp_ble_hts_init();
    esp_event_handler_register(BLE_HTS_EVENTS, ESP_EVENT_ANY_ID, app_ble_hts_event_handler, NULL);
    esp_ble_hts_set_temp_type(APP_HTS_SET_VAL(BLE_HTS_CHR_TEMPERATURE_TYPE_MAX));
}

static void app_ble_hts_timer_cb(void *arg)
{
    app_ble_hts_temp();
}

static void app_ble_conn_event_handler(void *handler_args, esp_event_base_t base, int32_t id, void *event_data)
{
    if (base != BLE_CONN_MGR_EVENTS) {
        return;
    }

    uint16_t interval_val = 0;
    esp_timer_create_args_t ble_hts_timer_conf = {
        .callback = app_ble_hts_timer_cb,
        .arg = NULL,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "thermometer"
    };

    switch (id) {
    case ESP_BLE_CONN_EVENT_CONNECTED:
        ESP_LOGI(TAG, "ESP_BLE_CONN_EVENT_CONNECTED");
        esp_ble_hts_set_measurement_interval(APP_HTS_SET_VAL(APP_HTS_MAX_U16));
        esp_ble_hts_get_measurement_interval(&interval_val);
        if (!s_ble_hts_timer) {
            esp_timer_create(&ble_hts_timer_conf, &s_ble_hts_timer);
            esp_timer_start_periodic(s_ble_hts_timer, interval_val * 1000000);
        }
        break;
    case ESP_BLE_CONN_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "ESP_BLE_CONN_EVENT_DISCONNECTED");
        if (s_ble_hts_timer) {
            esp_timer_stop(s_ble_hts_timer);
            esp_timer_delete(s_ble_hts_timer);
            s_ble_hts_timer = NULL;
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
    app_ble_hts_init();
    if (esp_ble_conn_start() != ESP_OK) {
        esp_ble_conn_stop();
        esp_ble_conn_deinit();
        esp_event_handler_unregister(BLE_CONN_MGR_EVENTS, ESP_EVENT_ANY_ID, app_ble_conn_event_handler);
        esp_event_handler_unregister(BLE_HTS_EVENTS, ESP_EVENT_ANY_ID, app_ble_hts_event_handler);
    }
}
