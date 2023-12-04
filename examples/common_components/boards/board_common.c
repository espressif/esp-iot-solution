/*
 * SPDX-FileCopyrightText: 2022-2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include "esp_log.h"
#include "board_common.h"
#include "led_strip.h"
#include "iot_board.h"

#define BOARD_IO_LED(index) BOARD_IO_LED_##index
#define BOARD_LED_TYPE(index) BOARD_LED_TYPE_##index
#define BOARD_LED_POLARITY(index) BOARD_LED_POLARITY_##index
#define BOARD_IO_BUTTON(index) BOARD_IO_BUTTON_##index
#define BOARD_BUTTON_TYPE(index) BOARD_BUTTON_TYPE_##index

/* default definitions */
#ifndef BOARD_LED_NUM
#if defined(BOARD_IO_LED_6)
#define BOARD_LED_NUM 6
#elif defined(BOARD_IO_LED_5)
#define BOARD_LED_NUM 5
#elif defined(BOARD_IO_LED_4)
#define BOARD_LED_NUM 4
#elif defined(BOARD_IO_LED_3)
#define BOARD_LED_NUM 3
#elif defined(BOARD_IO_LED_2)
#define BOARD_LED_NUM 2
#elif defined(BOARD_IO_LED_1)
#define BOARD_LED_NUM 1
#else
#define BOARD_LED_NUM 0
#endif
#endif

#ifndef BOARD_LED_POLARITY_1
#define BOARD_LED_POLARITY_1 _POSITIVE
#endif
#ifndef BOARD_LED_POLARITY_2
#define BOARD_LED_POLARITY_2 _POSITIVE
#endif
#ifndef BOARD_LED_POLARITY_3
#define BOARD_LED_POLARITY_3 _POSITIVE
#endif
#ifndef BOARD_LED_POLARITY_4
#define BOARD_LED_POLARITY_4 _POSITIVE
#endif
#ifndef BOARD_LED_POLARITY_5
#define BOARD_LED_POLARITY_5 _POSITIVE
#endif
#ifndef BOARD_LED_POLARITY_6
#define BOARD_LED_POLARITY_6 _POSITIVE
#endif

#ifndef BOARD_LED_TYPE_1
#define BOARD_LED_TYPE_1 LED_TYPE_GPIO
#endif
#ifndef BOARD_LED_TYPE_2
#define BOARD_LED_TYPE_2 LED_TYPE_GPIO
#endif
#ifndef BOARD_LED_TYPE_3
#define BOARD_LED_TYPE_3 LED_TYPE_GPIO
#endif
#ifndef BOARD_LED_TYPE_4
#define BOARD_LED_TYPE_4 LED_TYPE_GPIO
#endif
#ifndef BOARD_LED_TYPE_5
#define BOARD_LED_TYPE_5 LED_TYPE_GPIO
#endif
#ifndef BOARD_LED_TYPE_6
#define BOARD_LED_TYPE_6 LED_TYPE_GPIO
#endif

#ifndef BOARD_BUTTON_NUM
#if defined(BOARD_IO_BUTTON_6)
#define BOARD_BUTTON_NUM 6
#elif defined(BOARD_IO_BUTTON_5)
#define BOARD_BUTTON_NUM 5
#elif defined(BOARD_IO_BUTTON_4)
#define BOARD_BUTTON_NUM 4
#elif defined(BOARD_IO_BUTTON_3)
#define BOARD_BUTTON_NUM 3
#elif defined(BOARD_IO_BUTTON_2)
#define BOARD_BUTTON_NUM 2
#elif defined(BOARD_IO_BUTTON_1)
#define BOARD_BUTTON_NUM 1
#else
#define BOARD_BUTTON_NUM 0
#endif
#endif

#ifndef BOARD_BUTTON_POLARITY_1
#define BOARD_BUTTON_POLARITY_1 _NEGATIVE
#endif
#ifndef BOARD_BUTTON_POLARITY_2
#define BOARD_BUTTON_POLARITY_2 _NEGATIVE
#endif
#ifndef BOARD_BUTTON_POLARITY_3
#define BOARD_BUTTON_POLARITY_3 _NEGATIVE
#endif
#ifndef BOARD_BUTTON_POLARITY_4
#define BOARD_BUTTON_POLARITY_4 _NEGATIVE
#endif
#ifndef BOARD_BUTTON_POLARITY_5
#define BOARD_BUTTON_POLARITY_5 _NEGATIVE
#endif
#ifndef BOARD_BUTTON_POLARITY_6
#define BOARD_BUTTON_POLARITY_6 _NEGATIVE
#endif

#ifndef BOARD_NAME
#define BOARD_NAME "UNDEFINED"
#endif
#ifndef BOARD_VENDOR
#define BOARD_VENDOR "UNDEFINED"
#endif
#ifndef BOARD_URL
#define BOARD_URL "UNDEFINED"
#endif
#define BOARD_INFO ("BOARD_NAME: " BOARD_NAME "\n" "BOARD_VENDOR: " BOARD_VENDOR "\n" "BOARD_URL: " BOARD_URL "\n")

#define BOARD_LED_CONFIG(index) \
    { .io_num = BOARD_IO_LED(index),\
      .type = BOARD_LED_TYPE(index),\
      .state = 0,\
      .polarity = BOARD_LED_POLARITY(index) }

#define BOARD_BUTTON_CONFIG(index) \
    { .io_num = BOARD_IO_BUTTON(index), \
      .polarity = BOARD_BUTTON_POLARITY(index) }

typedef struct {
    uint8_t io_num;
    uint8_t type;
    uint8_t state;
    union {
        uint8_t polarity;
        led_strip_t *p_strip;
    };
} board_led_t;

typedef struct {
    uint8_t io_num;
    uint8_t polarity;
    button_handle_t handle;
} board_button_t;

#if defined(CONFIG_BOARD_LED_INIT) && (BOARD_LED_NUM > 0)
static board_led_t s_led[] = {
    BOARD_LED_CONFIG(1),
#if BOARD_LED_NUM > 1
    BOARD_LED_CONFIG(2),
#endif
#if BOARD_LED_NUM > 2
    BOARD_LED_CONFIG(3),
#endif
#if BOARD_LED_NUM > 3
    BOARD_LED_CONFIG(4),
#endif
#if BOARD_LED_NUM > 4
    BOARD_LED_CONFIG(5),
#endif
#if BOARD_LED_NUM > 5
    BOARD_LED_CONFIG(6),
#endif
#if BOARD_LED_NUM > 6
#error "Please increase length if required"
#endif
};
#endif

#if defined(CONFIG_BOARD_BUTTON_INIT) && (BOARD_BUTTON_NUM > 0)
const static int s_button[] = {
    BOARD_BUTTON_CONFIG(1),
#if BOARD_BUTTON_NUM > 1
    BOARD_BUTTON_CONFIG(2),
#endif
#if BOARD_BUTTON_NUM > 2
    BOARD_BUTTON_CONFIG(3),
#endif
#if BOARD_BUTTON_NUM > 3
    BOARD_BUTTON_CONFIG(4),
#endif
#if BOARD_BUTTON_NUM > 4
    BOARD_BUTTON_CONFIG(5),
#endif
#if BOARD_BUTTON_NUM > 5
    BOARD_BUTTON_CONFIG(6),
#endif
#if BOARD_BUTTON_NUM > 6
#error "Please increase length if required"
#endif
};
#endif

/**** Private handle ****/
static const char *TAG = "Board_Common";
static i2c_bus_handle_t s_i2c0_bus_handle = NULL;
static spi_bus_handle_t s_spi2_bus_handle = NULL;
static bool s_board_is_init = false;
static bool s_board_led_is_init = false;
static bool s_board_button_is_init = false;
static bool s_board_gpio_is_init = false;

/* board specific callbacks */
static board_specific_callbacks_t s_board_callbacks = {
    .pre_init_cb = NULL,
    .pre_deinit_cb = NULL,
    .post_init_cb = NULL,
    .post_deinit_cb = NULL,
    .get_handle_cb = NULL,
};

static esp_err_t board_gpio_init(void)
{
    BOARD_CHECK(!s_board_gpio_is_init, "gpio not inited", ESP_ERR_INVALID_STATE);
    esp_err_t ret = ESP_OK;
#ifdef CONFIG_BOARD_GPIO_INIT
    gpio_config_t io_conf = {
        //disable interrupt
        .intr_type = GPIO_INTR_DISABLE,
        //disable pull-down mode
        .pull_down_en = 0,
        //disable pull-up mode
        .pull_up_en = 0,
    };

#ifdef BOARD_IO_PIN_SEL_OUTPUT
    //configure GPIO with the given settings
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pin_bit_mask = BOARD_IO_PIN_SEL_OUTPUT;
    ret = gpio_config(&io_conf);
    BOARD_CHECK(ret == ESP_OK, "gpio out config failed", ret);
    ESP_LOGI(TAG, "gpio out init succeed");
    s_board_gpio_is_init = true;
#endif

#ifdef BOARD_IO_PIN_SEL_INPUT
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pin_bit_mask = BOARD_IO_PIN_SEL_INPUT;
    ret = gpio_config(&io_conf);
    BOARD_CHECK(ret == ESP_OK, "gpio in config failed", ret);
    ESP_LOGI(TAG, "gpio in init succeed");
    s_board_gpio_is_init = true;
#endif

#if !(BOARD_IO_PIN_SEL_INPUT || BOARD_IO_PIN_SEL_OUTPUT)
    ESP_LOGW(TAG, "no gpio defined");
    (void) io_conf;
    s_board_gpio_is_init = false;
#endif

#endif
    return ret;
}

static esp_err_t board_gpio_deinit(void)
{
    BOARD_CHECK(s_board_gpio_is_init, "gpio not inited", ESP_ERR_INVALID_STATE);
#ifdef CONFIG_BOARD_GPIO_INIT
    s_board_gpio_is_init = false;
    ESP_LOGI(TAG, "gpio deinit succeed");
#endif
    return ESP_OK;
}

static esp_err_t board_i2c_bus_init(void)
{
#if defined(CONFIG_BOARD_I2C0_INIT) && defined(BOARD_IO_I2C0_SDA) && defined(BOARD_IO_I2C0_SCL)
    i2c_config_t board_i2c_conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = BOARD_IO_I2C0_SDA,
        .sda_pullup_en = CONFIG_BOARD_I2C0_SDA_PULLUP_EN,
        .scl_io_num = BOARD_IO_I2C0_SCL,
        .scl_pullup_en = CONFIG_BOARD_I2C0_SCL_PULLUP_EN,
        .master.clk_speed = CONFIG_BOARD_I2C0_SPEED,
    };
    i2c_bus_handle_t handle = i2c_bus_create(I2C_NUM_0, &board_i2c_conf);
    BOARD_CHECK(handle != NULL, "i2c_bus create failed", ESP_FAIL);
    s_i2c0_bus_handle = handle;
    ESP_LOGI(TAG, "i2c bus create succeed");
#elif defined(CONFIG_BOARD_I2C0_INIT)
    ESP_LOGW(TAG, "i2c0 Init, but no i2c0 io defined");
#endif
    return ESP_OK;
}

static esp_err_t board_i2c_bus_deinit(void)
{
    if (s_i2c0_bus_handle != NULL) {
        i2c_bus_delete(&s_i2c0_bus_handle);
        BOARD_CHECK(s_i2c0_bus_handle == NULL, "i2c_bus delete failed", ESP_FAIL);
        ESP_LOGI(TAG, "i2c bus delete succeed");
    }
    return ESP_OK;
}

static esp_err_t board_spi_bus_init(void)
{
#if defined(CONFIG_BOARD_SPI2_INIT) && defined(BOARD_IO_SPI2_MISO) && defined(BOARD_IO_SPI2_MOSI) && defined(BOARD_IO_SPI2_SCK)
    spi_config_t bus_conf = {
        .miso_io_num = BOARD_IO_SPI2_MISO,
        .mosi_io_num = BOARD_IO_SPI2_MOSI,
        .sclk_io_num = BOARD_IO_SPI2_SCK,
        .max_transfer_sz = CONFIG_BOARD_SPI2_MAX_TXF_SIZE
    };
    s_spi2_bus_handle = spi_bus_create(SPI2_HOST, &bus_conf);
    BOARD_CHECK(s_spi2_bus_handle != NULL, "spi_bus2 create failed", ESP_FAIL);
    ESP_LOGI(TAG, "spi2 bus create succeed");
#elif defined(CONFIG_BOARD_SPI2_INIT)
    ESP_LOGW(TAG, "spi2 Init, but no spi2 io defined");
#endif
    return ESP_OK;
}

static esp_err_t board_spi_bus_deinit(void)
{
    if (s_spi2_bus_handle != NULL) {
        spi_bus_delete(&s_spi2_bus_handle);
        BOARD_CHECK(s_spi2_bus_handle == NULL, "spi2 bus delete failed", ESP_FAIL);
        ESP_LOGI(TAG, "spi2 bus delete succeed");
    }
    return ESP_OK;
}

static esp_err_t board_led_init(void)
{
#if defined(CONFIG_BOARD_LED_INIT) && (BOARD_LED_NUM > 0)
    ESP_LOGI(TAG, "BOARD_LED_NUM = %d", BOARD_LED_NUM);
    gpio_config_t io_conf = {
        //disable interrupt
        .intr_type = GPIO_INTR_DISABLE,
        //disable pull-down mode
        .pull_down_en = 0,
        //disable pull-up mode
        .pull_up_en = 0,
        .mode = GPIO_MODE_OUTPUT,
    };

    for (size_t i = 0; i < BOARD_LED_NUM; i++) {
        if (s_led[i].type == LED_TYPE_GPIO) {
            io_conf.pin_bit_mask = 1ULL << s_led[i].io_num;
            esp_err_t ret = gpio_config(&io_conf);
            BOARD_CHECK(ret == ESP_OK, "led config failed", ret);
            ESP_LOGI(TAG, "led init succeed io=%d", i);
        } else {
            /* LED strip initialization with the GPIO and pixels number*/
            static int rmt_channel = 0;
            BOARD_CHECK(rmt_channel < SOC_RMT_CHANNELS_PER_GROUP, "led no free rmt channel", ESP_FAIL);
            s_led[i].p_strip = led_strip_init(rmt_channel, s_led[i].io_num, 1);
            BOARD_CHECK(s_led[i].p_strip != NULL, "led(RGB) config failed", ESP_FAIL);
            ++rmt_channel;
            /* Set all LED off to clear all pixels */
            s_led[i].p_strip->clear(s_led[i].p_strip, 50);
            ESP_LOGI(TAG, "led(RGB) init succeed io=%d", s_led[i].io_num);
        }
    }

    s_board_led_is_init = true;
#elif defined(CONFIG_BOARD_LED_INIT)
    ESP_LOGW(TAG, "LED Init, but no LED defined");
#endif
    return ESP_OK;
}

static esp_err_t board_led_deinit(void)
{
#if defined(CONFIG_BOARD_LED_INIT) && (BOARD_LED_NUM > 0)
    BOARD_CHECK(s_board_led_is_init, "led not inited", ESP_ERR_INVALID_STATE);
    for (size_t i = 0; i < BOARD_LED_NUM; i++) {
        if (s_led[i].type == LED_TYPE_RGB) {
            esp_err_t ret = led_strip_denit(s_led[i].p_strip);
            BOARD_CHECK(ret == ESP_OK, "led(RGB) deinit failed", ret);
        }
    }
    s_board_led_is_init = false;
    ESP_LOGI(TAG, "led deinit succeed");
#endif
    return ESP_OK;
}

static button_handle_t board_button_io_to_handle(int gpio_num)
{
#if defined(CONFIG_BOARD_BUTTON_INIT) && (BOARD_BUTTON_NUM > 0)
    size_t i = 0;
    bool if_find = false;
    for (i = 0; i < BOARD_BUTTON_NUM; i++) {
        if (gpio_num == s_button[i].io_num) {
            if_find = true;
            break;
        }
    }

    if (!if_find) {
        ESP_LOGW(TAG, "Button not found io=%d", gpio_num);
        return NULL;
    }

    return s_button[i].handle;
#endif
    return NULL;
}

static esp_err_t board_button_init()
{
#if defined(CONFIG_BOARD_BUTTON_INIT) && (BOARD_BUTTON_NUM > 0)
    button_config_t cfg = {
        .type = BUTTON_TYPE_GPIO,
        .gpio_button_config = {
            .gpio_num = 0,
            .active_level = 0,
        },
    };
    for (size_t i = 0; i < BOARD_BUTTON_NUM; i++) {
        cfg.gpio_button_config.gpio_num = s_button[i].io_num;
        cfg.gpio_button_config.active_level = s_button[i].polarity;
        s_button[i].handle = iot_button_create(&cfg);
        BOARD_CHECK(s_button[i].handle != NULL, "button ok create failed", ESP_FAIL);
        ESP_LOGI(TAG, "button init succeed, io=%d plr=%d",  s_button[i].io_num, s_button[i].polarity);
    }
    s_board_button_is_init = true;
#elif defined(CONFIG_BOARD_BUTTON_INIT)
    ESP_LOGW(TAG, "button Init, but no button defined");
#endif
    return ESP_OK;
}

static esp_err_t board_button_deinit()
{
#if defined(CONFIG_BOARD_BUTTON_INIT) && (BOARD_BUTTON_NUM > 0)
    BOARD_CHECK(s_board_button_is_init, "adc not inited", ESP_ERR_INVALID_STATE);
    s_board_button_is_init = false;
    ESP_LOGI(TAG, "button deinit succeed");
#endif
    return ESP_OK;
}

ATTR_WEAK esp_err_t iot_board_init(void)
{
    ESP_LOGD(TAG, "%s", __func__);
    BOARD_CHECK(!s_board_is_init, "board has inited", ESP_ERR_INVALID_STATE);
    ESP_LOGI(TAG, "Board Init ...");
#ifdef BOARD_CALLBACKS
    s_board_callbacks.pre_init_cb = BOARD_CALLBACKS.pre_init_cb;
    s_board_callbacks.pre_deinit_cb = BOARD_CALLBACKS.pre_deinit_cb;
    s_board_callbacks.post_init_cb = BOARD_CALLBACKS.post_init_cb;
    s_board_callbacks.post_deinit_cb = BOARD_CALLBACKS.post_deinit_cb;
    s_board_callbacks.get_handle_cb = BOARD_CALLBACKS.get_handle_cb;
    ESP_LOGI(TAG, "register board specified callbacks");
#endif
    esp_err_t ret = ESP_OK;
    if (s_board_callbacks.pre_init_cb) {
        ret = s_board_callbacks.pre_init_cb();
        BOARD_CHECK(ret == ESP_OK, "board specific pre_init failed", ret);
    }

    ret = board_gpio_init();
    BOARD_CHECK(ret == ESP_OK, "gpio init failed", ret);

    ret = board_led_init();
    BOARD_CHECK(ret == ESP_OK, "led init failed", ret);

    ret = board_button_init();
    BOARD_CHECK(ret == ESP_OK, "button init failed", ret);

    ret = board_i2c_bus_init();
    BOARD_CHECK(ret == ESP_OK, "i2c init failed", ret);

    ret = board_spi_bus_init();
    BOARD_CHECK(ret == ESP_OK, "spi init failed", ret);

    if (s_board_callbacks.post_init_cb) {
        ret = s_board_callbacks.post_init_cb();
        BOARD_CHECK(ret == ESP_OK, "board specific post_init failed", ret);
    }

    s_board_is_init = true;
    ESP_LOGI(TAG, "Board Info: \n\n%s", iot_board_get_info());
    ESP_LOGI(TAG, "Board Init Done !");
    return ESP_OK;
}

ATTR_WEAK esp_err_t iot_board_deinit(void)
{
    ESP_LOGD(TAG, "%s", __func__);
    BOARD_CHECK(s_board_is_init, "board not inited", ESP_ERR_INVALID_STATE);

    esp_err_t ret = ESP_OK;
    if (s_board_callbacks.pre_deinit_cb) {
        ret = s_board_callbacks.pre_deinit_cb();
        BOARD_CHECK(ret == ESP_OK, "board specific pre_deinit failed", ret);
    }

    ret = board_gpio_deinit();
    BOARD_CHECK(ret == ESP_OK, "gpio de-init failed", ret);

    ret = board_led_deinit();
    BOARD_CHECK(ret == ESP_OK, "led deinit failed", ret);

    ret = board_button_deinit();
    BOARD_CHECK(ret == ESP_OK, "button deinit failed", ret);

    ret = board_i2c_bus_deinit();
    BOARD_CHECK(ret == ESP_OK, "i2c deinit failed", ret);

    ret = board_spi_bus_deinit();
    BOARD_CHECK(ret == ESP_OK, "spi deinit failed", ret);

    if (s_board_callbacks.post_deinit_cb) {
        ret = s_board_callbacks.post_deinit_cb();
        BOARD_CHECK(ret == ESP_OK, "board specific post_deinit failed", ret);
    }

    s_board_is_init = false;
    ESP_LOGI(TAG, "Board Deinit Done ...");
    return ESP_OK;
}

ATTR_WEAK bool iot_board_is_init(void)
{
    ESP_LOGD(TAG, "%s", __func__);
    return s_board_is_init;
}

ATTR_WEAK board_res_handle_t iot_board_get_handle(int id)
{
    ESP_LOGD(TAG, "%s", __func__);
    board_res_handle_t handle = NULL;
    switch (id) {
    case BOARD_I2C0_ID:
        handle = (board_res_handle_t)s_i2c0_bus_handle;
        break;
    case BOARD_SPI2_ID:
        handle = (board_res_handle_t)s_spi2_bus_handle;
        break;
    default:
        if (s_board_callbacks.get_handle_cb) {
            handle = s_board_callbacks.get_handle_cb(id);
        }
        break;
    }
    return handle;
}

ATTR_WEAK char* iot_board_get_info()
{
    ESP_LOGD(TAG, "%s", __func__);
    return BOARD_INFO;
}

ATTR_WEAK esp_err_t iot_board_led_set_state(int gpio_num, bool if_on)
{
    ESP_LOGD(TAG, "%s io%d=%d", __func__, gpio_num, if_on);
    BOARD_CHECK(s_board_led_is_init, "led control gpio not inited", ESP_FAIL);
#if defined(CONFIG_BOARD_LED_INIT) && (BOARD_LED_NUM > 0)
    int i = 0;
    for (i = 0; i < BOARD_LED_NUM; i++) {
        if (s_led[i].io_num == gpio_num) {
            break;
        }
    }

    if (i >= BOARD_LED_NUM) {
        ESP_LOGE(TAG, "GPIO %d is not a valid LED", gpio_num);
        return ESP_FAIL;
    }

    if (s_led[i].type == LED_TYPE_GPIO) {
        if (!s_led[i].polarity) { /*check led polarity*/
            if_on = !if_on;
        }
        gpio_set_level(gpio_num, if_on);
    } else {
        if (if_on) {
            /* Set the LED pixel using RGB from 0 (0%) to 255 (100%) for each color */
            s_led[i].p_strip->set_pixel(s_led[i].p_strip, 0, 16, 16, 16);
            /* Refresh the strip to send data */
            s_led[i].p_strip->refresh(s_led[i].p_strip, 100);
        } else {
            /* Set all LED off to clear all pixels */
            s_led[i].p_strip->clear(s_led[i].p_strip, 50);
        }
    }
    s_led[i].state = if_on;
#endif
    return ESP_OK;
}

ATTR_WEAK esp_err_t iot_board_led_all_set_state(bool if_on)
{
    ESP_LOGD(TAG, "%s %d", __func__, if_on);
    BOARD_CHECK(s_board_led_is_init, "led control gpio not inited", ESP_FAIL);
#if defined(CONFIG_BOARD_LED_INIT) && (BOARD_LED_NUM > 0)
    for (size_t i = 0; i < BOARD_LED_NUM; i++) {
        iot_board_led_set_state(s_led[i].io_num, if_on);
    }
#endif
    return ESP_OK;
}

ATTR_WEAK esp_err_t iot_board_led_toggle_state(int gpio_num)
{
    ESP_LOGD(TAG, "%s io%d", __func__, gpio_num);
    BOARD_CHECK(s_board_led_is_init, "led control gpio not inited", ESP_FAIL);
#if defined(CONFIG_BOARD_LED_INIT) && (BOARD_LED_NUM > 0)
    size_t i = 0;
    for (; i < BOARD_LED_NUM; i++) {
        if (s_led[i].io_num == gpio_num) {
            break;
        }
    }
    iot_board_led_set_state(gpio_num, !s_led[i].state);
#endif
    return ESP_OK;
}

ATTR_WEAK esp_err_t iot_board_button_register_cb(int gpio_num, button_event_t event, button_cb_t cb)
{
    ESP_LOGD(TAG, "%s", __func__);
    BOARD_CHECK(s_board_button_is_init, "button not inited", ESP_FAIL);
    button_handle_t btn_handle = board_button_io_to_handle(gpio_num);
    BOARD_CHECK(btn_handle != NULL && event < BUTTON_NONE_PRESS && cb != NULL, "invalid args", ESP_ERR_INVALID_ARG);
    return iot_button_register_cb(btn_handle, event, cb, NULL);
}

ATTR_WEAK esp_err_t iot_board_button_unregister_cb(int gpio_num, button_event_t event)
{
    ESP_LOGD(TAG, "%s", __func__);
    BOARD_CHECK(s_board_button_is_init, "button not inited", ESP_FAIL);
    button_handle_t btn_handle = board_button_io_to_handle(gpio_num);
    BOARD_CHECK(btn_handle != NULL && event < BUTTON_NONE_PRESS, "invalid args", ESP_ERR_INVALID_ARG);
    return iot_button_unregister_cb(btn_handle, event);
}
