/* SPDX-FileCopyrightText: 2022-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_log.h"

#include "nvs_flash.h"
#include "nvs.h"
#include "driver/gpio.h"
#include "power_measure.h"

#define BL0937_CF_GPIO   GPIO_NUM_3  // CF  pin
#define BL0937_SEL_GPIO  GPIO_NUM_4  // SEL pin
#define BL0937_CF1_GPIO  GPIO_NUM_7  // CF1 pin

static const char *TAG = "PowerMeasureExample";

// Default calibration factors
#define DEFAULT_KI 1.0f
#define DEFAULT_KU 1.0f
#define DEFAULT_KP 1.0f

void app_main(void)
{
    // Get calibration factors
    calibration_factor_t factor;
    power_measure_get_calibration_factor(&factor);

    // Initialize the power measure configuration
    power_measure_init_config_t config = {
        .chip_config = {
            .type = CHIP_BL0937,               // Set the chip type to BL0937
            .sel_gpio = BL0937_SEL_GPIO,       // GPIO for SEL pin
            .cf1_gpio = BL0937_CF1_GPIO,       // GPIO for CF1 pin
            .cf_gpio = BL0937_CF_GPIO,         // GPIO for CF pin
            .sampling_resistor = 0.001f,       // Sampling resistor value
            .divider_resistor = 1981.0f,       // Divider resistor value
            .factor = factor                   // Load calibration factor into chip_config
        },
        .overcurrent = 15,                     // Set overcurrent threshold
        .overvoltage = 260,                    // Set overvoltage threshold
        .undervoltage = 180,                   // Set undervoltage threshold
        .enable_energy_detection = true        // Enable energy detection
    };

    // Initialize the power measure component
    esp_err_t ret = power_measure_init(&config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize power measurement: %s", esp_err_to_name(ret));
        return;
    }

    while (1) {
        float voltage = 0.0f, current = 0.0f, power = 0.0f, energy = 0.0f;

        // Fetch the voltage measurement
        ret = power_measure_get_voltage(&voltage);
        if (ret == ESP_OK) {
            ESP_LOGI(TAG, "Voltage: %.2f V", voltage);
        } else {
            ESP_LOGE(TAG, "Failed to get voltage: %s", esp_err_to_name(ret));
        }

        // Fetch the current measurement
        ret = power_measure_get_current(&current);
        if (ret == ESP_OK) {
            ESP_LOGI(TAG, "Current: %.2f A", current);
        } else {
            ESP_LOGE(TAG, "Failed to get current: %s", esp_err_to_name(ret));
        }

        // Fetch the active power measurement
        ret = power_measure_get_active_power(&power);
        if (ret == ESP_OK) {
            ESP_LOGI(TAG, "Power: %.2f W", power);
        } else {
            ESP_LOGE(TAG, "Failed to get active power: %s", esp_err_to_name(ret));
        }

        // Fetch the energy measurement
        ret = power_measure_get_energy(&energy);
        if (ret == ESP_OK) {
            ESP_LOGI(TAG, "Energy: %.2f Kw/h", energy);
        } else {
            ESP_LOGE(TAG, "Failed to get energy: %s", esp_err_to_name(ret));
        }

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
    power_measure_deinit();
}
