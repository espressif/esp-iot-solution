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

#include "esp_ans.h"

static const char* TAG = "app_ans";

static const char* cat_info[BLE_ANS_CAT_NUM] = {"Simple Alert", "Email", "News", "Call", "Missed Call", "SMS", "Voice Mail", "Schedule"};

static esp_err_t app_ans_get_new_alert(uint8_t cat_id)
{
    uint8_t cat_val;

    esp_err_t rc = esp_ble_ans_get_new_alert(cat_id, &cat_val);
    if (rc) {
        return rc;
    }

    if (cat_id != 0xFF) {
        ESP_LOGI(TAG, "Category %d of Supported New Alert Category is %s", cat_id, cat_val ? "Enabled" : "Disabled");
    } else {
        ESP_LOGI(TAG, "The Value of Supported New Alert Category is 0x%0x", cat_val);
    }

    return ESP_OK;
}

static esp_err_t app_ans_get_unr_alert(uint8_t cat_id)
{
    uint8_t cat_val;

    esp_err_t rc = esp_ble_ans_get_unread_alert(cat_id, &cat_val);
    if (rc) {
        return rc;
    }

    if (cat_id != 0xFF) {
        ESP_LOGI(TAG, "Category %d of Supported Unread Alert Status Category is %s", cat_id, cat_val ? "Enabled" : "Disabled");
    } else {
        ESP_LOGI(TAG, "The Value of Supported Unread Alert Status Category is 0x%0x", cat_val);
    }

    return ESP_OK;
}

static esp_err_t app_ans_run(uint8_t type, uint8_t cat_id, uint8_t option)
{
    esp_err_t rc = ESP_FAIL;

    switch (type) {
    case 1:
        rc = app_ans_get_new_alert(cat_id);
        break;
    case 2:
        if (cat_id < BLE_ANS_CAT_NUM) {
            rc = esp_ble_ans_set_new_alert(cat_id, option ? cat_info[cat_id] : NULL);
            if (!rc) {
                ESP_LOGI(TAG, "Notify New Alert of Supported Category ID %d", cat_id);
            }
        }
        break;
    case 3:
        rc = app_ans_get_unr_alert(cat_id);
        break;
    case 4:
        rc = esp_ble_ans_set_unread_alert(cat_id);
        if (!rc) {
            ESP_LOGI(TAG, "Notify Unread Alert Status of Supported Category ID %d", cat_id);
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
    struct arg_int *category;
    struct arg_int *option;
    struct arg_end *end;
} svc_ans_args;

static int app_ans_func(int argc, char **argv)
{
    int nerrors = arg_parse(argc, argv, (void **) &svc_ans_args);
    if (nerrors != 0) {
        arg_print_errors(stderr, svc_ans_args.end, argv[0]);
        return 1;
    }

    if (!svc_ans_args.type->count) {
        return 1;
    };

    if (!svc_ans_args.category->count) {
        return 1;
    }

    if (!svc_ans_args.option->count) {
        return 1;
    }

    return app_ans_run(svc_ans_args.type->ival[0], svc_ans_args.category->ival[0], svc_ans_args.option->ival[0]);
}

void app_ans_register(void)
{
    svc_ans_args.type = arg_int0("t", "type", "<01~04>", "01 Get supported new alert category\n \
                     02 Set supported new alert category\n  \
                    03 Get supported unread alert status category\n  \
                    04 Set supported unread alert status category");
    svc_ans_args.category = arg_int0("c", "category", "<00~07>", "Category ID");
    svc_ans_args.option = arg_int0("o", "option", "<00~01>", "1: Enable, 0: Disable");
    svc_ans_args.end = arg_end(2);

    const esp_console_cmd_t svc_ans_cmd = {
        .command = "ans",
        .help = "Alert Notification Service",
        .hint = NULL,
        .func = &app_ans_func,
        .argtable = &svc_ans_args
    };

    ESP_ERROR_CHECK(esp_console_cmd_register(&svc_ans_cmd));
}
