/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/i2c.h"
#include "esp_system.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_check.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_touch.h"

static const char *TAG = "SPD2010";

typedef struct {
    uint8_t none0;
    uint8_t none1;
    uint8_t none2;
    uint8_t cpu_run;
    uint8_t tint_low;
    uint8_t tic_in_cpu;
    uint8_t tic_in_bios;
    uint8_t tic_busy;
} tp_status_high_t;

typedef struct {
    uint8_t pt_exist;
    uint8_t gesture;
    uint8_t key;
    uint8_t aux;
    uint8_t keep;
    uint8_t raw_or_pt;
    uint8_t none6;
    uint8_t none7;
} tp_status_low_t;

typedef struct {
    tp_status_low_t status_low;
    tp_status_high_t status_high;
    uint16_t read_len;
} tp_status_t;

typedef struct {
    uint8_t id;
    uint16_t x;
    uint16_t y;
    uint8_t weight;
} tp_report_t;

typedef struct {
    tp_report_t rpt[10];
    uint8_t touch_num;
    uint8_t pack_code;
    uint8_t down;
    uint8_t up;
    uint8_t gesture;
    uint16_t down_x;
    uint16_t down_y;
    uint16_t up_x;
    uint16_t up_y;
} tp_touch_t;

typedef struct {
    uint8_t status;
    uint16_t next_packet_len;
} tp_hdp_status_t;

static esp_err_t read_data(esp_lcd_touch_handle_t tp);
static bool get_xy(esp_lcd_touch_handle_t tp, uint16_t *x, uint16_t *y, uint16_t *strength, uint8_t *point_num, uint8_t max_point_num);
static esp_err_t del(esp_lcd_touch_handle_t tp);
static esp_err_t reset(esp_lcd_touch_handle_t tp);

static esp_err_t write_tp_point_mode_cmd(esp_lcd_touch_handle_t tp);
static esp_err_t write_tp_start_cmd(esp_lcd_touch_handle_t tp);
static esp_err_t write_tp_cpu_start_cmd(esp_lcd_touch_handle_t tp);
static esp_err_t write_tp_clear_int_cmd(esp_lcd_touch_handle_t tp);
static esp_err_t read_tp_status_length(esp_lcd_touch_handle_t tp, tp_status_t *tp_status);
static esp_err_t read_tp_hdp(esp_lcd_touch_handle_t tp, tp_status_t *tp_status, tp_touch_t *touch);
static esp_err_t read_tp_hdp_status(esp_lcd_touch_handle_t tp, tp_hdp_status_t *tp_hdp_status);
static esp_err_t read_fw_version(esp_lcd_touch_handle_t tp);
static esp_err_t tp_read_data(esp_lcd_touch_handle_t tp, tp_touch_t *touch);

esp_err_t esp_lcd_touch_new_i2c_spd2010(const esp_lcd_panel_io_handle_t io, const esp_lcd_touch_config_t *config, esp_lcd_touch_handle_t *tp)
{
    ESP_RETURN_ON_FALSE(io, ESP_ERR_INVALID_ARG, TAG, "Invalid io");
    ESP_RETURN_ON_FALSE(config, ESP_ERR_INVALID_ARG, TAG, "Invalid config");
    ESP_RETURN_ON_FALSE(tp, ESP_ERR_INVALID_ARG, TAG, "Invalid touch handle");

    /* Prepare main structure */
    esp_err_t ret = ESP_OK;
    esp_lcd_touch_handle_t spd2010 = calloc(1, sizeof(esp_lcd_touch_t));
    ESP_GOTO_ON_FALSE(spd2010, ESP_ERR_NO_MEM, err, TAG, "Touch handle malloc failed");

    /* Communication interface */
    spd2010->io = io;
    /* Only supported callbacks are set */
    spd2010->read_data = read_data;
    spd2010->get_xy = get_xy;
    spd2010->del = del;
    /* Mutex */
    spd2010->data.lock.owner = portMUX_FREE_VAL;
    /* Save config */
    memcpy(&spd2010->config, config, sizeof(esp_lcd_touch_config_t));

    /* Prepare pin for touch interrupt */
    if (config->int_gpio_num != GPIO_NUM_NC) {
        const gpio_config_t int_gpio_config = {
            .mode = GPIO_MODE_INPUT,
            .intr_type = (config->levels.interrupt) ? GPIO_INTR_POSEDGE : GPIO_INTR_NEGEDGE,
            .pin_bit_mask = BIT64(config->int_gpio_num)
        };
        ESP_GOTO_ON_ERROR(gpio_config(&int_gpio_config), err, TAG, "GPIO intr config failed");

        /* Register interrupt callback */
        if (config->interrupt_callback) {
            esp_lcd_touch_register_interrupt_callback(spd2010, config->interrupt_callback);
        }
    }
    /* Prepare pin for touch controller reset */
    if (config->rst_gpio_num != GPIO_NUM_NC) {
        const gpio_config_t rst_gpio_config = {
            .mode = GPIO_MODE_OUTPUT,
            .pin_bit_mask = BIT64(config->rst_gpio_num)
        };
        ESP_GOTO_ON_ERROR(gpio_config(&rst_gpio_config), err, TAG, "GPIO reset config failed");
    }
    /* Reset controller */
    ESP_GOTO_ON_ERROR(reset(spd2010), err, TAG, "Reset failed");
    ESP_GOTO_ON_ERROR(read_fw_version(spd2010), err, TAG, "Read version failed");

    ESP_LOGI(TAG, "Touch panel create success, version: %d.%d.%d", ESP_LCD_TOUCH_SPD2010_VER_MAJOR,
             ESP_LCD_TOUCH_SPD2010_VER_MINOR, ESP_LCD_TOUCH_SPD2010_VER_PATCH);

    *tp = spd2010;

    return ESP_OK;
err:
    if (spd2010) {
        del(spd2010);
    }
    ESP_LOGE(TAG, "Initialization failed!");
    return ret;
}

static esp_err_t read_data(esp_lcd_touch_handle_t tp)
{
    uint8_t touch_cnt = 0;

    tp_touch_t touch = {0};
    ESP_RETURN_ON_ERROR(tp_read_data(tp, &touch), TAG, "read data failed");

    portENTER_CRITICAL(&tp->data.lock);
    /* Expect Number of touched points */
    touch_cnt = (touch.touch_num > CONFIG_ESP_LCD_TOUCH_MAX_POINTS ? CONFIG_ESP_LCD_TOUCH_MAX_POINTS : touch.touch_num);
    tp->data.points = touch_cnt;

    /* Fill all coordinates */
    for (int i = 0; i < touch_cnt; i++) {
        tp->data.coords[i].x = touch.rpt[i].x;
        tp->data.coords[i].y = touch.rpt[i].y;
        tp->data.coords[i].strength = touch.rpt[i].weight;
    }
    portEXIT_CRITICAL(&tp->data.lock);

    return ESP_OK;
}

static bool get_xy(esp_lcd_touch_handle_t tp, uint16_t *x, uint16_t *y, uint16_t *strength, uint8_t *point_num, uint8_t max_point_num)
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
    /* Clear available touch points count */
    tp->data.points = 0;
    portEXIT_CRITICAL(&tp->data.lock);

    return (*point_num > 0);
}

static esp_err_t del(esp_lcd_touch_handle_t tp)
{
    /* Reset GPIO pin settings */
    if (tp->config.int_gpio_num != GPIO_NUM_NC) {
        gpio_reset_pin(tp->config.int_gpio_num);
    }
    if (tp->config.rst_gpio_num != GPIO_NUM_NC) {
        gpio_reset_pin(tp->config.rst_gpio_num);
        if (tp->config.interrupt_callback) {
            gpio_isr_handler_remove(tp->config.int_gpio_num);
        }
    }
    /* Release memory */
    free(tp);

    return ESP_OK;
}

static esp_err_t reset(esp_lcd_touch_handle_t tp)
{
    TickType_t delay_tick = 0;

    if (tp->config.rst_gpio_num != GPIO_NUM_NC) {
        ESP_RETURN_ON_ERROR(gpio_set_level(tp->config.rst_gpio_num, tp->config.levels.reset), TAG, "GPIO set level failed");
        delay_tick = pdMS_TO_TICKS(2);
        vTaskDelay((delay_tick > 0) ? delay_tick : 1);
        ESP_RETURN_ON_ERROR(gpio_set_level(tp->config.rst_gpio_num, !tp->config.levels.reset), TAG, "GPIO set level failed");
        delay_tick = pdMS_TO_TICKS(22);
        vTaskDelay((delay_tick > 0) ? delay_tick : 1);
    }

    return ESP_OK;
}

#define i2c_write(data_p, len)      ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(tp->io, 0, data_p, len), TAG, "Tx failed");
#define i2c_read(data_p, len)       ESP_RETURN_ON_ERROR(esp_lcd_panel_io_rx_param(tp->io, 0, data_p, len), TAG, "Rx failed");

static esp_err_t write_tp_point_mode_cmd(esp_lcd_touch_handle_t tp)
{
    uint8_t sample_data[4];

    sample_data[0] = 0x50;
    sample_data[1] = 0x00;
    sample_data[2] = 0x00;
    sample_data[3] = 0x00;

    i2c_write(&sample_data[0], sizeof(sample_data));
    esp_rom_delay_us(200);
    return ESP_OK;
}

static esp_err_t write_tp_start_cmd(esp_lcd_touch_handle_t tp)
{
    uint8_t sample_data[4];

    sample_data[0] = 0x46;
    sample_data[1] = 0x00;
    sample_data[2] = 0x00;
    sample_data[3] = 0x00;

    i2c_write(&sample_data[0], sizeof(sample_data));
    esp_rom_delay_us(200);
    return ESP_OK;
}

static esp_err_t write_tp_cpu_start_cmd(esp_lcd_touch_handle_t tp)
{
    uint8_t sample_data[4];

    sample_data[0] = 0x04;
    sample_data[1] = 0x00;
    sample_data[2] = 0x01;
    sample_data[3] = 0x00;

    i2c_write(&sample_data[0], sizeof(sample_data));
    esp_rom_delay_us(200);
    return ESP_OK;
}

static esp_err_t write_tp_clear_int_cmd(esp_lcd_touch_handle_t tp)
{
    uint8_t sample_data[4];

    sample_data[0] = 0x02;
    sample_data[1] = 0x00;
    sample_data[2] = 0x01;
    sample_data[3] = 0x00;

    i2c_write(&sample_data[0], sizeof(sample_data));
    esp_rom_delay_us(200);
    return ESP_OK;
}

static esp_err_t read_tp_status_length(esp_lcd_touch_handle_t tp, tp_status_t *tp_status)
{
    uint8_t sample_data[4];

    sample_data[0] = 0x20;
    sample_data[1] = 0x00;

    i2c_write(&sample_data[0], 2);
    esp_rom_delay_us(200);
    i2c_read(&sample_data[0], sizeof(sample_data));
    esp_rom_delay_us(200);
    tp_status->status_low.pt_exist = (sample_data[0] & 0x01);
    tp_status->status_low.gesture = (sample_data[0] & 0x02);
    tp_status->status_high.tic_busy = ((sample_data[1] & 0x80) >> 7);
    tp_status->status_high.tic_in_bios = ((sample_data[1] & 0x40) >> 6);
    tp_status->status_high.tic_in_cpu = ((sample_data[1] & 0x20) >> 5);
    tp_status->status_high.tint_low = ((sample_data[1] & 0x10) >> 4);
    tp_status->status_high.cpu_run = ((sample_data[1] & 0x08) >> 3);
    tp_status->status_low.aux = ((sample_data[0] & 0x08)); //aux, cytang

    tp_status->read_len = (sample_data[3] << 8 | sample_data[2]);
    return ESP_OK;
}

static esp_err_t read_tp_hdp(esp_lcd_touch_handle_t tp, tp_status_t *tp_status, tp_touch_t *touch)
{
    uint8_t sample_data[4+(10*6)]; // 4 Bytes Header + 10 Finger * 6 Bytes
    uint8_t i, offset;
    uint8_t check_id;

    sample_data[0] = 0x00;
    sample_data[1] = 0x03;

    i2c_write(&sample_data[0], 2);
    esp_rom_delay_us(200);
    i2c_read(&sample_data[0], tp_status->read_len);
    esp_rom_delay_us(200);

    check_id = sample_data[4];

    if ((check_id <= 0x0A) && tp_status->status_low.pt_exist) {
        touch->touch_num = ((tp_status->read_len - 4)/6);
        touch->gesture = 0x00;

        for (i = 0; i < touch->touch_num; i++) {
            offset = i*6;
            touch->rpt[i].id = sample_data[4 + offset];
            touch->rpt[i].x = (((sample_data[7 + offset] & 0xF0) << 4)| sample_data[5 + offset]);
            touch->rpt[i].y = (((sample_data[7 + offset] & 0x0F) << 8)| sample_data[6 + offset]);
            touch->rpt[i].weight = sample_data[8 + offset];
        }

        /* For slide gesture recognize */
        if ((touch->rpt[0].weight != 0) && (touch->down != 1)) {
            touch->down = 1;
            touch->up = 0 ;
            touch->down_x = touch->rpt[0].x;
            touch->down_y = touch->rpt[0].y;
        } else if ((touch->rpt[0].weight == 0) && (touch->down == 1)) {
            touch->up = 1;
            touch->down = 0;
            touch->up_x = touch->rpt[0].x;
            touch->up_y = touch->rpt[0].y;
        }

        /* Dump Log */
        for (uint8_t finger_num = 0; finger_num < touch->touch_num; finger_num++) {
            ESP_LOGD(TAG, "ID[%d], X[%d], Y[%d], Weight[%d]\n",
                         touch->rpt[finger_num].id,
                         touch->rpt[finger_num].x,
                         touch->rpt[finger_num].y,
                         touch->rpt[finger_num].weight);
        }
    } else if ((check_id == 0xF6) && tp_status->status_low.gesture) {
        touch->touch_num = 0x00;
        touch->up = 0;
        touch->down = 0;
        touch->gesture = sample_data[6] & 0x07;
        ESP_LOGD(TAG, "gesture : 0x%02x\n", touch->gesture);
    } else {
        touch->touch_num = 0x00;
        touch->gesture = 0x00;
    }
    return ESP_OK;
}

static esp_err_t read_tp_hdp_status(esp_lcd_touch_handle_t tp, tp_hdp_status_t *tp_hdp_status)
{
    uint8_t sample_data[8];

    sample_data[0] = 0xFC;
    sample_data[1] = 0x02;

    i2c_write(&sample_data[0], 2);
    esp_rom_delay_us(200);
    i2c_read(&sample_data[0], sizeof(sample_data));
    esp_rom_delay_us(200);

    tp_hdp_status->status = sample_data[5];
    tp_hdp_status->next_packet_len = (sample_data[2] | sample_data[3] << 8);
    return ESP_OK;
}

static esp_err_t Read_HDP_REMAIN_DATA(esp_lcd_touch_handle_t tp, tp_hdp_status_t *tp_hdp_status)
{
    uint8_t sample_data[32];

    sample_data[0] = 0x00;
    sample_data[1] = 0x03;

    i2c_write(&sample_data[0], 2);
    esp_rom_delay_us(200);
    i2c_read(&sample_data[0], tp_hdp_status->next_packet_len);
    esp_rom_delay_us(200);
    return ESP_OK;
}

static esp_err_t read_fw_version(esp_lcd_touch_handle_t tp)
{
    uint8_t sample_data[18];
	uint16_t DVer;
	uint32_t Dummy, PID, ICName_H, ICName_L;

    sample_data[0] = 0x26;
    sample_data[1] = 0x00;

    i2c_write(&sample_data[0], 2);
    esp_rom_delay_us(200);
    i2c_read(&sample_data[0], 18);
    esp_rom_delay_us(200);

	Dummy = ((sample_data[0] << 24) | (sample_data[1] << 16) | (sample_data[3] << 8) | (sample_data[0]));
	DVer = ((sample_data[5] << 8) | (sample_data[4]));
	PID = ((sample_data[9] << 24) | (sample_data[8] << 16) | (sample_data[7] << 8) | (sample_data[6]));
	ICName_L = ((sample_data[13] << 24) | (sample_data[12] << 16) | (sample_data[11] << 8) | (sample_data[10]));    // "2010"
	ICName_H = ((sample_data[17] << 24) | (sample_data[16] << 16) | (sample_data[15] << 8) | (sample_data[14]));    // "SPD"

    ESP_LOGD(TAG, "Dummy[%"PRIu32"], DVer[%"PRIu16"], PID[%"PRIu32"], Name[%"PRIu32"-%"PRIu32"]", Dummy, DVer, PID, ICName_H, ICName_L);

    return ESP_OK;
}

static esp_err_t tp_read_data(esp_lcd_touch_handle_t tp, tp_touch_t *touch)
{
    tp_status_t tp_status = {0};
    tp_hdp_status_t tp_hdp_status = {0};

    ESP_RETURN_ON_ERROR(read_tp_status_length(tp, &tp_status), TAG, "Read status length failed");

    if (tp_status.status_high.tic_in_bios) {
        /* Write Clear TINT Command */
        ESP_RETURN_ON_ERROR(write_tp_clear_int_cmd(tp), TAG, "Write clear int cmd failed");

        /* Write CPU Start Command */
        ESP_RETURN_ON_ERROR(write_tp_cpu_start_cmd(tp), TAG, "Write cpu start cmd failed");

    } else if (tp_status.status_high.tic_in_cpu) {
        /* Write Touch Change Command */
        ESP_RETURN_ON_ERROR(write_tp_point_mode_cmd(tp), TAG, "Write point mode cmd failed");

        /* Write Touch Start Command */
        ESP_RETURN_ON_ERROR(write_tp_start_cmd(tp), TAG, "Write start cmd failed");

        /* Write Clear TINT Command */
        ESP_RETURN_ON_ERROR(write_tp_clear_int_cmd(tp), TAG, "Write clear int cmd failed");

    } else if (tp_status.status_high.cpu_run && tp_status.read_len == 0) {
        ESP_RETURN_ON_ERROR(write_tp_clear_int_cmd(tp), TAG, "Write clear int cmd failed");
    } else if (tp_status.status_low.pt_exist || tp_status.status_low.gesture) {
        /* Read HDP */
        ESP_RETURN_ON_ERROR(read_tp_hdp(tp, &tp_status, touch), TAG, "Read hdp failed");

hdp_done_check:
        /* Read HDP Status */
        ESP_RETURN_ON_ERROR(read_tp_hdp_status(tp, &tp_hdp_status), TAG, "Read hdp status failed");

        if (tp_hdp_status.status == 0x82) {
            /* Clear INT */
            ESP_RETURN_ON_ERROR(write_tp_clear_int_cmd(tp), TAG, "Write clear int cmd failed");
        }
        else if (tp_hdp_status.status == 0x00) {
            /* Read HDP Remain Data */
            ESP_RETURN_ON_ERROR(Read_HDP_REMAIN_DATA(tp, &tp_hdp_status), TAG, "Read hdp remain data failed");
            goto hdp_done_check;
        }
    } else if (tp_status.status_high.cpu_run && tp_status.status_low.aux) {
        ESP_RETURN_ON_ERROR(write_tp_clear_int_cmd(tp), TAG, "Write clear int cmd failed");
    }

    return ESP_OK;
}

