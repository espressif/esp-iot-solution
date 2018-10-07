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
#include "iot_bme280.h"
#include "math.h"
#include "esp_log.h"

typedef struct {
    i2c_bus_handle_t bus;
    uint16_t dev_addr;
    bme280_data_t data_t;
    bme280_config_t config_t;
    bme280_ctrl_meas_t ctrl_meas_t;
    bme280_ctrl_hum_t ctrl_hum_t;
    int32_t t_fine;
} bme280_dev_t;

bme280_handle_t iot_bme280_create(i2c_bus_handle_t bus, uint16_t dev_addr)
{
    bme280_dev_t* dev = (bme280_dev_t*) calloc(1, sizeof(bme280_dev_t));
    dev->bus = bus;
    dev->dev_addr = dev_addr;
    return (bme280_handle_t) dev;
}

esp_err_t iot_bme280_delete(bme280_handle_t dev, bool del_bus)
{
    bme280_dev_t* device = (bme280_dev_t*) dev;
    if (del_bus) {
        iot_i2c_bus_delete(device->bus);
        device->bus = NULL;
    }
    free(device);
    return ESP_OK;
}

esp_err_t iot_bme280_write_byte(bme280_handle_t dev, uint8_t addr, uint8_t data)
{
    //start-device_addr-word_addr-data-stop
    esp_err_t ret;
    bme280_dev_t* device = (bme280_dev_t*) dev;
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (device->dev_addr << 1) | WRITE_BIT,
    ACK_CHECK_EN);
    i2c_master_write_byte(cmd, addr, ACK_CHECK_EN);
    i2c_master_write_byte(cmd, data, ACK_CHECK_EN);
    i2c_master_stop(cmd);
    ret = iot_i2c_bus_cmd_begin(device->bus, cmd, 1000 / portTICK_RATE_MS);
    i2c_cmd_link_delete(cmd);
    return ret;
}

esp_err_t iot_bme280_write(bme280_handle_t dev, uint8_t start_addr,
        uint8_t write_num, uint8_t *data_buf)
{
    uint32_t i = 0;
    i2c_cmd_handle_t cmd;
    esp_err_t ret = ESP_FAIL;
    uint32_t writeNum = write_num;
    bme280_dev_t* device = (bme280_dev_t*) dev;
    if (data_buf != NULL) {
        for (uint32_t j = 0; j < write_num; j += 8) {
            cmd = i2c_cmd_link_create();
            i2c_master_start(cmd);
            i2c_master_write_byte(cmd, (device->dev_addr << 1) | WRITE_BIT,
            ACK_CHECK_EN);
            i2c_master_write_byte(cmd, start_addr, ACK_CHECK_EN);
            for (i = j; i < ((writeNum >= 8) ? 8 : writeNum); i++) {
                i2c_master_write_byte(cmd, data_buf[i], ACK_CHECK_EN);
            }
            i2c_master_stop(cmd);
            ret = iot_i2c_bus_cmd_begin(device->bus, cmd,
                    1000 / portTICK_RATE_MS);
            i2c_cmd_link_delete(cmd);

            writeNum -= 8;              //write num count
            if (ret == ESP_FAIL) {
                return ret;
            }
        }
    }
    return ret;
}

esp_err_t iot_bme280_read_byte(bme280_handle_t dev, uint8_t addr, uint8_t *data)
{
    //start-device_addr-word_addr-start-device_addr-data-stop; no_ack of end data
    esp_err_t ret;
    bme280_dev_t* device = (bme280_dev_t*) dev;
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (device->dev_addr << 1) | WRITE_BIT,
    ACK_CHECK_EN);
    i2c_master_write_byte(cmd, addr, ACK_CHECK_EN);

    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (device->dev_addr << 1) | READ_BIT,
    ACK_CHECK_EN);
    i2c_master_read_byte(cmd, data, NACK_VAL);
    i2c_master_stop(cmd);
    ret = iot_i2c_bus_cmd_begin(device->bus, cmd, 1000 / portTICK_RATE_MS);
    i2c_cmd_link_delete(cmd);
    return ret;
}

esp_err_t iot_bme280_read(bme280_handle_t dev, uint8_t start_addr,
        uint8_t read_num, uint8_t *data_buf)
{
    uint32_t i = 0;
    esp_err_t ret = ESP_FAIL;
    if (data_buf != NULL) {
        bme280_dev_t* device = (bme280_dev_t*) dev;
        i2c_cmd_handle_t cmd = i2c_cmd_link_create();
        i2c_master_start(cmd);
        i2c_master_write_byte(cmd, (device->dev_addr << 1) | WRITE_BIT,
        ACK_CHECK_EN);
        i2c_master_write_byte(cmd, start_addr, ACK_CHECK_EN);

        i2c_master_start(cmd);
        i2c_master_write_byte(cmd, (device->dev_addr << 1) | READ_BIT,
        ACK_CHECK_EN);
        for (i = 0; i < read_num - 1; i++) {
            i2c_master_read_byte(cmd, &data_buf[i], ACK_VAL);
        }
        i2c_master_read_byte(cmd, &data_buf[i], NACK_VAL);
        i2c_master_stop(cmd);
        ret = iot_i2c_bus_cmd_begin(device->bus, cmd, 1000 / portTICK_RATE_MS);
        i2c_cmd_link_delete(cmd);
    }
    return ret;
}

static esp_err_t iot_bme280_read_uint16(bme280_handle_t dev, uint8_t addr,
        uint16_t *data)
{
    esp_err_t ret = ESP_FAIL;
    uint8_t data0, data1;
    if (iot_bme280_read_byte(dev, addr, &data0) == ESP_FAIL) {
        return ret;
    }
    if (iot_bme280_read_byte(dev, addr + 1, &data1) == ESP_FAIL) {
        return ret;
    }
    *data = (data0 << 8) | data1;
    return ESP_OK;
}

static esp_err_t iot_bme280_read_uint16_le(bme280_handle_t dev, uint8_t addr,
        uint16_t *data)
{
    esp_err_t ret = ESP_FAIL;
    uint8_t data0, data1;
    if (iot_bme280_read_byte(dev, addr, &data0) == ESP_FAIL) {
        return ret;
    }
    if (iot_bme280_read_byte(dev, addr + 1, &data1) == ESP_FAIL) {
        return ret;
    }
    *data = (data1 << 8) | data0;
    return ESP_OK;
}

unsigned int iot_bme280_getconfig(bme280_handle_t dev)
{
    bme280_dev_t* device = (bme280_dev_t*) dev;
    return (device->config_t.t_sb << 5) | (device->config_t.filter << 3)
            | device->config_t.spi3w_en;
}

unsigned int iot_bme280_getctrl_meas(bme280_handle_t dev)
{
    bme280_dev_t* device = (bme280_dev_t*) dev;
    return (device->ctrl_meas_t.osrs_t << 5) | (device->ctrl_meas_t.osrs_p << 3)
            | device->ctrl_meas_t.mode;
}

unsigned int iot_bme280_getctrl_hum(bme280_handle_t dev)
{
    bme280_dev_t* device = (bme280_dev_t*) dev;
    return (device->ctrl_hum_t.osrs_h);
}

bool iot_bme280_is_reading_calibration(bme280_handle_t dev)
{
    uint8_t rstatus = 0;
    if (iot_bme280_read_byte(dev, BME280_REGISTER_STATUS, &rstatus) == ESP_FAIL) {
        return false;
    }

    return (rstatus & (1 << 0)) != 0;
}

esp_err_t iot_bme280_read_coefficients(bme280_handle_t dev)
{
    uint8_t data = 0;
    uint8_t data1 = 0;
    uint16_t data16 = 0;
    bme280_dev_t* device = (bme280_dev_t*) dev;

    if (iot_bme280_read_uint16_le(dev, BME280_REGISTER_DIG_T1,
            &data16) == ESP_FAIL) {
        return ESP_FAIL;
    }
    device->data_t.dig_t1 = data16;
    if (iot_bme280_read_uint16_le(dev, BME280_REGISTER_DIG_T2,
            &data16) == ESP_FAIL) {
        return ESP_FAIL;
    }
    device->data_t.dig_t2 = (int16_t) data16;
    if (iot_bme280_read_uint16_le(dev, BME280_REGISTER_DIG_T3,
            &data16) == ESP_FAIL) {
        return ESP_FAIL;
    }
    device->data_t.dig_t3 = (int16_t) data16;

    if (iot_bme280_read_uint16_le(dev, BME280_REGISTER_DIG_P1,
            &data16) == ESP_FAIL) {
        return ESP_FAIL;
    }
    device->data_t.dig_p1 = data16;
    if (iot_bme280_read_uint16_le(dev, BME280_REGISTER_DIG_P2,
            &data16) == ESP_FAIL) {
        return ESP_FAIL;
    }
    device->data_t.dig_p2 = (int16_t) data16;
    if (iot_bme280_read_uint16_le(dev, BME280_REGISTER_DIG_P3,
            &data16) == ESP_FAIL) {
        return ESP_FAIL;
    }
    device->data_t.dig_p3 = (int16_t) data16;
    if (iot_bme280_read_uint16_le(dev, BME280_REGISTER_DIG_P4,
            &data16) == ESP_FAIL) {
        return ESP_FAIL;
    }
    device->data_t.dig_p4 = (int16_t) data16;
    if (iot_bme280_read_uint16_le(dev, BME280_REGISTER_DIG_P5,
            &data16) == ESP_FAIL) {
        return ESP_FAIL;
    }
    device->data_t.dig_p5 = (int16_t) data16;
    if (iot_bme280_read_uint16_le(dev, BME280_REGISTER_DIG_P6,
            &data16) == ESP_FAIL) {
        return ESP_FAIL;
    }
    device->data_t.dig_p6 = (int16_t) data16;
    if (iot_bme280_read_uint16_le(dev, BME280_REGISTER_DIG_P7,
            &data16) == ESP_FAIL) {
        return ESP_FAIL;
    }
    device->data_t.dig_p7 = (int16_t) data16;
    if (iot_bme280_read_uint16_le(dev, BME280_REGISTER_DIG_P8,
            &data16) == ESP_FAIL) {
        return ESP_FAIL;
    }
    device->data_t.dig_p8 = (int16_t) data16;
    if (iot_bme280_read_uint16_le(dev, BME280_REGISTER_DIG_P9,
            &data16) == ESP_FAIL) {
        return ESP_FAIL;
    }
    device->data_t.dig_p9 = (int16_t) data16;

    if (iot_bme280_read_byte(dev, BME280_REGISTER_DIG_H1, &data) == ESP_FAIL) {
        return ESP_FAIL;
    }
    device->data_t.dig_h1 = data;
    if (iot_bme280_read_uint16_le(dev, BME280_REGISTER_DIG_H2,
            &data16) == ESP_FAIL) {
        return ESP_FAIL;
    }
    device->data_t.dig_h2 = (int16_t) data16;
    if (iot_bme280_read_byte(dev, BME280_REGISTER_DIG_H3, &data) == ESP_FAIL) {
        return ESP_FAIL;
    }
    device->data_t.dig_h3 = data;
    if (iot_bme280_read_byte(dev, BME280_REGISTER_DIG_H4, &data) == ESP_FAIL) {
        return ESP_FAIL;
    }
    if (iot_bme280_read_byte(dev, BME280_REGISTER_DIG_H4 + 1,
            &data1) == ESP_FAIL) {
        return ESP_FAIL;
    }
    device->data_t.dig_h4 = (data << 4) | (data1 & 0xF);
    if (iot_bme280_read_byte(dev, BME280_REGISTER_DIG_H5 + 1, &data) == ESP_FAIL) {
        return ESP_FAIL;
    }
    if (iot_bme280_read_byte(dev, BME280_REGISTER_DIG_H5, &data1) == ESP_FAIL) {
        return ESP_FAIL;
    }
    device->data_t.dig_h5 = (data << 4) | (data1 >> 4);
    if (iot_bme280_read_byte(dev, BME280_REGISTER_DIG_H6, &data) == ESP_FAIL) {
        return ESP_FAIL;
    }
    device->data_t.dig_h6 = (int8_t) data;
    return ESP_OK;
}

esp_err_t iot_bme280_set_sampling(bme280_handle_t dev, bme280_sensor_mode mode,
        bme280_sensor_sampling tempSampling,
        bme280_sensor_sampling pressSampling,
        bme280_sensor_sampling humSampling, bme280_sensor_filter filter,
        bme280_standby_duration duration)
{
    bme280_dev_t* device = (bme280_dev_t*) dev;
    device->ctrl_meas_t.mode = mode;
    device->ctrl_meas_t.osrs_t = tempSampling;
    device->ctrl_meas_t.osrs_p = pressSampling;

    device->ctrl_hum_t.osrs_h = humSampling;
    device->config_t.filter = filter;
    device->config_t.t_sb = duration;

    // you must make sure to also set REGISTER_CONTROL after setting the
    // CONTROLHUMID register, otherwise the values won't be applied (see DS 5.4.3)
    if (iot_bme280_write_byte(dev, BME280_REGISTER_CONTROLHUMID,
            iot_bme280_getctrl_hum(dev)) == ESP_FAIL) {
        return ESP_FAIL;
    }
    if (iot_bme280_write_byte(dev, BME280_REGISTER_CONFIG,
            iot_bme280_getconfig(dev)) == ESP_FAIL) {
        return ESP_FAIL;
    }
    if (iot_bme280_write_byte(dev, BME280_REGISTER_CONTROL,
            iot_bme280_getctrl_meas(dev)) == ESP_FAIL) {
        return ESP_FAIL;
    }
    return ESP_OK;
}

esp_err_t iot_bme280_init(bme280_handle_t dev)
{
    // check if sensor, i.e. the chip ID is correct
    uint8_t chipid = 0;
    if(iot_bme280_read_byte(dev, BME280_REGISTER_CHIPID, &chipid) == ESP_FAIL){
        ESP_LOGI("BME280:", "iot_bme280_init->iot_bme280_read_byte ->BME280_REGISTER_CHIPID failed!!!!:%x", chipid);
        return ESP_FAIL;
    }
    if (chipid != BME280_DEFAULT_CHIPID) {
        ESP_LOGI("BME280:", "iot_bme280_init->BME280_DEFAULT_CHIPID:%x", chipid);
        return ESP_FAIL;
    }

    // reset the device using soft-reset, this makes sure the IIR is off, etc.
    if (iot_bme280_write_byte(dev, BME280_REGISTER_SOFTRESET, 0xB6) == ESP_FAIL) {
        return ESP_FAIL;
    }

    // wait for chip to wake up.
    vTaskDelay(300 / portTICK_RATE_MS);

    // if chip is still reading calibration, delay
    while (iot_bme280_is_reading_calibration(dev)) {
        vTaskDelay(100 / portTICK_RATE_MS);
    }

    if (iot_bme280_read_coefficients(dev) == ESP_FAIL) { // read trimming parameters, see DS 4.2.2
        return ESP_FAIL;
    }

    if (iot_bme280_set_sampling(dev, BME280_MODE_NORMAL, BME280_SAMPLING_X16,
            BME280_SAMPLING_X16, BME280_SAMPLING_X16, BME280_FILTER_OFF,
            BME280_STANDBY_MS_0_5) == ESP_FAIL) {    // use defaults
        return ESP_FAIL;
    }

    return ESP_OK;
}

esp_err_t iot_bme280_take_forced_measurement(bme280_handle_t dev)
{
    uint8_t data = 0;
    bme280_dev_t* device = (bme280_dev_t*) dev;
    if (device->ctrl_meas_t.mode == BME280_MODE_FORCED) {
        // set to forced mode, i.e. "take next measurement"
        if (iot_bme280_write_byte(dev, BME280_REGISTER_CONTROL,
                iot_bme280_getctrl_meas(dev)) == ESP_FAIL) {
            return ESP_FAIL;
        }
        // wait until measurement has been completed, otherwise we would read, the values from the last measurement
        if (iot_bme280_read_byte(dev, BME280_REGISTER_STATUS, &data) == ESP_FAIL) {
            return ESP_FAIL;
        }
        while (data & 0x08) {
            iot_bme280_read_byte(dev, BME280_REGISTER_STATUS, &data);
            vTaskDelay(10 / portTICK_RATE_MS);
        }
    }
    return ESP_OK;
}

float iot_bme280_read_temperature(bme280_handle_t dev)
{
    int32_t var1, var2;
    uint8_t data[3] = { 0 };
    bme280_dev_t* device = (bme280_dev_t*) dev;

    if (iot_bme280_read(dev, BME280_REGISTER_TEMPDATA, 3, data) == ESP_FAIL) {
        return ESP_FAIL;
    }

    int32_t adc_T = (data[0] << 16) | (data[1] << 8) | data[2];
    if (adc_T == 0x800000) {      // value in case temp measurement was disabled
        return ESP_FAIL;
    }
    adc_T >>= 4;

    var1 = ((((adc_T >> 3) - ((int32_t) device->data_t.dig_t1 << 1)))
            * ((int32_t) device->data_t.dig_t2)) >> 11;

    var2 = (((((adc_T >> 4) - ((int32_t) device->data_t.dig_t1))
            * ((adc_T >> 4) - ((int32_t) device->data_t.dig_t1))) >> 12)
            * ((int32_t) device->data_t.dig_t3)) >> 14;

    device->t_fine = var1 + var2;

    return (((device->t_fine * 5 + 128) >> 8) / 100.0);
}

float iot_bme280_read_pressure(bme280_handle_t dev)
{
    int64_t var1, var2, p;
    uint8_t data[3] = { 0 };
    bme280_dev_t* device = (bme280_dev_t*) dev;

    if (iot_bme280_read_temperature(dev) == ESP_FAIL) {
        // must be done first to get t_fine
        return ESP_FAIL;
    }

    if (iot_bme280_read(dev, BME280_REGISTER_PRESSUREDATA, 3, data) == ESP_FAIL) {
        return ESP_FAIL;
    }

    int32_t adc_P = (data[0] << 16) | (data[1] << 8) | data[2];
    if (adc_P == 0x800000) {  // value in case pressure measurement was disabled
        return ESP_FAIL;
    }

    adc_P >>= 4;

    var1 = ((int64_t) device->t_fine) - 128000;
    var2 = var1 * var1 * (int64_t) device->data_t.dig_p6;
    var2 = var2 + ((var1 * (int64_t) device->data_t.dig_p5) << 17);
    var2 = var2 + (((int64_t) device->data_t.dig_p4) << 35);
    var1 = ((var1 * var1 * (int64_t) device->data_t.dig_p3) >> 8)
            + ((var1 * (int64_t) device->data_t.dig_p2) << 12);
    var1 = (((((int64_t) 1) << 47) + var1)) * ((int64_t) device->data_t.dig_p1)
            >> 33;

    if (var1 == 0) {
        return ESP_FAIL; // avoid exception caused by division by zero
    }
    p = 1048576 - adc_P;
    p = (((p << 31) - var2) * 3125) / var1;
    var1 = (((int64_t) device->data_t.dig_p9) * (p >> 13) * (p >> 13)) >> 25;
    var2 = (((int64_t) device->data_t.dig_p8) * p) >> 19;

    p = ((p + var1 + var2) >> 8) + (((int64_t) device->data_t.dig_p7) << 4);
    p = p >> 8; // /256
    return ((float) p / 100);
}

float iot_bme280_read_humidity(bme280_handle_t dev)
{
    uint16_t data16;
    bme280_dev_t* device = (bme280_dev_t*) dev;

    if (iot_bme280_read_temperature(dev) == ESP_FAIL) {
        // must be done first to get t_fine
        return ESP_FAIL;
    }

    if (iot_bme280_read_uint16(dev, BME280_REGISTER_HUMIDDATA,
            &data16) == ESP_FAIL) {
        return ESP_FAIL;
    }

    int32_t adc_H = data16;
    if (adc_H == 0x8000) { // value in case humidity measurement was disabled
        return ESP_FAIL;
    }

    int32_t v_x1_u32r;

    v_x1_u32r = (device->t_fine - ((int32_t) 76800));

    v_x1_u32r = (((((adc_H << 14) - (((int32_t) device->data_t.dig_h4) << 20)
            - (((int32_t) device->data_t.dig_h5) * v_x1_u32r))
            + ((int32_t) 16384)) >> 15)
            * (((((((v_x1_u32r * ((int32_t) device->data_t.dig_h6)) >> 10)
                    * (((v_x1_u32r * ((int32_t) device->data_t.dig_h3)) >> 11)
                            + ((int32_t) 32768))) >> 10) + ((int32_t) 2097152))
                    * ((int32_t) device->data_t.dig_h2) + 8192) >> 14));

    v_x1_u32r = (v_x1_u32r
            - (((((v_x1_u32r >> 15) * (v_x1_u32r >> 15)) >> 7)
                    * ((int32_t) device->data_t.dig_h1)) >> 4));

    v_x1_u32r = (v_x1_u32r < 0) ? 0 : v_x1_u32r;
    v_x1_u32r = (v_x1_u32r > 419430400) ? 419430400 : v_x1_u32r;
    return ((v_x1_u32r >> 12) / 1024.0);
}

float iot_bme280_read_altitude(bme280_handle_t dev, float seaLevel)
{
    float pressure = 0;
    if (iot_bme280_read_pressure(dev) == ESP_FAIL) {
        return ESP_FAIL;
    }

    float atmospheric = pressure / 100.0F;
    return (44330.0 * (1.0 - pow(atmospheric / seaLevel, 0.1903)));
}

float iot_bme280_calculates_pressure(bme280_handle_t dev, float altitude,
        float atmospheric)
{
    return (atmospheric / pow(1.0 - (altitude / 44330.0), 5.255));
}

