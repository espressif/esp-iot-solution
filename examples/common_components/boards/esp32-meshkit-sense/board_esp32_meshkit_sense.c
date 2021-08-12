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
#include "board.h"
#include "esp_log.h"

static const char *TAG = "Board";

static bool s_board_is_init = false;
static bool s_board_gpio_isinit = false;

/****Private board level API ****/
static i2c_bus_handle_t s_i2c0_bus_handle = NULL;
static i2c_bus_handle_t s_spi2_bus_handle = NULL;

static esp_err_t board_i2c_bus_init(void)
{
#if(CONFIG_BOARD_I2C0_INIT)
    i2c_config_t board_i2c_conf = {
        .mode = BOARD_I2C0_MODE,
        .sda_io_num = BOARD_IO_I2C0_SDA,
        .sda_pullup_en = BOARD_I2C0_SDA_PULLUP_EN,
        .scl_io_num = BOARD_IO_I2C0_SCL,
        .scl_pullup_en = BOARD_I2C0_SCL_PULLUP_EN,
        .master.clk_speed = BOARD_I2C0_SPEED,
    };
    i2c_bus_handle_t handle = i2c_bus_create(I2C_NUM_0, &board_i2c_conf);
    BOARD_CHECK(handle != NULL, "i2c_bus create failed", ESP_FAIL);
    s_i2c0_bus_handle = handle;
    ESP_LOGI(TAG, "i2c_bus 0 create succeed");
#endif
    return ESP_OK;
}

static esp_err_t board_i2c_bus_deinit(void)
{
    if (s_i2c0_bus_handle != NULL) {
        i2c_bus_delete(&s_i2c0_bus_handle);
        BOARD_CHECK(s_i2c0_bus_handle == NULL, "i2c_bus delete failed", ESP_FAIL);
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
    BOARD_CHECK(s_spi2_bus_handle != NULL, "spi_bus2 create failed", ESP_FAIL);
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

static esp_err_t board_gpio_init(void)
{
    if (s_board_gpio_isinit) {
        return ESP_OK;
    }
    gpio_config_t io_conf;
    //disable interrupt
    io_conf.intr_type = GPIO_INTR_DISABLE;
    //set as output mode
    io_conf.mode = GPIO_MODE_OUTPUT;
    //bit mask of the pins that you want to set
    io_conf.pin_bit_mask = BOARD_IO_PIN_SEL_OUTPUT;
    //disable pull-down mode
    io_conf.pull_down_en = 0;
    //disable pull-up mode
    io_conf.pull_up_en = 0;
    //configure GPIO with the given settings
    esp_err_t ret = gpio_config(&io_conf);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "gpio init failed");
        return ret;
    }
    s_board_gpio_isinit = true;
    ESP_LOGI(TAG, "gpio init succeed");
    return ESP_OK;
}

static esp_err_t board_gpio_deinit(void)
{
    if (!s_board_gpio_isinit) {
        return ESP_OK;
    }
    s_board_gpio_isinit = false;
    return ESP_OK;
}

/****General board level API ****/
esp_err_t iot_board_init(void)
{
    if(s_board_is_init) {
        return ESP_OK;
    }
    esp_err_t ret = board_gpio_init();
    BOARD_CHECK(ret == ESP_OK, "gpio init failed", ret);
    iot_board_led_all_set_state(false);

    ret = board_i2c_bus_init();
    BOARD_CHECK(ret == ESP_OK, "i2c init failed", ret);

    ret = board_spi_bus_init();
    BOARD_CHECK(ret == ESP_OK, "spi init failed", ret);

#ifdef CONFIG_BOARD_POWER_SENSOR
    iot_board_sensor_set_power(true);
#else
    iot_board_sensor_set_power(false);
#endif

#ifdef CONFIG_BOARD_POWER_SCREEN
    iot_board_screen_set_power(true);
#else
    iot_board_screen_set_power(false);
#endif

    s_board_is_init = true;
    ESP_LOGI(TAG,"Board Info: %s", iot_board_get_info());
    ESP_LOGI(TAG,"Init Done ...");
    return ESP_OK;
}

esp_err_t iot_board_deinit(void)
{
    if(!s_board_is_init) {
        return ESP_OK;
    }
    esp_err_t ret = board_gpio_deinit();
    BOARD_CHECK(ret == ESP_OK, "gpio de-init failed", ret);

    ret = board_i2c_bus_deinit();
    BOARD_CHECK(ret == ESP_OK, "i2c de-init failed", ret);

    ret = board_spi_bus_deinit();
    BOARD_CHECK(ret == ESP_OK, "spi de-init failed", ret);

#ifdef CONFIG_BOARD_POWER_SENSOR
    iot_board_sensor_set_power(false);
#endif

#ifdef CONFIG_BOARD_POWER_SCREEN
    iot_board_screen_set_power(false);
#endif
    s_board_is_init = false;
    ESP_LOGI(TAG," Deinit Done ...");
    return ESP_OK;
}

bool iot_board_is_init(void)
{
    return s_board_is_init;
}

board_res_handle_t iot_board_get_handle(int id)
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

/****Extended board level API ****/
esp_err_t iot_board_sensor_set_power(bool on_off)
{
    if (!s_board_gpio_isinit) {
        return ESP_FAIL;
    }
    return gpio_set_level(BOARD_IO_POWER_ON_SENSOR_N, !on_off);
}

bool iot_board_sensor_get_power(void)
{
    if (!s_board_gpio_isinit) {
        return 0;
    }
    return !gpio_get_level(BOARD_IO_POWER_ON_SENSOR_N);
}

esp_err_t iot_board_screen_set_power(bool on_off)
{
    if (!s_board_gpio_isinit) {
        return ESP_FAIL;
    }
    return gpio_set_level(BOARD_IO_POWER_ON_SCREEN_N, !on_off);
}

bool iot_board_screen_get_power(void)
{
    if (!s_board_gpio_isinit) {
        return 0;
    }
    return gpio_get_level(BOARD_IO_POWER_ON_SCREEN_N);
}

static int s_led_io[BOARD_LED_NUM]={BOARD_IO_LED_1, BOARD_IO_LED_2, BOARD_IO_LED_3, BOARD_IO_LED_4};
static int s_led_polarity[BOARD_LED_NUM]={BOARD_LED_POLARITY_1, BOARD_LED_POLARITY_2, BOARD_LED_POLARITY_3, BOARD_LED_POLARITY_4};
static bool s_led_state[BOARD_LED_NUM]={0};

esp_err_t iot_board_led_set_state(int gpio_num, bool if_on)
{
    int i = 0;

    for (i = 0; i < BOARD_LED_NUM; i++){
        if (s_led_io[i]==gpio_num) {
            break;
        }
    }

    if (i >= BOARD_LED_NUM) {
        ESP_LOGE(TAG, "GPIO %d is not a valid LED io = %d", gpio_num, i);
        return ESP_FAIL;
    }

    if (!s_led_polarity[i]) { /*check led polarity*/
        if_on = !if_on;
    }

    gpio_set_level(gpio_num, if_on);
    return ESP_OK;
}

esp_err_t iot_board_led_all_set_state(bool if_on)
{
    for (size_t i = 0; i < BOARD_LED_NUM; i++){
        iot_board_led_set_state(s_led_io[i], if_on);
    }
    return ESP_OK;
}

esp_err_t iot_board_led_toggle_state(int gpio_num)
{
    int i = 0;

    for (i = 0; i < BOARD_LED_NUM; i++){
        if (s_led_io[i]==gpio_num) {
            break;
        }
    }

    if (i >= BOARD_LED_NUM) {
        ESP_LOGE(TAG, "GPIO %d is not a valid LED io", gpio_num);
        return ESP_FAIL;
    }

    s_led_state[i]=!s_led_state[i];
    iot_board_led_set_state(gpio_num, s_led_state[i]);
    return ESP_OK;
}
