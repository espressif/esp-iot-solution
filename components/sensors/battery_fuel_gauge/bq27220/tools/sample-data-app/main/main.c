/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "unity.h"
#include "string.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "esp_vfs_fat.h"
#include "bq27220.h"
#include "i2c_bus.h"
#include "esp_timer.h"

static const char *TAG = "bq27220_sample";

#define I2C_MASTER_SCL_IO          CONFIG_I2C_SCL_IO         /*!< gpio number for I2C master clock */
#define I2C_MASTER_SDA_IO          CONFIG_I2C_SDA_IO         /*!< gpio number for I2C master data  */
#define DISCHARGE_ENABLE_IO        CONFIG_DISCHARGE_ENABLE_IO         /*!< gpio number for discharge enable */

static i2c_bus_handle_t i2c_bus = NULL;
static bq27220_handle_t bq27220 = NULL;

#define TEST_MOUNT_PATH "/log"
#define TEST_PARTITION_LABEL "log"

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

static wl_handle_t wl_handle = WL_INVALID_HANDLE;

static void init_fs()
{
    const esp_vfs_fat_mount_config_t mount_config = {
        .format_if_mount_failed = true,
        .max_files = 5,
        .allocation_unit_size = 4096
    };
    ESP_ERROR_CHECK(esp_vfs_fat_spiflash_mount_rw_wl(TEST_MOUNT_PATH, TEST_PARTITION_LABEL, &mount_config, &wl_handle));
}

static void deinit_fs()
{
    ESP_ERROR_CHECK(esp_vfs_fat_spiflash_unmount_rw_wl(TEST_MOUNT_PATH, wl_handle));
}

static void recode_data(uint16_t low_vol)
{
    ESP_LOGI(TAG, "End discharge voltage: %d", low_vol);
    printf("\n-------- Start record --------\n");
    printf("ElapsedTime, \t Voltage, \t Current, \t Temperature\n");
    uint16_t vol = bq27220_get_voltage(bq27220);
    uint32_t start_time_ms = esp_timer_get_time() / 1000;
    while (vol > low_vol) {
        uint32_t elapsed_time_ms = esp_timer_get_time() / 1000 - start_time_ms;
        vol = bq27220_get_voltage(bq27220);
        int16_t current = bq27220_get_current(bq27220);
        float temp = bq27220_get_temperature(bq27220) / 10.0 - 273; // Convert from 0.1K to Celsius
        printf("%ld, \t %d, \t %d, \t %.1f\n", elapsed_time_ms / 1000, vol, current, temp);
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
    printf("\n-------- End record --------\n");
}

void app_main(void)
{
    ESP_LOGI(TAG, "Hello BQ27220 Battery Fuel Gauge!");
    ESP_LOGI(TAG, "Discharge enable IO: %d", DISCHARGE_ENABLE_IO);
    ESP_LOGI(TAG, "I2C master SCL IO: %d", I2C_MASTER_SCL_IO);
    ESP_LOGI(TAG, "I2C master SDA IO: %d", I2C_MASTER_SDA_IO);
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

    gpio_config_t power_gpio_config = {
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = BIT64(DISCHARGE_ENABLE_IO),
    };
    ESP_ERROR_CHECK(gpio_config(&power_gpio_config));
    gpio_set_level(DISCHARGE_ENABLE_IO, false);

    init_fs();
    gpio_set_level(DISCHARGE_ENABLE_IO, true);
    recode_data(CONFIG_END_DISCHAGE_VOLTAGE);
    gpio_set_level(DISCHARGE_ENABLE_IO, false);
    deinit_fs();
}
