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
#include "driver/gpio.h"
#include "driver/adc.h"
#include "esp_adc_cal.h"
#include "esp_log.h"
#include "i2c_bus.h"
#include "spi_bus.h"
#include "board.h"

static const char *TAG = "Board";

#define DEFAULT_VREF    1100        //Use adc2_vref_to_gpio() to obtain a better estimate
#define NO_OF_SAMPLES   16          //Multisampling

static esp_adc_cal_characteristics_t *adc1_chars = NULL;

#if CONFIG_IDF_TARGET_ESP32S2
static const adc_bits_width_t BOARD_ADC_WIDTH = ADC_WIDTH_BIT_13;
#endif
static const adc_atten_t BOARD_ADC_ATTEN = ADC_ATTEN_DB_11;
static const adc_unit_t BOARD_ADC_UNIT = ADC_UNIT_1;

static bool s_board_is_init = false;
static bool s_board_gpio_isinit = false;

#define BOARD_CHECK(a, str, ret) if(!(a)) { \
        ESP_LOGE(TAG,"%s:%d (%s):%s", __FILE__, __LINE__, __FUNCTION__, str); \
        return (ret); \
    }

/****Private board level API ****/
static i2c_bus_handle_t s_i2c0_bus_handle = NULL;
static i2c_bus_handle_t s_spi2_bus_handle = NULL;
static i2c_bus_handle_t s_spi3_bus_handle = NULL;
static button_handle_t s_btn_ok_hdl = NULL;
static button_handle_t s_btn_up_hdl = NULL;
static button_handle_t s_btn_dw_hdl = NULL;
static button_handle_t s_btn_menu_hdl = NULL;

static void _check_efuse(void)
{
#if CONFIG_IDF_TARGET_ESP32S2
    if (esp_adc_cal_check_efuse(ESP_ADC_CAL_VAL_EFUSE_TP) == ESP_OK) {
        printf("eFuse Two Point: Supported\n");
    } else {
        printf("Cannot retrieve eFuse Two Point calibration values. Default calibration values will be used.\n");
    }
#else
#error "This example is configured for ESP32S2."
#endif
}

static void _print_char_val_type(esp_adc_cal_value_t val_type)
{
    if (val_type == ESP_ADC_CAL_VAL_EFUSE_TP) {
        printf("Characterized using Two Point Value\n");
    } else if (val_type == ESP_ADC_CAL_VAL_EFUSE_VREF) {
        printf("Characterized using eFuse Vref\n");
    } else {
        printf("Characterized using Default Vref\n");
    }
}

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
    BOARD_CHECK(handle != NULL, "i2c_bus creat failed", ESP_FAIL);
    s_i2c0_bus_handle = handle;
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
#if(CONFIG_BOARD_SPI3_INIT)
    spi_config_t bus3_conf = {
        .miso_io_num = BOARD_IO_SPI3_MISO,
        .mosi_io_num = BOARD_IO_SPI3_MOSI,
        .sclk_io_num = BOARD_IO_SPI3_SCK,
    };
    s_spi3_bus_handle = spi_bus_create(SPI3_HOST, &bus3_conf);
    BOARD_CHECK(s_spi3_bus_handle != NULL, "spi_bus3 creat failed", ESP_FAIL);
#endif
#if(CONFIG_BOARD_SPI2_INIT)
    spi_config_t bus2_conf = {
        .miso_io_num = BOARD_IO_SPI2_MISO,
        .mosi_io_num = BOARD_IO_SPI2_MOSI,
        .sclk_io_num = BOARD_IO_SPI2_SCK,
        .max_transfer_sz = BOARD_LCD_WIDTH * BOARD_LCD_HEIGHT * 2+64,
    };
    s_spi2_bus_handle = spi_bus_create(SPI2_HOST, &bus2_conf);
    BOARD_CHECK(s_spi2_bus_handle != NULL, "spi_bus2 creat failed", ESP_FAIL);
#endif
    return ESP_OK;
}

static esp_err_t board_spi_bus_deinit(void)
{
    if (s_spi2_bus_handle != NULL) {
        spi_bus_delete(&s_spi2_bus_handle);
        BOARD_CHECK(s_spi2_bus_handle == NULL, "spi bus2 delete failed", ESP_FAIL);
    }
    if (s_spi3_bus_handle != NULL) {
        spi_bus_delete(&s_spi3_bus_handle);
        BOARD_CHECK(s_spi3_bus_handle == NULL, "spi bus3 delete failed", ESP_FAIL);
    }
    return ESP_OK;
}

static esp_err_t board_gpio_init(void)
{
    if (s_board_gpio_isinit) {
        return ESP_OK;
    }
    esp_err_t ret;
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
    ret = gpio_config(&io_conf);

    if (ret != ESP_OK) {
        return ret;
    }

    //set as output mode
    io_conf.mode = GPIO_MODE_INPUT;
    //bit mask of the pins that you want to set
    io_conf.pin_bit_mask = BOARD_IO_PIN_SEL_INPUT;
    //configure GPIO with the given settings
    ret = gpio_config(&io_conf);
    
    if (ret == ESP_OK) {
        s_board_gpio_isinit = true;
    }

    return ret;
}

static esp_err_t board_gpio_deinit(void)
{
    if (!s_board_gpio_isinit) {
        return ESP_OK;
    }
    s_board_gpio_isinit = false;
    return ESP_OK;
}


static esp_err_t board_adc_init(void)
{
    //Check if Two Point or Vref are burned into eFuse
    _check_efuse();

    //Configure ADC

    adc1_config_width(BOARD_ADC_WIDTH);
    adc1_config_channel_atten(BOARD_ADC_HOST_VOL_CHAN, BOARD_ADC_ATTEN);

    //Characterize ADC
    adc1_chars = calloc(1, sizeof(esp_adc_cal_characteristics_t));
    esp_adc_cal_value_t val_type = esp_adc_cal_characterize(BOARD_ADC_UNIT, BOARD_ADC_ATTEN, BOARD_ADC_WIDTH, DEFAULT_VREF, adc1_chars);
    _print_char_val_type(val_type);

    return ESP_OK;
}

static esp_err_t board_adc_deinit()
{
    return ESP_OK;//TODO:
}

static esp_err_t board_button_init()
{
#ifdef CONFIG_BOARD_BTN_INIT
    button_config_t cfg = {
        .type = BUTTON_TYPE_GPIO,
        .gpio_button_config = {
            .gpio_num = BOARD_IO_BUTTON_OK,
            .active_level = 0,
        },
    };
    s_btn_ok_hdl = iot_button_create(&cfg);
    BOARD_CHECK(s_btn_ok_hdl != NULL, "button ok create failed", ESP_FAIL);

    cfg.gpio_button_config.gpio_num = BOARD_IO_BUTTON_DW;
    s_btn_dw_hdl = iot_button_create(&cfg);
    BOARD_CHECK(s_btn_dw_hdl != NULL, "button down create failed", ESP_FAIL);

    cfg.gpio_button_config.gpio_num = BOARD_IO_BUTTON_UP;
    s_btn_up_hdl = iot_button_create(&cfg);
    BOARD_CHECK(s_btn_up_hdl != NULL, "button up create failed", ESP_FAIL);

    cfg.gpio_button_config.gpio_num = BOARD_IO_BUTTON_MENU;
    s_btn_menu_hdl = iot_button_create(&cfg);
    BOARD_CHECK(s_btn_menu_hdl != NULL, "button menu create failed", ESP_FAIL);
#endif
    return ESP_OK;
}

static esp_err_t board_button_deinit()
{
    return ESP_OK;//TODO:
}


/****General board level API ****/
esp_err_t iot_board_init(void)
{
    if(s_board_is_init) {
        return ESP_OK;
    }

    esp_err_t ret = board_gpio_init();
    BOARD_CHECK(ret == ESP_OK, "gpio init failed", ret);

    ret = board_button_init();
    BOARD_CHECK(ret == ESP_OK, "button init failed", ret);

    ret = board_adc_init();
    BOARD_CHECK(ret == ESP_OK, "ADC init failed", ret);

    iot_board_led_all_set_state(false);

    ret = board_i2c_bus_init();
    BOARD_CHECK(ret == ESP_OK, "i2c init failed", ret);

    ret = board_spi_bus_init();
    BOARD_CHECK(ret == ESP_OK, "spi init failed", ret);

    s_board_is_init = true;
    ESP_LOGI(TAG,"Board Info: %s", iot_board_get_info());
    ESP_LOGI(TAG,"Board Init Done ...");
    return ESP_OK;
}

esp_err_t iot_board_deinit(void)
{
    if(!s_board_is_init) {
        return ESP_OK;
    }

    esp_err_t ret = board_gpio_deinit();
    BOARD_CHECK(ret == ESP_OK, "gpio de-init failed", ret);

    ret = board_button_deinit();
    BOARD_CHECK(ret == ESP_OK, "button de-init failed", ret);

    ret = board_adc_deinit();
    BOARD_CHECK(ret == ESP_OK, "ADC de-init failed", ret);

    iot_board_led_all_set_state(false);

    ret = board_i2c_bus_deinit();
    BOARD_CHECK(ret == ESP_OK, "i2c de-init failed", ret);

    ret = board_spi_bus_deinit();
    BOARD_CHECK(ret == ESP_OK, "spi de-init failed", ret);

    s_board_is_init = false;
    ESP_LOGI(TAG," Board Deinit Done ...");
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
    case BOARD_SPI3_ID:
        handle = (board_res_handle_t)s_spi3_bus_handle;
        break;
    case BOARD_BTN_OK_ID:
        handle = (board_res_handle_t)s_btn_ok_hdl;
        break;
    case BOARD_BTN_UP_ID:
        handle = (board_res_handle_t)s_btn_up_hdl;
        break;
    case BOARD_BTN_DW_ID:
        handle = (board_res_handle_t)s_btn_dw_hdl;
        break;
    case BOARD_BTN_MN_ID:
        handle = (board_res_handle_t)s_btn_menu_hdl;
        break;
    default:
        handle = NULL;
        break;
    }
    return handle;
}

char* iot_board_get_info()
{
    static char* info = BOARD_NAME;
    return info;
}

/****Extended board level API ****/
esp_err_t iot_board_usb_device_set_power(bool on_off, bool from_battery)
{
    if (!s_board_gpio_isinit) {
        return ESP_FAIL;
    }

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

bool iot_board_usb_device_get_power(void)
{
    if (!s_board_gpio_isinit) {
        return 0;
    }
    return gpio_get_level(BOARD_IO_IDEV_LIMIT_EN);
}

float iot_board_get_host_voltage(void)
{
    BOARD_CHECK(adc1_chars != NULL, "ADC not inited", 0.0);
    uint32_t adc_reading = 0;
    //Multisampling
    for (int i = 0; i < NO_OF_SAMPLES; i++) {
        adc_reading += adc1_get_raw((adc1_channel_t)BOARD_ADC_HOST_VOL_CHAN);
    }
    adc_reading /= NO_OF_SAMPLES;
    //Convert adc_reading to voltage in mV
    uint32_t voltage = esp_adc_cal_raw_to_voltage(adc_reading, adc1_chars);
    return voltage * 370.0 / 100.0 / 1000.0;
}

float iot_board_get_battery_voltage(void)
{
    BOARD_CHECK(adc1_chars != NULL, "ADC not inited", 0.0);
    uint32_t adc_reading = 0;
    //Multisampling
    for (int i = 0; i < NO_OF_SAMPLES; i++) {
        adc_reading += adc1_get_raw((adc1_channel_t)BOARD_ADC_HOST_VOL_CHAN);
    }
    adc_reading /= NO_OF_SAMPLES;
    //Convert adc_reading to voltage in mV
    uint32_t voltage = esp_adc_cal_raw_to_voltage(adc_reading, adc1_chars);
    return voltage * 200.0 / 100.0 / 1000.0;
}

esp_err_t iot_board_button_register_cb(board_res_handle_t btn_handle, button_event_t event, button_cb_t cb)
{
    BOARD_CHECK(btn_handle != NULL, "invalid button handle", ESP_ERR_INVALID_ARG);
    return iot_button_register_cb((board_res_handle_t)btn_handle, event, cb);
}

esp_err_t iot_board_button_unregister_cb(board_res_handle_t btn_handle, button_event_t event)
{
    BOARD_CHECK(btn_handle != NULL, "invalid button handle", ESP_ERR_INVALID_ARG);
    return iot_button_unregister_cb((board_res_handle_t)btn_handle, event);
}

esp_err_t iot_board_usb_set_mode(usb_mode_t mode)
{
    if (!s_board_gpio_isinit) {
        return ESP_FAIL;
    }

    switch (mode)
    {
        case USB_DEVICE_MODE:
            gpio_set_level(BOARD_IO_USB_SEL, false); //USB_SEL
            break;
        case USB_HOST_MODE:
            gpio_set_level(BOARD_IO_USB_SEL, true); //USB_SEL
            break;
        default:
            assert(0);
            break;
    }
    return ESP_OK;
}

usb_mode_t iot_board_usb_get_mode(void)
{
    if (!s_board_gpio_isinit) {
        return USB_DEVICE_MODE;
    }

    int value = gpio_get_level(BOARD_IO_USB_SEL);
    if (value) {
        return USB_HOST_MODE;
    }
    return USB_DEVICE_MODE;
}

static int s_led_io[BOARD_LED_NUM]={BOARD_IO_LED_1, BOARD_IO_LED_2};
static int s_led_polarity[BOARD_LED_NUM]={BOARD_LED_POLARITY_1, BOARD_LED_POLARITY_2};
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