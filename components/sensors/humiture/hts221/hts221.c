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
#include "hts221.h"
#include "esp_log.h"

#define WRITE_BIT      I2C_MASTER_WRITE  /*!< I2C master write */
#define READ_BIT       I2C_MASTER_READ   /*!< I2C master read */
#define ACK_CHECK_EN   0x1               /*!< I2C master will check ack from slave*/
#define ACK_CHECK_DIS  0x0               /*!< I2C master will not check ack from slave */
#define ACK_VAL        0x0               /*!< I2C ack value */
#define NACK_VAL       0x1               /*!< I2C nack value */
#define TAG "HTS221"

typedef struct {
    i2c_bus_device_handle_t i2c_dev;
    uint8_t dev_addr;
} hts221_dev_t;

hts221_handle_t hts221_create(i2c_bus_handle_t bus, uint8_t dev_addr)
{
    hts221_dev_t *sens = (hts221_dev_t *) calloc(1, sizeof(hts221_dev_t));
    sens->i2c_dev = i2c_bus_device_create(bus, dev_addr, i2c_bus_get_current_clk_speed(bus));
    if (sens->i2c_dev == NULL) {
        free(sens);
        return NULL;
    }
    sens->dev_addr = dev_addr;
    return (hts221_handle_t) sens;
}

esp_err_t hts221_delete(hts221_handle_t *sensor)
{
    if (*sensor == NULL) {
        return ESP_OK;
    }
    hts221_dev_t *sens = (hts221_dev_t *)(*sensor);
    i2c_bus_device_delete(&sens->i2c_dev);
    free(sens);
    *sensor = NULL;
    return ESP_OK;
}

/**
 * @brief Call i2c_bus_read_byte in loop with register/memory address from [mem_start_address] to [mem_start_address + data_len].
 *
 * @param device I2C device handle
 * @param mem_start_address The start 8-bit internal reg/mem address to read from, later address will plus one automatically.
 * @param data_len Number of bytes to read
 * @param data Pointer to a buffer to save the data that was read .
 * @return
 *     - ESP_OK Success
 *     - ESP_FAIL Fail
 */
static esp_err_t i2c_bus_read_byte_loop(i2c_bus_device_handle_t device, uint8_t mem_start_address, size_t data_len, uint8_t *data)
{
    uint8_t byte = 0;
    if (data != NULL) {
        for (int i = 0; i < data_len; i++) {
            i2c_bus_read_byte(device, mem_start_address + i, &byte);
            data[i] = byte;
        }
        return ESP_OK;
    }
    return ESP_FAIL;
}

/**
 * @brief Call i2c_bus_write_byte in loop with register/memory address from [mem_start_address] to [mem_start_address + data_len].
 *
 * @param device I2C device handle
 * @param mem_start_address The start internal reg/mem address to write to, later address will plus one automatically.
 * @param data_len Number of bytes to write
 * @param data Pointer to the bytes to write.
 * @return
 *     - ESP_OK Success
 *     - ESP_FAIL Fail
 */
static esp_err_t i2c_bus_write_byte_loop(i2c_bus_device_handle_t device, uint8_t mem_start_address, size_t data_len, uint8_t *data)
{
    if (data != NULL) {
        for (int i = 0; i < data_len; i++) {
            i2c_bus_write_byte(device, mem_start_address + i, data[i]);
        }
        return ESP_OK;
    }
    return ESP_FAIL;
}

esp_err_t hts221_get_deviceid(hts221_handle_t sensor, uint8_t *deviceid)
{
    hts221_dev_t *sens = (hts221_dev_t *) sensor;
    return i2c_bus_read_byte(sens->i2c_dev, HTS221_WHO_AM_I_REG, deviceid);
}

esp_err_t hts221_set_config(hts221_handle_t sensor, hts221_config_t *hts221_config)
{
    hts221_dev_t *sens = (hts221_dev_t *) sensor;
    uint8_t buffer[3];
    i2c_bus_read_byte(sens->i2c_dev, HTS221_AV_CONF_REG, buffer);
    buffer[0] &= ~(HTS221_AVGH_MASK | HTS221_AVGT_MASK);
    buffer[0] |= (uint8_t)hts221_config->avg_h;
    buffer[0] |= (uint8_t)hts221_config->avg_t;
    i2c_bus_write_byte(sens->i2c_dev, HTS221_AV_CONF_REG, buffer[0]);
    i2c_bus_read_byte_loop(sens->i2c_dev, HTS221_CTRL_REG1, 3, buffer);
    buffer[0] &= ~(HTS221_BDU_MASK | HTS221_ODR_MASK);
    buffer[0] |= (uint8_t)hts221_config->odr;
    buffer[0] |= ((uint8_t)hts221_config->bdu_status) << HTS221_BDU_BIT;
    buffer[1] &= ~HTS221_HEATER_BIT;
    buffer[1] |= ((uint8_t)hts221_config->heater_status) << HTS221_HEATER_BIT;
    buffer[2] &= ~(HTS221_DRDY_H_L_MASK | HTS221_PP_OD_MASK | HTS221_DRDY_MASK);
    buffer[2] |= ((uint8_t)hts221_config->irq_level) << HTS221_DRDY_H_L_BIT;
    buffer[2] |= (uint8_t)hts221_config->irq_output_type;
    buffer[2] |= ((uint8_t)hts221_config->irq_enable) << HTS221_DRDY_BIT;
    i2c_bus_write_byte_loop(sens->i2c_dev, HTS221_CTRL_REG1, 3, buffer);
    return ESP_OK;
}

esp_err_t hts221_get_config(hts221_handle_t sensor, hts221_config_t *hts221_config)
{
    hts221_dev_t *sens = (hts221_dev_t *) sensor;
    uint8_t buffer[3];
    i2c_bus_read_byte(sens->i2c_dev, HTS221_AV_CONF_REG, buffer);
    hts221_config->avg_h = (hts221_avgh_t)(buffer[0] & HTS221_AVGH_MASK);
    hts221_config->avg_t = (hts221_avgt_t)(buffer[0] & HTS221_AVGT_MASK);
    i2c_bus_read_byte_loop(sens->i2c_dev, HTS221_CTRL_REG1, 3, buffer);
    hts221_config->odr = (hts221_odr_t)(buffer[0] & HTS221_ODR_MASK);
    hts221_config->bdu_status = (hts221_state_t)((buffer[0] & HTS221_BDU_MASK) >> HTS221_BDU_BIT);
    hts221_config->heater_status = (hts221_state_t)((buffer[1] & HTS221_HEATER_MASK) >> HTS221_HEATER_BIT);
    hts221_config->irq_level = (hts221_drdylevel_t)(buffer[2] & HTS221_DRDY_H_L_MASK);
    hts221_config->irq_output_type = (hts221_outputtype_t)(buffer[2] & HTS221_PP_OD_MASK);
    hts221_config->irq_enable = (hts221_state_t)((buffer[2] & HTS221_DRDY_MASK) >> HTS221_DRDY_BIT);
    return ESP_OK;
}

esp_err_t hts221_set_activate(hts221_handle_t sensor)
{
    hts221_dev_t *sens = (hts221_dev_t *) sensor;
    esp_err_t ret;
    uint8_t tmp;
    ret = i2c_bus_read_byte(sens->i2c_dev, HTS221_CTRL_REG1, &tmp);

    if (ret != ESP_OK) {
        return ret;
    }

    tmp |= HTS221_PD_MASK;
    ret = i2c_bus_write_byte(sens->i2c_dev, HTS221_CTRL_REG1, tmp);
    return ret;
}

esp_err_t hts221_set_powerdown(hts221_handle_t sensor)
{
    hts221_dev_t *sens = (hts221_dev_t *) sensor;
    esp_err_t ret;
    uint8_t tmp;
    ret = i2c_bus_read_byte(sens->i2c_dev, HTS221_CTRL_REG1, &tmp);

    if (ret != ESP_OK) {
        return ret;
    }

    tmp &= ~HTS221_PD_MASK;
    ret = i2c_bus_write_byte(sens->i2c_dev, HTS221_CTRL_REG1, tmp);
    return ret;
}

esp_err_t hts221_set_odr(hts221_handle_t sensor, hts221_odr_t odr)
{
    hts221_dev_t *sens = (hts221_dev_t *) sensor;
    esp_err_t ret;
    uint8_t tmp;
    ret = i2c_bus_read_byte(sens->i2c_dev, HTS221_CTRL_REG1, &tmp);

    if (ret != ESP_OK) {
        return ret;
    }

    tmp &= ~HTS221_ODR_MASK;
    tmp |= (uint8_t)odr;
    ret = i2c_bus_write_byte(sens->i2c_dev, HTS221_CTRL_REG1, tmp);
    return ret;
}

esp_err_t hts221_set_avgh(hts221_handle_t sensor, hts221_avgh_t avgh)
{
    hts221_dev_t *sens = (hts221_dev_t *) sensor;
    esp_err_t ret;
    uint8_t tmp;
    ret = i2c_bus_read_byte(sens->i2c_dev, HTS221_AV_CONF_REG, &tmp);

    if (ret != ESP_OK) {
        return ret;
    }

    tmp &= ~HTS221_AVGH_MASK;
    tmp |= (uint8_t)avgh;
    ret = i2c_bus_write_byte(sens->i2c_dev, HTS221_AV_CONF_REG, tmp);
    return ret;
}

esp_err_t hts221_set_avgt(hts221_handle_t sensor, hts221_avgt_t avgt)
{
    hts221_dev_t *sens = (hts221_dev_t *) sensor;
    esp_err_t ret;
    uint8_t tmp;
    ret = i2c_bus_read_byte(sens->i2c_dev, HTS221_AV_CONF_REG, &tmp);

    if (ret != ESP_OK) {
        return ret;
    }

    tmp &= ~HTS221_AVGT_MASK;
    tmp |= (uint8_t)avgt;
    ret = i2c_bus_write_byte(sens->i2c_dev, HTS221_AV_CONF_REG, tmp);
    return ret;
}

esp_err_t hts221_set_bdumode(hts221_handle_t sensor, hts221_state_t status)
{
    hts221_dev_t *sens = (hts221_dev_t *) sensor;
    esp_err_t ret;
    uint8_t tmp;
    ret = i2c_bus_read_byte(sens->i2c_dev, HTS221_CTRL_REG1, &tmp);

    if (ret != ESP_OK) {
        return ret;
    }

    tmp &= ~HTS221_BDU_MASK;
    tmp |= ((uint8_t)status) << HTS221_BDU_BIT;
    ret = i2c_bus_write_byte(sens->i2c_dev, HTS221_CTRL_REG1, tmp);
    return ret;
}

esp_err_t hts221_memory_boot(hts221_handle_t sensor)
{
    hts221_dev_t *sens = (hts221_dev_t *) sensor;
    esp_err_t ret;
    uint8_t tmp;
    ret = i2c_bus_read_byte(sens->i2c_dev, HTS221_CTRL_REG2, &tmp);

    if (ret != ESP_OK) {
        return ret;
    }

    tmp |= HTS221_BOOT_MASK;
    ret = i2c_bus_write_byte(sens->i2c_dev, HTS221_CTRL_REG2, tmp);
    return ret;
}

esp_err_t hts221_set_heaterstate(hts221_handle_t sensor, hts221_state_t status)
{
    hts221_dev_t *sens = (hts221_dev_t *) sensor;
    esp_err_t ret;
    uint8_t tmp;
    ret = i2c_bus_read_byte(sens->i2c_dev, HTS221_CTRL_REG2, &tmp);

    if (ret != ESP_OK) {
        return ret;
    }

    tmp &= ~HTS221_HEATER_MASK;
    tmp |= ((uint8_t)status) << HTS221_HEATER_BIT;
    ret = i2c_bus_write_byte(sens->i2c_dev, HTS221_CTRL_REG2, tmp);
    return ret;
}

esp_err_t hts221_start_oneshot(hts221_handle_t sensor)
{
    hts221_dev_t *sens = (hts221_dev_t *) sensor;
    esp_err_t ret;
    uint8_t tmp;
    ret = i2c_bus_read_byte(sens->i2c_dev, HTS221_CTRL_REG2, &tmp);

    if (ret != ESP_OK) {
        return ret;
    }

    tmp |= HTS221_ONE_SHOT_MASK;
    ret = i2c_bus_write_byte(sens->i2c_dev, HTS221_CTRL_REG2, tmp);
    return ret;
}

esp_err_t hts221_set_irq_activelevel(hts221_handle_t sensor, hts221_drdylevel_t value)
{
    hts221_dev_t *sens = (hts221_dev_t *) sensor;
    esp_err_t ret;
    uint8_t tmp;
    ret = i2c_bus_read_byte(sens->i2c_dev, HTS221_CTRL_REG3, &tmp);

    if (ret != ESP_OK) {
        return ret;
    }

    tmp &= ~HTS221_DRDY_H_L_MASK;
    tmp |= (uint8_t)value;
    ret = i2c_bus_write_byte(sens->i2c_dev, HTS221_CTRL_REG3, tmp);
    return ret;
}

esp_err_t hts221_set_irq_outputtype(hts221_handle_t sensor, hts221_outputtype_t value)
{
    hts221_dev_t *sens = (hts221_dev_t *) sensor;
    esp_err_t ret;
    uint8_t tmp;
    ret = i2c_bus_read_byte(sens->i2c_dev, HTS221_CTRL_REG3, &tmp);

    if (ret != ESP_OK) {
        return ret;
    }

    tmp &= ~HTS221_PP_OD_MASK;
    tmp |= (uint8_t)value;
    ret = i2c_bus_write_byte(sens->i2c_dev, HTS221_CTRL_REG3, tmp);
    return ret;
}

esp_err_t hts221_set_irq_enable(hts221_handle_t sensor, hts221_state_t status)
{
    hts221_dev_t *sens = (hts221_dev_t *) sensor;
    esp_err_t ret;
    uint8_t tmp;
    ret = i2c_bus_read_byte(sens->i2c_dev, HTS221_CTRL_REG3, &tmp);

    if (ret != ESP_OK) {
        return ret;
    }

    tmp &= ~HTS221_DRDY_MASK;
    tmp |= ((uint8_t)status) << HTS221_DRDY_BIT;
    ret = i2c_bus_write_byte(sens->i2c_dev, HTS221_CTRL_REG3, tmp);
    return ret;
}

esp_err_t hts221_get_raw_humidity(hts221_handle_t sensor, int16_t *humidity)
{
    hts221_dev_t *sens = (hts221_dev_t *) sensor;
    esp_err_t ret;
    uint8_t buffer[2];
    ret = i2c_bus_read_byte_loop(sens->i2c_dev, HTS221_HR_OUT_L_REG, 2, buffer);
    *humidity = (int16_t)((((uint16_t)buffer[1]) << 8) | (uint16_t)buffer[0]);
    return ret;
}

esp_err_t hts221_get_humidity(hts221_handle_t sensor, int16_t *humidity)
{
    hts221_dev_t *sens = (hts221_dev_t *) sensor;
    int16_t h0_t0_out, h1_t0_out, h_t_out;
    int16_t h0_rh, h1_rh;
    uint8_t buffer[2];
    int32_t tmp_32;
    i2c_bus_read_byte_loop(sens->i2c_dev, HTS221_H0_RH_X2, 2, buffer);
    h0_rh = buffer[0] >> 1;
    h1_rh = buffer[1] >> 1;
    i2c_bus_read_byte_loop(sens->i2c_dev, HTS221_H0_T0_OUT_L, 2, buffer);
    h0_t0_out = (int16_t)(((uint16_t)buffer[1]) << 8) | (uint16_t)buffer[0];
    i2c_bus_read_byte_loop(sens->i2c_dev, HTS221_H1_T0_OUT_L, 2, buffer);
    h1_t0_out = (int16_t)(((uint16_t)buffer[1]) << 8) | (uint16_t)buffer[0];
    i2c_bus_read_byte_loop(sens->i2c_dev, HTS221_HR_OUT_L_REG, 2, buffer);
    h_t_out = (int16_t)(((uint16_t)buffer[1]) << 8) | (uint16_t)buffer[0];
    tmp_32 = ((int32_t)(h_t_out - h0_t0_out)) * ((int32_t)(h1_rh - h0_rh) * 10);

    if (h1_t0_out - h0_t0_out == 0) {
        return ESP_FAIL;
    }

    *humidity = tmp_32 / (int32_t)(h1_t0_out - h0_t0_out)  + h0_rh * 10;

    if (*humidity > 1000) {
        *humidity = 1000;
    }

    return ESP_OK;
}

esp_err_t hts221_get_raw_temperature(hts221_handle_t sensor, int16_t *temperature)
{
    hts221_dev_t *sens = (hts221_dev_t *) sensor;
    esp_err_t ret;
    uint8_t buffer[2];
    ret = i2c_bus_read_byte_loop(sens->i2c_dev, HTS221_TEMP_OUT_L_REG, 2, buffer);
    *temperature = (int16_t)((((uint16_t)buffer[1]) << 8) | (uint16_t)buffer[0]);
    return ret;
}

esp_err_t hts221_get_temperature(hts221_handle_t sensor, int16_t *temperature)
{
    hts221_dev_t *sens = (hts221_dev_t *) sensor;
    int16_t t0_out, t1_out, t_out, t0_degc_x8_u16, t1_degc_x8_u16;
    int16_t t0_degc, t1_degc;
    uint8_t buffer[4], tmp_8;
    int32_t tmp_32;
    i2c_bus_read_byte_loop(sens->i2c_dev, HTS221_T0_DEGC_X8, 2, buffer);
    i2c_bus_read_byte(sens->i2c_dev, HTS221_T0_T1_DEGC_H2, &tmp_8);
    t0_degc_x8_u16 = (((uint16_t)(tmp_8 & 0x03)) << 8) | ((uint16_t)buffer[0]);
    t1_degc_x8_u16 = (((uint16_t)(tmp_8 & 0x0C)) << 6) | ((uint16_t)buffer[1]);
    t0_degc = t0_degc_x8_u16 >> 3;
    t1_degc = t1_degc_x8_u16 >> 3;
    i2c_bus_read_byte_loop(sens->i2c_dev, HTS221_T0_OUT_L, 4, buffer);
    t0_out = (((uint16_t)buffer[1]) << 8) | (uint16_t)buffer[0];
    t1_out = (((uint16_t)buffer[3]) << 8) | (uint16_t)buffer[2];
    i2c_bus_read_byte_loop(sens->i2c_dev, HTS221_TEMP_OUT_L_REG, 2, buffer);
    t_out = (((uint16_t)buffer[1]) << 8) | (uint16_t)buffer[0];
    tmp_32 = ((int32_t)(t_out - t0_out)) * ((int32_t)(t1_degc - t0_degc) * 10);

    if ((t1_out - t0_out) == 0) {
        return ESP_FAIL;
    }

    *temperature = tmp_32 / (int32_t)(t1_out - t0_out) + t0_degc * 10;
    return ESP_OK;
}

#ifdef CONFIG_SENSOR_HUMITURE_INCLUDED_HTS221

static hts221_handle_t hts221 = NULL;

esp_err_t humiture_hts221_init(i2c_bus_handle_t i2c_bus)
{
    if (hts221 || !i2c_bus) {
        return ESP_FAIL;
    }

    uint8_t hts221_deviceid;
    hts221_config_t hts221_config;
    hts221 = hts221_create(i2c_bus, HTS221_I2C_ADDRESS);
    esp_err_t ret = hts221_get_deviceid(hts221, &hts221_deviceid);
    ESP_LOGI(TAG, "device ID is: %02x\n", hts221_deviceid);
    ret = hts221_get_config(hts221, &hts221_config);
    hts221_config.avg_h = HTS221_AVGH_32;
    hts221_config.avg_t = HTS221_AVGT_16;
    hts221_config.odr = HTS221_ODR_7HZ;
    hts221_config.bdu_status = HTS221_DISABLE;
    hts221_config.heater_status = HTS221_DISABLE;
    ret = hts221_set_config(hts221, &hts221_config);
    ret = hts221_set_activate(hts221);
    return ret;
}

esp_err_t humiture_hts221_deinit(void)
{
    if (!hts221) {
        return ESP_FAIL;
    }

    esp_err_t ret = hts221_set_powerdown(hts221);
    ret = hts221_delete(&hts221);

    if (ret != ESP_OK) {
        return ESP_FAIL;
    }

    return ESP_OK;
}

esp_err_t humiture_hts221_test(void)
{
    if (!hts221) {
        return ESP_FAIL;
    }

    return ESP_OK;
}

esp_err_t humiture_hts221_acquire_humidity(float *h)
{
    if (!hts221) {
        return ESP_FAIL;
    }

    int16_t humidity;
    esp_err_t ret = hts221_get_humidity(hts221, &humidity);
    *h = (float)humidity / 10;
    return ret;
}

esp_err_t humiture_hts221_acquire_temperature(float *t)
{
    if (!hts221) {
        return ESP_FAIL;
    }

    int16_t temperature;
    esp_err_t ret = hts221_get_temperature(hts221, &temperature);
    *t = (float)temperature / 10;
    return ret;
}

esp_err_t humiture_hts221_sleep(void)
{
    if (!hts221) {
        return ESP_FAIL;
    }

    esp_err_t ret = hts221_set_powerdown(hts221);
    return ret;
}

esp_err_t humiture_hts221_wakeup(void)
{
    if (!hts221) {
        return ESP_FAIL;
    }

    esp_err_t ret = hts221_set_activate(hts221);
    return ret;
}

#endif