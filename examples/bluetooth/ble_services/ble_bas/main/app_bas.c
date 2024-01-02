/*
 * SPDX-FileCopyrightText: 2019-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <string.h>

#include "esp_idf_version.h"
#if (ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(4, 4, 0))
#include "esp_random.h"
#else
#include "esp_system.h"
#endif

#include "esp_log.h"
#include "esp_console.h"
#include "argtable3/argtable3.h"

#include "esp_bas.h"

static const char* TAG = "app_bas";

#define APP_BAS_SET_VAL(a)      (esp_random() % a)

#define APP_BAS_FLAGS           2
#define APP_BAS_MAX_BATTERY     4
#define APP_BAS_MAX_CHARGE      8
#define APP_BAS_MAX_L08         0x7F
#define APP_BAS_MAX_U08         0xFF
#define APP_BAS_MAX_U16         0xFFFF
#define APP_BAS_MAX_U24         0xFFFFFF
#define APP_BAS_MAX_LEVEL       100
#define APP_BAS_MAX_VOLTAGE     220

static esp_err_t app_get_bas(uint8_t type)
{
    esp_err_t rc = ESP_FAIL;

    switch (type) {
    case 1: {
        uint8_t level = APP_BAS_SET_VAL(APP_BAS_MAX_LEVEL);
        rc = esp_ble_bas_get_battery_level(&level);
    }
    break;
    case 2: {
        esp_ble_bas_level_status_t level_status;
        rc = esp_ble_bas_get_level_status(&level_status);
    }
    break;
    case 3: {
        uint32_t estimated_date = APP_BAS_SET_VAL(APP_BAS_MAX_U24);
        rc = esp_ble_bas_get_estimated_date(&estimated_date);
    }
    break;
    case 4: {
        esp_ble_bas_critical_status_t critical_status;
        rc = esp_ble_bas_get_critical_status(&critical_status);
    }
    break;
    case 5: {
        esp_ble_bas_energy_status_t energy_status;
        rc = esp_ble_bas_get_energy_status(&energy_status);
    }
    break;
    case 6: {
        esp_ble_bas_time_status_t time_status;
        rc = esp_ble_bas_get_time_status(&time_status);
    }
    break;
    case 7: {
        esp_ble_bas_health_status_t health_status;
        rc = esp_ble_bas_get_health_status(&health_status);
    }
    break;
    case 8: {
        esp_ble_bas_health_info_t health_info;
        rc = esp_ble_bas_get_health_info(&health_info);
    }
    break;
    case 9: {
        esp_ble_bas_battery_info_t battery_info;
        rc = esp_ble_bas_get_battery_info(&battery_info);
    }
    break;
    default:
        break;
    }

    if (rc) {
        ESP_LOGE(TAG, "Get characteristic %d, fail %s", type, esp_err_to_name(rc));
    }

    return rc;
}

static esp_err_t app_set_bas(uint8_t type)
{
    esp_err_t rc = ESP_FAIL;

    switch (type) {
    case 1: {
        uint8_t level = APP_BAS_SET_VAL(APP_BAS_MAX_LEVEL);
        rc = esp_ble_bas_set_battery_level(&level);
    }
    break;
    case 2: {
        esp_ble_bas_level_status_t level_status = {
            .flags = {
                .en_identifier = APP_BAS_SET_VAL(APP_BAS_FLAGS),
                .en_battery_level = APP_BAS_SET_VAL(APP_BAS_FLAGS),
                .en_additional_status = APP_BAS_SET_VAL(APP_BAS_FLAGS),
            },
            .power_state = {
                .battery = APP_BAS_SET_VAL(APP_BAS_FLAGS),
                .wired_external_power_source = APP_BAS_SET_VAL(APP_BAS_MAX_BATTERY),
                .wireless_external_power_source = APP_BAS_SET_VAL(APP_BAS_MAX_BATTERY),
                .battery_charge_state = APP_BAS_SET_VAL(APP_BAS_MAX_BATTERY),
                .battery_charge_level = APP_BAS_SET_VAL(APP_BAS_MAX_BATTERY),
                .battery_charge_type = APP_BAS_SET_VAL(APP_BAS_MAX_CHARGE),
                .charging_fault_reason = APP_BAS_SET_VAL(APP_BAS_MAX_CHARGE),
            },
            .identifier = APP_BAS_SET_VAL(APP_BAS_MAX_U16),
            .battery_level = APP_BAS_SET_VAL(APP_BAS_MAX_LEVEL),
            .additional_status = {
                .battery_fault = APP_BAS_SET_VAL(APP_BAS_FLAGS),
                .service_required = APP_BAS_SET_VAL(APP_BAS_MAX_BATTERY),
            },
        };

        rc = esp_ble_bas_set_level_status(&level_status);
    }
    break;
    case 3: {
        uint32_t estimated_date = APP_BAS_SET_VAL(APP_BAS_MAX_U24);
        rc = esp_ble_bas_set_estimated_date(&estimated_date);
    }
    break;
    case 4: {
        esp_ble_bas_critical_status_t critical_status = {
            .status = {
                .critical_power_state = APP_BAS_SET_VAL(APP_BAS_FLAGS),
                .immediate_service = APP_BAS_SET_VAL(APP_BAS_FLAGS),
            },
        };
        rc = esp_ble_bas_set_critical_status(&critical_status);
    }
    break;
    case 5: {
        esp_ble_bas_energy_status_t energy_status = {
            .flags = {
                .en_available_battery_capacity = APP_BAS_SET_VAL(APP_BAS_FLAGS),
                .en_available_energy = APP_BAS_SET_VAL(APP_BAS_FLAGS),
                .en_available_energy_last_charge = APP_BAS_SET_VAL(APP_BAS_FLAGS),
                .en_charge_rate = APP_BAS_SET_VAL(APP_BAS_FLAGS),
                .en_external_source_power = APP_BAS_SET_VAL(APP_BAS_FLAGS),
                .en_voltage = APP_BAS_SET_VAL(APP_BAS_FLAGS),
            },
            .available_battery_capacity = APP_BAS_SET_VAL(APP_BAS_MAX_U16),
            .available_energy = APP_BAS_SET_VAL(APP_BAS_MAX_U16),
            .available_energy_last_charge = APP_BAS_SET_VAL(APP_BAS_MAX_U16),
            .charge_rate = APP_BAS_SET_VAL(APP_BAS_MAX_U16),
            .external_source_power = APP_BAS_SET_VAL(APP_BAS_MAX_U16),
            .voltage = APP_BAS_SET_VAL(APP_BAS_MAX_VOLTAGE),
        };

        rc = esp_ble_bas_set_energy_status(&energy_status);
    }
    break;
    case 6: {
        esp_ble_bas_time_status_t time_status = {
            .flags = {
                .en_discharged_standby = APP_BAS_SET_VAL(APP_BAS_FLAGS),
                .en_recharged = APP_BAS_SET_VAL(APP_BAS_FLAGS),
            },
            .discharged.u24 = APP_BAS_SET_VAL(APP_BAS_MAX_U24),
                       .discharged_standby.u24 = APP_BAS_SET_VAL(APP_BAS_MAX_U24),
                       .recharged.u24 = APP_BAS_SET_VAL(APP_BAS_MAX_U24),
        };

        rc = esp_ble_bas_set_time_status(&time_status);
    }
    break;
    case 7: {
        esp_ble_bas_health_status_t health_status = {
            .flags = {
                .en_battery_health_summary = APP_BAS_SET_VAL(APP_BAS_FLAGS),
                .en_current_temperature = APP_BAS_SET_VAL(APP_BAS_FLAGS),
                .en_cycle_count = APP_BAS_SET_VAL(APP_BAS_FLAGS),
                .en_deep_discharge_count = APP_BAS_SET_VAL(APP_BAS_FLAGS),
            },
            .battery_health_summary = APP_BAS_SET_VAL(APP_BAS_MAX_LEVEL),
            .current_temperature = APP_BAS_SET_VAL(APP_BAS_MAX_L08),
            .cycle_count = APP_BAS_SET_VAL(APP_BAS_MAX_U16),
            .deep_discharge_count = APP_BAS_SET_VAL(APP_BAS_MAX_U16),
        };

        rc = esp_ble_bas_set_health_status(&health_status);
    }
    break;
    case 8: {
        esp_ble_bas_health_info_t health_info = {
            .flags = {
                .en_cycle_count_designed_lifetime = APP_BAS_SET_VAL(APP_BAS_FLAGS),
                .min_max_designed_operating_temperature = APP_BAS_SET_VAL(APP_BAS_FLAGS),
            },
            .cycle_count_designed_lifetime = APP_BAS_SET_VAL(APP_BAS_MAX_U16),
            .min_designed_operating_temperature = APP_BAS_SET_VAL(APP_BAS_MAX_L08),
            .max_designed_operating_temperature = APP_BAS_SET_VAL(APP_BAS_MAX_L08),
        };

        rc = esp_ble_bas_set_health_info(&health_info);
    }
    break;
    case 9: {
        esp_ble_bas_battery_info_t battery_info = {
            .flags = {
                .en_aggregation_group = APP_BAS_SET_VAL(APP_BAS_FLAGS),
                .en_chemistry = APP_BAS_SET_VAL(APP_BAS_FLAGS),
                .en_critical_energy = APP_BAS_SET_VAL(APP_BAS_FLAGS),
                .en_designed_capacity = APP_BAS_SET_VAL(APP_BAS_FLAGS),
                .en_expiration_date = APP_BAS_SET_VAL(APP_BAS_FLAGS),
                .en_low_energy = APP_BAS_SET_VAL(APP_BAS_FLAGS),
                .en_manufacture_date = APP_BAS_SET_VAL(APP_BAS_FLAGS),
                .en_nominalvoltage = APP_BAS_SET_VAL(APP_BAS_FLAGS),
            },

            .features = {
                .recharge_able = APP_BAS_SET_VAL(APP_BAS_FLAGS),
                .replace_able = APP_BAS_SET_VAL(APP_BAS_FLAGS),
            },

            .aggregation_group = APP_BAS_SET_VAL(APP_BAS_MAX_U08),
            .chemistry = APP_BAS_SET_VAL(APP_BAS_MAX_U08),
            .critical_energy = APP_BAS_SET_VAL(APP_BAS_MAX_U16),
            .designed_capacity = APP_BAS_SET_VAL(APP_BAS_MAX_U16),
            .expiration_date.u24 = APP_BAS_SET_VAL(APP_BAS_MAX_U24),
                            .low_energy = APP_BAS_SET_VAL(APP_BAS_MAX_U16),
                            .manufacture_date.u24 = APP_BAS_SET_VAL(APP_BAS_MAX_U24),
                            .nominalvoltage = APP_BAS_SET_VAL(APP_BAS_MAX_U16),
        };

        rc = esp_ble_bas_set_battery_info(&battery_info);
    }
    break;
    default:
        break;
    }

    if (rc) {
        ESP_LOGE(TAG, "Set characteristic %d, fail %s", type, esp_err_to_name(rc));
    }

    return rc;
}

static struct {
    struct arg_int *type;
    struct arg_int *characteristic;
    struct arg_end *end;
} svc_bas_args;

static int app_bas_func(int argc, char **argv)
{
    int nerrors = arg_parse(argc, argv, (void **) &svc_bas_args);
    if (nerrors != 0) {
        arg_print_errors(stderr, svc_bas_args.end, argv[0]);
        return 1;
    }

    if (!svc_bas_args.type->count) {
        return 1;
    };

    if (!svc_bas_args.characteristic->count) {
        return 1;
    };

    if (svc_bas_args.type->ival[0]) {
        nerrors = app_get_bas(svc_bas_args.characteristic->ival[0]);
    } else {
        nerrors = app_set_bas(svc_bas_args.characteristic->ival[0]);
    }

    return nerrors;
}

void app_bas_register(void)
{
    svc_bas_args.type = arg_int0("t", "type", "<00~01>", "00 Set, 01 Get");
    svc_bas_args.characteristic = arg_int0("c", "characteristic", "<01~09>", "01 Battery Level\n    \
                            02 Battery Level Status\n  \
                              03 Estimated Service Date\n    \
                            04 Battery Critical Status\n   \
                             05 Battery Energy Status\n \
                               06 Battery Time Status\n   \
                             07 Battery Health Status\n \
                               08 Battery Health Information\n    \
                            09 Battery Information\n");
    svc_bas_args.end = arg_end(2);

    const esp_console_cmd_t svc_bas_cmd = {
        .command = "bas",
        .help = "Battery Service",
        .hint = NULL,
        .func = &app_bas_func,
        .argtable = &svc_bas_args
    };

    ESP_ERROR_CHECK(esp_console_cmd_register(&svc_bas_cmd));
}
