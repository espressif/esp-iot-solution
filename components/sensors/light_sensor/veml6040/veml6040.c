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
#include <math.h>
#include <string.h>
#include "veml6040.h"

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
    i2c_bus_device_handle_t i2c_dev;
    uint8_t dev_addr;
    veml6040_config_t config;
} veml6040_dev_t;

static const char *TAG = "veml6040";

veml6040_handle_t veml6040_create(i2c_bus_handle_t bus, uint8_t dev_addr)
{
    veml6040_dev_t *sens = (veml6040_dev_t *) calloc(1, sizeof(veml6040_dev_t));
    sens->i2c_dev = i2c_bus_device_create(bus, dev_addr, i2c_bus_get_current_clk_speed(bus));
    if (sens->i2c_dev == NULL) {
        free(sens);
        return NULL;
    }
    sens->dev_addr = dev_addr;
    return (veml6040_handle_t) sens;
}

esp_err_t veml6040_delete(veml6040_handle_t *sensor)
{
    if (*sensor == NULL) {
        return ESP_OK;
    }
    veml6040_dev_t *sens = (veml6040_dev_t *)(*sensor);
    i2c_bus_device_delete(&sens->i2c_dev);
    free(sens);
    *sensor = NULL;
    return ESP_OK;
}

static int veml6040_read(veml6040_handle_t sensor, uint8_t cmd_code)
{
    if (sensor == NULL) {
        return ESP_FAIL;
    }
    uint8_t data_buf[2] = {0x00, 0x00};
    veml6040_dev_t *sens = (veml6040_dev_t *) sensor;
    esp_err_t ret = i2c_bus_read_bytes(sens->i2c_dev, cmd_code, 2, data_buf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "READ ERROR,ERROR ID:%d", ret);
        return VEML6040_I2C_ERR_RES;
    }
    return *(uint16_t *)data_buf;
}

esp_err_t veml6040_set_mode(veml6040_handle_t sensor, veml6040_config_t *device_info)
{
    if (sensor == NULL) {
        return ESP_FAIL;
    }
    veml6040_dev_t *sens = (veml6040_dev_t *) sensor;
    uint8_t cmd_buf[2] = {0x00};
    cmd_buf[0] = device_info->integration_time << 4
                 | device_info->trigger << 2
                 | device_info->mode << 1
                 | device_info->switch_en;
    sens->config.integration_time = device_info->integration_time;
    sens->config.trigger          = device_info->trigger;
    sens->config.mode             = device_info->mode;
    sens->config.switch_en        = device_info->switch_en;
    cmd_buf[1] = 0x00;
    esp_err_t ret = i2c_bus_write_bytes(sens->i2c_dev, VEML6040_SET_CMD, 2, cmd_buf);
    return ret;
}

esp_err_t veml6040_get_mode(veml6040_handle_t sensor, veml6040_config_t *device_info)
{
    if (sensor == NULL) {
        return ESP_FAIL;
    }
    veml6040_dev_t *sens = (veml6040_dev_t *) sensor;
    uint8_t data_buf[2] = {0x00};
    esp_err_t ret = i2c_bus_read_bytes(sens->i2c_dev, VEML6040_SET_CMD, 2, data_buf);
    if (ret != ESP_OK) {
        return ret;
    }
    sens->config.integration_time = device_info->integration_time = (data_buf[0] & 0x70) >> 4;
    sens->config.trigger          = device_info->trigger = (data_buf[0] & 0x04) >> 2;
    sens->config.mode             = device_info->mode = (data_buf[0] & 0x02) >> 1;
    sens->config.switch_en        = device_info->switch_en = (data_buf[0] & 0x01);
    return ret;
}

int veml6040_get_red(veml6040_handle_t sensor)
{
    return veml6040_read(sensor, VEML6040_READ_REG);
}

int veml6040_get_green(veml6040_handle_t sensor)
{
    return veml6040_read(sensor, VEML6040_READ_GREEN);
}

int veml6040_get_blue(veml6040_handle_t sensor)
{
    return veml6040_read(sensor, VEML6040_READ_BLUE);
}

int veml6040_get_white(veml6040_handle_t sensor)
{
    return veml6040_read(sensor, VEML6040_READ_WHITE);
}

float veml6040_get_lux(veml6040_handle_t sensor)
{
    uint16_t sensorValue;
    float ambientLightInLux;
    veml6040_dev_t *dev = (veml6040_dev_t *) sensor;
    sensorValue = veml6040_get_green(sensor);
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

float veml6040_get_cct(veml6040_handle_t sensor, float offset)
{
    uint16_t red, blue, green;
    float cct, ccti;
    red   = veml6040_get_red(sensor);
    green = veml6040_get_green(sensor);
    blue  = veml6040_get_blue(sensor);
    ccti  = ((float) red - (float) blue) / (float) green;
    ccti  = ccti + offset;
    cct   = 4278.6 * pow(ccti, -1.2455);
    return cct;
}

#ifdef CONFIG_SENSOR_LIGHT_INCLUDED_VEML6040

static veml6040_handle_t veml6040 = NULL;
static bool is_init = false;

esp_err_t light_sensor_veml6040_init(i2c_bus_handle_t i2c_bus)
{
    if (is_init || !i2c_bus) {
        return ESP_FAIL;
    }
    veml6040 = veml6040_create(i2c_bus, VEML6040_I2C_ADDRESS);
    if (!veml6040) {
        return ESP_FAIL;
    }
    veml6040_config_t veml6040_info;
    memset(&veml6040_info, 0, sizeof(veml6040_info));
    veml6040_info.integration_time = VEML6040_INTEGRATION_TIME_160MS;
    veml6040_info.mode = VEML6040_MODE_AUTO;
    veml6040_info.trigger = VEML6040_TRIGGER_DIS;
    veml6040_info.switch_en = VEML6040_SWITCH_EN;
    esp_err_t ret = veml6040_set_mode(veml6040, &veml6040_info);
    if (ret != ESP_OK) {
        return ESP_FAIL;
    }
    is_init = true;
    return ESP_OK;
}

esp_err_t light_sensor_veml6040_deinit(void)
{
    if (!is_init) {
        return ESP_FAIL;
    }
    esp_err_t ret = veml6040_delete(&veml6040);

    if (ret != ESP_OK) {
        return ESP_FAIL;
    }
    is_init = false;
    return ESP_OK;
}

esp_err_t light_sensor_veml6040_test(void)
{
    if (!is_init) {
        return ESP_FAIL;
    }
    return ESP_OK;
}

esp_err_t light_sensor_veml6040_acquire_rgbw(float *r, float *g, float *b, float *w)
{
    if (!is_init) {
        return ESP_FAIL;
    }
    /*TODO:should more carefully*/
    *r = veml6040_get_red(veml6040);
    *g = veml6040_get_green(veml6040);
    *b = veml6040_get_blue(veml6040);
    *w = veml6040_get_white(veml6040);
    return ESP_OK;
}

#endif
