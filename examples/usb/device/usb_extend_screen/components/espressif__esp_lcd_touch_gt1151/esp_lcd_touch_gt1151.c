/*
 * SPDX-FileCopyrightText: 2015-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_system.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_check.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_touch.h"

static const char *TAG = "gt1151";

#define READ_XY_REG        (0x814E)
#define PRODUCT_ID_REG     (0x8140)

#define MAX_TOUCH_NUM      (10)
/* Buffer Length = StatusReg(1) + TouchData(8 * TouchNum) + KeyValue(1) + Checksum(1) */
#define DATA_BUFF_LEN(touch_num)    (1 + 8 * (touch_num) + 2)
#define IS_NUM_OR_CHAR(x)           (((x) >= 'A' && (x) <= 'Z') || ((x) >= '0' && (x) <= '9'))

static esp_err_t read_data(esp_lcd_touch_handle_t tp);
static bool get_xy(esp_lcd_touch_handle_t tp, uint16_t *x, uint16_t *y, uint16_t *strength, uint8_t *track_id, uint8_t *point_num, uint8_t max_point_num);
static esp_err_t del(esp_lcd_touch_handle_t tp);

static esp_err_t i2c_read_bytes(esp_lcd_touch_handle_t tp, uint16_t reg, uint8_t *data, uint8_t len);
static esp_err_t i2c_write_byte(esp_lcd_touch_handle_t tp, uint16_t reg, uint8_t data);

static esp_err_t reset(esp_lcd_touch_handle_t tp);
static esp_err_t read_product_id(esp_lcd_touch_handle_t tp);

esp_err_t esp_lcd_touch_new_i2c_gt1151(const esp_lcd_panel_io_handle_t io, const esp_lcd_touch_config_t *config, esp_lcd_touch_handle_t *tp)
{
    ESP_RETURN_ON_FALSE(io, ESP_ERR_INVALID_ARG, TAG, "Invalid io");
    ESP_RETURN_ON_FALSE(config, ESP_ERR_INVALID_ARG, TAG, "Invalid config");
    ESP_RETURN_ON_FALSE(tp, ESP_ERR_INVALID_ARG, TAG, "Invalid touch handle");

    /* Prepare main structure */
    esp_err_t ret = ESP_OK;
    esp_lcd_touch_handle_t gt1151 = calloc(1, sizeof(esp_lcd_touch_t));
    ESP_GOTO_ON_FALSE(gt1151, ESP_ERR_NO_MEM, err, TAG, "Touch handle malloc failed");

    /* Communication interface */
    gt1151->io = io;
    /* Only supported callbacks are set */
    gt1151->read_data = read_data;
    gt1151->get_xy = get_xy;
    gt1151->del = del;
    /* Mutex */
    gt1151->data.lock.owner = portMUX_FREE_VAL;
    /* Save config */
    memcpy(&gt1151->config, config, sizeof(esp_lcd_touch_config_t));

    /* Prepare pin for touch interrupt */
    if (gt1151->config.int_gpio_num != GPIO_NUM_NC) {
        const gpio_config_t int_gpio_config = {
            .mode = GPIO_MODE_INPUT,
            .intr_type = (gt1151->config.levels.interrupt ? GPIO_INTR_POSEDGE : GPIO_INTR_NEGEDGE),
            .pin_bit_mask = BIT64(gt1151->config.int_gpio_num)
        };
        ESP_GOTO_ON_ERROR(gpio_config(&int_gpio_config), err, TAG, "GPIO intr config failed");

        /* Register interrupt callback */
        if (gt1151->config.interrupt_callback) {
            esp_lcd_touch_register_interrupt_callback(gt1151, gt1151->config.interrupt_callback);
        }
    }
    /* Prepare pin for touch controller reset */
    if (gt1151->config.rst_gpio_num != GPIO_NUM_NC) {
        const gpio_config_t rst_gpio_config = {
            .mode = GPIO_MODE_OUTPUT,
            .pin_bit_mask = BIT64(gt1151->config.rst_gpio_num)
        };
        ESP_GOTO_ON_ERROR(gpio_config(&rst_gpio_config), err, TAG, "GPIO reset config failed");
    }
    /* Reset controller */
    ESP_GOTO_ON_ERROR(reset(gt1151), err, TAG, "Reset failed");
    /* Read product id */
    ESP_GOTO_ON_ERROR(read_product_id(gt1151), err, TAG, "Read product id failed");
    *tp = gt1151;

    return ESP_OK;
err:
    if (gt1151) {
        del(gt1151);
    }
    ESP_LOGE(TAG, "Initialization failed!");
    return ret;
}

static esp_err_t read_data(esp_lcd_touch_handle_t tp)
{
    typedef struct {
        uint8_t touch_id : 4;
        uint8_t : 4;
        uint16_t x;
        uint16_t y;
        uint16_t strength;
        uint8_t useless;
    } __attribute__((packed)) touch_record_t;

    typedef struct {
        uint8_t touch_cnt : 4;
        uint8_t : 4;
        touch_record_t touch_record[0];
    } __attribute__((packed)) touch_report_t;

    uint8_t touch_cnt;
    ESP_RETURN_ON_ERROR(i2c_read_bytes(tp, READ_XY_REG, &touch_cnt, sizeof(touch_cnt)), TAG, "I2C read failed!");
    touch_cnt &= 0x0f;
    /* Any touch data? */
    if (touch_cnt > MAX_TOUCH_NUM || touch_cnt == 0) {
        i2c_write_byte(tp, READ_XY_REG, 0);
        return ESP_OK;
    }

    uint8_t buf[DATA_BUFF_LEN(MAX_TOUCH_NUM)];
    /* Read all points */
    ESP_RETURN_ON_ERROR(i2c_read_bytes(tp, READ_XY_REG, buf, DATA_BUFF_LEN(touch_cnt)), TAG, "I2C read failed");
    /* Clear all */
    i2c_write_byte(tp, READ_XY_REG, 0);
    /* Calculate checksum */
    uint8_t checksum = 0;
    for (int i = 0; i < DATA_BUFF_LEN(touch_cnt); i++) {
        checksum += buf[i];
    }
    ESP_RETURN_ON_FALSE(!checksum, ESP_ERR_INVALID_CRC, TAG, "Checksum error");

    touch_report_t *touch_report = (touch_report_t *)buf;
    portENTER_CRITICAL(&tp->data.lock);
    /* Expect Number of touched points */
    touch_cnt = (touch_cnt > CONFIG_ESP_LCD_TOUCH_MAX_POINTS ? CONFIG_ESP_LCD_TOUCH_MAX_POINTS : touch_cnt);
    tp->data.points = touch_cnt;

    /* Fill all coordinates */
    for (int i = 0; i < touch_cnt; i++) {
        tp->data.coords[i].x = touch_report->touch_record[i].x;
        tp->data.coords[i].y = touch_report->touch_record[i].y;
        tp->data.coords[i].strength = touch_report->touch_record[i].strength;
    }
    portEXIT_CRITICAL(&tp->data.lock);

    return ESP_OK;
}

static bool get_xy(esp_lcd_touch_handle_t tp, uint16_t *x, uint16_t *y, uint16_t *strength, uint8_t *track_id, uint8_t *point_num, uint8_t max_point_num)
{
    portENTER_CRITICAL(&tp->data.lock);
    /* Count of points */
    *point_num = (tp->data.points > max_point_num ? max_point_num : tp->data.points);
    for (size_t i = 0; i < *point_num; i++) {
        x[i] = tp->data.coords[i].x;
        y[i] = tp->data.coords[i].y;

        if (strength) {
            strength[i] = tp->data.coords[i].strength;
        }
    }
    /* Invalidate */
    tp->data.points = 0;
    portEXIT_CRITICAL(&tp->data.lock);

    return (*point_num > 0);
}

static esp_err_t del(esp_lcd_touch_handle_t tp)
{
    /* Reset GPIO pin settings */
    if (tp->config.int_gpio_num != GPIO_NUM_NC) {
        gpio_reset_pin(tp->config.int_gpio_num);
        if (tp->config.interrupt_callback) {
            gpio_isr_handler_remove(tp->config.int_gpio_num);
        }
    }
    if (tp->config.rst_gpio_num != GPIO_NUM_NC) {
        gpio_reset_pin(tp->config.rst_gpio_num);
    }
    /* Release memory */
    free(tp);

    return ESP_OK;
}

static esp_err_t reset(esp_lcd_touch_handle_t tp)
{
    if (tp->config.rst_gpio_num != GPIO_NUM_NC) {
        ESP_RETURN_ON_ERROR(gpio_set_level(tp->config.rst_gpio_num, tp->config.levels.reset), TAG, "GPIO set level failed");
        vTaskDelay(pdMS_TO_TICKS(10));
        ESP_RETURN_ON_ERROR(gpio_set_level(tp->config.rst_gpio_num, !tp->config.levels.reset), TAG, "GPIO set level failed");
        vTaskDelay(pdMS_TO_TICKS(10));
    }

    return ESP_OK;
}

static esp_err_t read_product_id(esp_lcd_touch_handle_t tp)
{
    uint8_t buf[11] = {0};
    uint8_t checksum = 0;
    ESP_RETURN_ON_ERROR(i2c_read_bytes(tp, PRODUCT_ID_REG, buf, sizeof(buf)), TAG, "I2C read failed");
    for (int i = 0; i < sizeof(buf); i++) {
        checksum += buf[i];
    }
    /* First 3 bytes must be number or char, and sensor id != 0xFF */
    ESP_RETURN_ON_FALSE(checksum, ESP_ERR_INVALID_CRC, TAG, "Checksum error");
    ESP_RETURN_ON_FALSE(
        IS_NUM_OR_CHAR(buf[0]) && IS_NUM_OR_CHAR(buf[1]) && IS_NUM_OR_CHAR(buf[2]) && buf[10] != 0xFF,
        ESP_ERR_INVALID_CRC, TAG, "Product id format error");

    uint32_t mask_id, patch_id, sensor_id;
    char product_id[5] = {0};
    mask_id = (uint32_t)((buf[7] << 16) | (buf[8] << 8) | buf[9]);
    patch_id = (uint32_t)((buf[4] << 16) | (buf[5] << 8) | buf[6]);
    memcpy(product_id, buf, 4);
    sensor_id = buf[10] & 0x0F;
    ESP_LOGI(
        TAG, "IC version: GT%s_%06" PRIX32 "(Patch)_%04" PRIX32 "(Mask)_%02" PRIX32 "(SensorID)",
        product_id, patch_id, mask_id >> 8, sensor_id);
    return ESP_OK;
}

static esp_err_t i2c_read_bytes(esp_lcd_touch_handle_t tp, uint16_t reg, uint8_t *data, uint8_t len)
{
    ESP_RETURN_ON_FALSE(data, ESP_ERR_INVALID_ARG, TAG, "Invalid data");

    return esp_lcd_panel_io_rx_param(tp->io, reg, data, len);
}

static esp_err_t i2c_write_byte(esp_lcd_touch_handle_t tp, uint16_t reg, uint8_t data)
{
    // *INDENT-OFF*
    return esp_lcd_panel_io_tx_param(tp->io, reg, (uint8_t[]){data}, 1);
    // *INDENT-ON*
}
