/*
 * SPDX-FileCopyrightText: 2015-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_check.h"
#include "driver/gpio.h"
#include "driver/i2c.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_touch.h"

static const char *TAG = "FT5x06";

/* Registers */
#define FT5x06_DEVICE_MODE      (0x00)
#define FT5x06_GESTURE_ID       (0x01)
#define FT5x06_TOUCH_POINTS     (0x02)

#define FT5x06_TOUCH1_EV_FLAG   (0x03)
#define FT5x06_TOUCH1_XH        (0x03)
#define FT5x06_TOUCH1_XL        (0x04)
#define FT5x06_TOUCH1_YH        (0x05)
#define FT5x06_TOUCH1_YL        (0x06)

#define FT5x06_TOUCH2_EV_FLAG   (0x09)
#define FT5x06_TOUCH2_XH        (0x09)
#define FT5x06_TOUCH2_XL        (0x0A)
#define FT5x06_TOUCH2_YH        (0x0B)
#define FT5x06_TOUCH2_YL        (0x0C)

#define FT5x06_TOUCH3_EV_FLAG   (0x0F)
#define FT5x06_TOUCH3_XH        (0x0F)
#define FT5x06_TOUCH3_XL        (0x10)
#define FT5x06_TOUCH3_YH        (0x11)
#define FT5x06_TOUCH3_YL        (0x12)

#define FT5x06_TOUCH4_EV_FLAG   (0x15)
#define FT5x06_TOUCH4_XH        (0x15)
#define FT5x06_TOUCH4_XL        (0x16)
#define FT5x06_TOUCH4_YH        (0x17)
#define FT5x06_TOUCH4_YL        (0x18)

#define FT5x06_TOUCH5_EV_FLAG   (0x1B)
#define FT5x06_TOUCH5_XH        (0x1B)
#define FT5x06_TOUCH5_XL        (0x1C)
#define FT5x06_TOUCH5_YH        (0x1D)
#define FT5x06_TOUCH5_YL        (0x1E)

#define FT5x06_ID_G_THGROUP             (0x80)
#define FT5x06_ID_G_THPEAK              (0x81)
#define FT5x06_ID_G_THCAL               (0x82)
#define FT5x06_ID_G_THWATER             (0x83)
#define FT5x06_ID_G_THTEMP              (0x84)
#define FT5x06_ID_G_THDIFF              (0x85)
#define FT5x06_ID_G_CTRL                (0x86)
#define FT5x06_ID_G_TIME_ENTER_MONITOR  (0x87)
#define FT5x06_ID_G_PERIODACTIVE        (0x88)
#define FT5x06_ID_G_PERIODMONITOR       (0x89)
#define FT5x06_ID_G_AUTO_CLB_MODE       (0xA0)
#define FT5x06_ID_G_LIB_VERSION_H       (0xA1)
#define FT5x06_ID_G_LIB_VERSION_L       (0xA2)
#define FT5x06_ID_G_CIPHER              (0xA3)
#define FT5x06_ID_G_MODE                (0xA4)
#define FT5x06_ID_G_PMODE               (0xA5)
#define FT5x06_ID_G_FIRMID              (0xA6)
#define FT5x06_ID_G_STATE               (0xA7)
#define FT5x06_ID_G_FT5201ID            (0xA8)
#define FT5x06_ID_G_ERR                 (0xA9)

/*******************************************************************************
* Function definitions
*******************************************************************************/
static esp_err_t esp_lcd_touch_ft5x06_read_data(esp_lcd_touch_handle_t tp);
static bool esp_lcd_touch_ft5x06_get_xy(esp_lcd_touch_handle_t tp, uint16_t *x, uint16_t *y, uint16_t *strength, uint8_t *track_id, uint8_t *point_num, uint8_t max_point_num);
static esp_err_t esp_lcd_touch_ft5x06_del(esp_lcd_touch_handle_t tp);

/* I2C read */
static esp_err_t touch_ft5x06_i2c_write(esp_lcd_touch_handle_t tp, uint8_t reg, uint8_t data);
static esp_err_t touch_ft5x06_i2c_read(esp_lcd_touch_handle_t tp, uint8_t reg, uint8_t *data, uint8_t len);

/* FT5x06 init */
static esp_err_t touch_ft5x06_init(esp_lcd_touch_handle_t tp);
/* FT5x06 reset */
static esp_err_t touch_ft5x06_reset(esp_lcd_touch_handle_t tp);

/*******************************************************************************
* Public API functions
*******************************************************************************/

esp_err_t esp_lcd_touch_new_i2c_ft5x06(const esp_lcd_panel_io_handle_t io, const esp_lcd_touch_config_t *config, esp_lcd_touch_handle_t *out_touch)
{
    esp_err_t ret = ESP_OK;

    assert(config != NULL);
    assert(out_touch != NULL);

    /* Prepare main structure */
    esp_lcd_touch_handle_t esp_lcd_touch_ft5x06 = heap_caps_calloc(1, sizeof(esp_lcd_touch_t), MALLOC_CAP_DEFAULT);
    ESP_GOTO_ON_FALSE(esp_lcd_touch_ft5x06, ESP_ERR_NO_MEM, err, TAG, "no mem for FT5x06 controller");

    /* Communication interface */
    esp_lcd_touch_ft5x06->io = io;

    /* Only supported callbacks are set */
    esp_lcd_touch_ft5x06->read_data = esp_lcd_touch_ft5x06_read_data;
    esp_lcd_touch_ft5x06->get_xy = esp_lcd_touch_ft5x06_get_xy;
    esp_lcd_touch_ft5x06->del = esp_lcd_touch_ft5x06_del;

    /* Mutex */
    esp_lcd_touch_ft5x06->data.lock.owner = portMUX_FREE_VAL;

    /* Save config */
    memcpy(&esp_lcd_touch_ft5x06->config, config, sizeof(esp_lcd_touch_config_t));

    /* Prepare pin for touch interrupt */
    if (esp_lcd_touch_ft5x06->config.int_gpio_num != GPIO_NUM_NC) {
        const gpio_config_t int_gpio_config = {
            .mode = GPIO_MODE_INPUT,
            .intr_type = (esp_lcd_touch_ft5x06->config.levels.interrupt ? GPIO_INTR_POSEDGE : GPIO_INTR_NEGEDGE),
            .pin_bit_mask = BIT64(esp_lcd_touch_ft5x06->config.int_gpio_num)
        };
        ret = gpio_config(&int_gpio_config);
        ESP_GOTO_ON_ERROR(ret, err, TAG, "GPIO config failed");

        /* Register interrupt callback */
        if (esp_lcd_touch_ft5x06->config.interrupt_callback) {
            esp_lcd_touch_register_interrupt_callback(esp_lcd_touch_ft5x06, esp_lcd_touch_ft5x06->config.interrupt_callback);
        }
    }

    /* Prepare pin for touch controller reset */
    if (esp_lcd_touch_ft5x06->config.rst_gpio_num != GPIO_NUM_NC) {
        const gpio_config_t rst_gpio_config = {
            .mode = GPIO_MODE_OUTPUT,
            .pin_bit_mask = BIT64(esp_lcd_touch_ft5x06->config.rst_gpio_num)
        };
        ret = gpio_config(&rst_gpio_config);
        ESP_GOTO_ON_ERROR(ret, err, TAG, "GPIO config failed");
    }

    /* Reset controller */
    ret = touch_ft5x06_reset(esp_lcd_touch_ft5x06);
    ESP_GOTO_ON_ERROR(ret, err, TAG, "FT5x06 reset failed");

    /* Init controller */
    ret = touch_ft5x06_init(esp_lcd_touch_ft5x06);
    ESP_GOTO_ON_ERROR(ret, err, TAG, "FT5x06 init failed");

err:
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Error (0x%x)! Touch controller FT5x06 initialization failed!", ret);
        if (esp_lcd_touch_ft5x06) {
            esp_lcd_touch_ft5x06_del(esp_lcd_touch_ft5x06);
        }
    }

    *out_touch = esp_lcd_touch_ft5x06;

    return ret;
}

static esp_err_t esp_lcd_touch_ft5x06_read_data(esp_lcd_touch_handle_t tp)
{
    esp_err_t err;
    uint8_t data[30];
    uint8_t points;
    size_t i = 0;

    assert(tp != NULL);

    err = touch_ft5x06_i2c_read(tp, FT5x06_TOUCH_POINTS, &points, 1);
    ESP_RETURN_ON_ERROR(err, TAG, "I2C read error!");

    if (points > 5 || points == 0) {
        return ESP_OK;
    }

    /* Number of touched points */
    points = (points > CONFIG_ESP_LCD_TOUCH_MAX_POINTS ? CONFIG_ESP_LCD_TOUCH_MAX_POINTS : points);

    err = touch_ft5x06_i2c_read(tp, FT5x06_TOUCH1_XH, data, 6 * points);
    ESP_RETURN_ON_ERROR(err, TAG, "I2C read error!");

    portENTER_CRITICAL(&tp->data.lock);

    /* Number of touched points */
    tp->data.points = points;

    /* Fill all coordinates */
    for (i = 0; i < points; i++) {
        tp->data.coords[i].track_id = ((data[(i * 6) + 2] & 0xf0) >> 4);
        tp->data.coords[i].x = (((uint16_t)data[(i * 6) + 0] & 0x0f) << 8) + data[(i * 6) + 1];
        tp->data.coords[i].y = (((uint16_t)data[(i * 6) + 2] & 0x0f) << 8) + data[(i * 6) + 3];
    }

    portEXIT_CRITICAL(&tp->data.lock);

    return ESP_OK;
}

static bool esp_lcd_touch_ft5x06_get_xy(esp_lcd_touch_handle_t tp, uint16_t *x, uint16_t *y, uint16_t *strength, uint8_t *track_id, uint8_t *point_num, uint8_t max_point_num)
{
    assert(tp != NULL);
    assert(x != NULL);
    assert(y != NULL);
    assert(point_num != NULL);
    assert(max_point_num > 0);

    portENTER_CRITICAL(&tp->data.lock);

    /* Count of points */
    *point_num = (tp->data.points > max_point_num ? max_point_num : tp->data.points);

    for (size_t i = 0; i < *point_num; i++) {
        x[i] = tp->data.coords[i].x;
        y[i] = tp->data.coords[i].y;

        if (strength) {
            strength[i] = tp->data.coords[i].strength;
        }

        if (track_id) {
            track_id[i] = tp->data.coords[i].track_id;
        }
    }

    /* Invalidate */
    tp->data.points = 0;

    portEXIT_CRITICAL(&tp->data.lock);

    return (*point_num > 0);
}

static esp_err_t esp_lcd_touch_ft5x06_del(esp_lcd_touch_handle_t tp)
{
    assert(tp != NULL);

    /* Reset GPIO pin settings */
    if (tp->config.int_gpio_num != GPIO_NUM_NC) {
        gpio_reset_pin(tp->config.int_gpio_num);
        if (tp->config.interrupt_callback) {
            gpio_isr_handler_remove(tp->config.int_gpio_num);
        }
    }

    /* Reset GPIO pin settings */
    if (tp->config.rst_gpio_num != GPIO_NUM_NC) {
        gpio_reset_pin(tp->config.rst_gpio_num);
    }

    free(tp);

    return ESP_OK;
}

/*******************************************************************************
* Private API function
*******************************************************************************/

static esp_err_t touch_ft5x06_init(esp_lcd_touch_handle_t tp)
{
    esp_err_t ret = ESP_OK;

    // Valid touching detect threshold
    ret |= touch_ft5x06_i2c_write(tp, FT5x06_ID_G_THGROUP, 70);

    // valid touching peak detect threshold
    ret |= touch_ft5x06_i2c_write(tp, FT5x06_ID_G_THPEAK, 60);

    // Touch focus threshold
    ret |= touch_ft5x06_i2c_write(tp, FT5x06_ID_G_THCAL, 16);

    // threshold when there is surface water
    ret |= touch_ft5x06_i2c_write(tp, FT5x06_ID_G_THWATER, 60);

    // threshold of temperature compensation
    ret |= touch_ft5x06_i2c_write(tp, FT5x06_ID_G_THTEMP, 10);

    // Touch difference threshold
    ret |= touch_ft5x06_i2c_write(tp, FT5x06_ID_G_THDIFF, 20);

    // Delay to enter 'Monitor' status (s)
    ret |= touch_ft5x06_i2c_write(tp, FT5x06_ID_G_TIME_ENTER_MONITOR, 2);

    // Period of 'Active' status (ms)
    ret |= touch_ft5x06_i2c_write(tp, FT5x06_ID_G_PERIODACTIVE, 12);

    // Timer to enter 'idle' when in 'Monitor' (ms)
    ret |= touch_ft5x06_i2c_write(tp, FT5x06_ID_G_PERIODMONITOR, 40);

    return ret;
}

/* Reset controller */
static esp_err_t touch_ft5x06_reset(esp_lcd_touch_handle_t tp)
{
    assert(tp != NULL);

    if (tp->config.rst_gpio_num != GPIO_NUM_NC) {
        ESP_RETURN_ON_ERROR(gpio_set_level(tp->config.rst_gpio_num, tp->config.levels.reset), TAG, "GPIO set level error!");
        vTaskDelay(pdMS_TO_TICKS(10));
        ESP_RETURN_ON_ERROR(gpio_set_level(tp->config.rst_gpio_num, !tp->config.levels.reset), TAG, "GPIO set level error!");
        vTaskDelay(pdMS_TO_TICKS(10));
    }

    return ESP_OK;
}

static esp_err_t touch_ft5x06_i2c_write(esp_lcd_touch_handle_t tp, uint8_t reg, uint8_t data)
{
    assert(tp != NULL);

    // *INDENT-OFF*
    /* Write data */
    return esp_lcd_panel_io_tx_param(tp->io, reg, (uint8_t[]){data}, 1);
    // *INDENT-ON*
}

static esp_err_t touch_ft5x06_i2c_read(esp_lcd_touch_handle_t tp, uint8_t reg, uint8_t *data, uint8_t len)
{
    assert(tp != NULL);
    assert(data != NULL);

    /* Read data */
    return esp_lcd_panel_io_rx_param(tp->io, reg, data, len);
}
