/*
 * SPDX-FileCopyrightText: 2022-2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <string.h>
#include "esp_log.h"
#include "ch450.h"
#include "i2c_bus.h"

static const char *TAG = "CH450";

#define CH450_CHECK(a, str, ret)  if(!(a)) {                                             \
        ESP_LOGE(TAG,"%s:%d (%s):%s", __FILE__, __LINE__, __FUNCTION__, str);      \
        return (ret);                                                                   \
        }

#define ACK_CHECK_EN   0x1     /*!< I2C master will check ack from slave*/
#define ACK_CHECK_DIS  0x0     /*!< I2C master will not check ack from slave */

/** Define system parameter */
#define CH450_SYS_SLEEP_MASK  (1<<7)
#define CH450_SYS_INTENS_MASK (3<<5)
#define CH450_SYS_KEYB_MASK   (1<<1)
#define CH450_SYS_DISP_MASK   (1<<0)

/**
 * @brief The digit array depend on hardware connection
 *
 */
static const uint8_t ch450_digit[10] = {
    0xee, // number 0
    0x28, // number 1
    0xcd, // number 2
    0x6d, // number 3
    0x2b, // number 4
    0x67, // number 5
    0xe7, // number 6
    0x2c, // number 7
    0xef, // number 8
    0x6f, // number 9
};

typedef struct {
    i2c_bus_device_handle_t i2c_dev;
} ch450_dev_t;

ch450_handle_t ch450_create(i2c_bus_handle_t bus)
{
    CH450_CHECK(NULL != bus, "handle of i2c bus is invalid", NULL);
    ch450_dev_t *seg = (ch450_dev_t *) calloc(1, sizeof(ch450_dev_t));
    CH450_CHECK(NULL != seg, "memory for ch450 handle is not enough", NULL);
    i2c_bus_device_handle_t i2c_dev = i2c_bus_device_create(bus, 0, 0); /** note: The ch450 device dose not have slave address */
    if (NULL == i2c_dev) {
        free(seg);
        CH450_CHECK(false, "create i2c device failed", NULL);
    }

    seg->i2c_dev = i2c_dev;
    /** Set ch450 to display and disable keyboard */
    esp_err_t ret = ch450_write(seg, CH450_SYS, CH450_SYS_DISP_MASK);
    if (ESP_OK != ret) {
        ch450_delete((ch450_handle_t)seg);
        CH450_CHECK(false, "Configure system parameter failed", NULL);
    }
    return (ch450_handle_t) seg;
}

esp_err_t ch450_delete(ch450_handle_t dev)
{
    CH450_CHECK(NULL != dev, "handle is invalid", ESP_ERR_INVALID_ARG);
    ch450_dev_t *seg = (ch450_dev_t *) dev;
    i2c_bus_device_delete(&seg->i2c_dev);
    free(seg);
    return ESP_OK;
}

esp_err_t ch450_write(ch450_handle_t dev, ch450_cmd_t ch450_cmd, uint8_t val)
{
    CH450_CHECK(NULL != dev, "handle is invalid", ESP_ERR_INVALID_ARG);
    ch450_dev_t *seg = (ch450_dev_t *) dev;
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (uint8_t) ch450_cmd, ACK_CHECK_EN);
    i2c_master_write_byte(cmd, val, ACK_CHECK_EN);
    i2c_master_stop(cmd);
    esp_err_t ret = i2c_bus_cmd_begin(seg->i2c_dev, cmd);
    i2c_cmd_link_delete(cmd);
    return ret;
}

esp_err_t ch450_write_num(ch450_handle_t dev, uint8_t seg_idx, uint8_t val)
{
    CH450_CHECK(NULL != dev, "handle is invalid", ESP_ERR_INVALID_ARG);
    ch450_cmd_t seg_cmd = CH450_SEG_2 + seg_idx * 2;
    CH450_CHECK(val < 10, "Value should be less than 10", ESP_ERR_INVALID_ARG);
    return ch450_write(dev, seg_cmd, ch450_digit[val]);
}
