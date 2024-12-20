/* SPDX-FileCopyrightText: 2022-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include <sys/param.h>
#include <sys/time.h>
#include "math.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"

#include "esp_log.h"
#include "esp_timer.h"
#include "esp_check.h"

#include "priv_include/esp_bl0937.h"
#include "power_measure.h"

static const char* TAG = "power_measure";

#define TIMER_UPDATE_MS (1000 * 1000U)
#define CALIBRATION_TIMEOUT_S 5
#define SAVE_INTERVAL 0.1
#define TIME_TO_REPORT_S 28800

typedef struct {
    float current;
    float voltage;
    float power;
    float energy;
    float power_factor;
    float accumulated_energy;
} power_measure_live_value_group_t;

static power_measure_live_value_group_t* g_value = NULL;
static power_measure_init_config_t* g_config = NULL;
static TaskHandle_t g_task_handle = NULL;
static SemaphoreHandle_t xSemaphore = NULL;
static float accumulated_value = 0;
static bool g_init_done = true;
static float start_value = 0;

static void update_value(void)
{
    if (g_config->chip_config.type == CHIP_BL0937) {
        if (g_config->enable_energy_detection == true) {
            g_value->energy = bl0937_get_energy() / 3600000; // Unit: KW/h
            accumulated_value = g_value->energy;
        }
        g_value->current = bl0937_get_current();
        g_value->voltage = bl0937_get_voltage();
        g_value->power = bl0937_get_active_power();
        g_value->power_factor = bl0937_get_power_factor();
    }
}

static void clear_up(void)
{
    if (g_config) {
        free(g_config);
        g_config = NULL;
    }
    if (g_value) {
        free(g_value);
        g_value = NULL;
    }
    if (g_task_handle) {
        vTaskDelete(g_task_handle);
        g_task_handle = NULL;
    }
}

static void power_measure_task(void* arg)
{
    update_value();

    float energyNow = 0, energyLast = start_value;
    time_t tsNow = 0, tsLast = 0;
    xSemaphore = xSemaphoreCreateMutex();
    while (1) {
        vTaskDelay(1000 / portTICK_PERIOD_MS);
        update_value();

        energyNow = accumulated_value + start_value;
        if (fabs(energyNow - energyLast) > SAVE_INTERVAL) {
            xSemaphoreTake(xSemaphore, portMAX_DELAY);
            energyLast = energyNow;
            xSemaphoreGive(xSemaphore);
        }

        time(&tsNow);
        if (llabs(tsNow - tsLast) >= TIME_TO_REPORT_S) {
            tsLast = tsNow;
            /* reset pulse count and start value */
            power_measure_start_energy_calculation();
        }
    }
    vTaskDelete(NULL);
}

esp_err_t power_measure_init(power_measure_init_config_t* config)
{
    ESP_LOGI(TAG, "Power_Measure Version: %d.%d.%d", POWER_MEASURE_VER_MAJOR, POWER_MEASURE_VER_MINOR, POWER_MEASURE_VER_PATCH);
    esp_err_t err = ESP_FAIL;
    esp_err_t ret = ESP_OK;

    ESP_RETURN_ON_ERROR(config == NULL, TAG, "Invalid argument");
    ESP_RETURN_ON_ERROR(g_config != NULL, TAG, "Has been initialized.");

    g_config = calloc(1, sizeof(power_measure_init_config_t));
    ESP_RETURN_ON_ERROR(g_config == NULL, TAG, "calloc fail");

    g_value = calloc(1, sizeof(power_measure_live_value_group_t));
    ESP_RETURN_ON_ERROR(g_value == NULL, TAG, "calloc fail");

    memcpy(&g_config->chip_config, &config->chip_config, sizeof(power_measure_init_config_t));
    /* driver init */
    ESP_GOTO_ON_ERROR(g_config->chip_config.type >= CHIP_MAX, EXIT, TAG, "Unsupported chip type");
    if (config->chip_config.type == CHIP_BL0937) {
        err = bl0937_init(g_config->chip_config);
    }
    ESP_GOTO_ON_ERROR(err != ESP_OK, EXIT, TAG, "init fail");

    BaseType_t ret_pd = xTaskCreate(power_measure_task, "power_measure_task", 4096, NULL, 5, &g_task_handle);
    ESP_GOTO_ON_ERROR(ret_pd != pdTRUE, EXIT, TAG, "init fail");

    g_init_done = true;
    ESP_LOGI(TAG, "Power measure initialization done.");
    return ret;

EXIT:
    clear_up();
    return ESP_FAIL;
}

esp_err_t power_measure_deinit(void)
{
    ESP_LOGW(TAG, "Measure DeInit.");
    clear_up();
    g_init_done = false;
    return ESP_OK;
}

esp_err_t power_measure_get_voltage(float* voltage)
{
    ESP_RETURN_ON_ERROR(!g_init_done, TAG, "power measure deinit");
    ESP_RETURN_ON_ERROR(g_config == NULL || voltage == NULL, TAG, "Invalid argument");
    *voltage = g_value->voltage;
    return ESP_OK;
}

esp_err_t power_measure_get_current(float* current)
{
    ESP_RETURN_ON_ERROR(!g_init_done, TAG, "power measure deinit");
    ESP_RETURN_ON_ERROR(g_config == NULL || current == NULL, TAG, "Invalid argument");
    *current = g_value->current;
    return ESP_OK;
}

esp_err_t power_measure_get_active_power(float* active_power)
{
    ESP_RETURN_ON_ERROR(!g_init_done, TAG, "power measure deinit");
    ESP_RETURN_ON_ERROR(g_config == NULL || active_power == NULL, TAG, "Invalid argument");
    *active_power = g_value->power;
    return ESP_OK;
}

esp_err_t power_measure_get_power_factor(float* power_factor)
{
    ESP_RETURN_ON_ERROR(!g_init_done, TAG, "power measure deinit");
    ESP_RETURN_ON_ERROR(g_config == NULL || power_factor == NULL, TAG, "Invalid argument");
    *power_factor = g_value->power_factor;
    return ESP_OK;
}

esp_err_t power_measure_get_energy(float* energy)
{
    ESP_RETURN_ON_ERROR(!g_init_done, TAG, "power measure deinit");
    ESP_RETURN_ON_ERROR(g_config == NULL || energy == NULL, TAG, "Invalid argument");
    ESP_RETURN_ON_ERROR(!g_config->enable_energy_detection, TAG, "Invalid state");
    *energy = accumulated_value + start_value;
    return ESP_OK;
}

esp_err_t power_measure_start_energy_calculation(void)
{
    ESP_RETURN_ON_ERROR(!g_init_done, TAG, "power measure deinit");
    esp_err_t ret = ESP_OK;
    reset_energe();
    g_config->enable_energy_detection = true;
    return ret;
}

esp_err_t power_measure_stop_energy_calculation(void)
{
    ESP_RETURN_ON_ERROR(!g_init_done, TAG, "power measure deinit");
    g_config->enable_energy_detection = false;
    return ESP_OK;
}

esp_err_t power_measure_reset_energy_calculation(void)
{
    ESP_RETURN_ON_ERROR(!g_init_done, TAG, "power measure deinit");
    accumulated_value = start_value = 0;
    reset_energe();
    return ESP_OK;
}

esp_err_t power_measure_calibration_factor_reset(void)
{
    esp_err_t ret = ESP_OK;
    bl0937_multiplier_init();
    return ret;
}

esp_err_t power_measure_start_calibration(calibration_parameter_t* para)
{
    ESP_RETURN_ON_ERROR(!g_init_done, TAG, "power measure deinit");
    ESP_RETURN_ON_ERROR(para == NULL, TAG, "Invalid argument");
    bl0937_expected_voltage(para->standard_voltage);
    bl0937_expected_current(para->standard_current);
    bl0937_expected_active_power(para->standard_power);
    return ESP_OK;
}

esp_err_t power_measure_get_calibration_factor(calibration_factor_t* factor)
{
    ESP_RETURN_ON_ERROR(!g_init_done, TAG, "power measure deinit");
    ESP_RETURN_ON_ERROR(factor == NULL, TAG, "Invalid argument");
    factor->ki = bl0937_get_current_multiplier();
    factor->ku = bl0937_get_voltage_multiplier();
    factor->kp = bl0937_get_power_multiplier();
    return ESP_OK;
}
