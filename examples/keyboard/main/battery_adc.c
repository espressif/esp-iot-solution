/*
 * SPDX-FileCopyrightText: 2024-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <esp_log.h>
#include <esp_adc/adc_oneshot.h>
#include "ble_hid.h"
#include "bsp/esp-bsp.h"
#include "battery_adc.h"
#include "adc_battery_estimation.h"

static const char *TAG = "BATTERY-ADC";

static TaskHandle_t battery_monitor_task_handle;
static void battery_monitor_task(void *pvParameters);

/* Pull up resistor value. Unit: KOhm */
#define PULL_UP_RESISTOR    (51)
/* Pull down resistor value. Unit: KOhm */
#define PULL_DOWN_RESISTOR  (100)

esp_err_t battery_adc_init(void)
{
    if (battery_monitor_task_handle != NULL) {
        ESP_LOGW(TAG, "Battery monitor task already created! ");
        return ESP_OK;
    }

    if (xTaskCreate(battery_monitor_task, "battery_monitor", 4096, NULL, 2, &battery_monitor_task_handle) != pdPASS) {
        return ESP_ERR_NO_MEM;
    }

    return ESP_OK;
}

static void battery_monitor_task(void *pvParameters)
{
    ESP_UNUSED(pvParameters);
    adc_battery_estimation_t config = {
        .internal = {
            .adc_unit = KBD_BATTERY_MONITOR_ADC_UNIT,
            .adc_bitwidth = ADC_BITWIDTH_DEFAULT,
            /* Theoretical battery voltage measurement range is about 2.45 to 2.78V.
             * So set the input attenuation to 12 dB (about 4x) */
            .adc_atten = ADC_ATTEN_DB_12
        },
        .adc_channel = KBD_BATTERY_MONITOR_CHANNEL,
        .lower_resistor = PULL_DOWN_RESISTOR,
        .upper_resistor = PULL_UP_RESISTOR,
    };

    adc_battery_estimation_handle_t adc_battery_estimation_handle = adc_battery_estimation_create(&config);
    if (adc_battery_estimation_handle == NULL) {
        ESP_LOGE(TAG, "Initialize adc battery estimation failed! ");
        vTaskDelete(battery_monitor_task_handle);
        vTaskDelay(portMAX_DELAY);
    }

    float capacity = 0.0f;

    while (1) {
        adc_battery_estimation_get_capacity(adc_battery_estimation_handle, &capacity);
        ble_hid_battery_report((int) capacity);

        /* Sample period. */
        vTaskDelay(pdMS_TO_TICKS(10000));
    }
}
