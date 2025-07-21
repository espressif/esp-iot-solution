/*
 * SPDX-FileCopyrightText: 2022-2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include "unity.h"
#include "bh1750.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define I2C_MASTER_SCL_IO           2           /*!< gpio number for I2C master clock */
#define I2C_MASTER_SDA_IO           1           /*!< gpio number for I2C master data  */
#define I2C_MASTER_NUM              I2C_NUM_0   /*!< I2C port number for master dev */
#define I2C_MASTER_TX_BUF_DISABLE   0           /*!< I2C master do not need buffer */
#define I2C_MASTER_RX_BUF_DISABLE   0           /*!< I2C master do not need buffer */
#define I2C_MASTER_FREQ_HZ          100000      /*!< I2C master clock frequency */

static i2c_bus_handle_t i2c_bus = NULL;
static bh1750_handle_t bh1750 = NULL;

static void bh1750_test_init()
{
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = I2C_MASTER_SDA_IO,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_io_num = I2C_MASTER_SCL_IO,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = I2C_MASTER_FREQ_HZ,
    };
    i2c_bus = i2c_bus_create(I2C_MASTER_NUM, &conf);
    bh1750 = bh1750_create(i2c_bus, BH1750_I2C_ADDRESS_DEFAULT);
}

static void bh1750_test_deinit()
{
    bh1750_delete(&bh1750);
    i2c_bus_delete(&i2c_bus);
}

static void bh1750_test_get_data()
{
    int ret;
    bh1750_cmd_measure_t cmd_measure;
    float bh1750_data;
    int cnt = 10;

    while (cnt--) {
        bh1750_power_on(bh1750);
        cmd_measure = BH1750_ONETIME_4LX_RES;
        bh1750_set_measure_mode(bh1750, cmd_measure);
        vTaskDelay(30 / portTICK_RATE_MS);
        ret = bh1750_get_data(bh1750, &bh1750_data);

        if (ret == ESP_OK) {
            printf("bh1750 val(one time mode): %f\n", bh1750_data);
        } else {
            printf("No ack, sensor not connected...\n");
        }

        cmd_measure = BH1750_CONTINUE_4LX_RES;
        bh1750_set_measure_mode(bh1750, cmd_measure);
        vTaskDelay(30 / portTICK_RATE_MS);
        ret = bh1750_get_data(bh1750, &bh1750_data);

        if (ret == ESP_OK) {
            printf("bh1750 val(continuously mode): %f\n", bh1750_data);
        } else {
            printf("No ack, sensor not connected...\n");
        }

        vTaskDelay(1000 / portTICK_RATE_MS);
    }
}

TEST_CASE("Sensor BH1750 test", "[bh1750][iot][sensor]")
{
    bh1750_test_init();
    bh1750_test_get_data();
    bh1750_test_deinit();
}

void app_main(void)
{
    printf(" ____  _   _ _ _____ ____   ___    _____ _____ ____ _____ \n");
    printf("| __ )| | | / |___  | ___| / _ \\  |_   _| ____/ ___|_   _|\n");
    printf("|  _ \\| |_| | |  / /|___ \\| | | |   | | |  _| \\___ \\ | |  \n");
    printf("| |_) |  _  | | / /  ___) | |_| |   | | | |___ ___) || |  \n");
    printf("|____/|_| |_|_|/_/  |____/ \\___/    |_| |_____|____/ |_|  \n");
    printf("                                                          \n");

    unity_run_menu();
}
