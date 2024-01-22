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

#include "esp_anp.h"

static const char* TAG = "app_anp";

static esp_err_t app_anp_get_new_alert(uint8_t cat_id)
{
    uint8_t cat_val;

    esp_err_t rc = esp_ble_anp_get_new_alert(cat_id, &cat_val);
    if (rc) {
        return rc;
    }

    if (cat_id != 0xFF) {
        ESP_LOGI(TAG, "4.12 Category %d of Supported New Alert Category is %s", cat_id, cat_val ? "Enabled" : "Disabled");
    } else {
        ESP_LOGI(TAG, "4.03 The Value of Supported New Alert Category is 0x%0x", cat_val);
    }

    return ESP_OK;
}

static esp_err_t app_anp_set_new_alert(uint8_t cat_id, uint8_t option)
{
    esp_err_t rc = esp_ble_anp_set_new_alert(cat_id, option);
    if (rc) {
        return rc;
    }

    if (cat_id != 0xFF) {
        ESP_LOGI(TAG, "4.06 Request Category %d of Supported New Alert Category to Notify", cat_id);
    } else {
        ESP_LOGI(TAG, "4.10 Recover All Category of Supported New Alert Category to Notify");
    }

    return ESP_OK;
}

static esp_err_t app_anp_get_unr_alert(uint8_t cat_id)
{
    uint8_t cat_val;

    esp_err_t rc = esp_ble_anp_get_unr_alert(cat_id, &cat_val);
    if (rc) {
        return rc;
    }

    if (cat_id != 0xFF) {
        ESP_LOGI(TAG, "4.13 Category %d of Supported Unread Alert Status Category is %s", cat_id, cat_val ? "Enabled" : "Disabled");
    } else {
        ESP_LOGI(TAG, "4.04 The Value of Supported Unread Alert Status Category is 0x%0x", cat_val);
    }

    return ESP_OK;
}

static esp_err_t app_anp_set_unr_alert(uint8_t cat_id, uint8_t option)
{
    esp_err_t rc = esp_ble_anp_set_unr_alert(cat_id, option);
    if (rc) {
        return rc;
    }

    if (cat_id != 0xFF) {
        ESP_LOGI(TAG, "4.08 Request Category %d of Supported Unread Alert Status Category to Notify", cat_id);
    } else {
        ESP_LOGI(TAG, "4.11 Recover All Category of Supported Unread Alert Status Category to Notify");
    }

    return ESP_OK;
}

static esp_err_t app_anp_run(uint8_t type, uint8_t cat_id, uint8_t option)
{
    esp_err_t rc = ESP_FAIL;

    switch (type) {
    case 1:
        rc = app_anp_get_new_alert(cat_id);
        break;
    case 2:
        rc = app_anp_set_new_alert(cat_id, option);
        break;
    case 3:
        rc = app_anp_get_unr_alert(cat_id);
        break;
    case 4:
        rc = app_anp_set_unr_alert(cat_id, option);
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
    struct arg_int *category;
    struct arg_int *option;
    struct arg_end *end;
} anp_args;

static int app_anp_func(int argc, char **argv)
{
    int nerrors = arg_parse(argc, argv, (void **) &anp_args);
    if (nerrors != 0) {
        arg_print_errors(stderr, anp_args.end, argv[0]);
        return 1;
    }

    if (!anp_args.type->count) {
        return 1;
    };

    if (!anp_args.category->count) {
        return 1;
    }

    if (!anp_args.option->count) {
        return 1;
    }

    return app_anp_run(anp_args.type->ival[0], anp_args.category->ival[0], anp_args.option->ival[0]);
}

static void app_anp_event_handler(void *handler_args, esp_event_base_t base, int32_t id, void *event_data)
{
    if (base != BLE_ANP_EVENTS) {
        return;
    }

    esp_ble_anp_data_t *ble_anp_data = (esp_ble_anp_data_t *)event_data;
    switch (id) {
    case BLE_ANP_CHR_UUID16_NEW_ALERT:
        ESP_LOGI(TAG, "Get the current message counts %d from category %d of supported new alert", ble_anp_data->unr_alert_stat.cat_id, ble_anp_data->unr_alert_stat.count);
        break;
    case BLE_ANP_CHR_UUID16_UNR_ALERT_STAT:
        ESP_LOGI(TAG, "Get the current message counts %d from category %d of supported unread alert status", ble_anp_data->unr_alert_stat.cat_id, ble_anp_data->unr_alert_stat.count);
        break;
    default:
        break;
    }
}

void register_anp(void)
{
    anp_args.type = arg_int0("t", "type", "<01~04>", "01 Get supported new alert category\n \
                     02 Set supported new alert category\n  \
                    03 Get supported unread alert status category\n  \
                    04 Set supported unread alert status category");
    anp_args.category = arg_int0("c", "category", "<01~255>", "Category ID");
    anp_args.option = arg_int0("o", "option", "<00~02>", "0: Enable, 1: Disable, 2: Recover");
    anp_args.end = arg_end(2);

    const esp_console_cmd_t join_cmd = {
        .command = "anp",
        .help = "Alert Notification Profile",
        .hint = NULL,
        .func = &app_anp_func,
        .argtable = &anp_args
    };

    ESP_ERROR_CHECK(esp_console_cmd_register(&join_cmd));

    esp_ble_anp_init();
    esp_event_handler_register(BLE_ANP_EVENTS, ESP_EVENT_ANY_ID, app_anp_event_handler, NULL);
}
