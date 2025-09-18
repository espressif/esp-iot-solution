/*
 * SPDX-FileCopyrightText: 2024-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "unity.h"
#include "string.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "bq27220.h"

#define I2C_MASTER_SCL_IO          GPIO_NUM_1         /*!< gpio number for I2C master clock */
#define I2C_MASTER_SDA_IO          GPIO_NUM_2         /*!< gpio number for I2C master data  */

#define TEST_MEMORY_LEAK_FIRST_THRESHOLD (-110)

static i2c_bus_handle_t i2c_bus = NULL;
static bq27220_handle_t bq27220 = NULL;

static const char *TAG = "bq27220_test";

static const parameter_cedv_t default_cedv = {
    .full_charge_cap = 650,
    .design_cap = 650,
    .reserve_cap = 0,
    .near_full = 200,
    .self_discharge_rate = 20,
    .EDV0 = 3490,
    .EDV1 = 3511,
    .EDV2 = 3535,
    .EMF = 3670,
    .C0 = 115,
    .R0 = 968,
    .T0 = 4547,
    .R1 = 4764,
    .TC = 11,
    .C1 = 0,
    .DOD0 = 4147,
    .DOD10 = 4002,
    .DOD20 = 3969,
    .DOD30 = 3938,
    .DOD40 = 3880,
    .DOD50 = 3824,
    .DOD60 = 3794,
    .DOD70 = 3753,
    .DOD80 = 3677,
    .DOD90 = 3574,
    .DOD100 = 3490,
};

static const gauging_config_t default_config = {
    .CCT = 1,
    .CSYNC = 0,
    .EDV_CMP = 0,
    .SC = 1,
    .FIXED_EDV0 = 0,
    .FCC_LIM = 1,
    .FC_FOR_VDQ = 1,
    .IGNORE_SD = 1,
    .SME0 = 0,
};

static void test_bq27220_init()
{
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = I2C_MASTER_SDA_IO,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_io_num = I2C_MASTER_SCL_IO,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = 400 * 1000,
    };
    i2c_bus = i2c_bus_create(I2C_NUM_0, &conf);

    bq27220_config_t bq27220_cfg = {
        .i2c_bus = i2c_bus,
        .cfg = &default_config,
        .cedv = &default_cedv,
    };
    bq27220 = bq27220_create(&bq27220_cfg);
    TEST_ASSERT_NOT_NULL(bq27220);
}

static void test_bq27220_deinit()
{
    TEST_ASSERT(bq27220_delete(bq27220) == ESP_OK);
    bq27220 = NULL;
    TEST_ASSERT(i2c_bus_delete(&i2c_bus) == ESP_OK);
}

static void test_bq27220_print_info(bq27220_handle_t bq27220Handle)
{
    battery_status_t status = {};
    bq27220_get_battery_status(bq27220Handle, &status);
    ESP_LOGI(TAG, "Battery Status - DSG: %d, SYSDWN: %d, TDA: %d, BATTPRES: %d, AUTH_GD: %d, OCVGD: %d, TCA: %d, RSVD: %d, CHGINH: %d, FC: %d, OTD: %d, OTC: %d, SLEEP: %d, OCVFAIL: %d, OCVCOMP: %d, FD: %d",
             status.DSG, status.SYSDWN, status.TDA, status.BATTPRES,
             status.AUTH_GD, status.OCVGD, status.TCA, status.RSVD,
             status.CHGINH, status.FC, status.OTD, status.OTC,
             status.SLEEP, status.OCVFAIL, status.OCVCOMP, status.FD);

    uint16_t vol = bq27220_get_voltage(bq27220Handle);
    int16_t current = bq27220_get_current(bq27220Handle);
    uint16_t rc = bq27220_get_remaining_capacity(bq27220Handle);
    uint16_t full_cap = bq27220_get_full_charge_capacity(bq27220Handle);
    uint16_t temp = bq27220_get_temperature(bq27220Handle) / 10 - 273; // Convert from 0.1K to Celsius
    uint16_t cycle_cnt = bq27220_get_cycle_count(bq27220Handle);
    uint16_t soc = bq27220_get_state_of_charge(bq27220Handle);
    int16_t avg_power = bq27220_get_average_power(bq27220Handle); // in mW
    int16_t max_load = bq27220_get_maxload_current(bq27220Handle); // in mA
    uint16_t time_to_empty = bq27220_get_time_to_empty(bq27220Handle);
    uint16_t time_to_full = bq27220_get_time_to_full(bq27220Handle);

    ESP_LOGI(TAG, "Battery Info - Vol: %dmv, Current: %dmA, Power: %dmW, Remaining Capacity: %dmAh, Full Charge Capacity: %dmAh, Temperature: %dC, Cycle Count: %d, SOC: %d%%, Max Load: %dmA, Time to empty: %dmin, Time to full: %dmin",
             vol, current, avg_power, rc, full_cap, temp, cycle_cnt, soc, max_load, time_to_empty, time_to_full);
}

TEST_CASE("bq27220 basic information query test", "[voltage][percent][charge_rate]")
{
    test_bq27220_init();
    test_bq27220_print_info(bq27220);
    test_bq27220_deinit();
}

static size_t before_free_8bit;
static size_t before_free_32bit;

static void check_leak(size_t before_free, size_t after_free, ssize_t threshold, const char *type)
{
    ssize_t delta = after_free - before_free;
    printf("MALLOC_CAP_%s: Before %u bytes free, After %u bytes free (delta %d)\n", type, before_free, after_free, delta);
    if (!(delta >= threshold)) {
        ESP_LOGE(TAG, "Memory leak detected, delta: %d bytes, threshold: %d bytes", delta, threshold);
    }
}

void setUp(void)
{
    before_free_8bit = heap_caps_get_free_size(MALLOC_CAP_8BIT);
    before_free_32bit = heap_caps_get_free_size(MALLOC_CAP_32BIT);
}

void tearDown(void)
{
    ssize_t threshold = TEST_MEMORY_LEAK_FIRST_THRESHOLD;
    static bool is_first = true;
    if (is_first) {
        is_first = false;
    } else {
        threshold = 0;
    }
    size_t after_free_8bit = heap_caps_get_free_size(MALLOC_CAP_8BIT);
    size_t after_free_32bit = heap_caps_get_free_size(MALLOC_CAP_32BIT);
    check_leak(before_free_8bit, after_free_8bit, threshold, "8BIT");
    check_leak(before_free_32bit, after_free_32bit, threshold, "32BIT");
}

void app_main(void)
{
    printf("BQ27220 TEST \n");
    unity_run_menu();
}
