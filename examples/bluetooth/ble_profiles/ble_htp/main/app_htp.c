/*
 * SPDX-FileCopyrightText: 2019-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <string.h>
#include "esp_system.h"
#include "esp_log.h"
#include "esp_console.h"
#include "argtable3/argtable3.h"

#include "esp_htp.h"

static const char* TAG = "app_htp";

#if (ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0))
#define UINTSTR "%ld"
#else
#define UINTSTR "%d"
#endif

static esp_err_t app_htp_run(uint8_t type, uint16_t temp_val)
{
    esp_err_t rc = ESP_FAIL;
    uint8_t  temp_type = 0;

    switch (type) {
    case 1:
        rc = esp_ble_htp_get_temp_type(&temp_type);
        if (!rc) {
            ESP_LOGI(TAG, "Gets the current temperature type value %d", temp_type);
        }
        break;
    case 2:
        rc = esp_ble_htp_get_measurement_interval(&temp_val);
        if (!rc) {
            ESP_LOGI(TAG, "Gets the measurement interval value %d", temp_val);
        }
        break;
    case 3:
        rc = esp_ble_htp_set_measurement_interval(temp_val);
        if (!rc) {
            ESP_LOGI(TAG, "Sets the measurement interval value %d", temp_val);
        }
        break;
    default:
        break;
    }

    if (rc) {
        ESP_LOGE(TAG, "Invalid option");
    }

    return rc;
}

static struct {
    struct arg_int *type;
    struct arg_int *interval;
    struct arg_end *end;
} htp_args;

static int app_htp_func(int argc, char **argv)
{
    int nerrors = arg_parse(argc, argv, (void **) &htp_args);
    if (nerrors != 0) {
        arg_print_errors(stderr, htp_args.end, argv[0]);
        return 1;
    }

    if (!htp_args.type->count) {
        return 1;
    };

    return app_htp_run(htp_args.type->ival[0], htp_args.interval->count ? htp_args.interval->ival[0] : htp_args.interval->count);
}

static void app_htp_event_handler(void *handler_args, esp_event_base_t base, int32_t id, void *event_data)
{
    if (base != BLE_HTP_EVENTS) {
        return;
    }

    esp_ble_htp_data_t *ble_htp_data = (esp_ble_htp_data_t *)event_data;
    switch (id) {
    case BLE_HTP_CHR_UUID16_TEMPERATURE_MEASUREMENT:
        if (ble_htp_data->flags.time_stamp) {
            ESP_LOGI(TAG, "%02d年%d月%d日  %d:%d:%d",
                     ble_htp_data->timestamp.year,  ble_htp_data->timestamp.month,   ble_htp_data->timestamp.day,
                     ble_htp_data->timestamp.hours, ble_htp_data->timestamp.minutes, ble_htp_data->timestamp.seconds);
        }

        if (ble_htp_data->flags.temperature_type) {
            ESP_LOGI(TAG, "Temperature Type %d", ble_htp_data->location);
        }

        if (ble_htp_data->flags.temperature_unit) {
            ESP_LOGI(TAG, "Measurement Temperature "UINTSTR"°F", ble_htp_data->temperature.fahrenheit);
        } else {
            ESP_LOGI(TAG, "Measurement Temperature "UINTSTR"°C", ble_htp_data->temperature.celsius);
        }
        break;
    case BLE_HTP_CHR_UUID16_INTERMEDIATE_TEMPERATURE:
        if (ble_htp_data->flags.time_stamp) {
            ESP_LOGI(TAG, "%02d年%d月%d日  %d:%d:%d",
                     ble_htp_data->timestamp.year,  ble_htp_data->timestamp.month,   ble_htp_data->timestamp.day,
                     ble_htp_data->timestamp.hours, ble_htp_data->timestamp.minutes, ble_htp_data->timestamp.seconds);
        }

        if (ble_htp_data->flags.temperature_type) {
            ESP_LOGI(TAG, "Temperature Type %d", ble_htp_data->location);
        }

        if (ble_htp_data->flags.temperature_unit) {
            ESP_LOGI(TAG, "Intermediate Temperature "UINTSTR"°F", ble_htp_data->temperature.fahrenheit);
        } else {
            ESP_LOGI(TAG, "Intermediate Temperature "UINTSTR"°C", ble_htp_data->temperature.celsius);
        }
        break;
    default:
        break;
    }
}

void register_htp(void)
{
    htp_args.type = arg_int0("t", "type", "<01~03>", "01 Get Temperature Type Characteristic\n \
                     02 Get Measurement Interval Characteristic\n  \
                    03 Set Measurement Interval Characteristic");
    htp_args.interval = arg_int0("i", "interval", "<0~65535>", "0: No periodic measurement, Other: Duration of measurement interval");
    htp_args.end = arg_end(2);

    const esp_console_cmd_t join_cmd = {
        .command = "htp",
        .help = "Health Thermometer Profile",
        .hint = NULL,
        .func = &app_htp_func,
        .argtable = &htp_args
    };

    ESP_ERROR_CHECK(esp_console_cmd_register(&join_cmd));

    esp_ble_htp_init();
    esp_event_handler_register(BLE_HTP_EVENTS, ESP_EVENT_ANY_ID, app_htp_event_handler, NULL);
}
