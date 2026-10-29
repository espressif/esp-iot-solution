/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <sys/time.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "hal/gpio_ll.h"
#include "bl0937.h"

#define TAG         "BL0937"

#ifdef CONFIG_BL0937_IRAM_OPTIMIZED
#define IRAM_OPT IRAM_ATTR
#else
#define IRAM_OPT
#endif

/**
 * @brief Device structure for BL0937 driver
 */
typedef struct {
    // Configuration parameters
    float sensor_current_resistor;
    float sensor_voltage_resistor;
    float vref;
    uint8_t current_mode;
    volatile uint8_t mode;
    uint8_t setpin_io;
    uint32_t pulse_timeout_us;

    // Multipliers
    volatile float sensor_current_multiplier; // Unit: us/A
    volatile float sensor_voltage_multiplier; // Unit: us/V
    volatile float sensor_power_multiplier;   // Unit: us/W

    // Interrupt timing
    volatile uint64_t last_cf_interrupt;
    volatile uint64_t last_cf1_interrupt;
    volatile uint64_t first_cf1_interrupt;

    // Pulse measurements
    volatile uint32_t sensor_voltage_pulse_width; //Unit: us
    volatile uint32_t sensor_current_pulse_width; //Unit: us
    volatile uint32_t sensor_power_pulse_width;   //Unit: us
    volatile uint32_t sensor_pulse_wave_counts;

    // Current measurements
    float sensor_current;
    float sensor_voltage;
    float sensor_power;
} bl0937_dev_t;

void IRAM_OPT reset_energe(bl0937_handle_t handle)
{
    bl0937_dev_t *dev = (bl0937_dev_t *)handle;
    dev->sensor_pulse_wave_counts = 0;
}

static void IRAM_OPT bl0937_cf_isr_handler(void* arg)
{
    bl0937_dev_t *dev = (bl0937_dev_t *)arg;
    uint64_t now = esp_timer_get_time();
    dev->sensor_power_pulse_width = now - dev->last_cf_interrupt;
    dev->last_cf_interrupt = now;
    dev->sensor_pulse_wave_counts++;
}

static void IRAM_OPT bl0937_cf1_isr_handler(void* arg)
{
    bl0937_dev_t *dev = (bl0937_dev_t *)arg;
    uint64_t now = esp_timer_get_time();

    if ((now - dev->first_cf1_interrupt) > dev->pulse_timeout_us) {
        uint32_t pulse_width;

        if (dev->last_cf1_interrupt == dev->first_cf1_interrupt) {
            pulse_width = 0;
        } else {
            pulse_width = now - dev->last_cf1_interrupt;
        }

        if (dev->mode == dev->current_mode) {
            dev->sensor_current_pulse_width = pulse_width;
        } else {
            dev->sensor_voltage_pulse_width = pulse_width;
        }

        dev->mode = 1 - dev->mode;
        gpio_ll_set_level(&GPIO, dev->setpin_io, dev->mode);
        dev->first_cf1_interrupt = now;
    }
    dev->last_cf1_interrupt = now;
}

/**
 * @brief DefaultMultipliers:
 *        For power a frequency of 1Hz means around 12W
 *        For current a frequency of 1Hz means around 15mA
 *        For voltage a frequency of 1Hz means around 0.5V
 */
static void IRAM_OPT calculate_default_multipliers(bl0937_dev_t *dev)
{
    dev->sensor_power_multiplier = (50850000.0 * dev->vref * dev->vref * dev->sensor_voltage_resistor / dev->sensor_current_resistor / 48.0 / F_OSC_BL0937) / 1.1371681416f;      //15102450
    dev->sensor_voltage_multiplier = (221380000.0 * dev->vref * dev->sensor_voltage_resistor /  2.0 / F_OSC_BL0937) / 1.0474137931f;  //221384120,171674
    dev->sensor_current_multiplier = (531500000.0 * dev->vref / dev->sensor_current_resistor / 24.0 / F_OSC_BL0937) / 1.166666f;  //
}

esp_err_t bl0937_create(bl0937_handle_t *handle, driver_bl0937_config_t *config)
{
    if (!handle || !config) {
        return ESP_ERR_INVALID_ARG;
    }

    bl0937_dev_t *dev = malloc(sizeof(bl0937_dev_t));
    if (dev == NULL) {
        ESP_LOGE(TAG, "Failed to allocate memory for BL0937 device");
        return ESP_ERR_NO_MEM;
    }
    memset(dev, 0, sizeof(bl0937_dev_t));

    esp_err_t ret;

    // Initialize device structure
    dev->sensor_voltage_resistor = config->divider_resistor;
    dev->sensor_current_resistor = config->sampling_resistor;
    dev->current_mode = config->pin_mode;
    dev->vref = V_REF_BL0937;
    dev->setpin_io = config->sel_gpio;
    dev->pulse_timeout_us = PULSE_TIMEOUT_US;

    // Initialize pulse measurements
    dev->sensor_voltage_pulse_width = 0;
    dev->sensor_current_pulse_width = 0;
    dev->sensor_power_pulse_width = 0;
    dev->sensor_pulse_wave_counts = 0;

    // Initialize measurements
    dev->sensor_current = 0;
    dev->sensor_voltage = 0;
    dev->sensor_power = 0;

    gpio_ll_intr_disable(&GPIO, config->cf_gpio);
    gpio_ll_intr_disable(&GPIO, config->cf1_gpio);

    gpio_config_t io_conf;
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pin_bit_mask = (1ULL << config->sel_gpio);
    io_conf.pull_down_en = 0;
    io_conf.pull_up_en = 0;
    ret = gpio_config(&io_conf);
    if (ret != ESP_OK) {
        free(dev);
        return ret;
    }

    // interrupt of rising edge
    io_conf.intr_type = GPIO_INTR_POSEDGE;
    io_conf.pin_bit_mask = ((1ULL << config->cf1_gpio) | (1ULL << config->cf_gpio));
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pull_up_en = 1;
    ret = gpio_config(&io_conf);
    if (ret != ESP_OK) {
        free(dev);
        return ret;
    }

#ifdef CONFIG_BL0937_IRAM_OPTIMIZED
    gpio_install_isr_service(ESP_INTR_FLAG_IRAM);
#else
    gpio_install_isr_service(0);
#endif

    gpio_isr_handler_add(config->cf_gpio, bl0937_cf_isr_handler, dev);
    gpio_isr_handler_add(config->cf1_gpio, bl0937_cf1_isr_handler, dev);

    calculate_default_multipliers(dev);
    dev->mode = dev->current_mode;

    gpio_ll_set_level(&GPIO, config->sel_gpio, dev->mode);

    gpio_ll_intr_enable_on_core(&GPIO, 0, config->cf_gpio);
    gpio_ll_intr_enable_on_core(&GPIO, 0, config->cf1_gpio);

    *handle = (bl0937_handle_t)dev;
    return ESP_OK;
}

esp_err_t bl0937_delete(bl0937_handle_t handle)
{
    if (!handle) {
        return ESP_ERR_INVALID_ARG;
    }
    free((bl0937_dev_t *)handle);
    return ESP_OK;
}

float IRAM_OPT bl0937_get_current_multiplier(bl0937_handle_t handle)
{
    bl0937_dev_t *dev = (bl0937_dev_t *)handle;
    return dev->sensor_current_multiplier;
}

float IRAM_OPT bl0937_get_voltage_multiplier(bl0937_handle_t handle)
{
    bl0937_dev_t *dev = (bl0937_dev_t *)handle;
    return dev->sensor_voltage_multiplier;
}

float IRAM_OPT bl0937_get_power_multiplier(bl0937_handle_t handle)
{
    bl0937_dev_t *dev = (bl0937_dev_t *)handle;
    return dev->sensor_power_multiplier;
}

void IRAM_OPT bl0937_set_current_multiplier(bl0937_handle_t handle, float current_multiplier)
{
    bl0937_dev_t *dev = (bl0937_dev_t *)handle;
    dev->sensor_current_multiplier = current_multiplier;
}

void IRAM_OPT bl0937_set_voltage_multiplier(bl0937_handle_t handle, float voltage_multiplier)
{
    bl0937_dev_t *dev = (bl0937_dev_t *)handle;
    dev->sensor_voltage_multiplier = voltage_multiplier;
}

void IRAM_OPT bl0937_set_power_multiplier(bl0937_handle_t handle, float power_multiplier)
{
    bl0937_dev_t *dev = (bl0937_dev_t *)handle;
    dev->sensor_power_multiplier = power_multiplier;
}

void IRAM_OPT bl0937_setmode(bl0937_handle_t handle, bl0937_mode_t mode_type)
{
    bl0937_dev_t *dev = (bl0937_dev_t *)handle;
    uint8_t gpio_level = (mode_type == MODE_CURRENT) ? dev->current_mode : 1 - dev->current_mode;
    gpio_ll_set_level(&GPIO, dev->setpin_io, gpio_level);
    dev->mode = gpio_level;

    dev->last_cf1_interrupt = dev->first_cf1_interrupt = esp_timer_get_time();
}

bl0937_mode_t IRAM_OPT bl0937_getmode(bl0937_handle_t handle)
{
    bl0937_dev_t *dev = (bl0937_dev_t *)handle;
    return (dev->mode == dev->current_mode) ? MODE_CURRENT : MODE_VOLTAGE;
}

bl0937_mode_t IRAM_OPT bl0937_togglemode(bl0937_handle_t handle)
{
    bl0937_mode_t new_mode = bl0937_getmode(handle) == MODE_CURRENT ? MODE_VOLTAGE : MODE_CURRENT;
    bl0937_setmode(handle, new_mode);

    return new_mode;
}

float IRAM_OPT bl0937_get_energy(bl0937_handle_t handle)
{
    bl0937_dev_t *dev = (bl0937_dev_t *)handle;
    return dev->sensor_pulse_wave_counts * dev->sensor_power_multiplier / 1000000.;
}

void IRAM_OPT bl0937_checkcfsignal(bl0937_handle_t handle)
{
    bl0937_dev_t *dev = (bl0937_dev_t *)handle;
    if ((esp_timer_get_time() - dev->last_cf_interrupt) > dev->pulse_timeout_us) {
        dev->sensor_power_pulse_width = 0;
    }
}

void IRAM_OPT bl0937_checkcf1signal(bl0937_handle_t handle)
{
    bl0937_dev_t *dev = (bl0937_dev_t *)handle;
    if ((esp_timer_get_time() - dev->last_cf1_interrupt) > dev->pulse_timeout_us) {
        if (dev->mode == dev->current_mode) {
            dev->sensor_current_pulse_width = 0;
        } else {
            dev->sensor_voltage_pulse_width = 0;
        }
        bl0937_togglemode(handle);
    }
}

float IRAM_OPT bl0937_get_voltage(bl0937_handle_t handle)
{
    bl0937_dev_t *dev = (bl0937_dev_t *)handle;
    bl0937_checkcf1signal(handle);

    dev->sensor_voltage = (dev->sensor_voltage_pulse_width > 0) ? dev->sensor_voltage_multiplier / dev->sensor_voltage_pulse_width : 0;

    return dev->sensor_voltage;
}

void bl0937_multiplier_init(bl0937_handle_t handle)
{
    bl0937_dev_t *dev = (bl0937_dev_t *)handle;
    calculate_default_multipliers(dev);
}

void IRAM_OPT bl0937_expected_voltage(bl0937_handle_t handle, float value)
{
    bl0937_dev_t *dev = (bl0937_dev_t *)handle;
    if (dev->sensor_voltage == 0) {
        bl0937_get_voltage(handle);
    }

    if (dev->sensor_voltage > 0) {
        dev->sensor_voltage_multiplier *= ((float) value / dev->sensor_voltage);
    }
}

void IRAM_OPT bl0937_expected_current(bl0937_handle_t handle, float value)
{
    bl0937_dev_t *dev = (bl0937_dev_t *)handle;
    if (dev->sensor_current == 0) {
        bl0937_get_current(handle);
    }

    if (dev->sensor_current > 0) {
        dev->sensor_current_multiplier *= (value / dev->sensor_current);
    }
}

void IRAM_OPT bl0937_expected_active_power(bl0937_handle_t handle, float value)
{
    bl0937_dev_t *dev = (bl0937_dev_t *)handle;
    if (dev->sensor_power == 0) {
        bl0937_get_active_power(handle);
    }

    if (dev->sensor_power > 0) {
        dev->sensor_power_multiplier *= ((float) value / dev->sensor_power);
    }
}

float IRAM_OPT bl0937_get_active_power(bl0937_handle_t handle)
{
    bl0937_dev_t *dev = (bl0937_dev_t *)handle;
    bl0937_checkcfsignal(handle);

    dev->sensor_power = (dev->sensor_power_pulse_width > 0) ? dev->sensor_power_multiplier / dev->sensor_power_pulse_width : 0;

    if (dev->sensor_power_pulse_width > 200000 && dev->sensor_power < 0.1f) {
        return 0.0f;
    }

    return dev->sensor_power;
}

/*
    Power measurements are more sensitive to switch offs,
    so we first check if power is 0 to set _current to 0 too
*/
float IRAM_OPT bl0937_get_current(bl0937_handle_t handle)
{
    bl0937_dev_t *dev = (bl0937_dev_t *)handle;
    bl0937_get_active_power(handle);

    if (dev->sensor_power == 0) {
        dev->sensor_current_pulse_width = 0;
    } else {
        bl0937_checkcf1signal(handle);
    }
    dev->sensor_current = (dev->sensor_current_pulse_width > 0) ? dev->sensor_current_multiplier / dev->sensor_current_pulse_width : 0;

    return dev->sensor_current;
}

float IRAM_OPT bl0937_get_apparent_power(bl0937_handle_t handle)
{
    float current = bl0937_get_current(handle);
    float voltage = bl0937_get_voltage(handle);

    return voltage * current;
}

/**
 * @brief Reset multipliers to their default calculated values
 */
void IRAM_OPT bl0937_reset_multipliers(bl0937_handle_t handle)
{
    bl0937_dev_t *dev = (bl0937_dev_t *)handle;
    calculate_default_multipliers(dev);
    ESP_LOGI("BL0937", "Multipliers reset to defaults: V=%.0f, I=%.0f, P=%.0f",
             dev->sensor_voltage_multiplier, dev->sensor_current_multiplier, dev->sensor_power_multiplier);
}

float IRAM_OPT bl0937_get_power_factor(bl0937_handle_t handle)
{
    float active = bl0937_get_active_power(handle);
    float apparent = bl0937_get_apparent_power(handle);

    if (active > apparent) {
        return 1;
    }

    if (apparent == 0) {
        return 0;
    }

    return (float) active / apparent;
}
