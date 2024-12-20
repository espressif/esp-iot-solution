/* SPDX-FileCopyrightText: 2022-2024 Espressif Systems (Shanghai) CO LTD
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
#include "priv_include/esp_bl0937.h"

#define TAG         "BL0937"

float sensor_current_resistor = R_CURRENT_BL0937;
float sensor_voltage_resistor = R_VOLTAGE_BL0937;
float vref = V_REF_BL0937;

uint8_t current_mode;
volatile uint8_t mode;
static uint8_t setpin_io;

volatile float sensor_current_multiplier; // Unit: us/A
volatile float sensor_voltage_multiplier; // Unit: us/V
volatile float sensor_power_multiplier;   // Unit: us/W

volatile uint64_t last_cf_interrupt;
volatile uint64_t last_cf1_interrupt;
volatile uint64_t first_cf1_interrupt;

uint32_t pulse_timeout_us = PULSE_TIMEOUT_US;    //Unit: us
volatile uint32_t sensor_voltage_pulse_width; //Unit: us
volatile uint32_t sensor_current_pulse_width; //Unit: us
volatile uint32_t sensor_power_pulse_width;   //Unit: us
volatile uint32_t sensor_pulse_wave_counts;

float sensor_current;
float sensor_voltage;
float sensor_power;

#ifdef CONFIG_BL0937_IRAM_OPTIMIZED
#define IRAM_OPT IRAM_ATTR
#else
#define IRAM_OPT
#endif

void IRAM_ATTR reset_energe(void)
{
    sensor_pulse_wave_counts = 0;
}

static void IRAM_OPT bl0937_cf_isr_handler(void* arg)
{
    uint64_t now = esp_timer_get_time();
    sensor_power_pulse_width = now - last_cf_interrupt;
    last_cf_interrupt = now;
    sensor_pulse_wave_counts++;
}

static void IRAM_OPT bl0937_cf1_isr_handler(void* arg)
{
    uint64_t now = esp_timer_get_time();

    if ((now - first_cf1_interrupt) > pulse_timeout_us) {
        uint32_t pulse_width;

        if (last_cf1_interrupt == first_cf1_interrupt) {
            pulse_width = 0;
        } else {
            pulse_width = now - last_cf1_interrupt;
        }

        if (mode == current_mode) {
            sensor_current_pulse_width = pulse_width;
        } else {
            sensor_voltage_pulse_width = pulse_width;
        }

        mode = 1 - mode;
        gpio_ll_set_level(&GPIO, setpin_io, mode);
        first_cf1_interrupt = now;
    }
    last_cf1_interrupt = now;
}

/**
 * @brief DefaultMultipliers:
 *        For power a frequency of 1Hz means around 12W
 *        For current a frequency of 1Hz means around 15mA
 *        For voltage a frequency of 1Hz means around 0.5V
 */
static void IRAM_OPT calculatedefaultmultipliers()
{
    sensor_power_multiplier = (50850000.0 * vref * vref * sensor_voltage_resistor / sensor_current_resistor / 48.0 / F_OSC_BL0937) / 1.1371681416f;      //15102450
    sensor_voltage_multiplier = (221380000.0 * vref * sensor_voltage_resistor /  2.0 / F_OSC_BL0937) / 1.0474137931f;  //221384120,171674
    sensor_current_multiplier = (531500000.0 * vref / sensor_current_resistor / 24.0 / F_OSC_BL0937) / 1.166666f;  //
}

esp_err_t IRAM_OPT bl0937_init(chip_config_t config)
{
    esp_err_t ret;
    sensor_voltage_resistor = config.divider_resistor;
    sensor_current_resistor = config.sampling_resistor;
    current_mode = config.pin_mode;
    vref = V_REF_BL0937;
    setpin_io = config.sel_gpio;

    gpio_ll_intr_disable(&GPIO, config.cf_gpio);
    gpio_ll_intr_disable(&GPIO, config.cf1_gpio);

    gpio_config_t io_conf;
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pin_bit_mask = (1ULL << config.sel_gpio);
    io_conf.pull_down_en = 0;
    io_conf.pull_up_en = 0;
    ret = gpio_config(&io_conf);
    if (ret != ESP_OK) {
        return ret;
    }

    //interrupt of rising edge
    io_conf.intr_type = GPIO_INTR_POSEDGE;
    io_conf.pin_bit_mask = ((1ULL << config.cf1_gpio) | (1ULL << config.cf_gpio));
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pull_up_en = 1;
    ret = gpio_config(&io_conf);
    if (ret != ESP_OK) {
        return ret;
    }

    gpio_install_isr_service(ESP_INTR_FLAG_IRAM);
    gpio_isr_handler_add(config.cf_gpio, bl0937_cf_isr_handler, NULL);
    gpio_isr_handler_add(config.cf1_gpio, bl0937_cf1_isr_handler, NULL);

    calculatedefaultmultipliers();
    mode = current_mode;

    gpio_ll_set_level(&GPIO, config.sel_gpio, mode);

    gpio_ll_intr_enable_on_core(&GPIO, 0, config.cf_gpio);
    gpio_ll_intr_enable_on_core(&GPIO, 0, config.cf1_gpio);

    return ESP_OK;
}

float IRAM_OPT bl0937_get_current_multiplier()
{
    return sensor_current_multiplier;
}

float IRAM_OPT bl0937_get_voltage_multiplier()
{
    return sensor_voltage_multiplier;
}

float IRAM_OPT bl0937_get_power_multiplier()
{
    return sensor_power_multiplier;
}

void IRAM_OPT bl0937_set_current_multiplier(float current_multiplier)
{
    sensor_current_multiplier = current_multiplier;
}

void IRAM_OPT bl0937_set_voltage_multiplier(float voltage_multiplier)
{
    sensor_voltage_multiplier = voltage_multiplier;
}

void IRAM_OPT bl0937_set_power_multiplier(float power_multiplier)
{
    sensor_power_multiplier = power_multiplier;
}

void IRAM_OPT bl0937_setmode(bl0937_mode_t mode)
{
    mode = (mode == MODE_CURRENT) ? current_mode : 1 - current_mode;
    gpio_ll_set_level(&GPIO, setpin_io, mode);

    last_cf1_interrupt = first_cf1_interrupt = esp_timer_get_time();
}

bl0937_mode_t IRAM_OPT bl0937_getmode()
{
    return (mode == current_mode) ? MODE_CURRENT : MODE_VOLTAGE;
}

bl0937_mode_t IRAM_OPT bl0937_togglemode()
{
    bl0937_mode_t new_mode = bl0937_getmode() == MODE_CURRENT ? MODE_VOLTAGE : MODE_CURRENT;
    bl0937_setmode(new_mode);

    return new_mode;
}

float IRAM_OPT bl0937_get_energy()
{
    return sensor_pulse_wave_counts * sensor_power_multiplier / 1000000.;
}

void IRAM_OPT bl0937_checkcfsignal()
{
    if ((esp_timer_get_time() - last_cf_interrupt) > pulse_timeout_us) {
        sensor_power_pulse_width = 0;
    }
}

void IRAM_OPT bl0937_checkcf1signal()
{
    if ((esp_timer_get_time() - last_cf1_interrupt) > pulse_timeout_us) {
        if (mode == current_mode) {
            sensor_current_pulse_width = 0;
        } else {
            sensor_voltage_pulse_width = 0;
        }
        bl0937_togglemode();
    }
}

float IRAM_OPT bl0937_get_voltage()
{
    bl0937_checkcf1signal();

    sensor_voltage = (sensor_voltage_pulse_width > 0) ? sensor_voltage_multiplier / sensor_voltage_pulse_width : 0;

    return sensor_voltage;
}

void bl0937_multiplier_init()
{
    calculatedefaultmultipliers();
}

void IRAM_OPT bl0937_expected_voltage(float value)
{
    if (sensor_voltage == 0) {
        bl0937_get_voltage();
    }

    if (sensor_voltage > 0) {
        sensor_voltage_multiplier *= ((float) value / sensor_voltage);
    }
}

void IRAM_OPT bl0937_expected_current(float value)
{
    if (sensor_current == 0) {
        bl0937_get_current();
    }

    if (sensor_current > 0) {
        sensor_current_multiplier *= (value / sensor_current);
    }
}

void IRAM_OPT bl0937_expected_active_power(float value)
{
    if (sensor_power == 0) {
        bl0937_get_active_power();
    }

    if (sensor_power > 0) {
        sensor_power_multiplier *= ((float) value / sensor_power);
    }
}

float IRAM_OPT bl0937_get_active_power()
{
    bl0937_checkcfsignal();

    sensor_power = (sensor_power_pulse_width > 0) ? sensor_power_multiplier / sensor_power_pulse_width : 0;

    return sensor_power;
}

/*
    Power measurements are more sensitive to switch offs,
    so we first check if power is 0 to set _current to 0 too
*/
float IRAM_OPT bl0937_get_current()
{
    bl0937_get_active_power();

    if (sensor_power == 0) {
        sensor_current_pulse_width = 0;
    } else {
        bl0937_checkcf1signal();
    }
    sensor_current = (sensor_current_pulse_width > 0) ? sensor_current_multiplier / sensor_current_pulse_width  : 0;

    return sensor_current;
}

float IRAM_OPT bl0937_getapparentpower()
{
    float current = bl0937_get_current();
    float voltage = bl0937_get_voltage();

    return voltage * current;
}

float IRAM_OPT bl0937_get_power_factor()
{
    float active = bl0937_get_active_power();
    float apparent = bl0937_getapparentpower();

    if (active > apparent) {
        return 1;
    }

    if (apparent == 0) {
        return 0;
    }

    return (float) active / apparent;
}
