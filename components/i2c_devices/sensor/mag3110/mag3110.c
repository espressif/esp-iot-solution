// Written by Serena Yeoh (Nov 2019)
// for the ESP32 Azure IoT Kit by Espressif Systems (Shanghai) PTE LTD
//
// References:
//  Datasheet
//  https://www.nxp.com/docs/en/data-sheet/MAG3110.pdf
//
//  Sample implementation of drivers for the ESP IoT Solutions
//  https://github.com/espressif/esp-iot-solution/tree/master/components/i2c_devices/sensor
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
#include <time.h>
#include <sys/time.h>
#include "esp_system.h"
#include "driver/i2c.h"
#include "iot_i2c_bus.h"
#include "iot_mag3110.h"
#include "esp_log.h"

typedef struct {
    i2c_bus_handle_t bus;
    uint16_t dev_addr;
    mag3110_operating_modes current_mode;
} mag3110_dev_t;

mag3110_handle_t iot_mag3110_create(i2c_bus_handle_t bus, uint16_t dev_addr)
{
    mag3110_dev_t* sensor = (mag3110_dev_t*) calloc(1, sizeof(mag3110_dev_t));
    sensor->bus = bus;
    sensor->dev_addr = dev_addr;

    return (mag3110_handle_t) sensor;
}

esp_err_t iot_mag3110_delete(mag3110_handle_t sensor, bool del_bus)
{
    mag3110_dev_t* sens = (mag3110_dev_t*) sensor;
    if(del_bus) 
    {
        iot_i2c_bus_delete(sens->bus);
        sens->bus = NULL;
    }
    free(sens);
    return ESP_OK;
}

esp_err_t iot_mag3110_read_byte(mag3110_handle_t sensor, uint8_t reg_addr, uint8_t *data)
{
    esp_err_t ret;
    mag3110_dev_t* sens = (mag3110_dev_t*) sensor;

    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, ( sens->dev_addr << 1 ) | WRITE_BIT, ACK_CHECK_EN);
    i2c_master_write_byte(cmd, reg_addr, ACK_CHECK_EN);

    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (sens->dev_addr << 1) | READ_BIT, ACK_CHECK_EN);
    i2c_master_read_byte(cmd, data, NACK_VAL);
    i2c_master_stop(cmd);
    ret = iot_i2c_bus_cmd_begin(sens->bus, cmd, 1000 / portTICK_RATE_MS);
    i2c_cmd_link_delete(cmd);

    return ret;
}

esp_err_t iot_mag3110_read(mag3110_handle_t sensor, uint8_t reg_addr, uint8_t reg_num, uint8_t* data_buf)
{
    esp_err_t ret;
    mag3110_dev_t* sens = (mag3110_dev_t*) sensor;

    uint8_t max = reg_num - 1; 
    if (data_buf != NULL && reg_num > 0) 
    {
        i2c_cmd_handle_t cmd = i2c_cmd_link_create();

        i2c_master_start(cmd);
        i2c_master_write_byte(cmd, ( sens->dev_addr << 1 ) | WRITE_BIT, ACK_CHECK_EN);
        i2c_master_write_byte(cmd, reg_addr, ACK_CHECK_EN);

        i2c_master_start(cmd);
        i2c_master_write_byte(cmd, (sens->dev_addr << 1) | READ_BIT, ACK_CHECK_EN);
        i2c_master_read(cmd, data_buf, max, ACK_VAL);
        i2c_master_read(cmd, data_buf + max, 1, NACK_VAL);
        i2c_master_stop(cmd);
        ret = iot_i2c_bus_cmd_begin(sens->bus, cmd, 1000 / portTICK_RATE_MS);
        i2c_cmd_link_delete(cmd);
        
        if (ret == ESP_FAIL) return ret;
    }

    return ESP_OK;
}

esp_err_t iot_mag3110_write(mag3110_handle_t sensor, uint8_t reg_addr, uint8_t reg_num, uint8_t* data_buf)
{
    esp_err_t  ret;
    mag3110_dev_t* sens = (mag3110_dev_t*) sensor;

    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (sens->dev_addr << 1) | WRITE_BIT, ACK_CHECK_EN);
    i2c_master_write_byte(cmd, reg_addr, ACK_CHECK_EN);    
    i2c_master_write(cmd, data_buf, reg_num, ACK_CHECK_EN);
    ret = iot_i2c_bus_cmd_begin(sens->bus, cmd, 1000 / portTICK_RATE_MS);
    i2c_cmd_link_delete(cmd);
    
    if (ret == ESP_FAIL) return ret;

    return ESP_OK;
}

esp_err_t iot_mag3110_write_byte(mag3110_handle_t sensor, uint8_t reg_addr, uint8_t data)
{
    esp_err_t  ret;
    mag3110_dev_t* sens = (mag3110_dev_t*) sensor;

    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (sens->dev_addr << 1) | WRITE_BIT, ACK_CHECK_EN);
    i2c_master_write_byte(cmd, reg_addr, ACK_CHECK_EN);
    i2c_master_write_byte(cmd, data, ACK_CHECK_EN);
    ret = iot_i2c_bus_cmd_begin(sens->bus, cmd, 1000 / portTICK_RATE_MS);
    i2c_cmd_link_delete(cmd);
    
    if (ret == ESP_FAIL) return ret;

    return ESP_OK;
}

esp_err_t iot_mag3110_get_chip_id(mag3110_handle_t sensor, uint8_t* chipid)
{
    uint8_t data;
    esp_err_t ret = iot_mag3110_read_byte(sensor, MAG3110_WHO_AM_I_REG, &data);
    if (ret == ESP_FAIL) return ret;

    *chipid = data;
    return ESP_OK;
}

esp_err_t iot_mag3110_get_die_temperature(mag3110_handle_t sensor, int8_t* temperature)
{
    uint8_t data = 0;
    esp_err_t ret = iot_mag3110_read_byte(sensor, MAG3110_DIE_TEMP_REG, &data);
    if (ret == ESP_FAIL) return ret;

    // Configure the offset to suit your environment.
    *temperature = (int8_t)data + MAG3110_DIE_TEMP_OFFSET;
    return ESP_OK;
}

esp_err_t iot_mag3110_set_mode(mag3110_handle_t sensor, mag3110_operating_modes mode)
{
    esp_err_t ret;
    uint8_t data = 0;
    ret = iot_mag3110_read_byte(sensor, MAG3110_CTRL_REG1, &data);
    if (ret == ESP_FAIL) return ret;

    data &= (~MAG3110_CTRL_REG1_AC);
    data |= mode;

    ret = iot_mag3110_write_byte(sensor, MAG3110_CTRL_REG1, data);
    if (ret == ESP_FAIL) return ret;

    mag3110_dev_t* sens = (mag3110_dev_t*) sensor;
    sens->current_mode = mode;

    return ESP_OK;
}

bool iot_mag3110_is_data_ready(mag3110_handle_t sensor)
{
    uint8_t data = 0;
    esp_err_t ret = iot_mag3110_read_byte(sensor, MAG3110_DR_STATUS_REG, &data);
    if (ret == ESP_FAIL) return false;

    return (data & MAG3110_DR_STATUS_ZYXDR);
}

esp_err_t iot_mag3110_init(mag3110_handle_t sensor)
{
    esp_err_t ret;

    uint8_t chipid;
    ret = iot_mag3110_get_chip_id(sensor, &chipid);
    if (ret == ESP_FAIL || chipid != MAG3110_CHIP_ID) 
    {
        return ESP_FAIL;
    }

    ret = iot_mag3110_set_mode(sensor, MAG3110_MODE_STANDBY);
    if (ret == ESP_FAIL) return ret;

    ret = iot_mag3110_enable_auto_reset(sensor, true);
    if (ret == ESP_FAIL) return ret;

    ret = iot_mag3110_set_mode(sensor, MAG3110_MODE_ACTIVE);
    if (ret == ESP_FAIL) return ret;

    return ESP_OK;
}

esp_err_t iot_mag3110_get_user_offsets(mag3110_handle_t sensor, mag3110_values_t* values_t)
{
    esp_err_t ret;
    uint8_t buf[6] = {0};

    ret = iot_mag3110_read(sensor, MAG3110_OFF_X_MSB_REG, 6, buf);
    if (ret == ESP_FAIL) return ret;

    values_t->X = (int16_t)(buf[0] << 8 | buf[1]);
    values_t->Y = (int16_t)(buf[2] << 8 | buf[3]);
    values_t->Z = (int16_t)(buf[4] << 8 | buf[5]);

    return ESP_OK;
}

esp_err_t iot_mag3110_set_user_offsets(mag3110_handle_t sensor, int16_t x, int16_t y, int16_t z)
{
    esp_err_t ret;

    uint8_t buf[6] = {0};
    buf[0] = (x >> 8);
    buf[1] = x;
    buf[2] = (y >> 8);
    buf[3] = y;
    buf[4] = (z >> 8);
    buf[5] = z;

    ret = iot_mag3110_write(sensor, MAG3110_OFF_X_MSB_REG, 6, buf);

    return ret;
}

esp_err_t iot_mag3110_set_DR_OSR(mag3110_handle_t sensor, uint8_t dr_osr)
{
    esp_err_t ret;
    uint8_t data;

    if (dr_osr > 31) return ESP_FAIL;

    mag3110_dev_t* sens = (mag3110_dev_t*) sensor;
    mag3110_operating_modes prev_mode = sens->current_mode;

    // Set mode to Standby if it is currently Active.
    if (prev_mode == MAG3110_MODE_ACTIVE)
    {
        ret = iot_mag3110_set_mode(sensor, MAG3110_MODE_STANDBY);
        if (ret == ESP_FAIL) return ret;

        vTaskDelay(15 / portTICK_RATE_MS);
    }

    ret = iot_mag3110_read_byte(sensor, MAG3110_CTRL_REG1, &data);
    if (ret == ESP_FAIL) return ret;

   
    data &= 0x07;  // Remove first 5 bits
    data |= (dr_osr << 3); // Prepend DR_OSR as first 5 bits

    ret = iot_mag3110_write_byte(sensor, MAG3110_CTRL_REG1, data);
    if (ret == ESP_FAIL) return ret;

    // Set mode back to Active if it was previously Active.
    if (prev_mode == MAG3110_MODE_ACTIVE && sens->current_mode == MAG3110_MODE_STANDBY)
    {
        ret = iot_mag3110_set_mode(sensor, MAG3110_MODE_ACTIVE);
        if (ret == ESP_FAIL) return ret;
    }

    return ESP_OK;
}

/* private */
esp_err_t set_ctrl_reg_bit(mag3110_handle_t sensor, uint8_t reg, uint8_t bit, bool enabled)
{
    esp_err_t ret;
    uint8_t data = 0;
    mag3110_dev_t* sens = (mag3110_dev_t*) sensor;
    mag3110_operating_modes prev_mode = sens->current_mode;

    // Set mode to Standby if it is currently Active.
    if (prev_mode == MAG3110_MODE_ACTIVE)
    {
        ret = iot_mag3110_set_mode(sensor, MAG3110_MODE_STANDBY);
        if (ret == ESP_FAIL) return ret;

        vTaskDelay(15 / portTICK_RATE_MS);
    }

    ret = iot_mag3110_read_byte(sensor, reg, &data);
    if (ret == ESP_FAIL) return ret;

    data &= (~bit);

    if (enabled)
    {
        data |= bit;
    }

    ret = iot_mag3110_write_byte(sensor, reg, data);
    if (ret == ESP_FAIL) return ret;
    
    // Set mode back to Active if it was previously Active.
    if (prev_mode == MAG3110_MODE_ACTIVE && sens->current_mode == MAG3110_MODE_STANDBY)
    {
        ret = iot_mag3110_set_mode(sensor, MAG3110_MODE_ACTIVE);
        if (ret == ESP_FAIL) return ret;
    }

    return ESP_OK;
}

esp_err_t iot_mag3110_enable_raw_mode(mag3110_handle_t sensor, bool enabled)
{
    return set_ctrl_reg_bit(sensor, MAG3110_CTRL_REG2, MAG3110_CTRL_REG2_RAW, enabled);
}

esp_err_t iot_mag3110_enable_auto_reset(mag3110_handle_t sensor, bool enabled)
{
   return set_ctrl_reg_bit(sensor, MAG3110_CTRL_REG2, MAG3110_CTRL_REG2_AUTO_MRST_EN, enabled); 
}

esp_err_t iot_mag3110_get_data(mag3110_handle_t sensor, mag3110_values_t* values_t)
{
    esp_err_t ret;
    uint8_t buf[6] = {0};

    ret = iot_mag3110_read(sensor, MAG3110_OUT_X_MSB_REG, 6, buf);
    if (ret == ESP_FAIL) return ret;

    values_t->X = (int16_t)(buf[0] << 8 | buf[1]);
    values_t->Y = (int16_t)(buf[2] << 8 | buf[3]);
    values_t->Z = (int16_t)(buf[4] << 8 | buf[5]);

    return ESP_OK;
}

