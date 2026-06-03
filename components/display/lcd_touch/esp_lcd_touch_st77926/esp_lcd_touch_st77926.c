/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <inttypes.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_check.h"
#include "esp_log.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_touch.h"

#include "esp_lcd_touch_st77926.h"

#define ST77926_REG_FIRMWARE_VERSION       (0x0000)
#define ST77926_REG_X_RESOLUTION_HIGH      (0x0005)
#define ST77926_REG_MAX_TOUCHES            (0x0009)
#define ST77926_REG_FIRMWARE_REVISION_3    (0x000C)
#define ST77926_REG_TOUCH_INFO             (0x0010)
#define ST77926_REG_MISC_INFO              (0x00F0)
#define ST77926_REG_CHIP_ID                (0x00F4)

#define ST77926_CHIP_ID                    (0x83)
#define ST77926_CHIP_ID_ALT                (0x84)
#define ST77926_MAX_TOUCH_POINTS           (10)
#define ST77926_TOUCH_HEADER_BYTES         (4)
#define ST77926_TOUCH_BYTES_PER_POINT      (7)
#define ST77926_TOUCH_CHECKSUM_BYTES       (1)
#define ST77926_TOUCH_VALID_BIT            (0x80)
#define ST77926_TOUCH_COORD_HIGH_MASK      (0x3F)
#define ST77926_MISC_COORD_CHECKSUM_FLAG   (0x10)
#define ST77926_CHECKSUM_INIT              (0x5A)

static const char *TAG = "st77926";

typedef struct {
    esp_lcd_touch_t base;
    uint8_t max_touches;
    uint16_t x_res;
    uint16_t y_res;
    uint8_t fw_version;
    uint8_t fw_revision[4];
    uint8_t chip_id;
    bool coord_checksum;
} st77926_touch_t;

static esp_err_t read_data(esp_lcd_touch_handle_t tp);
static bool get_xy(esp_lcd_touch_handle_t tp, uint16_t *x, uint16_t *y, uint16_t *strength, uint8_t *point_num, uint8_t max_point_num);
static esp_err_t del(esp_lcd_touch_handle_t tp);
static esp_err_t reset(esp_lcd_touch_handle_t tp);
static esp_err_t read_info(esp_lcd_touch_handle_t tp);
static esp_err_t i2c_read_bytes(esp_lcd_touch_handle_t tp, uint16_t reg, uint8_t *data, uint8_t len);
static uint8_t checksum_update(uint8_t checksum, const uint8_t *data, size_t len);

esp_err_t esp_lcd_touch_new_i2c_st77926(const esp_lcd_panel_io_handle_t io, const esp_lcd_touch_config_t *config,
                                        esp_lcd_touch_handle_t *tp)
{
    ESP_RETURN_ON_FALSE(io, ESP_ERR_INVALID_ARG, TAG, "Invalid io");
    ESP_RETURN_ON_FALSE(config, ESP_ERR_INVALID_ARG, TAG, "Invalid config");
    ESP_RETURN_ON_FALSE(tp, ESP_ERR_INVALID_ARG, TAG, "Invalid touch handle");

    esp_err_t ret = ESP_OK;
    st77926_touch_t *st77926 = (st77926_touch_t *)calloc(1, sizeof(st77926_touch_t));
    ESP_GOTO_ON_FALSE(st77926, ESP_ERR_NO_MEM, err, TAG, "Touch handle malloc failed");

    st77926->base.io = io;
    st77926->base.read_data = read_data;
    st77926->base.get_xy = get_xy;
    st77926->base.del = del;
    st77926->base.data.lock.owner = portMUX_FREE_VAL;
    memcpy(&st77926->base.config, config, sizeof(esp_lcd_touch_config_t));

    if (config->int_gpio_num != GPIO_NUM_NC) {
        const gpio_config_t int_gpio_config = {
            .mode = GPIO_MODE_INPUT,
            .intr_type = config->levels.interrupt ? GPIO_INTR_POSEDGE : GPIO_INTR_NEGEDGE,
            .pin_bit_mask = BIT64(config->int_gpio_num),
        };
        ESP_GOTO_ON_ERROR(gpio_config(&int_gpio_config), err, TAG, "GPIO intr config failed");

        if (config->interrupt_callback) {
            esp_lcd_touch_register_interrupt_callback(&st77926->base, config->interrupt_callback);
        }
    }

    if (config->rst_gpio_num != GPIO_NUM_NC) {
        const gpio_config_t rst_gpio_config = {
            .mode = GPIO_MODE_OUTPUT,
            .pin_bit_mask = BIT64(config->rst_gpio_num),
        };
        ESP_GOTO_ON_ERROR(gpio_config(&rst_gpio_config), err, TAG, "GPIO reset config failed");
    }

    ESP_GOTO_ON_ERROR(reset(&st77926->base), err, TAG, "Reset failed");
    ESP_GOTO_ON_ERROR(read_info(&st77926->base), err, TAG, "Read touch info failed");

    ESP_LOGI(TAG, "Touch panel create success, chip: 0x%02" PRIx8 ", fw: 0x%02" PRIx8 ", resolution: %" PRIu16 "x%" PRIu16
             ", max touches: %" PRIu8,
             st77926->chip_id, st77926->fw_version, st77926->x_res, st77926->y_res, st77926->max_touches);

    *tp = &st77926->base;
    return ESP_OK;

err:
    if (st77926) {
        del(&st77926->base);
    }
    return ret;
}

static esp_err_t read_data(esp_lcd_touch_handle_t tp)
{
    st77926_touch_t *st77926 = (st77926_touch_t *)tp;
    uint8_t max_touches = st77926->max_touches;
    uint8_t touch_buf[ST77926_TOUCH_HEADER_BYTES + ST77926_MAX_TOUCH_POINTS * ST77926_TOUCH_BYTES_PER_POINT + ST77926_TOUCH_CHECKSUM_BYTES] = {0};
    uint8_t read_len = ST77926_TOUCH_HEADER_BYTES + max_touches * ST77926_TOUCH_BYTES_PER_POINT + ST77926_TOUCH_CHECKSUM_BYTES;

    ESP_RETURN_ON_FALSE(max_touches > 0 && max_touches <= ST77926_MAX_TOUCH_POINTS, ESP_ERR_INVALID_STATE, TAG, "Invalid max touches");
    ESP_RETURN_ON_ERROR(i2c_read_bytes(tp, ST77926_REG_TOUCH_INFO, touch_buf, read_len), TAG, "Read touch info failed");

    if (touch_buf[0] & 0x80) {
        ESP_LOGW(TAG, "Touch firmware requests reset, status=0x%02" PRIx8, touch_buf[0]);
        portENTER_CRITICAL(&tp->data.lock);
        tp->data.points = 0;
        portEXIT_CRITICAL(&tp->data.lock);
        return ESP_OK;
    }

    if (st77926->coord_checksum) {
        uint8_t checksum = ST77926_CHECKSUM_INIT;
        checksum = checksum_update(checksum, touch_buf, ST77926_TOUCH_HEADER_BYTES);
        if (touch_buf[0] & 0x08) {
            checksum = checksum_update(checksum, touch_buf + ST77926_TOUCH_HEADER_BYTES, max_touches * ST77926_TOUCH_BYTES_PER_POINT);
        }
        if (checksum != touch_buf[read_len - 1]) {
            ESP_LOGW(TAG, "Touch checksum mismatch, expect=0x%02" PRIx8 ", got=0x%02" PRIx8, checksum, touch_buf[read_len - 1]);
            return ESP_ERR_INVALID_CRC;
        }
    }

    portENTER_CRITICAL(&tp->data.lock);
    tp->data.points = 0;
    const uint8_t *point = touch_buf + ST77926_TOUCH_HEADER_BYTES;
    for (int i = 0; i < max_touches && tp->data.points < CONFIG_ESP_LCD_TOUCH_MAX_POINTS; i++) {
        if (point[0] & ST77926_TOUCH_VALID_BIT) {
            uint8_t point_index = tp->data.points++;
            tp->data.coords[point_index].x = (((uint16_t)(point[0] & ST77926_TOUCH_COORD_HIGH_MASK)) << 8) | point[1];
            tp->data.coords[point_index].y = (((uint16_t)(point[2] & ST77926_TOUCH_COORD_HIGH_MASK)) << 8) | point[3];
            tp->data.coords[point_index].strength = point[4];
        }
        point += ST77926_TOUCH_BYTES_PER_POINT;
    }
    portEXIT_CRITICAL(&tp->data.lock);

    return ESP_OK;
}

static bool get_xy(esp_lcd_touch_handle_t tp, uint16_t *x, uint16_t *y, uint16_t *strength, uint8_t *point_num, uint8_t max_point_num)
{
    portENTER_CRITICAL(&tp->data.lock);
    *point_num = (tp->data.points > max_point_num ? max_point_num : tp->data.points);
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
    if (tp->config.rst_gpio_num != GPIO_NUM_NC) {
        gpio_reset_pin(tp->config.rst_gpio_num);
    }

    free(tp);
    return ESP_OK;
}

static esp_err_t reset(esp_lcd_touch_handle_t tp)
{
    if (tp->config.rst_gpio_num != GPIO_NUM_NC) {
        ESP_RETURN_ON_ERROR(gpio_set_level(tp->config.rst_gpio_num, tp->config.levels.reset), TAG, "GPIO set level failed");
        vTaskDelay(pdMS_TO_TICKS(1));
        ESP_RETURN_ON_ERROR(gpio_set_level(tp->config.rst_gpio_num, !tp->config.levels.reset), TAG, "GPIO set level failed");
        vTaskDelay(pdMS_TO_TICKS(100));
    }

    return ESP_OK;
}

static esp_err_t read_info(esp_lcd_touch_handle_t tp)
{
    st77926_touch_t *st77926 = (st77926_touch_t *)tp;
    uint8_t resolution[4] = {0};
    uint8_t misc_info = 0;

    ESP_RETURN_ON_ERROR(i2c_read_bytes(tp, ST77926_REG_FIRMWARE_VERSION, &st77926->fw_version, 1), TAG, "Read firmware version failed");
    ESP_RETURN_ON_ERROR(i2c_read_bytes(tp, ST77926_REG_FIRMWARE_REVISION_3, st77926->fw_revision, sizeof(st77926->fw_revision)),
                        TAG, "Read firmware revision failed");
    ESP_RETURN_ON_ERROR(i2c_read_bytes(tp, ST77926_REG_X_RESOLUTION_HIGH, resolution, sizeof(resolution)), TAG, "Read resolution failed");
    ESP_RETURN_ON_ERROR(i2c_read_bytes(tp, ST77926_REG_MAX_TOUCHES, &st77926->max_touches, 1), TAG, "Read max touches failed");
    ESP_RETURN_ON_ERROR(i2c_read_bytes(tp, ST77926_REG_CHIP_ID, &st77926->chip_id, 1), TAG, "Read chip ID failed");
    ESP_RETURN_ON_ERROR(i2c_read_bytes(tp, ST77926_REG_MISC_INFO, &misc_info, 1), TAG, "Read misc info failed");

    st77926->x_res = (((uint16_t)(resolution[0] & ST77926_TOUCH_COORD_HIGH_MASK)) << 8) | resolution[1];
    st77926->y_res = (((uint16_t)(resolution[2] & ST77926_TOUCH_COORD_HIGH_MASK)) << 8) | resolution[3];
    st77926->coord_checksum = (misc_info & ST77926_MISC_COORD_CHECKSUM_FLAG) != 0;

    if (st77926->max_touches == 0 || st77926->max_touches > ST77926_MAX_TOUCH_POINTS) {
        ESP_LOGW(TAG, "Unexpected max touches %" PRIu8 ", clamp to %" PRIu8, st77926->max_touches, ST77926_MAX_TOUCH_POINTS);
        st77926->max_touches = ST77926_MAX_TOUCH_POINTS;
    }

    if ((st77926->chip_id != ST77926_CHIP_ID) && (st77926->chip_id != ST77926_CHIP_ID_ALT)) {
        ESP_LOGW(TAG, "Unexpected chip ID 0x%02" PRIx8 ", expected 0x%02x or 0x%02x", st77926->chip_id, ST77926_CHIP_ID,
                 ST77926_CHIP_ID_ALT);
    }

    ESP_LOGI(TAG, "FW revision: 0x%02" PRIx8 " 0x%02" PRIx8 " 0x%02" PRIx8 " 0x%02" PRIx8
             ", misc: 0x%02" PRIx8 ", checksum: %s",
             st77926->fw_revision[0], st77926->fw_revision[1], st77926->fw_revision[2], st77926->fw_revision[3],
             misc_info, st77926->coord_checksum ? "on" : "off");

    return ESP_OK;
}

static esp_err_t i2c_read_bytes(esp_lcd_touch_handle_t tp, uint16_t reg, uint8_t *data, uint8_t len)
{
    ESP_RETURN_ON_FALSE((len == 0) || (data != NULL), ESP_ERR_INVALID_ARG, TAG, "Invalid data");

    return esp_lcd_panel_io_rx_param(tp->io, reg, data, len);
}

static uint8_t checksum_update(uint8_t checksum, const uint8_t *data, size_t len)
{
    for (size_t i = 0; i < len; i++) {
        checksum += data[i];
    }
    return checksum;
}
