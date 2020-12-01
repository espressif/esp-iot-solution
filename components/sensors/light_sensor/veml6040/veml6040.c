// Copyright 2015-2016 Espressif Systems (Shanghai) PTE LTD
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
#include "driver/i2c.h"
#include "iot_i2c_bus.h"
#include "iot_veml6040.h"
#include "esp_log.h"
#include <math.h>

#define WRITE_BIT             I2C_MASTER_WRITE  /*!< I2C master write */
#define READ_BIT              I2C_MASTER_READ   /*!< I2C master read */
#define ACK_CHECK_EN          0x1               /*!< I2C master will check ack from slave*/
#define ACK_CHECK_DIS         0x0               /*!< I2C master will not check ack from slave */
#define ACK_VAL               0x0               /*!< I2C ack value */
#define NACK_VAL              0x1               /*!< I2C nack value */
//CMD
#define VEML6040_SET_CMD      0x00
#define VEML6040_READ_REG     0x08
#define VEML6040_READ_GREEN   0x09
#define VEML6040_READ_BLUE    0x0A
#define VEML6040_READ_WHITE   0x0B
// G SENSITIVITY
#define VEML6040_GSENS_40MS   0.25168
#define VEML6040_GSENS_80MS   0.12584
#define VEML6040_GSENS_160MS  0.06292
#define VEML6040_GSENS_320MS  0.03146
#define VEML6040_GSENS_640MS  0.01573
#define VEML6040_GSENS_1280MS 0.007865

typedef struct {
    i2c_bus_handle_t bus;
    uint16_t dev_addr;
    veml6040_config_t config;
} veml6040_dev_t;

static const char *TAG = "veml6040";

veml6040_handle_t iot_veml6040_create(i2c_bus_handle_t bus, uint16_t dev_addr)
{
    veml6040_dev_t* sensor = (veml6040_dev_t*) calloc(1, sizeof(veml6040_dev_t));
    sensor->bus = bus;
    sensor->dev_addr = dev_addr;
    sensor->config.integration_time = VEML6040_INTEGRATION_TIME_DEFAULT;
    sensor->config.mode = VEML6040_MODE_DEFAULT;
    sensor->config.trigger = VEML6040_TRIGGER_DEFAULT;
    sensor->config.switch_en = VEML6040_SWITCH_DEFAULT;
    iot_veml6040_set_mode(sensor, &sensor->config);
    return (veml6040_handle_t) sensor;
}

esp_err_t iot_veml6040_delete(veml6040_handle_t sensor, bool del_bus)
{
    veml6040_dev_t* device = (veml6040_dev_t*) sensor;
    if (del_bus) {
        iot_i2c_bus_delete(device->bus);
        device->bus = NULL;
    }
    free(device);
    return ESP_OK;
}

static esp_err_t iot_veml6040_write(veml6040_handle_t sensor, uint8_t config_val)
{
    esp_err_t ret;
    veml6040_dev_t* device = (veml6040_dev_t*) sensor;
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (device->dev_addr << 1) | WRITE_BIT, ACK_CHECK_EN);
    i2c_master_write_byte(cmd, VEML6040_SET_CMD, ACK_CHECK_EN);
    i2c_master_write_byte(cmd, config_val, ACK_CHECK_EN);
    i2c_master_write_byte(cmd, 0, ACK_CHECK_EN);
    i2c_master_stop(cmd);
    ret = iot_i2c_bus_cmd_begin(device->bus, cmd, 1000 / portTICK_RATE_MS);
    i2c_cmd_link_delete(cmd);
    return ret;
}

static int iot_veml6040_read(veml6040_handle_t sensor, uint8_t cmd_code)
{
    esp_err_t ret;
    uint8_t data_low = 0;
    uint8_t data_high = 0;
    veml6040_dev_t* device = (veml6040_dev_t*) sensor;
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (device->dev_addr << 1) | WRITE_BIT, ACK_CHECK_EN);
    i2c_master_write_byte(cmd, cmd_code, ACK_CHECK_EN);
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (device->dev_addr << 1) | READ_BIT, ACK_CHECK_EN);
    i2c_master_read_byte(cmd, &data_low, ACK_VAL);
    i2c_master_read_byte(cmd, &data_high, ACK_VAL);
    i2c_master_stop(cmd);
    ret = iot_i2c_bus_cmd_begin(device->bus, cmd, 1000 / portTICK_RATE_MS);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "READ ERROR,ERROR ID:%d", ret);
    }
    i2c_cmd_link_delete(cmd);
    return (ret == ESP_OK) ? (data_low | (data_high << 8)) : VEML6040_I2C_ERR_RES;
}

esp_err_t iot_veml6040_set_mode(veml6040_handle_t sensor, veml6040_config_t * device_info)
{
    uint8_t cmd_buf = 0;
    veml6040_dev_t* dev = (veml6040_dev_t*) sensor;
    cmd_buf = device_info->integration_time << 4
            | device_info->trigger << 2
            | device_info->mode << 1
            | device_info->switch_en;
    dev->config.integration_time = device_info->integration_time;
    dev->config.trigger          = device_info->trigger;
    dev->config.mode             = device_info->mode;
    dev->config.switch_en        = device_info->switch_en;
    return iot_veml6040_write(sensor, cmd_buf);
}

int iot_veml6040_get_red(veml6040_handle_t sensor)
{
    return iot_veml6040_read(sensor, VEML6040_READ_REG);
}

int iot_veml6040_get_green(veml6040_handle_t sensor)
{
    return iot_veml6040_read(sensor, VEML6040_READ_GREEN);
}

int iot_veml6040_get_blue(veml6040_handle_t sensor)
{
    return iot_veml6040_read(sensor, VEML6040_READ_BLUE);
}

int iot_veml6040_get_white(veml6040_handle_t sensor)
{
    return iot_veml6040_read(sensor, VEML6040_READ_WHITE);
}

float iot_veml6040_get_lux(veml6040_handle_t sensor)
{
    uint16_t sensorValue;
    float ambientLightInLux;
    veml6040_dev_t* dev = (veml6040_dev_t*) sensor;
    sensorValue = iot_veml6040_get_green(sensor);
    switch (dev->config.integration_time) {
        case VEML6040_INTEGRATION_TIME_40MS:
            ambientLightInLux = sensorValue * VEML6040_GSENS_40MS;
            break;
        case VEML6040_INTEGRATION_TIME_80MS:
            ambientLightInLux = sensorValue * VEML6040_GSENS_80MS;
            break;
        case VEML6040_INTEGRATION_TIME_160MS:
            ambientLightInLux = sensorValue * VEML6040_GSENS_160MS;
            break;
        case VEML6040_INTEGRATION_TIME_320MS:
            ambientLightInLux = sensorValue * VEML6040_GSENS_320MS;
            break;
        case VEML6040_INTEGRATION_TIME_640MS:
            ambientLightInLux = sensorValue * VEML6040_GSENS_640MS;
            break;
        case VEML6040_INTEGRATION_TIME_1280MS:
            ambientLightInLux = sensorValue * VEML6040_GSENS_1280MS;
            break;
        default:
            ambientLightInLux = VEML6040_I2C_ERR_RES;
            break;
    }
    return ambientLightInLux;
}

float iot_veml6040_get_cct(veml6040_handle_t sensor, float offset)
{
    uint16_t red, blue, green;
    float cct, ccti;
    red   = iot_veml6040_get_red(sensor);
    green = iot_veml6040_get_green(sensor);
    blue  = iot_veml6040_get_blue(sensor);
    ccti  = ((float) red - (float) blue) / (float) green;
    ccti  = ccti + offset;
    cct   = 4278.6 * pow(ccti, -1.2455);
    return cct;
}
