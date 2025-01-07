/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "esp_log.h"
#include "esp_check.h"
#include "driver/gpio.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_touch.h"
#include "esp_lcd_touch_ili2118.h"

static const char *TAG = "ili2118";

// ILI2118A registers
#define ILITEK_TP_CMD_READ_SUB_DATA        (0x11)
#define ILITEK_TP_CMD_GET_RESOLUTION       (0x20)
#define ILITEK_TP_CMD_GET_FIRMWARE_VERSION (0x40)
#define ILITEK_TP_CMD_GET_PROTOCOL_VERSION (0x42)

#define ILI2118_TOUCH_DATA_MAGIC_1         (0x5A)
#define ILI2118_TOUCH_DATA_MAGIC_2         (0x99)
#define ILI2118_INVALID_COORD              (0xFF)
#define ILI2118_BUF_SIZE                   (53)

#define TOUCH_MAX_WIDTH                    (2048)
#define TOUCH_MAX_HEIGHT                   (2048)

typedef struct {
    uint16_t max_x;
    uint16_t max_y;
    uint8_t max_touch_num;
    uint8_t firmware_ver[4];
    uint16_t protocol_ver;
    uint8_t x_ch;
    uint8_t y_ch;
} ili2118a_info_t;

static esp_err_t read_data(esp_lcd_touch_handle_t tp);
static bool get_xy(esp_lcd_touch_handle_t tp, uint16_t *x, uint16_t *y, uint16_t *strength, uint8_t *point_num, uint8_t max_point_num);
static esp_err_t del(esp_lcd_touch_handle_t tp);
static esp_err_t read_info(esp_lcd_touch_handle_t tp);

esp_err_t esp_lcd_touch_new_i2c_ili2118(const esp_lcd_panel_io_handle_t io, const esp_lcd_touch_config_t *config, esp_lcd_touch_handle_t *tp)
{
    esp_err_t ret = ESP_OK;
    esp_lcd_touch_handle_t ili2118a = NULL;

    ESP_GOTO_ON_FALSE(io && config && tp, ESP_ERR_INVALID_ARG, err, TAG, "Invalid argument");

    ili2118a = (esp_lcd_touch_handle_t)calloc(1, sizeof(esp_lcd_touch_t));
    ESP_GOTO_ON_FALSE(ili2118a, ESP_ERR_NO_MEM, err, TAG, "Touch handle malloc failed");

    ili2118a->io = io;
    ili2118a->read_data = read_data;
    ili2118a->get_xy = get_xy;
    ili2118a->del = del;
    ili2118a->data.lock.owner = portMUX_FREE_VAL;
    memcpy(&ili2118a->config, config, sizeof(esp_lcd_touch_config_t));

    // Configure interrupt pin
    if (config->int_gpio_num != GPIO_NUM_NC) {
        const gpio_config_t int_gpio_config = {
            .mode = GPIO_MODE_INPUT,
            .intr_type = config->levels.interrupt ? GPIO_INTR_POSEDGE : GPIO_INTR_NEGEDGE,
            .pin_bit_mask = BIT64(config->int_gpio_num)
        };
        ESP_GOTO_ON_ERROR(gpio_config(&int_gpio_config), err, TAG, "GPIO intr config failed");

        if (config->interrupt_callback) {
            esp_lcd_touch_register_interrupt_callback(ili2118a, config->interrupt_callback);
        }
    }

    // Read touch info
    ESP_GOTO_ON_ERROR(read_info(ili2118a), err, TAG, "Read touch info failed");

    ESP_LOGI(TAG, "Touch panel create success, version: %d.%d.%d", ili2118a->config.x_max, ili2118a->config.y_max, ili2118a->config.x_max);

    *tp = ili2118a;
    return ESP_OK;

err:
    if (ili2118a) {
        del(ili2118a);
    }
    return ret;
}

static esp_err_t read_data(esp_lcd_touch_handle_t tp)
{
    uint8_t buf[ILI2118_BUF_SIZE] = {0};
    esp_err_t ret;

    // Read touch data
    ret = esp_lcd_panel_io_rx_param(tp->io, ILITEK_TP_CMD_READ_SUB_DATA, buf, sizeof(buf));
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Read touch data failed");
        return ret;
    }
    portENTER_CRITICAL(&tp->data.lock);
    tp->data.points = 0;

    if (buf[0] == ILI2118_TOUCH_DATA_MAGIC_1) {
        if ((buf[1] != ILI2118_INVALID_COORD) || (buf[2] != ILI2118_INVALID_COORD) || (buf[3] != ILI2118_INVALID_COORD)) {
            uint16_t raw_x = ((buf[1] & 0xF0) << 4) | buf[2];
            uint16_t raw_y = ((buf[1] & 0x0F) << 8) | buf[3];

            // Coordinate Transformation
            tp->data.coords[0].x = (raw_x * tp->config.x_max) / TOUCH_MAX_WIDTH;
            tp->data.coords[0].y = (raw_y * tp->config.y_max) / TOUCH_MAX_HEIGHT;
            tp->data.coords[0].strength = 1;
            tp->data.points = 1;
        }
    } else if (buf[0] == ILI2118_TOUCH_DATA_MAGIC_2) {
        if ((buf[6] != ILI2118_INVALID_COORD) || (buf[7] != ILI2118_INVALID_COORD) || (buf[8] != ILI2118_INVALID_COORD)) {
            uint16_t raw_y = ((buf[6] & 0xF0) << 4) | buf[7];
            uint16_t raw_x = ((buf[6] & 0x0F) << 8) | buf[8];

            // Coordinate Transformation
            tp->data.coords[0].x = (raw_x * tp->config.x_max) / TOUCH_MAX_WIDTH;
            tp->data.coords[0].y = (raw_y * tp->config.y_max) / TOUCH_MAX_HEIGHT;
            tp->data.coords[0].strength = 1;
            tp->data.points = 1;
        }
    }

    portEXIT_CRITICAL(&tp->data.lock);
    return ESP_OK;
}

static bool get_xy(esp_lcd_touch_handle_t tp, uint16_t *x, uint16_t *y, uint16_t *strength, uint8_t *point_num, uint8_t max_point_num)
{
    portENTER_CRITICAL(&tp->data.lock);

    *point_num = (tp->data.points > max_point_num) ? max_point_num : tp->data.points;
    for (size_t i = 0; i < *point_num; i++) {
        x[i] = tp->data.coords[i].x;
        y[i] = tp->data.coords[i].y;
        if (strength) {
            strength[i] = tp->data.coords[i].strength;
        }
    }

    tp->data.points = 0;
    portEXIT_CRITICAL(&tp->data.lock);

    return (*point_num > 0);
}

static esp_err_t del(esp_lcd_touch_handle_t tp)
{
    if (tp->config.int_gpio_num != GPIO_NUM_NC) {
        gpio_reset_pin(tp->config.int_gpio_num);
        if (tp->config.interrupt_callback) {
            gpio_isr_handler_remove(tp->config.int_gpio_num);
        }
    }
    free(tp);
    tp = NULL;
    return ESP_OK;
}

static esp_err_t read_info(esp_lcd_touch_handle_t tp)
{
    uint8_t buf[10] = {0};
    esp_err_t ret;
    ili2118a_info_t info;

    // Read firmware version
    ret = esp_lcd_panel_io_rx_param(tp->io, ILITEK_TP_CMD_GET_FIRMWARE_VERSION, info.firmware_ver, sizeof(info.firmware_ver));
    if (ret != ESP_OK) {
        return ret;
    }

    // Read protocol version
    ret = esp_lcd_panel_io_rx_param(tp->io, ILITEK_TP_CMD_GET_PROTOCOL_VERSION, buf, sizeof(buf));
    if (ret != ESP_OK) {
        return ret;
    }
    info.protocol_ver = (buf[0] << 8) | buf[1];

    // Read resolution
    ret = esp_lcd_panel_io_rx_param(tp->io, ILITEK_TP_CMD_GET_RESOLUTION, buf, sizeof(buf));
    if (ret != ESP_OK) {
        return ret;
    }

    info.max_x = (buf[1] << 8) | buf[0];
    info.max_y = (buf[3] << 8) | buf[2];
    info.x_ch = buf[4];
    info.y_ch = buf[5];
    info.max_touch_num = buf[6];

    ESP_LOGI(TAG, "Touch info: %dx%d, protocol v%d.%d, firmware v%d.%d.%d.%d",
             info.max_x, info.max_y,
             (info.protocol_ver >> 8), (info.protocol_ver & 0xFF),
             info.firmware_ver[0], info.firmware_ver[1],
             info.firmware_ver[2], info.firmware_ver[3]);

    return ESP_OK;
}
