/*
 * SPDX-FileCopyrightText: 2022-2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include "driver/gpio.h"
#include "driver/adc.h"
#include "esp_adc_cal.h"
#include "esp_log.h"
#include "i2c_bus.h"
#include "spi_bus.h"
#include "iot_board.h"

static const char *TAG = BOARD_NAME;

//ADC Calibration
static esp_adc_cal_characteristics_t s_adc1_chars;
static esp_adc_cal_characteristics_t s_adc2_chars;

static bool s_board_is_init = false;
static bool s_board_gpio_is_init = false;
static bool s_board_adc_is_init = false;

/**** Private handle ****/
static i2c_bus_handle_t s_spi3_bus_handle = NULL;

/**** board specified handler ****/
esp_err_t iot_board_specific_init(void);
esp_err_t iot_board_specific_deinit(void);
board_res_handle_t iot_board_specific_get_handle(int id);

const board_specific_callbacks_t board_esp32_s3_otg_callbacks = {
    .pre_init_cb = NULL,
    .pre_deinit_cb = NULL,
    .post_init_cb = iot_board_specific_init,
    .post_deinit_cb = iot_board_specific_deinit,
    .get_handle_cb = iot_board_specific_get_handle,
};

static esp_err_t board_spi_bus_init(void)
{
#if(CONFIG_BOARD_SPI3_INIT)
    spi_config_t bus3_conf = {
        .miso_io_num = BOARD_IO_SPI3_MISO,
        .mosi_io_num = BOARD_IO_SPI3_MOSI,
        .sclk_io_num = BOARD_IO_SPI3_SCK,
    };
    s_spi3_bus_handle = spi_bus_create(SPI3_HOST, &bus3_conf);
    BOARD_CHECK(s_spi3_bus_handle != NULL, "spi_bus3 creat failed", ESP_FAIL);
    ESP_LOGI(TAG, "spi3 bus init succeed");
#endif
    return ESP_OK;
}

static esp_err_t board_spi_bus_deinit(void)
{
    if (s_spi3_bus_handle != NULL) {
        spi_bus_delete(&s_spi3_bus_handle);
        BOARD_CHECK(s_spi3_bus_handle == NULL, "spi bus3 delete failed", ESP_FAIL);
        ESP_LOGI(TAG, "spi3 bus delete succeed");
    }
    return ESP_OK;
}

static esp_err_t board_adc_init(void)
{
#ifdef CONFIG_BOARD_ADC_INIT
    //Check if Two Point or Vref are burned into eFuse
    esp_err_t ret = esp_adc_cal_check_efuse(BOARD_ADC_CALI_SCHEME);
    BOARD_CHECK(ret == ESP_OK, "ADC Calibration scheme not supported", ret);
    esp_adc_cal_characterize(ADC_UNIT_1, BOARD_ADC_ATTEN, BOARD_ADC_WIDTH, 0, &s_adc1_chars);
    esp_adc_cal_characterize(ADC_UNIT_2, BOARD_ADC_ATTEN, BOARD_ADC_WIDTH, 0, &s_adc2_chars);

    //Configure ADC
    if (BOARD_ADC_UNIT == ADC_UNIT_1) {
        adc1_config_width(BOARD_ADC_WIDTH);
        adc1_config_channel_atten(BOARD_ADC_HOST_VOL_CHAN, BOARD_ADC_ATTEN);
        adc1_config_channel_atten(BOARD_ADC_BAT_VOL_CHAN, BOARD_ADC_ATTEN);
    } else {
        adc1_config_channel_atten(BOARD_ADC_HOST_VOL_CHAN, BOARD_ADC_ATTEN);
        adc2_config_channel_atten((adc2_channel_t)BOARD_ADC_BAT_VOL_CHAN, BOARD_ADC_ATTEN);
    }
    s_board_adc_is_init = true;
    ESP_LOGI(TAG, "adc init succeed");
#endif
    return ESP_OK;
}

static esp_err_t board_adc_deinit()
{
    BOARD_CHECK(s_board_adc_is_init, "adc not inited", ESP_ERR_INVALID_STATE);
    s_board_adc_is_init = false;
    ESP_LOGI(TAG, "adc deinit succeed");
    return ESP_OK;
}

/**** Common board  API ****/
esp_err_t iot_board_specific_init(void)
{
    BOARD_CHECK(!s_board_is_init, "board has inited", ESP_ERR_INVALID_STATE);

    esp_err_t ret = board_adc_init();
    BOARD_CHECK(ret == ESP_OK, "ADC init failed", ret);

    ret = board_spi_bus_init();
    BOARD_CHECK(ret == ESP_OK, "spi init failed", ret);

    s_board_is_init = true;
#if CONFIG_BOARD_GPIO_INIT
    s_board_gpio_is_init = true;
#endif
    return ESP_OK;
}

esp_err_t iot_board_specific_deinit(void)
{
    BOARD_CHECK(s_board_is_init, "board not inited", ESP_ERR_INVALID_STATE);

    esp_err_t ret = board_adc_deinit();
    BOARD_CHECK(ret == ESP_OK, "ADC de-init failed", ret);

    ret = board_spi_bus_deinit();
    BOARD_CHECK(ret == ESP_OK, "spi de-init failed", ret);

    s_board_is_init = false;
#if CONFIG_BOARD_GPIO_INIT
    s_board_gpio_is_init = false;
#endif
    return ESP_OK;
}

board_res_handle_t iot_board_specific_get_handle(int id)
{
    board_res_handle_t handle;
    switch (id) {
    case BOARD_SPI3_ID:
        handle = (board_res_handle_t)s_spi3_bus_handle;
        break;
    default:
        handle = NULL;
        break;
    }
    return handle;
}

esp_err_t iot_board_usb_host_set_power(bool on_off, bool from_battery)
{
    BOARD_CHECK(s_board_gpio_is_init, "power control gpio not inited", ESP_FAIL);
    if (from_battery) {
        gpio_set_level(BOARD_IO_HOST_BOOST_EN, on_off); //BOOST_EN
        gpio_set_level(BOARD_IO_DEV_VBUS_EN, !on_off);//DEV_VBUS_EN
        gpio_set_level(BOARD_IO_IDEV_LIMIT_EN, on_off);//DEV_VBUS_EN
    } else {
        gpio_set_level(BOARD_IO_HOST_BOOST_EN, !on_off); //BOOST_EN
        gpio_set_level(BOARD_IO_DEV_VBUS_EN, on_off);//DEV_VBUS_EN
        gpio_set_level(BOARD_IO_IDEV_LIMIT_EN, on_off);//DEV_VBUS_EN
    }
    return ESP_OK;
}

bool iot_board_usb_host_get_power(void)
{
    BOARD_CHECK(s_board_gpio_is_init, "power control gpio not inited", false);
    return gpio_get_level(BOARD_IO_IDEV_LIMIT_EN);
}

float iot_board_usb_device_voltage(void)
{
    BOARD_CHECK(s_board_adc_is_init, "adc not inited", 0.0);
    int adc_reading = 0;
    //Multisampling
    for (int i = 0; i < BOARD_ADC_NO_OF_SAMPLES; i++) {
        if (BOARD_ADC_UNIT == ADC_UNIT_1) {
            adc_reading += adc1_get_raw((adc1_channel_t)BOARD_ADC_HOST_VOL_CHAN);
        } else {
            adc_reading += adc1_get_raw((adc1_channel_t)BOARD_ADC_HOST_VOL_CHAN);
        }
    }
    adc_reading /= BOARD_ADC_NO_OF_SAMPLES;
    //Convert adc_reading to voltage in mV
    uint32_t voltage = esp_adc_cal_raw_to_voltage(adc_reading, &s_adc1_chars);
    return voltage * 370.0 / 100.0 / 1000.0;
}

float iot_board_battery_voltage(void)
{
    BOARD_CHECK(s_board_adc_is_init, "adc not inited", 0.0);
    int adc_reading = 0, voltage = 0;
    //Multisampling
    for (int i = 0; i < BOARD_ADC_NO_OF_SAMPLES; i++) {
        if (BOARD_ADC_UNIT == ADC_UNIT_1) {
            adc_reading += adc1_get_raw((adc1_channel_t)BOARD_ADC_BAT_VOL_CHAN);
        } else {
            int adc2_reading = 0;
            adc2_get_raw((adc2_channel_t)BOARD_ADC_BAT_VOL_CHAN, BOARD_ADC_WIDTH, &adc2_reading);
            adc_reading += adc2_reading;
        }
    }
    adc_reading /= BOARD_ADC_NO_OF_SAMPLES;
    //Convert adc_reading to voltage in mV
    if (BOARD_ADC_UNIT == ADC_UNIT_1) {
        voltage = esp_adc_cal_raw_to_voltage(adc_reading, &s_adc1_chars);
    } else {
        voltage = esp_adc_cal_raw_to_voltage(adc_reading, &s_adc2_chars);
    }
    return voltage * 200.0 / 100.0 / 1000.0;
}

esp_err_t iot_board_usb_set_mode(usb_mode_t mode)
{
    BOARD_CHECK(s_board_gpio_is_init, "mode control gpio not inited", ESP_FAIL);
    switch (mode) {
    case USB_DEVICE_MODE:
        gpio_set_level(BOARD_IO_USB_SEL, false);
        break;
    case USB_HOST_MODE:
        gpio_set_level(BOARD_IO_USB_SEL, true);
        break;
    default:
        gpio_set_level(BOARD_IO_USB_SEL, false); //default to device mode
        break;
    }
    ESP_LOGI(TAG, "switch to usb %s port", mode == USB_HOST_MODE ? "host" : "device");
    return ESP_OK;
}

usb_mode_t iot_board_usb_get_mode(void)
{
    BOARD_CHECK(s_board_gpio_is_init, "mode control gpio not inited", USB_DEVICE_MODE);

    int value = gpio_get_level(BOARD_IO_USB_SEL);
    if (value) {
        return USB_HOST_MODE;
    }
    return USB_DEVICE_MODE;
}
