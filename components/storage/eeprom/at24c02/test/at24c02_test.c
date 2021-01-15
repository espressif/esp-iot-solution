// Copyright 2015-2016 Espressif Systems (Shanghai) PTE LTD
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
#define AT24C02_TEST_CODE 1
#if AT24C02_TEST_CODE

#include <stdio.h>
#include "unity.h"
#include "esp_log.h"
#include "driver/i2c.h"
#include "at24c02.h"

#define I2C_MASTER_SCL_IO           22          /*!< gpio number for I2C master clock IO21*/
#define I2C_MASTER_SDA_IO           21          /*!< gpio number for I2C master data  IO15*/
#define I2C_MASTER_NUM              I2C_NUM_1   /*!< I2C port number for master device */
#define I2C_MASTER_TX_BUF_DISABLE   0           /*!< I2C master do not need buffer */
#define I2C_MASTER_RX_BUF_DISABLE   0           /*!< I2C master do not need buffer */
#define I2C_MASTER_FREQ_HZ          100000      /*!< I2C master clock frequency */

static i2c_bus_handle_t i2c_bus = NULL;
static at24c02_handle_t at24c02 = NULL;
static const char *TAG = "at24c02";

void at24c02_init()
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
    at24c02 = at24c02_create(i2c_bus, AT24C02_I2C_ADDRESS_DEFAULT);
}

void at24c02_deinit()
{
    at24c02_delete(&at24c02);
    i2c_bus_delete(&i2c_bus);
}

void at24c02_test_byte_write_read()
{
    uint8_t cnt = 5;
    while (cnt--) {
        /****One Byte Test****/
        uint8_t data0 = cnt;
        TEST_ASSERT(ESP_OK == at24c02_write_byte(at24c02, cnt, data0));
        ESP_LOGI(TAG, "write data of address %u :%u",cnt, data0);
        vTaskDelay(50 / portTICK_RATE_MS);
        uint8_t data0_read = 0;
        TEST_ASSERT(ESP_OK == at24c02_read_byte(at24c02, cnt, &data0_read));
        ESP_LOGI(TAG, "read data of address %u :%u",cnt, data0_read);
        TEST_ASSERT_EQUAL_UINT8(data0, data0_read);
    }
}

void at24c02_test_bytes_write_read()
{
    uint8_t cnt = 5;
    while (cnt--) {
        /****** multi Byte Test ****/
        uint8_t data[5] = {0x00};
        for (size_t i = 0; i < 5; i++) {
            data[i] = cnt + i;
        }
        TEST_ASSERT(ESP_OK == at24c02_write(at24c02, cnt, sizeof(data), data));
        ESP_LOGI(TAG, "write data start from address %u: %u,%u,%u,%u,%u",cnt, data[0],
                data[1], data[2], data[3], data[4]);
        vTaskDelay(50 / portTICK_RATE_MS);
        uint8_t data_read[5] = {0x00};
        TEST_ASSERT(ESP_OK == at24c02_read(at24c02, cnt, sizeof(data_read), data_read));
        ESP_LOGI(TAG, "read data start from address %u: %u,%u,%u,%u,%u",cnt, data_read[0],
                data_read[1], data_read[2], data_read[3], data_read[4]);
        TEST_ASSERT_EQUAL_UINT8_ARRAY(data, data_read, 5);
    }
}

void at24c02_test_bit_write_read()
{
    uint8_t cnt = 5;
    while (cnt--) {
        /****** operate Bit and Bits Test ****/
        uint8_t data2 = cnt + 20;
        uint8_t data2_read = 0;
        TEST_ASSERT(ESP_OK == at24c02_write_byte(at24c02, cnt, 0x00));
        vTaskDelay(50 / portTICK_RATE_MS);
        ESP_LOGI(TAG, "write byte of address %u %x",cnt, 0x00);
        for (size_t i = 0; i < 8; i++) {
            TEST_ASSERT(ESP_OK == at24c02_write_bit(at24c02, cnt, i, (data2 >> i)&0x01));
            ESP_LOGI(TAG, "write bit of address %u:bit%u = %u",cnt, i, (data2 >> i)&0x01);
            data2_read = 0;
            vTaskDelay(50 / portTICK_RATE_MS);
            TEST_ASSERT(ESP_OK == at24c02_read_bit(at24c02, cnt, i, &data2_read));
            ESP_LOGI(TAG, "read bit of address %u:bit%u = %u",cnt, i, data2_read);
            data2_read = 0;
            TEST_ASSERT(ESP_OK == at24c02_read_byte(at24c02, cnt, &data2_read));
            ESP_LOGI(TAG, "read byte of address %u :%u",cnt, data2_read);
        }
        TEST_ASSERT_EQUAL_UINT8(data2, data2_read);
    }
}

void at24c02_test_bits_write_read()
{
    uint8_t cnt = 5;
    while (cnt--) {
        /****** operate Bit and Bits Test ****/
        uint8_t data2 = cnt + 20;
        uint8_t data2_read = 0;
        TEST_ASSERT(ESP_OK == at24c02_write_byte(at24c02, cnt, 0x00));
        vTaskDelay(50 / portTICK_RATE_MS);
        ESP_LOGI(TAG, "write byte of address %u %x",cnt, 0x00);
        for (size_t i = 0; i < 2; i++) {
            TEST_ASSERT(ESP_OK == at24c02_write_bits(at24c02, cnt, i*4+3, 4, ((data2>>(i*4))&0x0f)));
            ESP_LOGI(TAG, "write bits from address %u:bit%u = %u",cnt, i*4, ((data2>>(i*4))&0x0f));
            data2_read = 0;
            vTaskDelay(50 / portTICK_RATE_MS);
            TEST_ASSERT(ESP_OK == at24c02_read_bits(at24c02, cnt, i*4+3, 4, &data2_read));
            ESP_LOGI(TAG, "read bits of address %u:bit%u = %u",cnt, i*4, data2_read);
            data2_read = 0;
            TEST_ASSERT(ESP_OK == at24c02_read_byte(at24c02, cnt, &data2_read));
            ESP_LOGI(TAG, "read byte of address %u :%u",cnt, data2_read);
        }
        TEST_ASSERT_EQUAL_UINT8(data2, data2_read);
    }
}

TEST_CASE("at24c02 byte operations", "[at24c02][eeprom][i2c_bus][storage]")
{
    at24c02_init();
    at24c02_test_byte_write_read();
    at24c02_deinit();
}

TEST_CASE("at24c02 bytes operations", "[at24c02][eeprom][i2c_bus][storage]")
{
    at24c02_init();
    at24c02_test_bytes_write_read();
    at24c02_deinit();
}

TEST_CASE("at24c02 bit operations", "[at24c02][eeprom][i2c_bus][storage]")
{
    at24c02_init();
    at24c02_test_bit_write_read();
    at24c02_deinit();
}

TEST_CASE("at24c02 bits operations", "[at24c02][eeprom][i2c_bus][storage]")
{
    at24c02_init();
    at24c02_test_bits_write_read();
    at24c02_deinit();
}

#endif
