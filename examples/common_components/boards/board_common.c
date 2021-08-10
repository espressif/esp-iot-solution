// Copyright 2020-2021 Espressif Systems (Shanghai) PTE LTD
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at

//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <stdio.h>
#include "esp_log.h"
#include "board_common.h"
#include "board.h"

static const char *TAG = "BOARD_COMMON";
static bool s_board_is_init = false;

static i2c_bus_handle_t s_i2c0_bus_handle = NULL;
static spi_bus_handle_t s_spi2_bus_handle = NULL;

static esp_err_t board_i2c_bus_init(void)
{
#if(CONFIG_BOARD_I2C0_INIT)
    i2c_config_t board_i2c0_conf = {
        .mode = BOARD_I2C0_MODE,
        .sda_io_num = BOARD_IO_I2C0_SDA,
        .sda_pullup_en = BOARD_I2C0_SDA_PULLUP_EN,
        .scl_io_num = BOARD_IO_I2C0_SCL,
        .scl_pullup_en = BOARD_I2C0_SCL_PULLUP_EN,
        .master.clk_speed = BOARD_I2C0_SPEED,
    };
    i2c_bus_handle_t handle0 = i2c_bus_create(I2C_NUM_0, &board_i2c0_conf);
    BOARD_CHECK(handle0 != NULL, "i2c_bus0 create failed", ESP_FAIL);
    s_i2c0_bus_handle = handle0;
    ESP_LOGI(TAG, "i2c_bus 0 create succeed");
#endif
    return ESP_OK;
}

static esp_err_t board_i2c_bus_deinit(void)
{
    if (s_i2c0_bus_handle != NULL) {
        i2c_bus_delete(&s_i2c0_bus_handle);
        if (s_i2c0_bus_handle != NULL) {
            return ESP_FAIL;
        }
    }
    return ESP_OK;
}

static esp_err_t board_spi_bus_init(void)
{
#if(CONFIG_BOARD_SPI2_INIT)
    spi_config_t bus_conf = {
        .miso_io_num = BOARD_IO_SPI2_MISO,
        .mosi_io_num = BOARD_IO_SPI2_MOSI,
        .sclk_io_num = BOARD_IO_SPI2_SCK,
    };
    s_spi2_bus_handle = spi_bus_create(SPI2_HOST, &bus_conf);
    BOARD_CHECK(s_spi2_bus_handle != NULL, "spi_bus2 creat failed", ESP_FAIL);
    ESP_LOGI(TAG, "spi_bus 2 create succeed");
#endif
    return ESP_OK;
}

static esp_err_t board_spi_bus_deinit(void)
{
    if (s_spi2_bus_handle != NULL) {
        spi_bus_delete(&s_spi2_bus_handle);
        BOARD_CHECK(s_spi2_bus_handle == NULL, "i2c_bus delete failed", ESP_FAIL);
    }
    return ESP_OK;
}

ATTR_WEAK esp_err_t iot_board_init(void)
{
    if(s_board_is_init) {
        return ESP_OK;
    }
    int ret;

    ret = board_i2c_bus_init();
    BOARD_CHECK(ret == ESP_OK, "i2c init failed", ret);

    ret = board_spi_bus_init();
    BOARD_CHECK(ret == ESP_OK, "spi init failed", ret);

    s_board_is_init = true;
    ESP_LOGI(TAG,"Board Info: %s", iot_board_get_info());
    ESP_LOGI(TAG,"Board Init Done ...");
    return ESP_OK;
}

ATTR_WEAK esp_err_t iot_board_deinit(void)
{
    if(!s_board_is_init) {
        return ESP_OK;
    }
    int ret;

    ret = board_i2c_bus_deinit();
    BOARD_CHECK(ret == ESP_OK, "i2c de-init failed", ret);

    ret = board_spi_bus_deinit();
    BOARD_CHECK(ret == ESP_OK, "spi de-init failed", ret);

    s_board_is_init = false;
    ESP_LOGI(TAG,"Board Deinit Done ...");
    return ESP_OK;
}

ATTR_WEAK bool iot_board_is_init(void)
{
    return s_board_is_init;
}

ATTR_WEAK board_res_handle_t iot_board_get_handle(int id)
{
    board_res_handle_t handle;
    switch (id)
    {
    case BOARD_I2C0_ID:
        handle = (board_res_handle_t)s_i2c0_bus_handle;
        break;
    case BOARD_SPI2_ID:
        handle = (board_res_handle_t)s_spi2_bus_handle;
        break;
    default:
        handle = NULL;
        break;
    }
    return handle;
}

ATTR_WEAK char* iot_board_get_info()
{
    static char* info = BOARD_NAME;
    return info;
}
