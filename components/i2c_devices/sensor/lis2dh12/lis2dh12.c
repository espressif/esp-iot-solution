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
#include "iot_lis2dh12.h"
#include "esp_log.h"

static const char* TAG = "lis2dh12";
#define IOT_CHECK(tag, a, ret)  if(!(a)) {                                 \
        ESP_LOGE(tag,"%s:%d (%s)", __FILE__, __LINE__, __FUNCTION__);      \
        return (ret);                                                      \
        }

#define ERR_ASSERT(tag, param)      IOT_CHECK(tag, (param) == ESP_OK, ESP_FAIL)
#define POINT_ASSERT(tag, param)    IOT_CHECK(tag, (param) != NULL, ESP_FAIL)
#define RES_ASSERT(tag, res, ret)   IOT_CHECK(tag, (res) != pdFALSE, ret)

typedef struct {
    i2c_bus_handle_t bus;
    uint16_t dev_addr;
} lis2dh12_dev_t;

esp_err_t iot_lis2dh12_write_byte(lis2dh12_handle_t sensor, uint8_t reg_addr, uint8_t data)
{
    lis2dh12_dev_t* sens = (lis2dh12_dev_t*) sensor;
    esp_err_t  ret;
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (sens->dev_addr << 1) | WRITE_BIT, ACK_CHECK_EN);
    i2c_master_write_byte(cmd, reg_addr, ACK_CHECK_EN);
    i2c_master_write_byte(cmd, data, ACK_CHECK_EN);
    ret = iot_i2c_bus_cmd_begin(sens->bus, cmd, 1000 / portTICK_RATE_MS);
    i2c_cmd_link_delete(cmd);
    return ret;
}

esp_err_t iot_lis2dh12_write(lis2dh12_handle_t sensor, uint8_t reg_start_addr, uint8_t reg_num, uint8_t *data_buf)
{
    uint32_t i = 0;
    for(i=0; i<reg_num; i++){
        ERR_ASSERT(TAG, iot_lis2dh12_write_byte(sensor, reg_start_addr+i, data_buf[i]));
    }
    return ESP_OK;
}

esp_err_t iot_lis2dh12_read_byte(lis2dh12_handle_t sensor, uint8_t reg, uint8_t *data)
{
    lis2dh12_dev_t* sens = (lis2dh12_dev_t*) sensor;
    esp_err_t ret;
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (sens->dev_addr << 1) | WRITE_BIT, ACK_CHECK_EN);
    i2c_master_write_byte(cmd, reg, ACK_CHECK_EN);
    i2c_master_stop(cmd);
    ret = iot_i2c_bus_cmd_begin(sens->bus, cmd, 1000 / portTICK_RATE_MS);
    i2c_cmd_link_delete(cmd);
    if (ret != ESP_OK) {
        return ret;
    }
    cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (sens->dev_addr << 1) | READ_BIT, ACK_CHECK_EN);
    i2c_master_read_byte(cmd, data, NACK_VAL);
    i2c_master_stop(cmd);
    ret = iot_i2c_bus_cmd_begin(sens->bus, cmd, 1000 / portTICK_RATE_MS);
    i2c_cmd_link_delete(cmd);
    return ret;
}

esp_err_t iot_lis2dh12_read(lis2dh12_handle_t sensor, uint8_t reg_start_addr, uint8_t reg_num, uint8_t *data_buf)
{
    uint32_t i = 0;
    uint8_t data_t = 0;
    if (data_buf != NULL) {
        for (i = 0; i < reg_num; i++) {
            ERR_ASSERT(TAG, iot_lis2dh12_read_byte(sensor, reg_start_addr + i, &data_t));
            data_buf[i] = data_t;
        }
        return ESP_OK;
    } else {
        return ESP_FAIL;
    }
}

esp_err_t iot_lis2dh12_get_deviceid(lis2dh12_handle_t sensor, uint8_t* deviceid)
{
    uint8_t tmp;
    ERR_ASSERT(TAG, iot_lis2dh12_read_byte(sensor, LIS2DH12_WHO_AM_I_REG, &tmp));
    *deviceid = tmp;
    return ESP_OK;
}

esp_err_t iot_lis2dh12_set_config(lis2dh12_handle_t sensor, lis2dh12_config_t* lis2dh12_config)
{
    uint8_t buffer[6];
    ERR_ASSERT(TAG, iot_lis2dh12_read(sensor, LIS2DH12_TEMP_CFG_REG, 1, buffer));
    buffer[0] &= ~LIS2DH12_TEMP_EN_MASK;
    buffer[0] |= ((uint8_t)lis2dh12_config->temp_enable)<<LIS2DH12_TEMP_EN_BIT;
    ERR_ASSERT(TAG, iot_lis2dh12_write(sensor, LIS2DH12_TEMP_CFG_REG, 1, buffer));
    ERR_ASSERT(TAG, iot_lis2dh12_read(sensor, LIS2DH12_CTRL_REG1, 6, buffer));

    buffer[0] &= (uint8_t) ~(LIS2DH12_ODR_MASK | LIS2DH12_LP_EN_MASK | LIS2DH12_Z_EN_MASK | LIS2DH12_Y_EN_MASK | LIS2DH12_X_EN_MASK);
    buffer[0] |= ((uint8_t)lis2dh12_config->odr)<<LIS2DH12_ODR_BIT;
    buffer[0] |= ((uint8_t)((lis2dh12_config->opt_mode>>1)<<LIS2DH12_LP_EN_BIT)&LIS2DH12_LP_EN_MASK);
    buffer[0] |= ((uint8_t)lis2dh12_config->z_enable)<<LIS2DH12_Z_EN_BIT;
    buffer[0] |= ((uint8_t)lis2dh12_config->y_enable)<<LIS2DH12_Y_EN_BIT;
    buffer[0] |= ((uint8_t)lis2dh12_config->x_enable)<<LIS2DH12_X_EN_BIT;

    buffer[3] &= ~(LIS2DH12_BDU_MASK | LIS2DH12_FS_MASK |LIS2DH12_HR_MASK);
    buffer[3] |= ((uint8_t)lis2dh12_config->bdu_status)<<LIS2DH12_BDU_BIT;
    buffer[3] |= ((uint8_t)lis2dh12_config->fs)<<LIS2DH12_FS_BIT;
    buffer[3] |= ((uint8_t)((lis2dh12_config->opt_mode)<<LIS2DH12_HR_BIT)&LIS2DH12_HR_MASK);
    ERR_ASSERT(TAG, iot_lis2dh12_write(sensor, LIS2DH12_CTRL_REG1, 6, buffer));
    return ESP_OK;
}

esp_err_t iot_lis2dh12_get_config(lis2dh12_handle_t sensor, lis2dh12_config_t* lis2dh12_config)
{
    uint8_t buffer[6];
    ERR_ASSERT(TAG, iot_lis2dh12_read(sensor, LIS2DH12_TEMP_CFG_REG, 1, buffer));
    lis2dh12_config->temp_enable = (lis2dh12_temp_en_t)((buffer[0] & LIS2DH12_TEMP_EN_MASK) >> LIS2DH12_TEMP_EN_BIT);
    
    ERR_ASSERT(TAG, iot_lis2dh12_read(sensor, LIS2DH12_CTRL_REG1, 6, buffer));
    lis2dh12_config->odr = (lis2dh12_odr_t)((buffer[0] & LIS2DH12_ODR_MASK) >> LIS2DH12_ODR_BIT);
    lis2dh12_config->z_enable = (lis2dh12_state_t)((buffer[0] & LIS2DH12_Z_EN_MASK) >> LIS2DH12_Z_EN_BIT);
    lis2dh12_config->y_enable = (lis2dh12_state_t)((buffer[0] & LIS2DH12_Y_EN_MASK) >> LIS2DH12_Y_EN_BIT);
    lis2dh12_config->x_enable = (lis2dh12_state_t)((buffer[0] & LIS2DH12_X_EN_MASK) >> LIS2DH12_X_EN_BIT);
    lis2dh12_config->bdu_status = (lis2dh12_state_t)((buffer[3] & LIS2DH12_BDU_MASK) >> LIS2DH12_BDU_BIT);
    lis2dh12_config->fs = (lis2dh12_fs_t)((buffer[3] & LIS2DH12_FS_MASK) >> LIS2DH12_FS_BIT);
    lis2dh12_config->opt_mode = (lis2dh12_opt_mode_t)((((buffer[0] & LIS2DH12_LP_EN_MASK)<<1) >> LIS2DH12_LP_EN_BIT)|((buffer[3] & LIS2DH12_HR_MASK) >> LIS2DH12_HR_BIT));
    return ESP_OK;
}

esp_err_t iot_lis2dh12_set_temp_enable(lis2dh12_handle_t sensor, lis2dh12_temp_en_t temp_en)
{
    uint8_t tmp;
    ERR_ASSERT(TAG, iot_lis2dh12_read_byte(sensor, LIS2DH12_TEMP_CFG_REG, &tmp));
    tmp &= ~LIS2DH12_TEMP_EN_MASK;
    tmp |= ((uint8_t)temp_en)<<LIS2DH12_TEMP_EN_BIT;
    ERR_ASSERT(TAG, iot_lis2dh12_write_byte(sensor, LIS2DH12_TEMP_CFG_REG, tmp));
    return ESP_OK;
}

esp_err_t iot_lis2dh12_set_odr(lis2dh12_handle_t sensor, lis2dh12_odr_t odr)
{
    uint8_t tmp;
    ERR_ASSERT(TAG, iot_lis2dh12_read_byte(sensor, LIS2DH12_CTRL_REG1, &tmp));
    tmp &= ~LIS2DH12_ODR_MASK;
    tmp |= ((uint8_t)odr)<<LIS2DH12_ODR_BIT;
    ERR_ASSERT(TAG, iot_lis2dh12_write_byte(sensor, LIS2DH12_CTRL_REG1, tmp));
    return ESP_OK;
}

esp_err_t iot_lis2dh12_set_z_enable(lis2dh12_handle_t sensor, lis2dh12_state_t status)
{
    uint8_t tmp;
    ERR_ASSERT(TAG, iot_lis2dh12_read_byte(sensor, LIS2DH12_CTRL_REG1, &tmp));
    tmp &= ~LIS2DH12_Z_EN_MASK;
    tmp |= ((uint8_t)status)<<LIS2DH12_Z_EN_BIT;
    ERR_ASSERT(TAG, iot_lis2dh12_write_byte(sensor, LIS2DH12_CTRL_REG1, tmp));
    return ESP_OK;
}

esp_err_t iot_lis2dh12_set_y_enable(lis2dh12_handle_t sensor, lis2dh12_state_t status)
{
    uint8_t tmp;
    ERR_ASSERT(TAG, iot_lis2dh12_read_byte(sensor, LIS2DH12_CTRL_REG1, &tmp));
    tmp &= ~LIS2DH12_Y_EN_MASK;
    tmp |= ((uint8_t)status)<<LIS2DH12_Y_EN_BIT;
    ERR_ASSERT(TAG, iot_lis2dh12_write_byte(sensor, LIS2DH12_CTRL_REG1, tmp));
    return ESP_OK;
}

esp_err_t iot_lis2dh12_set_x_enable(lis2dh12_handle_t sensor, lis2dh12_state_t status)
{
    uint8_t tmp;
    ERR_ASSERT(TAG, iot_lis2dh12_read_byte(sensor, LIS2DH12_CTRL_REG1, &tmp));
    tmp &= ~LIS2DH12_X_EN_MASK;
    tmp |= ((uint8_t)status)<<LIS2DH12_X_EN_BIT;
    ERR_ASSERT(TAG, iot_lis2dh12_write_byte(sensor, LIS2DH12_CTRL_REG1, tmp));
    return ESP_OK;
}

esp_err_t iot_lis2dh12_set_bdumode(lis2dh12_handle_t sensor, lis2dh12_state_t status)
{
    uint8_t tmp;
    ERR_ASSERT(TAG, iot_lis2dh12_read_byte(sensor, LIS2DH12_CTRL_REG4, &tmp));
    tmp &= ~LIS2DH12_BDU_MASK;
    tmp |= ((uint8_t)status)<<LIS2DH12_BDU_BIT;
    ERR_ASSERT(TAG, iot_lis2dh12_write_byte(sensor, LIS2DH12_CTRL_REG4, tmp));
    return ESP_OK;
}

esp_err_t iot_lis2dh12_set_fs(lis2dh12_handle_t sensor, lis2dh12_fs_t fs)
{
    uint8_t tmp;
    ERR_ASSERT(TAG, iot_lis2dh12_read_byte(sensor, LIS2DH12_CTRL_REG4, &tmp));
    tmp &= ~LIS2DH12_FS_MASK;
    tmp |= ((uint8_t)fs)<<LIS2DH12_FS_BIT;
    ERR_ASSERT(TAG, iot_lis2dh12_write_byte(sensor, LIS2DH12_CTRL_REG4, tmp));
    return ESP_OK;
}

esp_err_t iot_lis2dh12_set_opt_mode(lis2dh12_handle_t sensor, lis2dh12_opt_mode_t opt_mode)
{
    uint8_t tmp1,tmp2;
    ERR_ASSERT(TAG, iot_lis2dh12_read_byte(sensor, LIS2DH12_CTRL_REG1, &tmp1));
    ERR_ASSERT(TAG, iot_lis2dh12_read_byte(sensor, LIS2DH12_CTRL_REG4, &tmp2));
    tmp1 &= ~LIS2DH12_LP_EN_MASK;
    tmp1 |= ((uint8_t)((opt_mode>>1)<<LIS2DH12_LP_EN_BIT)&LIS2DH12_LP_EN_MASK);
    tmp2 &= ~LIS2DH12_HR_MASK;
    tmp2 |= ((uint8_t)opt_mode&LIS2DH12_HR_MASK)<<LIS2DH12_HR_BIT;
    ERR_ASSERT(TAG, iot_lis2dh12_write_byte(sensor, LIS2DH12_CTRL_REG1, tmp1));
    ERR_ASSERT(TAG, iot_lis2dh12_write_byte(sensor, LIS2DH12_CTRL_REG4, tmp2));
    return ESP_OK;
}

esp_err_t iot_lis2dh12_get_x_acc(lis2dh12_handle_t sensor, uint16_t *x_acc)
{
    uint8_t buffer[2];
    ERR_ASSERT(TAG, iot_lis2dh12_read(sensor, LIS2DH12_OUT_X_L_REG, 2, buffer));
    *x_acc = (int16_t)((((uint16_t)buffer[1])<<8) | (uint16_t)buffer[0]);
    return ESP_OK;
}

esp_err_t iot_lis2dh12_get_y_acc(lis2dh12_handle_t sensor, uint16_t *y_acc)
{
    uint8_t buffer[2];
    ERR_ASSERT(TAG, iot_lis2dh12_read(sensor, LIS2DH12_OUT_Y_L_REG, 2, buffer));
    *y_acc = (int16_t)((((uint16_t)buffer[1])<<8) | (uint16_t)buffer[0]);
    return ESP_OK;
}

esp_err_t iot_lis2dh12_get_z_acc(lis2dh12_handle_t sensor, uint16_t *z_acc)
{
    uint8_t buffer[2];
    ERR_ASSERT(TAG, iot_lis2dh12_read(sensor, LIS2DH12_OUT_Z_L_REG, 2, buffer));
    *z_acc = (int16_t)((((uint16_t)buffer[1])<<8) | (uint16_t)buffer[0]);
    return ESP_OK;
}

lis2dh12_handle_t iot_lis2dh12_create(lis2dh12_handle_t bus, uint16_t dev_addr)
{
    lis2dh12_dev_t* sensor = (lis2dh12_dev_t*) calloc(1, sizeof(lis2dh12_dev_t));
    sensor->bus = bus;
    sensor->dev_addr = dev_addr;
    return (lis2dh12_handle_t) sensor;
}

esp_err_t iot_lis2dh12_delete(lis2dh12_handle_t sensor, bool del_bus)
{
    lis2dh12_dev_t* sens = (lis2dh12_dev_t*) sensor;
    if(del_bus) {
        iot_i2c_bus_delete(sens->bus);
        sens->bus = NULL;
    }
    free(sens);
    return ESP_OK;
}

