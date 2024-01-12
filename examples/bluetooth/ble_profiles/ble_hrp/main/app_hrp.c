/*
 * SPDX-FileCopyrightText: 2019-2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <string.h>
#include "esp_system.h"
#include "esp_log.h"
#include "esp_console.h"
#include "argtable3/argtable3.h"

#include "esp_hrp.h"

static const char* TAG = "app_hrp";;

static esp_err_t app_hrp_run(uint8_t type, uint16_t temp_val)
{
    esp_err_t rc = ESP_FAIL;
    uint8_t  temp_type = 0;

    switch (type) {
    case 1:
        rc = esp_ble_hrp_get_location(&temp_type);
        if (!rc) {
            ESP_LOGI(TAG, "Gets the Value of Body Sensor Location %d", temp_type);
        }
        break;
    case 2:
        rc = esp_ble_hrp_set_ctrl(temp_val);
        if (!rc) {
            ESP_LOGI(TAG, "Sets the Value of Heart Rate Control Point %d", temp_val);
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
    struct arg_int *cmd_id;
    struct arg_end *end;
} hrp_args;

static int app_hrp_func(int argc, char **argv)
{
    int nerrors = arg_parse(argc, argv, (void **) &hrp_args);
    if (nerrors != 0) {
        arg_print_errors(stderr, hrp_args.end, argv[0]);
        return 1;
    }

    if (!hrp_args.type->count) {
        return 1;
    };

    return app_hrp_run(hrp_args.type->ival[0], hrp_args.cmd_id->count ? hrp_args.cmd_id->ival[0] : hrp_args.cmd_id->count);
}

static void app_hrp_event_handler(void *handler_args, esp_event_base_t base, int32_t id, void *event_data)
{
    if (base != BLE_HRP_EVENTS) {
        return;
    }

    esp_ble_hrp_data_t *ble_hrp_data = (esp_ble_hrp_data_t *)event_data;
    switch (id) {
    case BLE_HRP_CHR_UUID16_MEASUREMENT:
        ESP_LOGI(TAG, "Heart Rate Value %d", ble_hrp_data->flags.format ? ble_hrp_data->heartrate.u16 : ble_hrp_data->heartrate.u8);

        if (!ble_hrp_data->flags.supported && ble_hrp_data->flags.detected) {
            ESP_LOGW(TAG, "Skin Contact should be detected on Sensor Contact Feature Supported");
        } else {
            ESP_LOGW(TAG, "Sensor Contact Feature %s Supported", ble_hrp_data->flags.supported ? "is" : "isn't");
            ESP_LOGW(TAG, "Skin Contact %s Detected", ble_hrp_data->flags.detected ? "is" : "isn't");
        }

        if (ble_hrp_data->flags.energy) {
            ESP_LOGI(TAG, "The Energy Expended feature is supported and accumulated %dkJ", ble_hrp_data->energy_val);
        }

        if (ble_hrp_data->flags.interval) {
            ESP_LOGI(TAG, "The multiple time between two R-Wave detections");
            ESP_LOG_BUFFER_HEX_LEVEL(TAG, ble_hrp_data->interval_buf, sizeof(ble_hrp_data->interval_buf), ESP_LOG_INFO);
        }
        break;
    default:
        break;
    }
}

void register_hrp(void)
{
    hrp_args.type = arg_int0("t", "type", "<01~02>", "01 Gets Body Sensor Location Characteristic\n \
                     02 Set Heart Rate Control Point Characteristic");
    hrp_args.cmd_id = arg_int0("c", "cmd", "<0~255>", "1: Reset Energy Expended, Other: Reserved for Future Use");
    hrp_args.end = arg_end(2);

    const esp_console_cmd_t join_cmd = {
        .command = "hrp",
        .help = "Heart Rate Profile",
        .hint = NULL,
        .func = &app_hrp_func,
        .argtable = &hrp_args
    };

    ESP_ERROR_CHECK(esp_console_cmd_register(&join_cmd));

    esp_ble_hrp_init();
    esp_event_handler_register(BLE_HRP_EVENTS, ESP_EVENT_ANY_ID, app_hrp_event_handler, NULL);
}
