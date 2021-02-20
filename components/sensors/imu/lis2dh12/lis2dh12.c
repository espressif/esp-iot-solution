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
#include "lis2dh12.h"
#include "esp_log.h"

static const char *TAG = "lis2dh12";
#define IOT_CHECK(tag, a, ret)  if(!(a)) {                                 \
        ESP_LOGE(tag,"%s:%d (%s)", __FILE__, __LINE__, __FUNCTION__);      \
        return (ret);                                                      \
    }

#define ERR_ASSERT(tag, param)      IOT_CHECK(tag, (param) == ESP_OK, ESP_FAIL)
#define POINT_ASSERT(tag, param)    IOT_CHECK(tag, (param) != NULL, ESP_FAIL)
#define RES_ASSERT(tag, res, ret)   IOT_CHECK(tag, (res) != pdFALSE, ret)

typedef struct {
    i2c_bus_device_handle_t i2c_dev;
    uint8_t dev_addr;
} lis2dh12_dev_t;

lis2dh12_handle_t lis2dh12_create(i2c_bus_handle_t bus, uint8_t dev_addr)
{
    lis2dh12_dev_t *sens = (lis2dh12_dev_t *) calloc(1, sizeof(lis2dh12_dev_t));
    sens->i2c_dev = i2c_bus_device_create(bus, dev_addr, i2c_bus_get_current_clk_speed(bus));
    if (sens->i2c_dev == NULL) {
        free(sens);
        return NULL;
    }
    sens->dev_addr = dev_addr;
    return (lis2dh12_handle_t) sens;
}

esp_err_t lis2dh12_delete(lis2dh12_handle_t *sensor)
{
    if (*sensor == NULL) {
        return ESP_OK;
    }

    lis2dh12_dev_t *sens = (lis2dh12_dev_t *)(*sensor);
    i2c_bus_device_delete(&sens->i2c_dev);
    free(sens);
    *sensor = NULL;
    return ESP_OK;
}

esp_err_t lis2dh12_get_deviceid(lis2dh12_handle_t sensor, uint8_t *deviceid)
{
    lis2dh12_dev_t *sens = (lis2dh12_dev_t *) sensor;
    uint8_t tmp;
    ERR_ASSERT(TAG, i2c_bus_read_byte(sens->i2c_dev, LIS2DH12_WHO_AM_I_REG, &tmp));
    *deviceid = tmp;
    return ESP_OK;
}

esp_err_t lis2dh12_set_config(lis2dh12_handle_t sensor, lis2dh12_config_t *lis2dh12_config)
{
    lis2dh12_dev_t *sens = (lis2dh12_dev_t *) sensor;
    uint8_t buffer[6];
    ERR_ASSERT(TAG, i2c_bus_read_byte(sens->i2c_dev, LIS2DH12_TEMP_CFG_REG, buffer));
    buffer[0] &= ~LIS2DH12_TEMP_EN_MASK;
    buffer[0] |= ((uint8_t)lis2dh12_config->temp_enable) << LIS2DH12_TEMP_EN_BIT;
    ERR_ASSERT(TAG, i2c_bus_write_byte(sens->i2c_dev, LIS2DH12_TEMP_CFG_REG, buffer[0]));
    ERR_ASSERT(TAG, i2c_bus_read_bytes(sens->i2c_dev, LIS2DH12_CTRL_REG1 | LIS2DH12_I2C_MULTI_REG_ONCE, 6, buffer));

    buffer[0] &= (uint8_t) ~(LIS2DH12_ODR_MASK | LIS2DH12_LP_EN_MASK | LIS2DH12_Z_EN_MASK | LIS2DH12_Y_EN_MASK | LIS2DH12_X_EN_MASK);
    buffer[0] |= ((uint8_t)lis2dh12_config->odr) << LIS2DH12_ODR_BIT;
    buffer[0] |= ((uint8_t)((lis2dh12_config->opt_mode >> 1) << LIS2DH12_LP_EN_BIT)&LIS2DH12_LP_EN_MASK);
    buffer[0] |= ((uint8_t)lis2dh12_config->z_enable) << LIS2DH12_Z_EN_BIT;
    buffer[0] |= ((uint8_t)lis2dh12_config->y_enable) << LIS2DH12_Y_EN_BIT;
    buffer[0] |= ((uint8_t)lis2dh12_config->x_enable) << LIS2DH12_X_EN_BIT;

    buffer[3] &= ~(LIS2DH12_BDU_MASK | LIS2DH12_FS_MASK | LIS2DH12_HR_MASK);
    buffer[3] |= ((uint8_t)lis2dh12_config->bdu_status) << LIS2DH12_BDU_BIT;
    buffer[3] |= ((uint8_t)lis2dh12_config->fs) << LIS2DH12_FS_BIT;
    buffer[3] |= ((uint8_t)((lis2dh12_config->opt_mode) << LIS2DH12_HR_BIT)&LIS2DH12_HR_MASK);
    ERR_ASSERT(TAG, i2c_bus_write_bytes(sens->i2c_dev, LIS2DH12_CTRL_REG1 | LIS2DH12_I2C_MULTI_REG_ONCE, 6, buffer));
    return ESP_OK;
}

esp_err_t lis2dh12_get_config(lis2dh12_handle_t sensor, lis2dh12_config_t *lis2dh12_config)
{
    lis2dh12_dev_t *sens = (lis2dh12_dev_t *) sensor;
    uint8_t buffer[6];
    ERR_ASSERT(TAG, i2c_bus_read_bytes(sens->i2c_dev, LIS2DH12_TEMP_CFG_REG, 1, buffer));
    lis2dh12_config->temp_enable = (lis2dh12_temp_en_t)((buffer[0] & LIS2DH12_TEMP_EN_MASK) >> LIS2DH12_TEMP_EN_BIT);

    ERR_ASSERT(TAG, i2c_bus_read_bytes(sens->i2c_dev, LIS2DH12_CTRL_REG1 | LIS2DH12_I2C_MULTI_REG_ONCE, 6, buffer));
    lis2dh12_config->odr = (lis2dh12_odr_t)((buffer[0] & LIS2DH12_ODR_MASK) >> LIS2DH12_ODR_BIT);
    lis2dh12_config->z_enable = (lis2dh12_state_t)((buffer[0] & LIS2DH12_Z_EN_MASK) >> LIS2DH12_Z_EN_BIT);
    lis2dh12_config->y_enable = (lis2dh12_state_t)((buffer[0] & LIS2DH12_Y_EN_MASK) >> LIS2DH12_Y_EN_BIT);
    lis2dh12_config->x_enable = (lis2dh12_state_t)((buffer[0] & LIS2DH12_X_EN_MASK) >> LIS2DH12_X_EN_BIT);
    lis2dh12_config->bdu_status = (lis2dh12_state_t)((buffer[3] & LIS2DH12_BDU_MASK) >> LIS2DH12_BDU_BIT);
    lis2dh12_config->fs = (lis2dh12_fs_t)((buffer[3] & LIS2DH12_FS_MASK) >> LIS2DH12_FS_BIT);
    lis2dh12_config->opt_mode = (lis2dh12_opt_mode_t)((((buffer[0] & LIS2DH12_LP_EN_MASK) << 1) >> LIS2DH12_LP_EN_BIT) | ((buffer[3] & LIS2DH12_HR_MASK) >> LIS2DH12_HR_BIT));
    return ESP_OK;
}

esp_err_t lis2dh12_set_temp_enable(lis2dh12_handle_t sensor, lis2dh12_temp_en_t temp_en)
{
    lis2dh12_dev_t *sens = (lis2dh12_dev_t *) sensor;
    uint8_t tmp;
    ERR_ASSERT(TAG, i2c_bus_read_byte(sens->i2c_dev, LIS2DH12_TEMP_CFG_REG, &tmp));
    tmp &= ~LIS2DH12_TEMP_EN_MASK;
    tmp |= ((uint8_t)temp_en) << LIS2DH12_TEMP_EN_BIT;
    ERR_ASSERT(TAG, i2c_bus_write_byte(sens->i2c_dev, LIS2DH12_TEMP_CFG_REG, tmp));
    return ESP_OK;
}

esp_err_t lis2dh12_set_odr(lis2dh12_handle_t sensor, lis2dh12_odr_t odr)
{
    lis2dh12_dev_t *sens = (lis2dh12_dev_t *) sensor;
    uint8_t tmp;
    ERR_ASSERT(TAG, i2c_bus_read_byte(sens->i2c_dev, LIS2DH12_CTRL_REG1, &tmp));
    tmp &= ~LIS2DH12_ODR_MASK;
    tmp |= ((uint8_t)odr) << LIS2DH12_ODR_BIT;
    ERR_ASSERT(TAG, i2c_bus_write_byte(sens->i2c_dev, LIS2DH12_CTRL_REG1, tmp));
    return ESP_OK;
}

esp_err_t lis2dh12_set_z_enable(lis2dh12_handle_t sensor, lis2dh12_state_t status)
{
    lis2dh12_dev_t *sens = (lis2dh12_dev_t *) sensor;
    uint8_t tmp;
    ERR_ASSERT(TAG, i2c_bus_read_byte(sens->i2c_dev, LIS2DH12_CTRL_REG1, &tmp));
    tmp &= ~LIS2DH12_Z_EN_MASK;
    tmp |= ((uint8_t)status) << LIS2DH12_Z_EN_BIT;
    ERR_ASSERT(TAG, i2c_bus_write_byte(sens->i2c_dev, LIS2DH12_CTRL_REG1, tmp));
    return ESP_OK;
}

esp_err_t lis2dh12_set_y_enable(lis2dh12_handle_t sensor, lis2dh12_state_t status)
{
    lis2dh12_dev_t *sens = (lis2dh12_dev_t *) sensor;
    uint8_t tmp;
    ERR_ASSERT(TAG, i2c_bus_read_byte(sens->i2c_dev, LIS2DH12_CTRL_REG1, &tmp));
    tmp &= ~LIS2DH12_Y_EN_MASK;
    tmp |= ((uint8_t)status) << LIS2DH12_Y_EN_BIT;
    ERR_ASSERT(TAG, i2c_bus_write_byte(sens->i2c_dev, LIS2DH12_CTRL_REG1, tmp));
    return ESP_OK;
}

esp_err_t lis2dh12_set_x_enable(lis2dh12_handle_t sensor, lis2dh12_state_t status)
{
    lis2dh12_dev_t *sens = (lis2dh12_dev_t *) sensor;
    uint8_t tmp;
    ERR_ASSERT(TAG, i2c_bus_read_byte(sens->i2c_dev, LIS2DH12_CTRL_REG1, &tmp));
    tmp &= ~LIS2DH12_X_EN_MASK;
    tmp |= ((uint8_t)status) << LIS2DH12_X_EN_BIT;
    ERR_ASSERT(TAG, i2c_bus_write_byte(sens->i2c_dev, LIS2DH12_CTRL_REG1, tmp));
    return ESP_OK;
}

esp_err_t lis2dh12_set_bdumode(lis2dh12_handle_t sensor, lis2dh12_state_t status)
{
    lis2dh12_dev_t *sens = (lis2dh12_dev_t *) sensor;
    uint8_t tmp;
    ERR_ASSERT(TAG, i2c_bus_read_byte(sens->i2c_dev, LIS2DH12_CTRL_REG4, &tmp));
    tmp &= ~LIS2DH12_BDU_MASK;
    tmp |= ((uint8_t)status) << LIS2DH12_BDU_BIT;
    ERR_ASSERT(TAG, i2c_bus_write_byte(sens->i2c_dev, LIS2DH12_CTRL_REG4, tmp));
    return ESP_OK;
}

esp_err_t lis2dh12_set_fs(lis2dh12_handle_t sensor, lis2dh12_fs_t fs)
{
    lis2dh12_dev_t *sens = (lis2dh12_dev_t *) sensor;
    uint8_t tmp;
    ERR_ASSERT(TAG, i2c_bus_read_byte(sens->i2c_dev, LIS2DH12_CTRL_REG4, &tmp));
    tmp &= ~LIS2DH12_FS_MASK;
    tmp |= ((uint8_t)fs) << LIS2DH12_FS_BIT;
    ERR_ASSERT(TAG, i2c_bus_write_byte(sens->i2c_dev, LIS2DH12_CTRL_REG4, tmp));
    return ESP_OK;
}

esp_err_t lis2dh12_get_fs(lis2dh12_handle_t sensor, lis2dh12_fs_t *fs)
{
    lis2dh12_dev_t *sens = (lis2dh12_dev_t *) sensor;
    uint8_t tmp;
    ERR_ASSERT(TAG, i2c_bus_read_byte(sens->i2c_dev, LIS2DH12_CTRL_REG4, &tmp));
    *fs = (lis2dh12_fs_t)((tmp & LIS2DH12_FS_MASK) >> LIS2DH12_FS_BIT);
    return ESP_OK;
}

esp_err_t lis2dh12_set_opt_mode(lis2dh12_handle_t sensor, lis2dh12_opt_mode_t opt_mode)
{
    lis2dh12_dev_t *sens = (lis2dh12_dev_t *) sensor;
    uint8_t tmp1, tmp2;
    ERR_ASSERT(TAG, i2c_bus_read_byte(sens->i2c_dev, LIS2DH12_CTRL_REG1, &tmp1));
    ERR_ASSERT(TAG, i2c_bus_read_byte(sens->i2c_dev, LIS2DH12_CTRL_REG4, &tmp2));
    tmp1 &= ~LIS2DH12_LP_EN_MASK;
    tmp1 |= ((uint8_t)((opt_mode >> 1) << LIS2DH12_LP_EN_BIT)&LIS2DH12_LP_EN_MASK);
    tmp2 &= ~LIS2DH12_HR_MASK;
    tmp2 |= ((uint8_t)opt_mode & LIS2DH12_HR_MASK) << LIS2DH12_HR_BIT;
    ERR_ASSERT(TAG, i2c_bus_write_byte(sens->i2c_dev, LIS2DH12_CTRL_REG1, tmp1));
    ERR_ASSERT(TAG, i2c_bus_write_byte(sens->i2c_dev, LIS2DH12_CTRL_REG4, tmp2));
    return ESP_OK;
}

esp_err_t lis2dh12_get_x_acc(lis2dh12_handle_t sensor, uint16_t *x_acc)
{
    lis2dh12_dev_t *sens = (lis2dh12_dev_t *) sensor;
    uint8_t buffer[2];
    ERR_ASSERT(TAG, i2c_bus_read_bytes(sens->i2c_dev, LIS2DH12_OUT_X_L_REG | LIS2DH12_I2C_MULTI_REG_ONCE, 2, buffer));
    *x_acc = (int16_t)((((uint16_t)buffer[1]) << 8) | (uint16_t)buffer[0]);
    return ESP_OK;
}

esp_err_t lis2dh12_get_y_acc(lis2dh12_handle_t sensor, uint16_t *y_acc)
{
    lis2dh12_dev_t *sens = (lis2dh12_dev_t *) sensor;
    uint8_t buffer[2];
    ERR_ASSERT(TAG, i2c_bus_read_bytes(sens->i2c_dev, LIS2DH12_OUT_Y_L_REG | LIS2DH12_I2C_MULTI_REG_ONCE, 2, buffer));
    *y_acc = (int16_t)((((uint16_t)buffer[1]) << 8) | (uint16_t)buffer[0]);
    return ESP_OK;
}

esp_err_t lis2dh12_get_z_acc(lis2dh12_handle_t sensor, uint16_t *z_acc)
{
    lis2dh12_dev_t *sens = (lis2dh12_dev_t *) sensor;
    uint8_t buffer[2];
    ERR_ASSERT(TAG, i2c_bus_read_bytes(sens->i2c_dev, LIS2DH12_OUT_Z_L_REG | LIS2DH12_I2C_MULTI_REG_ONCE, 2, buffer));
    *z_acc = (int16_t)((((uint16_t)buffer[1]) << 8) | (uint16_t)buffer[0]);
    return ESP_OK;
}

esp_err_t lis2dh12_get_raw_acce(lis2dh12_handle_t sensor, lis2dh12_raw_acce_value_t *raw_acce_value)
{
    lis2dh12_dev_t *sens = (lis2dh12_dev_t *) sensor;
    uint8_t buffer[6];
    ERR_ASSERT(TAG, i2c_bus_read_bytes(sens->i2c_dev, LIS2DH12_OUT_X_L_REG | LIS2DH12_I2C_MULTI_REG_ONCE, 6, buffer));
    raw_acce_value->raw_acce_x = *(int16_t *)buffer;
    raw_acce_value->raw_acce_y = *(int16_t *)(buffer + 2);
    raw_acce_value->raw_acce_z = *(int16_t *)(buffer + 4);
    return ESP_OK;
}

esp_err_t lis2dh12_get_acce(lis2dh12_handle_t sensor, lis2dh12_acce_value_t *acce_value)
{
    lis2dh12_fs_t fs;
    lis2dh12_raw_acce_value_t raw_acce_value;
    esp_err_t ret = lis2dh12_get_fs(sensor, &fs);

    if (ret != ESP_OK) {
        return ret;
    }

    ret = lis2dh12_get_raw_acce(sensor, &raw_acce_value);

    if (fs == LIS2DH12_FS_2G) {
        acce_value->acce_x = LIS2DH12_FROM_FS_2g_HR_TO_mg(raw_acce_value.raw_acce_x) / 1000.0;
        acce_value->acce_y = LIS2DH12_FROM_FS_2g_HR_TO_mg(raw_acce_value.raw_acce_y) / 1000.0;
        acce_value->acce_z = LIS2DH12_FROM_FS_2g_HR_TO_mg(raw_acce_value.raw_acce_z) / 1000.0;
    } else if (fs == LIS2DH12_FS_4G) {
        acce_value->acce_x = LIS2DH12_FROM_FS_4g_HR_TO_mg(raw_acce_value.raw_acce_x) / 1000.0;
        acce_value->acce_y = LIS2DH12_FROM_FS_4g_HR_TO_mg(raw_acce_value.raw_acce_y) / 1000.0;
        acce_value->acce_z = LIS2DH12_FROM_FS_4g_HR_TO_mg(raw_acce_value.raw_acce_z) / 1000.0;
    } else if (fs == LIS2DH12_FS_8G) {
        acce_value->acce_x = LIS2DH12_FROM_FS_8g_HR_TO_mg(raw_acce_value.raw_acce_x) / 1000.0;
        acce_value->acce_y = LIS2DH12_FROM_FS_8g_HR_TO_mg(raw_acce_value.raw_acce_y) / 1000.0;
        acce_value->acce_z = LIS2DH12_FROM_FS_8g_HR_TO_mg(raw_acce_value.raw_acce_z) / 1000.0;
    } else if (fs == LIS2DH12_FS_16G) {
        acce_value->acce_x = LIS2DH12_FROM_FS_16g_HR_TO_mg(raw_acce_value.raw_acce_x) / 1000.0;
        acce_value->acce_y = LIS2DH12_FROM_FS_16g_HR_TO_mg(raw_acce_value.raw_acce_y) / 1000.0;
        acce_value->acce_z = LIS2DH12_FROM_FS_16g_HR_TO_mg(raw_acce_value.raw_acce_z) / 1000.0;
    }

    return ret;
}

/***sensors hal interface****/
#ifdef CONFIG_SENSOR_IMU_INCLUDED_LIS2DH12

static lis2dh12_handle_t lis2dh12 = NULL;
static bool is_init = false;

esp_err_t imu_lis2dh12_init(i2c_bus_handle_t i2c_bus)
{
    if (is_init || !i2c_bus) {
        return ESP_FAIL;
    }

    lis2dh12 = lis2dh12_create(i2c_bus, LIS2DH12_I2C_ADDRESS);

    if (!lis2dh12) {
        return ESP_FAIL;
    }

    uint8_t lis2dh12_deviceid;
    lis2dh12_config_t  lis2dh12_config;
    esp_err_t ret = lis2dh12_get_deviceid(lis2dh12, &lis2dh12_deviceid);
    ESP_LOGI(TAG, "lis2dh12 device address is: 0x%02x\n", lis2dh12_deviceid);
    lis2dh12_config.temp_enable = LIS2DH12_TEMP_DISABLE;
    lis2dh12_config.odr = LIS2DH12_ODR_100HZ;
    lis2dh12_config.opt_mode = LIS2DH12_OPT_NORMAL;
    lis2dh12_config.z_enable = LIS2DH12_ENABLE;
    lis2dh12_config.y_enable = LIS2DH12_ENABLE;
    lis2dh12_config.x_enable = LIS2DH12_ENABLE;
    lis2dh12_config.bdu_status = LIS2DH12_DISABLE;
    lis2dh12_config.fs = LIS2DH12_FS_4G;
    ret = lis2dh12_set_config(lis2dh12, &lis2dh12_config);

    if (ret == ESP_OK) {
        is_init = true;
    }

    return ret;
}

esp_err_t imu_lis2dh12_deinit(void)
{
    if (!is_init) {
        return ESP_OK;
    }

    esp_err_t ret = lis2dh12_delete(&lis2dh12);

    if (ret == ESP_OK) {
        is_init = false;
    }

    return ret;
}

esp_err_t imu_lis2dh12_test(void)
{
    if (!is_init) {
        return ESP_FAIL;
    }

    return ESP_OK;
}

esp_err_t imu_lis2dh12_acquire_acce(float *acce_x, float *acce_y, float *acce_z)
{
    if (!is_init || !acce_x || !acce_y || !acce_z) {
        return ESP_FAIL;
    }

    lis2dh12_acce_value_t acce = {0, 0, 0};

    if (ESP_OK == lis2dh12_get_acce(lis2dh12, &acce)) {
        *acce_x = acce.acce_x;
        *acce_y = acce.acce_y;
        *acce_z = acce.acce_z;
        return ESP_OK;
    }

    *acce_x = 0;
    *acce_y = 0;
    *acce_z = 0;
    return ESP_FAIL;
}

#endif