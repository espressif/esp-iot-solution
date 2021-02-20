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
#include "bh1750.h"

#define WRITE_BIT  I2C_MASTER_WRITE  /*!< I2C master write */
#define READ_BIT   I2C_MASTER_READ   /*!< I2C master read */
#define ACK_CHECK_EN   0x1           /*!< I2C master will check ack from slave*/
#define ACK_CHECK_DIS  0x0           /*!< I2C master will not check ack from slave */
#define ACK_VAL    0x0               /*!< I2C ack value */
#define NACK_VAL   0x1               /*!< I2C nack value */

#define BH_1750_MEASUREMENT_ACCURACY    1.2    /*!< the typical measurement accuracy of  BH1750 sensor */

#define BH1750_POWER_DOWN        0x00    /*!< Command to set Power Down*/
#define BH1750_POWER_ON          0x01    /*!< Command to set Power On*/
#define BH1750_DATA_REG_RESET    0x07    /*!< Command to reset data register, not acceptable in power down mode*/

typedef struct {
    i2c_bus_device_handle_t i2c_dev;
    uint8_t dev_addr;
} bh1750_dev_t;

bh1750_handle_t bh1750_create(i2c_bus_handle_t bus, uint8_t dev_addr)
{
    bh1750_dev_t *sensor = (bh1750_dev_t *) calloc(1, sizeof(bh1750_dev_t));
    sensor->i2c_dev = i2c_bus_device_create(bus, dev_addr, i2c_bus_get_current_clk_speed(bus));
    if (sensor->i2c_dev == NULL) {
        free(sensor);
        return NULL;
    }
    sensor->dev_addr = dev_addr;
    return (bh1750_handle_t) sensor;
}

esp_err_t bh1750_delete(bh1750_handle_t *sensor)
{
    if (*sensor == NULL) {
        return ESP_OK;
    }

    bh1750_dev_t *sens = (bh1750_dev_t *)(*sensor);
    i2c_bus_device_delete(&sens->i2c_dev);
    free(sens);
    *sensor = NULL;
    return ESP_OK;
}

esp_err_t bh1750_power_down(bh1750_handle_t sensor)
{
    bh1750_dev_t *sens = (bh1750_dev_t *) sensor;
    esp_err_t ret = i2c_bus_write_byte(sens->i2c_dev, NULL_I2C_MEM_ADDR, BH1750_POWER_DOWN);
    return ret;
}

esp_err_t bh1750_power_on(bh1750_handle_t sensor)
{
    bh1750_dev_t *sens = (bh1750_dev_t *) sensor;
    esp_err_t ret = i2c_bus_write_byte(sens->i2c_dev, NULL_I2C_MEM_ADDR, BH1750_POWER_ON);
    return ret;
}

esp_err_t bh1750_reset_data_register(bh1750_handle_t sensor)
{
    bh1750_dev_t *sens = (bh1750_dev_t *) sensor;
    bh1750_power_on(sensor);
    esp_err_t ret = i2c_bus_write_byte(sens->i2c_dev, NULL_I2C_MEM_ADDR, BH1750_DATA_REG_RESET);
    return ret;
}

esp_err_t bh1750_change_measure_time(bh1750_handle_t sensor, uint8_t measure_time)
{
    bh1750_dev_t *sens = (bh1750_dev_t *) sensor;
    uint8_t buf[2] = {0x40, 0x60};
    buf[0] |= measure_time >> 5;
    buf[1] |= measure_time & 0x1F;
    esp_err_t ret = i2c_bus_write_bytes(sens->i2c_dev, NULL_I2C_MEM_ADDR, 2, &buf[0]);
    return ret;
}

esp_err_t bh1750_set_measure_mode(bh1750_handle_t sensor, bh1750_cmd_measure_t cmd_measure)
{
    bh1750_dev_t *sens = (bh1750_dev_t *) sensor;
    esp_err_t ret = i2c_bus_write_byte(sens->i2c_dev, NULL_I2C_MEM_ADDR, cmd_measure);
    return ret;
}

esp_err_t bh1750_get_data(bh1750_handle_t sensor, float *data)
{
    bh1750_dev_t *sens = (bh1750_dev_t *) sensor;
    uint8_t bh1750_data[2] = {0};
    esp_err_t ret = i2c_bus_read_bytes(sens->i2c_dev, NULL_I2C_MEM_ADDR, 2, &bh1750_data[0]);
    *data = ((bh1750_data[0] << 8 | bh1750_data[1]) / BH_1750_MEASUREMENT_ACCURACY);
    return ret;
}

esp_err_t bh1750_get_light_intensity(bh1750_handle_t sensor, bh1750_cmd_measure_t cmd_measure, float *data)
{
    esp_err_t ret = bh1750_set_measure_mode(sensor, cmd_measure);

    if (ret != ESP_OK) {
        return ret;
    }

    if ((cmd_measure == BH1750_CONTINUE_4LX_RES) || (cmd_measure == BH1750_ONETIME_4LX_RES)) {
        vTaskDelay(30 / portTICK_RATE_MS);
    } else {
        vTaskDelay(180 / portTICK_RATE_MS);
    }

    ret = bh1750_get_data(sensor, data);
    return ret;
}

#ifdef CONFIG_SENSOR_LIGHT_INCLUDED_BH1750

static bh1750_handle_t bh1750 = NULL;
static bool is_init = false;

esp_err_t light_sensor_bh1750_init(i2c_bus_handle_t i2c_bus)
{
    if (is_init || !i2c_bus) {
        return ESP_FAIL;
    }

    bh1750 = bh1750_create(i2c_bus, BH1750_I2C_ADDRESS_DEFAULT);

    if (!bh1750) {
        return ESP_FAIL;
    }

    is_init = true;
    return ESP_OK;
}

esp_err_t light_sensor_bh1750_deinit(void)
{
    if (!is_init) {
        return ESP_FAIL;
    }

    esp_err_t ret = bh1750_delete(&bh1750);

    if (ret == ESP_OK) {
        is_init = false;
    }

    return ESP_OK;
}

esp_err_t light_sensor_bh1750_test(void)
{
    if (!is_init) {
        return ESP_FAIL;
    }

    return ESP_OK;
}

esp_err_t light_sensor_bh1750_acquire_light(float *l)
{
    if (!is_init) {
        return ESP_FAIL;
    }

    bh1750_cmd_measure_t cmd_measure;
    float bh1750_data;
    esp_err_t ret = bh1750_power_on(bh1750);
    cmd_measure = BH1750_ONETIME_4LX_RES;
    ret = bh1750_set_measure_mode(bh1750, cmd_measure);
    vTaskDelay(30 / portTICK_RATE_MS);/*TODO:check if need*/
    ret = bh1750_get_data(bh1750, &bh1750_data);

    if (ret == ESP_OK) {
        *l = bh1750_data;
        return ESP_OK;
    }

    *l = 0;
    return ESP_FAIL;
}

#endif
