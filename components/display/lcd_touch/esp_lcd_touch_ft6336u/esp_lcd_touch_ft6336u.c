/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include "driver/gpio.h"
#include "esp_check.h"
#include "esp_heap_caps.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_touch.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_lcd_touch_ft6336u.h"

static const char *TAG = "ft6336u";

#define FT6336U_REG_TD_STATUS              (0x02)
#define FT6336U_REG_TOUCH1_XH              (0x03)
#define FT6336U_REG_ID_G_CIPHER            (0xA3)
#define FT6336U_CHIP_ID                    (0x64)
#define FT6336U_MAX_TOUCH_POINTS           (2)
#define FT6336U_BYTES_PER_POINT            (6)
#define FT6336U_TOUCH_EVENT_MASK           (0xC0)
#define FT6336U_TOUCH_EVENT_DOWN           (0x00)
#define FT6336U_TOUCH_EVENT_CONTACT        (0x80)

static esp_err_t ft6336u_read_data(esp_lcd_touch_handle_t tp);
static bool ft6336u_get_xy(esp_lcd_touch_handle_t tp, uint16_t *x, uint16_t *y,
                           uint16_t *strength, uint8_t *point_num, uint8_t max_point_num);
static esp_err_t ft6336u_del(esp_lcd_touch_handle_t tp);
static esp_err_t ft6336u_reset(esp_lcd_touch_handle_t tp);
static esp_err_t ft6336u_i2c_read(esp_lcd_touch_handle_t tp, uint8_t reg, uint8_t *data, uint8_t len);

esp_err_t esp_lcd_touch_new_i2c_ft6336u(const esp_lcd_panel_io_handle_t io,
                                        const esp_lcd_touch_config_t *config,
                                        esp_lcd_touch_handle_t *out_touch)
{
    ESP_RETURN_ON_FALSE(io, ESP_ERR_INVALID_ARG, TAG, "invalid IO handle");
    ESP_RETURN_ON_FALSE(config, ESP_ERR_INVALID_ARG, TAG, "invalid touch config");
    ESP_RETURN_ON_FALSE(out_touch, ESP_ERR_INVALID_ARG, TAG, "invalid touch handle output");

    esp_err_t ret = ESP_OK;
    esp_lcd_touch_handle_t touch = heap_caps_calloc(1, sizeof(esp_lcd_touch_t), MALLOC_CAP_DEFAULT);
    ESP_RETURN_ON_FALSE(touch, ESP_ERR_NO_MEM, TAG, "no mem for FT6336U touch");

    touch->io = io;
    touch->read_data = ft6336u_read_data;
    touch->get_xy = ft6336u_get_xy;
    touch->del = ft6336u_del;
    touch->data.lock.owner = portMUX_FREE_VAL;
    memcpy(&touch->config, config, sizeof(esp_lcd_touch_config_t));

    if (touch->config.int_gpio_num != GPIO_NUM_NC) {
        const gpio_config_t int_gpio_config = {
            .mode = GPIO_MODE_INPUT,
            .intr_type = touch->config.levels.interrupt ? GPIO_INTR_POSEDGE : GPIO_INTR_NEGEDGE,
            .pin_bit_mask = BIT64(touch->config.int_gpio_num),
        };
        ESP_GOTO_ON_ERROR(gpio_config(&int_gpio_config), err, TAG, "configure interrupt GPIO failed");

        if (touch->config.interrupt_callback) {
            esp_lcd_touch_register_interrupt_callback(touch, touch->config.interrupt_callback);
        }
    }

    if (touch->config.rst_gpio_num != GPIO_NUM_NC) {
        const gpio_config_t rst_gpio_config = {
            .mode = GPIO_MODE_OUTPUT,
            .pin_bit_mask = BIT64(touch->config.rst_gpio_num),
        };
        ESP_GOTO_ON_ERROR(gpio_config(&rst_gpio_config), err, TAG, "configure reset GPIO failed");
    }

    ESP_GOTO_ON_ERROR(ft6336u_reset(touch), err, TAG, "reset failed");

    uint8_t chip_id = 0;
    ESP_GOTO_ON_ERROR(ft6336u_i2c_read(touch, FT6336U_REG_ID_G_CIPHER, &chip_id, 1), err, TAG, "read chip ID failed");
    if (chip_id != FT6336U_CHIP_ID) {
        ESP_LOGE(TAG, "unexpected chip ID 0x%02x, expected 0x%02x", chip_id, FT6336U_CHIP_ID);
        ret = ESP_ERR_NOT_SUPPORTED;
        goto err;
    }

    *out_touch = touch;
    ESP_LOGI(TAG, "FT6336U touch initialized, int_gpio=%d, rst_gpio=%d",
             touch->config.int_gpio_num, touch->config.rst_gpio_num);
    return ESP_OK;

err:
    ft6336u_del(touch);
    return ret;
}

static esp_err_t ft6336u_read_data(esp_lcd_touch_handle_t tp)
{
    ESP_RETURN_ON_FALSE(tp, ESP_ERR_INVALID_ARG, TAG, "invalid touch handle");

    uint8_t points = 0;
    ESP_RETURN_ON_ERROR(ft6336u_i2c_read(tp, FT6336U_REG_TD_STATUS, &points, 1), TAG, "read point count failed");
    points &= 0x0F;

    portENTER_CRITICAL(&tp->data.lock);
    tp->data.points = 0;
    portEXIT_CRITICAL(&tp->data.lock);

    if (points == 0 || points > FT6336U_MAX_TOUCH_POINTS) {
        return ESP_OK;
    }

    points = points > CONFIG_ESP_LCD_TOUCH_MAX_POINTS ? CONFIG_ESP_LCD_TOUCH_MAX_POINTS : points;

    uint8_t data[FT6336U_MAX_TOUCH_POINTS * FT6336U_BYTES_PER_POINT] = {0};
    ESP_RETURN_ON_ERROR(ft6336u_i2c_read(tp, FT6336U_REG_TOUCH1_XH, data, points * FT6336U_BYTES_PER_POINT),
                        TAG, "read coordinates failed");

    portENTER_CRITICAL(&tp->data.lock);
    for (uint8_t i = 0; i < points; i++) {
        const uint8_t *point = data + i * FT6336U_BYTES_PER_POINT;
        uint8_t event = point[0] & FT6336U_TOUCH_EVENT_MASK;
        if (event != FT6336U_TOUCH_EVENT_DOWN && event != FT6336U_TOUCH_EVENT_CONTACT) {
            continue;
        }

        uint8_t index = tp->data.points++;
        tp->data.coords[index].x = (((uint16_t)(point[0] & 0x0F)) << 8) | point[1];
        tp->data.coords[index].y = (((uint16_t)(point[2] & 0x0F)) << 8) | point[3];
        tp->data.coords[index].strength = 1;
    }
    portEXIT_CRITICAL(&tp->data.lock);

    return ESP_OK;
}

static bool ft6336u_get_xy(esp_lcd_touch_handle_t tp, uint16_t *x, uint16_t *y,
                           uint16_t *strength, uint8_t *point_num, uint8_t max_point_num)
{
    ESP_RETURN_ON_FALSE(tp && x && y && point_num && max_point_num > 0, false, TAG, "invalid arguments");

    portENTER_CRITICAL(&tp->data.lock);
    *point_num = tp->data.points > max_point_num ? max_point_num : tp->data.points;
    for (uint8_t i = 0; i < *point_num; i++) {
        x[i] = tp->data.coords[i].x;
        y[i] = tp->data.coords[i].y;
        if (strength) {
            strength[i] = tp->data.coords[i].strength;
        }
    }
    tp->data.points = 0;
    portEXIT_CRITICAL(&tp->data.lock);

    return *point_num > 0;
}

static esp_err_t ft6336u_del(esp_lcd_touch_handle_t tp)
{
    if (!tp) {
        return ESP_OK;
    }

    if (tp->config.int_gpio_num != GPIO_NUM_NC) {
        gpio_reset_pin(tp->config.int_gpio_num);
        if (tp->config.interrupt_callback) {
            gpio_isr_handler_remove(tp->config.int_gpio_num);
        }
    }
    if (tp->config.rst_gpio_num != GPIO_NUM_NC) {
        gpio_reset_pin(tp->config.rst_gpio_num);
    }

    free(tp);
    return ESP_OK;
}

static esp_err_t ft6336u_reset(esp_lcd_touch_handle_t tp)
{
    if (tp->config.rst_gpio_num != GPIO_NUM_NC) {
        ESP_RETURN_ON_ERROR(gpio_set_level(tp->config.rst_gpio_num, tp->config.levels.reset), TAG, "assert reset failed");
        vTaskDelay(pdMS_TO_TICKS(10));
        ESP_RETURN_ON_ERROR(gpio_set_level(tp->config.rst_gpio_num, !tp->config.levels.reset), TAG, "release reset failed");
        vTaskDelay(pdMS_TO_TICKS(500));
    }

    return ESP_OK;
}

static esp_err_t ft6336u_i2c_read(esp_lcd_touch_handle_t tp, uint8_t reg, uint8_t *data, uint8_t len)
{
    ESP_RETURN_ON_FALSE(data || len == 0, ESP_ERR_INVALID_ARG, TAG, "invalid read buffer");
    return esp_lcd_panel_io_rx_param(tp->io, reg, data, len);
}
