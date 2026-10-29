/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "hid_transport.h"

#include "sdkconfig.h"
#include "esp_check.h"
#include "esp_log.h"
#include "esp_pm.h"

#include "ble_hid.h"
#include "settings.h"
#include "tinyusb_hid.h"

static const char *TAG = "hid_transport";

static btn_report_type_t s_active = TINYUSB_HID_REPORT;

esp_err_t hid_transport_configure_power(btn_report_type_t type)
{
    esp_pm_config_t pm_config = {
        .max_freq_mhz = CONFIG_ESP_MAX_CPU_FREQ_MHZ,
        .min_freq_mhz = CONFIG_ESP_MIN_CPU_FREQ_MHZ,
#if CONFIG_FREERTOS_USE_TICKLESS_IDLE
        .light_sleep_enable = (type == BLE_HID_REPORT),
#endif
    };

    return esp_pm_configure(&pm_config);
}

esp_err_t hid_transport_init(void)
{
    const sys_param_t *p = settings_get_parameter();
    ESP_RETURN_ON_ERROR(hid_transport_configure_power(p->report_type), TAG, "configure power failed");

    switch (p->report_type) {
    case TINYUSB_HID_REPORT: {
        esp_err_t ret = tinyusb_hid_init();
        if (ret != ESP_OK) {
            return ret;
        }
        s_active = TINYUSB_HID_REPORT;
        return ESP_OK;
    }
    case BLE_HID_REPORT: {
        esp_err_t ret = ble_hid_init();
        if (ret != ESP_OK) {
            return ret;
        }
        s_active = BLE_HID_REPORT;
        return ESP_OK;
    }
    default: {
        ESP_LOGW(TAG, "unknown report_type, default USB");
        esp_err_t ret = tinyusb_hid_init();
        if (ret != ESP_OK) {
            return ret;
        }
        s_active = TINYUSB_HID_REPORT;
        return ESP_OK;
    }
    }
}

void hid_transport_deinit(void)
{
    switch (s_active) {
    case TINYUSB_HID_REPORT:
        break;
    case BLE_HID_REPORT:
        ble_hid_deinit();
        break;
    default:
        break;
    }
}

void hid_transport_keyboard_report(hid_report_t report)
{
    switch (s_active) {
    case TINYUSB_HID_REPORT:
        tinyusb_hid_keyboard_report(report);
        break;
    case BLE_HID_REPORT:
        ble_hid_keyboard_report(report);
        break;
    default:
        break;
    }
}

void hid_transport_battery_report(int battery_level)
{
    switch (s_active) {
    case BLE_HID_REPORT:
        ble_hid_battery_report(battery_level);
        break;
    default:
        break;
    }
}

bool hid_transport_needs_ble_style_pm(void)
{
    return settings_get_parameter()->report_type == BLE_HID_REPORT;
}
