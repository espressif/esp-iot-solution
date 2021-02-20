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
#include "i2c_bus.h"
#include "esp_log.h"
#include "hdc2010.h"

#define WRITE_BIT                       I2C_MASTER_WRITE  /*!< I2C master write */
#define READ_BIT                        I2C_MASTER_READ   /*!< I2C master read */
#define ACK_CHECK_EN                    0x1               /*!< I2C master will check ack from slave*/
#define ACK_CHECK_DIS                   0x0               /*!< I2C master will not check ack from slave */
#define ACK_VAL                         0x0               /*!< I2C ack value */
#define NACK_VAL                        0x1               /*!< I2C nack value */

typedef struct {
    i2c_bus_device_handle_t i2c_dev;
    uint8_t dev_addr;
} hdc2010_dev_t;

hdc2010_handle_t hdc2010_create(i2c_bus_handle_t bus, uint8_t dev_addr)
{
    hdc2010_dev_t *sens = (hdc2010_dev_t *) calloc(1, sizeof(hdc2010_dev_t));
    sens->i2c_dev = i2c_bus_device_create(bus, dev_addr, i2c_bus_get_current_clk_speed(bus));
    if (sens->i2c_dev == NULL) {
        free(sens);
        return NULL;
    }
    sens->dev_addr = dev_addr;
    return (hdc2010_handle_t) sens;
}

esp_err_t hdc2010_delete(hdc2010_handle_t *sensor)
{
    if (*sensor == NULL) {
        return ESP_OK;
    }

    hdc2010_dev_t *sens = (hdc2010_dev_t *)(*sensor);
    i2c_bus_device_delete(&sens->i2c_dev);
    free(sens);
    *sensor = NULL;
    return ESP_OK;
}

static bool _not_valid_return_val(int val)
{
    return (val == HDC2010_ERR_VAL);
}

float hdc2010_get_temperature(hdc2010_handle_t sensor)
{
    hdc2010_dev_t *sens = (hdc2010_dev_t *) sensor;
    uint8_t temperature_high = 0;
    uint8_t temperature_low = 0;
    uint16_t temperature = 0;
    i2c_bus_read_byte(sens->i2c_dev, HDC2010_TEMP_HIGH, &temperature_high);
    i2c_bus_read_byte(sens->i2c_dev, HDC2010_TEMP_LOW, &temperature_low);

    if (_not_valid_return_val(temperature_high) || _not_valid_return_val(temperature_low)) {
        return (float)HDC2010_ERR_VAL;
    }

    temperature = temperature_high << 8 | temperature_low;
    float real_temp = ((float)temperature / (1 << 16) * 165 - 40);
    return real_temp;
}

float hdc2010_get_humidity(hdc2010_handle_t sensor)
{
    hdc2010_dev_t *sens = (hdc2010_dev_t *) sensor;
    uint8_t humidity_high = 0;
    uint8_t humidity_low = 0;
    uint16_t humidity = 0;
    float real_humi = 0;
    i2c_bus_read_byte(sens->i2c_dev, HDC2010_HUM_HIGH, &humidity_high);
    i2c_bus_read_byte(sens->i2c_dev, HDC2010_HUM_LOW, &humidity_low);

    if (_not_valid_return_val(humidity_high) || _not_valid_return_val(humidity_low)) {
        return (float)HDC2010_ERR_VAL;
    }

    humidity = humidity_high << 8 | humidity_low;
    real_humi = (float)(humidity * 100) / (1 << 16);
    return real_humi;
}

esp_err_t hdc2010_get_interrupt_info(hdc2010_handle_t sensor, hdc2010_interrupt_info_t *info)
{
    hdc2010_dev_t *sens = (hdc2010_dev_t *) sensor;
    uint8_t config_data = 0;
    i2c_bus_read_byte(sens->i2c_dev, HDC2010_INTERRUPT, &config_data);

    if (_not_valid_return_val(config_data)) {
        return ESP_FAIL;
    }

    info->drdy_status = (config_data & 0x80) && 1;
    info->hh_status = (config_data & 0x40) && 1;
    info->hl_status = (config_data & 0x20) && 1;
    info->th_status = (config_data & 0x10) && 1;
    info->tl_status = (config_data & 0x08) && 1;
    return ESP_OK;
}

esp_err_t hdc2010_set_interrupt_config(hdc2010_handle_t sensor, hdc2010_interrupt_config_t *config)
{
    hdc2010_dev_t *sens = (hdc2010_dev_t *) sensor;
    uint8_t config_data = 0;
    config_data |= config->drdy_mask << 7;
    config_data |= config->th_mask << 6;
    config_data |= config->tl_mask << 5;
    config_data |= config->hh_mask << 4;
    config_data |= config->hh_mask << 3;
    return i2c_bus_write_byte(sens->i2c_dev, HDC2010_INT_MASK, config_data);
}

float hdc2010_get_max_temperature(hdc2010_handle_t sensor)
{
    hdc2010_dev_t *sens = (hdc2010_dev_t *) sensor;
    uint8_t temperature_data = 0;
    float real_max_temp = 0;
    i2c_bus_read_byte(sens->i2c_dev, HDC2010_TEMPE_MAX, &temperature_data);

    if (_not_valid_return_val(temperature_data)) {
        return (float)HDC2010_ERR_VAL;
    }

    real_max_temp = (((float) temperature_data / (1 << 8) * 165) - 40);
    return real_max_temp;
}

float hdc2010_get_max_humidity(hdc2010_handle_t sensor)
{
    hdc2010_dev_t *sens = (hdc2010_dev_t *) sensor;
    uint8_t humidity_data = 0;
    float real_max_humi = 0;
    i2c_bus_read_byte(sens->i2c_dev, HDC2010_HUM_MAX, &humidity_data);

    if (_not_valid_return_val(humidity_data)) {
        return (float)HDC2010_ERR_VAL;
    }

    real_max_humi = ((float)(humidity_data * 100) / (1 << 8));
    return real_max_humi;
}

esp_err_t hdc2010_set_temperature_Offset(hdc2010_handle_t sensor, uint8_t Offset_data)
{
    hdc2010_dev_t *sens = (hdc2010_dev_t *) sensor;
    return i2c_bus_write_byte(sens->i2c_dev, HDC2010_TEMP_OFFSET, Offset_data);
}

esp_err_t hdc2010_set_humidity_Offset(hdc2010_handle_t sensor, uint8_t Offset_data)
{
    hdc2010_dev_t *sens = (hdc2010_dev_t *) sensor;
    return i2c_bus_write_byte(sens->i2c_dev, HDC2010_HUM_OFFSET, Offset_data);
}

esp_err_t hdc2010_set_temperature_threshold(hdc2010_handle_t sensor, float temperature_data)
{
    hdc2010_dev_t *sens = (hdc2010_dev_t *) sensor;
    esp_err_t ret;
    uint16_t temperature;
    temperature = (uint16_t)(temperature_data + 40) / 165 * (1 << 16);
    ret = i2c_bus_write_byte(sens->i2c_dev, HDC2010_TEMP_THR_H, (temperature >> 8) & 0xFF);

    if (ret != ESP_OK) {
        return ESP_FAIL;
    }

    ret = i2c_bus_write_byte(sens->i2c_dev, HDC2010_TEMP_THR_L, temperature & 0xFF);

    if (ret != ESP_OK) {
        return ESP_FAIL;
    }

    return ESP_OK;
}

esp_err_t hdc2010_set_humidity_threshold(hdc2010_handle_t sensor, float humidity_data)
{
    hdc2010_dev_t *sens = (hdc2010_dev_t *) sensor;
    esp_err_t ret;
    uint16_t humidity;
    humidity = (uint16_t) humidity_data / 100 * (1 << 16);
    ret = i2c_bus_write_byte(sens->i2c_dev, HDC2010_HUM_THR_H, (humidity >> 8) & 0xFF);

    if (ret != ESP_OK) {
        return ESP_FAIL;
    }

    ret = i2c_bus_write_byte(sens->i2c_dev, HDC2010_HUM_THR_L, humidity & 0xFF);

    if (ret != ESP_OK) {
        return ESP_FAIL;
    }

    return ESP_OK;
}

esp_err_t hdc2010_set_reset_and_drdy(hdc2010_handle_t sensor, hdc2010_reset_and_drdy_t *config)
{
    hdc2010_dev_t *sens = (hdc2010_dev_t *) sensor;
    uint8_t config_data = 0;
    config_data |= config->heat_en << 7;
    config_data |= config->output_rate << 4;
    config_data |= config->heat_en << 3;
    config_data |= config->int_en << 2;
    config_data |= config->int_pol << 1;
    config_data |= config->int_mode << 0;
    return i2c_bus_write_byte(sens->i2c_dev, HDC2010_RESET_INT_CONF, config_data);
}

esp_err_t hdc2010_set_measurement_config(hdc2010_handle_t sensor, hdc2010_measurement_config_t *config)
{
    hdc2010_dev_t *sens = (hdc2010_dev_t *) sensor;
    uint8_t config_data = 0;
    config_data |= config->tres << 6;
    config_data |= config->hres << 4;
    config_data |= config->meas_conf << 1;
    config_data |= config->meas_trig << 0;
    return i2c_bus_write_byte(sens->i2c_dev, HDC2010_MEASURE_CONF, config_data);
}

int hdc2010_get_manufacturer_id(hdc2010_handle_t sensor)
{
    hdc2010_dev_t *sens = (hdc2010_dev_t *) sensor;
    uint8_t manufacturer_id_high = 0;
    uint8_t manufacturer_id_low = 0;
    int manufacturer = 0;
    i2c_bus_read_byte(sens->i2c_dev, HDC2010_MANUFACTURER_ID_H, &manufacturer_id_high);
    i2c_bus_read_byte(sens->i2c_dev, HDC2010_MANUFACTURER_ID_L, &manufacturer_id_low);

    if (_not_valid_return_val(manufacturer_id_high) || _not_valid_return_val(manufacturer_id_low)) {
        return HDC2010_ERR_VAL;
    }

    manufacturer = (manufacturer_id_high << 8) + manufacturer_id_low;
    return manufacturer;
}

int hdc2010_get_device_id(hdc2010_handle_t sensor)
{
    hdc2010_dev_t *sens = (hdc2010_dev_t *) sensor;
    uint8_t device_id_high = 0;
    uint8_t device_id_low = 0;
    int device_id = 0;
    i2c_bus_read_byte(sens->i2c_dev, HDC2010_DEVICE_ID_H, &device_id_high);
    i2c_bus_read_byte(sens->i2c_dev, HDC2010_DEVICE_ID_L, &device_id_low);

    if (_not_valid_return_val(device_id_high) || _not_valid_return_val(device_id_low)) {
        return HDC2010_ERR_VAL;
    }

    device_id = (device_id_high << 8) + device_id_low;
    return device_id;
}

esp_err_t hdc2010_default_init(hdc2010_handle_t sensor)
{
    esp_err_t ret;
    hdc2010_reset_and_drdy_t reset_config;
    hdc2010_measurement_config_t measurement_config;
    reset_config.soft_res = HDC2010_SOFT_RESET;
    reset_config.output_rate = HDC2010_ODR_100;
    reset_config.heat_en = HDC2010_HEATER_OFF;
    reset_config.int_en = HDC2010_DRDY_INT_EN_HIGH_Z;
    reset_config.int_pol = HDC2010_INT_POL_ACTIVE_LOW;
    reset_config.int_mode = HDC2010_INT_LEVEL_SENSITIVE;
    measurement_config.tres = HDC2010_TRES_BIT_14;
    measurement_config.hres = HDC2010_HRES_BIT_14;
    measurement_config.meas_conf = HDC2010_MEAS_CONF_HUM_AND_TEMP;
    measurement_config.meas_trig = HDC2010_MEAS_START;
    ret = hdc2010_set_reset_and_drdy(sensor, &reset_config);

    if (ret != ESP_OK) {
        return ESP_FAIL;
    }

    ret = hdc2010_set_measurement_config(sensor, &measurement_config);

    if (ret != ESP_OK) {
        return ESP_FAIL;
    }

    return ESP_OK;
}
