// Copyright 2020-2021 Espressif Systems (Shanghai) PTE LTD
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
#include <string.h>
#include <math.h>
#include "veml6075.h"

veml6075_handle_t veml6075_create(i2c_bus_handle_t bus, uint8_t dev_addr)
{
    veml6075_dev_t *sens = (veml6075_dev_t *) calloc(1, sizeof(veml6075_dev_t));
    sens->i2c_dev = i2c_bus_device_create(bus, dev_addr, i2c_bus_get_current_clk_speed(bus));
    if (sens->i2c_dev == NULL) {
        free(sens);
        return NULL;
    }
    sens->dev_addr = dev_addr;
    return (veml6075_handle_t) sens;
}

esp_err_t veml6075_delete(veml6075_handle_t *sensor)
{
    if (*sensor == NULL) {
        return ESP_OK;
    }
    veml6075_dev_t *sens = (veml6075_dev_t *)(*sensor);
    i2c_bus_device_delete(&sens->i2c_dev);
    free(sens);
    *sensor = NULL;
    return ESP_OK;
}

esp_err_t veml6075_set_mode(veml6075_handle_t sensor, veml6075_config_t *device_info)
{
    if (sensor == NULL) {
        return ESP_FAIL;
    }
    veml6075_dev_t *sens = (veml6075_dev_t *) sensor;
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
    esp_err_t ret = i2c_bus_write_bytes(sens->i2c_dev, VEML6075_RW_CFG, 2, cmd_buf);
    return ret;
}

esp_err_t veml6075_get_mode(veml6075_handle_t sensor, veml6075_config_t *device_info)
{
    if (sensor == NULL) {
        return ESP_FAIL;
    }
    veml6075_dev_t *sens = (veml6075_dev_t *) sensor;
    uint8_t data_buf[2] = {0x00};
    esp_err_t ret = i2c_bus_read_bytes(sens->i2c_dev, VEML6075_RW_CFG, 2, data_buf);

    if (ret != ESP_OK) {
        return ret;
    }
    sens->config.integration_time = device_info->integration_time = (data_buf[0] & 0x70) >> 4;
    sens->config.trigger          = device_info->trigger = (data_buf[0] & 0x04) >> 2;
    sens->config.mode             = device_info->mode = (data_buf[0] & 0x02) >> 1;
    sens->config.switch_en        = device_info->switch_en = (data_buf[0] & 0x01);
    return ret;
}

static int veml6075_read(veml6075_handle_t sensor, uint8_t cmd_code)
{
    if (sensor == NULL) {
        return 0;
    }
    uint8_t data_buf[2] = {0x00, 0x00};
    veml6075_dev_t *sens = (veml6075_dev_t *) sensor;
    esp_err_t ret = i2c_bus_read_bytes(sens->i2c_dev, cmd_code, 2, data_buf);
    if (ret != ESP_OK) {
        printf("READ ERROR,ERROR ID:%d", ret);
        return VEML6075_I2C_ERR_RES;
    }
    uint16_t value = (data_buf[1] << 8) | data_buf[0];
    return (int)value;
}

esp_err_t veml6075_get_raw_data(veml6075_handle_t sensor)
{
    if (sensor == NULL) {
        return ESP_FAIL;
    }
    veml6075_dev_t *sens = (veml6075_dev_t *)(sensor);
    sens->raw_data.raw_uva  = veml6075_read(sensor, VEML6075_READ_UVA);
    sens->raw_data.raw_uvb  = veml6075_read(sensor, VEML6075_READ_UVB);
    sens->raw_data.raw_dark = veml6075_read(sensor, VEML6075_READ_DARK);
    sens->raw_data.raw_vis  = veml6075_read(sensor, VEML6075_READ_VIS);
    sens->raw_data.raw_ir   = veml6075_read(sensor, VEML6075_READ_IR);
    return ESP_OK;
}

float veml6075_get_uva(veml6075_handle_t sensor)
{
    float comp_vis;
    float comp_ir;
    float comp_uva;
    if (ESP_OK != veml6075_get_raw_data(sensor)) {
        return 0;
    }
    veml6075_dev_t *sens = (veml6075_dev_t *)(sensor);
    comp_vis = sens->raw_data.raw_vis - sens->raw_data.raw_dark;
    comp_ir  = sens->raw_data.raw_ir - sens->raw_data.raw_dark;
    comp_uva = sens->raw_data.raw_uva - sens->raw_data.raw_dark;
    comp_uva -= (VEML6075_UVI_UVA_VIS_COEFF * comp_vis) - (VEML6075_UVI_UVA_IR_COEFF * comp_ir);
    return comp_uva;
}

float veml6075_get_uvb(veml6075_handle_t sensor)
{
    float comp_vis;
    float comp_ir;
    float comp_uvb;
    if (ESP_OK != veml6075_get_raw_data(sensor)) {
        return 0;
    }
    veml6075_dev_t *sens = (veml6075_dev_t *)(sensor);
    comp_vis = sens->raw_data.raw_vis - sens->raw_data.raw_dark;
    comp_ir  = sens->raw_data.raw_ir - sens->raw_data.raw_dark;
    comp_uvb = sens->raw_data.raw_uvb - sens->raw_data.raw_dark;
    comp_uvb -= (VEML6075_UVI_UVB_VIS_COEFF * comp_vis) - (VEML6075_UVI_UVB_IR_COEFF * comp_ir);
    return comp_uvb;
}

float veml6075_get_uv_index(veml6075_handle_t sensor)
{
    float uva_weighted;
    float uvb_weighted;
    if (ESP_OK != veml6075_get_raw_data(sensor)) {
        return 0;
    }
    uva_weighted = veml6075_get_uva(sensor) * VEML6075_UVI_UVA_RESPONSE;
    uvb_weighted = veml6075_get_uvb(sensor) * VEML6075_UVI_UVB_RESPONSE;
    return (uva_weighted + uvb_weighted) / 2.0;
}

uint16_t veml6075_get_raw_uva(veml6075_handle_t sensor)
{
    if (ESP_OK != veml6075_get_raw_data(sensor)) {
        return 0;
    }
    veml6075_dev_t *sens = (veml6075_dev_t *)(sensor);
    return sens->raw_data.raw_uva;
}

uint16_t veml6075_get_raw_uvb(veml6075_handle_t sensor)
{
    if (ESP_OK != veml6075_get_raw_data(sensor)) {
        return 0;
    }
    veml6075_dev_t *sens = (veml6075_dev_t *)(sensor);
    return sens->raw_data.raw_uvb;
}

uint16_t veml6075_get_raw_dark(veml6075_handle_t sensor)
{
    if (ESP_OK != veml6075_get_raw_data(sensor)) {
        return 0;
    }
    veml6075_dev_t *sens = (veml6075_dev_t *)(sensor);
    return sens->raw_data.raw_dark;
}

uint16_t veml6075_get_raw_vis(veml6075_handle_t sensor)
{
    if (ESP_OK != veml6075_get_raw_data(sensor)) {
        return 0;
    }
    veml6075_dev_t *sens = (veml6075_dev_t *)(sensor);
    return sens->raw_data.raw_vis;
}

uint16_t veml6075_get_raw_ir(veml6075_handle_t sensor)
{
    if (ESP_OK != veml6075_get_raw_data(sensor)) {
        return 0;
    }
    veml6075_dev_t *sens = (veml6075_dev_t *)(sensor);
    return sens->raw_data.raw_ir;
}

#ifdef CONFIG_SENSOR_LIGHT_INCLUDED_VEML6075

static veml6075_handle_t veml6075 = NULL;
static bool is_init = false;

esp_err_t light_sensor_veml6075_init(i2c_bus_handle_t i2c_bus)
{
    if (is_init || !i2c_bus) {
        return ESP_FAIL;
    }
    veml6075 = veml6075_create(i2c_bus, VEML6075_I2C_ADDRESS);
    if (!veml6075) {
        return ESP_FAIL;
    }
    veml6075_config_t veml6075_info;
    memset(&veml6075_info, 0, sizeof(veml6075_info));
    veml6075_info.integration_time = VEML6075_INTEGRATION_TIME_200MS;
    veml6075_info.mode = VEML6075_MODE_AUTO;
    veml6075_info.trigger = VEML6075_TRIGGER_DIS;
    veml6075_info.switch_en = VEML6075_SWITCH_EN;
    esp_err_t ret = veml6075_set_mode(veml6075, &veml6075_info);
    if (ret != ESP_OK) {
        return ESP_FAIL;
    }
    is_init = true;
    return ESP_OK;
}

esp_err_t light_sensor_veml6075_deinit(void)
{
    if (!is_init) {
        return ESP_FAIL;
    }
    esp_err_t ret = veml6075_delete(&veml6075);
    if (ret != ESP_OK) {
        return ESP_FAIL;
    }
    is_init = false;
    return ESP_OK;
}

esp_err_t light_sensor_veml6075_test(void)
{
    if (!is_init) {
        return ESP_FAIL;
    }
    return ESP_OK;
}

esp_err_t light_sensor_veml6075_acquire_uv(float *uv, float *uva, float *uvb)
{
    if (!is_init) {
        return ESP_FAIL;
    }
    *uva = veml6075_get_uva(veml6075);
    *uvb = veml6075_get_uvb(veml6075);
    *uv = veml6075_get_uv_index(veml6075);
    return ESP_OK;
}

#endif