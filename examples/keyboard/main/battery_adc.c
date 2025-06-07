/*
 * SPDX-FileCopyrightText: 2024-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <driver/adc.h>

#include "battery_adc.h"

#include "ble_hid.h"
#include <esp_log.h>
#include <esp_adc/adc_oneshot.h>

#include "bsp/esp-bsp.h"

int g_battery_percent;

static const char *TAG = "BATTERY-ADC";

static adc_oneshot_unit_handle_t adc1_handle;
static TaskHandle_t battery_task_handle = NULL;

static void battery_monitor_task(void *pvParameters);

/* Pull up resistor value. Unit: KOhm */
#define PULL_UP_RESISTOR    (51)
/* Pull down resistor value. Unit: KOhm */
#define PULL_DOWN_RESISTOR  (100)
/* Max voltage value. Unit: mV */
#define MAX_REF_VOLTAGE     (3469)

esp_err_t battery_adc_init(void)
{
    if (battery_task_handle != NULL) {
        ESP_LOGW(TAG, "Battery ADC already initialized");
        return ESP_OK;
    }

    /* ADC1 Init */
    adc_oneshot_unit_init_cfg_t unit_init_cfg = {
        .unit_id = ADC_UNIT_1,
        .clk_src = ADC_RTC_CLK_SRC_DEFAULT,
        .ulp_mode = ADC_ULP_MODE_DISABLE
    };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&unit_init_cfg, &adc1_handle));

    /* ADC1 Config */
    adc_oneshot_chan_cfg_t channel_config = {
        /* Theoretical battery voltage measurement range is about 2.45 to 2.78V.
         * So set the input attenuation to 12 dB (about 4x) */
        .atten = ADC_ATTEN_DB_12,
        .bitwidth = ADC_BITWIDTH_12
    };
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc1_handle, KBD_BATTERY_MONITOR_CHANNEL, &channel_config));

    ESP_LOGI(TAG, "Free memory size: %"PRIu32, esp_get_free_heap_size());
    if (xTaskCreate(battery_monitor_task, "battery_monitor", 4096, NULL, 2, &battery_task_handle) != pdPASS) {
        return ESP_ERR_NO_MEM;
    }
    ESP_LOGI(TAG, "Free memory size: %"PRIu32, esp_get_free_heap_size());

    return ESP_OK;
}

static void battery_monitor_task(void *pvParameters)
{
    ESP_UNUSED(pvParameters);
    /* The table of voltage convert percent. */
    const static int ocv_capacity_table[] = {
        3680, 3700, 3740, 3770, 3790, 3820, 3870, 3920, 3980, 4060, 4200
    };

    int adc_raw_value;
    int adc_voltage;
    int battery_voltage;

    int battery_level;

    while (1) {
        adc_oneshot_read(adc1_handle, KBD_BATTERY_MONITOR_CHANNEL, &adc_raw_value);

        /* Get the battery voltage. */
        adc_voltage = MAX_REF_VOLTAGE * adc_raw_value / 4095;
        battery_voltage = adc_voltage * (PULL_UP_RESISTOR + PULL_DOWN_RESISTOR) / PULL_DOWN_RESISTOR;

        /* Get the percent. */
        for (battery_level = 0; battery_level < 11; ++battery_level) {
            if (battery_voltage <= ocv_capacity_table[battery_level]) {
                break;
            }
        }

        if (battery_level == 0) {
            /* Low battery. */
            g_battery_percent = 0;
        } else if (battery_level >= 11) {
            /* Over voltage. */
            g_battery_percent = 100;
        } else {
            g_battery_percent = (battery_level - 1) * 10;
            g_battery_percent += ((battery_voltage - ocv_capacity_table[battery_level]) * 10 / (
                                      ocv_capacity_table[battery_level] - ocv_capacity_table[battery_level - 1]));
        }

        ble_hid_battery_report(g_battery_percent);

        /* Sample period. */
        vTaskDelay(pdMS_TO_TICKS(10000));
    }
}
