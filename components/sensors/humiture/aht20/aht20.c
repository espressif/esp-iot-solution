/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
/*
 * @File: aht20.c
 *
 * @brief: AHT20 driver function definitions
 *
 * @Date: May 2, 2025
 *
 * @Author: Rohan Jeet <jeetrohan92@gmail.com>
 *
 */

#include <stdio.h>
#include "aht20.h"

static const char *s_TAG = "AHT20";

/**
* @brief a function used to handle resetting of registers of the device, if not found calibrated when initialized
*
* @param[in] aht20_handle AHT20 device handle
*
* @param[in] read_buffer storage for read values
*
* @param[in] read_size data size to read
*
*/
static esp_err_t aht20_read_reg(aht20_handle_t sensor, uint8_t * read_buffer, uint8_t read_size);

/**
* @brief a function used to handle resetting of registers of the device, if not found calibrated when initialized
*
* @param[in] aht20_handle AHT20 device handle
*
* @param[in] cmd AHT20 command to be written
*
* @param[in] write_size data size to write
*
*/
static esp_err_t aht20_write_reg(aht20_handle_t sensor, uint8_t * cmd, uint8_t write_size);

/**
* @brief a function used to handle reinitialization of registers of the device, if not found calibrated when initialized
*
* @param[in] aht20_handle AHT20 device handle
*
*/
static esp_err_t aht20_Start_Init(aht20_handle_t aht20_handle);

/**
* @brief a function used to handle resetting of registers of the device, if not found calibrated when initialized
*
* @param[in] aht20_handle AHT20 device handle
*
* @param[in] addr AHT20 internal register, undocumented in datasheet
*
*/
static esp_err_t aht20_JH_Reset_REG(aht20_handle_t aht20_handle, uint8_t addr);

/**
* @brief check crc validity of response received
*
* @param[in] message AHT reading
*
*
* @param[in] num Data bytes to check in message for crc
*
* @return  crc calculated from the provided message
*
*/
static uint8_t calc_CRC8(uint8_t *message, uint8_t Num);

static esp_err_t aht20_read_reg(aht20_handle_t sensor, uint8_t * read_buffer, uint8_t read_size)
{
    ESP_RETURN_ON_ERROR(i2c_bus_read_bytes(sensor->i2c_dev, NULL_I2C_MEM_ADDR, read_size, read_buffer),
                        s_TAG, "unable to read from aht20");

    return ESP_OK;
}

static esp_err_t aht20_write_reg(aht20_handle_t sensor, uint8_t * cmd, uint8_t write_size)
{
    ESP_RETURN_ON_ERROR(i2c_bus_write_bytes(sensor->i2c_dev, NULL_I2C_MEM_ADDR, write_size, cmd),
                        s_TAG, "unable to set mode for AHT20\n");

    return ESP_OK;
}

static uint8_t calc_CRC8(uint8_t *message, uint8_t Num)
{
    uint8_t i;
    uint8_t byte;
    uint8_t crc = 0xFF;
    for (byte = 0; byte < Num; byte++) {
        crc ^= (message[byte]);
        for (i = 8; i > 0; --i) {
            if (crc & 0x80) {
                crc = (crc << 1) ^ 0x31;
            } else {
                crc = (crc << 1);
            }
        }
    }
    return crc;
}

esp_err_t aht20_read_raw(aht20_handle_t aht20_handle, aht20_raw_reading_t *raw_read)
{
    ESP_RETURN_ON_FALSE((aht20_handle != NULL), ESP_ERR_INVALID_ARG, s_TAG, "empty handle, provide a valid AHT20 handle");

    uint8_t measure_cmd[] = {AHT20_MEASURE_CYC, 0x33, 0x00};

    ESP_RETURN_ON_ERROR(aht20_write_reg(aht20_handle, measure_cmd, sizeof(measure_cmd)),
                        s_TAG, "unable to set mode for AHT20\n");

    uint8_t read_measurement[7], read_bytes = 6;;
#ifdef CONFIG_AHT20_CHECK_CRC
    read_bytes = 7;
#endif

    bool busy = true;
    while (busy) {
        aht20_busy_status(aht20_handle, &busy);    // wait for measurement
    }

    ESP_RETURN_ON_ERROR(aht20_read_reg(aht20_handle, read_measurement, read_bytes),
                        s_TAG, "unable to read raw measurement");

#ifdef CONFIG_AHT20_CHECK_CRC
    ESP_RETURN_ON_FALSE((calc_CRC8(read_measurement, 6) == read_measurement[6]),
                        ESP_ERR_INVALID_RESPONSE,
                        s_TAG, "CRC match failed, invalid response received from AHT20");
#endif

    raw_read->humidity = ((read_measurement[1] << 16) | (read_measurement[2] << 8) | read_measurement[3]) >> 4;
    raw_read->temperature = ((read_measurement[3] << 16) | (read_measurement[4] << 8) | read_measurement[5]) & 0xfffff;

    return ESP_OK;
}

esp_err_t aht20_read_humiture(aht20_handle_t aht20_handle)
{
    aht20_raw_reading_t raw_read;
    ESP_RETURN_ON_ERROR(aht20_read_raw(aht20_handle, &raw_read),
                        "", "");

    aht20_handle->humiture.humidity = raw_read.humidity * 100.0 / 1024 / 1024; //Calculated humidity value
    aht20_handle->humiture.temperature = (raw_read.temperature * 200.0 / 1024 / 1024) - 50; //Calculated temperature value

    return ESP_OK;
}

esp_err_t aht20_read_humidity(aht20_handle_t aht20_handle, float_t *humidity)
{
    aht20_raw_reading_t raw_read;
    ESP_RETURN_ON_ERROR(aht20_read_raw(aht20_handle, &raw_read),
                        "", "");

    *humidity = raw_read.humidity * 100.0 / 1024 / 1024; //Calculated humidity value
    return ESP_OK;
}

esp_err_t aht20_read_temperature(aht20_handle_t aht20_handle, float_t *temperature)
{
    aht20_raw_reading_t raw_read;
    ESP_RETURN_ON_ERROR(aht20_read_raw(aht20_handle, &raw_read),
                        "", "");

    *temperature = (raw_read.temperature * 200.0 / 1024 / 1024) - 50; //Calculated temperature value

    return ESP_OK;
}

esp_err_t aht20_calibration_status(aht20_handle_t aht20_handle, bool *calibration)
{
    ESP_RETURN_ON_FALSE((aht20_handle != NULL), ESP_ERR_INVALID_ARG,
                        s_TAG, "empty handle, initialize AHT20 handle");

    ESP_RETURN_ON_FALSE((calibration != NULL), ESP_ERR_INVALID_ARG,
                        s_TAG, "provide a variable to store status value");

    uint8_t read_status;

    ESP_RETURN_ON_ERROR(aht20_read_reg(aht20_handle, &read_status, sizeof(read_status)),
                        s_TAG, "unable to read status");

    if (read_status & BIT3) {
        *calibration = true;
    } else {
        *calibration = false;
    }

    return ESP_OK;
}

esp_err_t aht20_busy_status(aht20_handle_t aht20_handle, bool *busy)
{
    ESP_RETURN_ON_FALSE((aht20_handle != NULL), ESP_ERR_INVALID_ARG,
                        s_TAG, "empty handle, initialize AHT20 handle");

    ESP_RETURN_ON_FALSE((busy != NULL), ESP_ERR_INVALID_ARG,
                        s_TAG, "provide a variable to store status value");
    uint8_t read_status;
    ESP_RETURN_ON_ERROR(aht20_read_reg(aht20_handle, &read_status, sizeof(read_status)),
                        s_TAG, "unable to read status");

    if (read_status & BIT7) {
        *busy = true;
    } else {
        *busy = false;
    }

    return ESP_OK;
}

static esp_err_t aht20_JH_Reset_REG(aht20_handle_t aht20_handle, uint8_t addr)
{

    uint8_t reset_cmd[] = {addr, 0x00, 0x00}, read_bytes[3];

    ESP_RETURN_ON_ERROR(aht20_write_reg(aht20_handle, reset_cmd, sizeof(reset_cmd)),
                        s_TAG, "unable to reset, check log");

    ESP_RETURN_ON_ERROR(aht20_read_reg(aht20_handle, read_bytes, sizeof(read_bytes)),
                        s_TAG, "unable to reset, check log");

    vTaskDelay(10 / portTICK_PERIOD_MS);
    reset_cmd[0] = 0xB0 | addr;
    reset_cmd[1] = read_bytes[1];
    reset_cmd[2] = read_bytes[2];

    ESP_RETURN_ON_ERROR(aht20_write_reg(aht20_handle, reset_cmd, sizeof(reset_cmd)),
                        s_TAG, "unable to reset, check log");

    return ESP_OK;
}

static esp_err_t aht20_Start_Init(aht20_handle_t aht20_handle)
{
    ESP_RETURN_ON_ERROR(aht20_JH_Reset_REG(aht20_handle, 0x1b), "", "");
    ESP_RETURN_ON_ERROR(aht20_JH_Reset_REG(aht20_handle, 0x1c), "", "");
    ESP_RETURN_ON_ERROR(aht20_JH_Reset_REG(aht20_handle, 0x1e), "", "");

    return ESP_OK;
}

esp_err_t aht20_init(aht20_handle_t aht20_handle)
{
    ESP_RETURN_ON_FALSE((aht20_handle != NULL), ESP_ERR_INVALID_ARG, s_TAG, "empty handle, initialize AHT20 handle");

    vTaskDelay(20 / portTICK_PERIOD_MS); //time for AHT20 SCL to stabilize

    /***********************************************************************************/
    /** // This is undocumented in user manual
        //The first time the power is turned on, read the status word at 0x71, determine whether the status word is 0x18,
        //if it is not 0x18,  reset the registers
    **/
    uint8_t read_status;

    ESP_RETURN_ON_ERROR(aht20_read_reg(aht20_handle, &read_status, sizeof(read_status)),
                        s_TAG, "unable to read status");

    if ((read_status & 0x18) != 0x18) {
        ESP_RETURN_ON_ERROR(aht20_Start_Init(aht20_handle),
                            s_TAG, "reset failed, retry");
        vTaskDelay(10 / portTICK_PERIOD_MS);
    }
    /***********************************************************************************/

    // initialize AHT20
    uint8_t aht20_init_cmd [] =  { AHT20_INIT_REG, 0x08, 0x00 };

    ESP_RETURN_ON_ERROR(aht20_write_reg(aht20_handle, aht20_init_cmd, sizeof(aht20_init_cmd)),
                        s_TAG, "unable to initialize AHT20\n");

    ESP_LOGI(s_TAG, "AHT20 initialized\n");

    return ESP_OK;

}

aht20_handle_t aht20_create(i2c_bus_handle_t bus_handle, uint8_t aht20_address)
{
    ESP_LOGI(s_TAG, "adding aht20 as device to bus\n");
    i2c_bus_device_handle_t dev_handle = i2c_bus_device_create(bus_handle, aht20_address, CONFIG_AHT20_I2C_CLK_SPEED);
    ESP_RETURN_ON_FALSE((dev_handle != NULL), NULL,
                        s_TAG, "unable to create device\n");
    ESP_LOGI(s_TAG, "device added to bus\n");

    aht20_handle_t my_aht20_handle = malloc(sizeof(aht20_dev_config_t));

    ESP_RETURN_ON_FALSE((my_aht20_handle != NULL), NULL,
                        s_TAG, "unable to allocate memory to initialize aht20 handle");

    my_aht20_handle->i2c_dev =  dev_handle;
    return my_aht20_handle;
}

void aht20_remove(aht20_handle_t *aht20ptr)
{
    i2c_bus_device_delete(&((*aht20ptr)->i2c_dev));
    free(*aht20ptr);
    *aht20ptr = NULL; // now AHT20 handle is not a dangling pointer
}
