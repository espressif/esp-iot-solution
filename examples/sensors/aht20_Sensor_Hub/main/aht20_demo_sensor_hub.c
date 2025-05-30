/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
/*
 * @file: aht20_demo.c
 *
 * @brief: A simple demo to use the AHT20 driver
 *
 * @date: May 22, 2025
 *
 * @Author: Rohan Jeet <jeetrohan92@gmail.com>
 *
 */
#include <stdio.h>
#include <stdbool.h>
#include <unistd.h>

#include "iot_sensor_hub.h"

#include "aht20.h"

//i2c configuration values
#define I2C_MASTER_SCL_IO           CONFIG_I2C_MASTER_SCL
#define I2C_MASTER_SDA_IO           CONFIG_I2C_MASTER_SDA
#define I2C_MASTER_NUM              I2C_NUM_0
#define I2C_MASTER_FREQ_HZ          (400000)  // I2C frequency

#define TAG "AHT20_SENSOR_HUB"

#define SENSOR_PERIOD           CONFIG_AHT20_SAMPLE_PERIOD

i2c_bus_handle_t my_i2c_bus_handle;

static void sensor_event_handler(void *handler_args, esp_event_base_t base, int32_t id, void *event_data)
{
    sensor_data_t *sensor_data = (sensor_data_t *)event_data;

    switch (id) {
    case SENSOR_STARTED:
        ESP_LOGI(TAG, "Timestamp = %llu - %s_0x%x STARTED",
                 sensor_data->timestamp,
                 sensor_data->sensor_name,
                 sensor_data->sensor_addr);
        break;
    case SENSOR_STOPED:
        ESP_LOGI(TAG, "Timestamp = %llu - %s_0x%x STOPPED",
                 sensor_data->timestamp,
                 sensor_data->sensor_name,
                 sensor_data->sensor_addr);
        break;
    case SENSOR_TEMP_DATA_READY:
        ESP_LOGI(TAG, "Timestamp = %llu - %s_0x%x TEMP_DATA_READY - "
                 "temperature=%.2f\n",
                 sensor_data->timestamp,
                 sensor_data->sensor_name,
                 sensor_data->sensor_addr,
                 sensor_data->temperature);
        break;
    case SENSOR_HUMI_DATA_READY:
        ESP_LOGI(TAG, "Timestamp = %llu - %s_0x%x HUMIDITY_DATA_READY - "
                 "humidity=%.2f\n",
                 sensor_data->timestamp,
                 sensor_data->sensor_name,
                 sensor_data->sensor_addr,
                 sensor_data->humidity);
        break;
    default:
        ESP_LOGI(TAG, "Timestamp = %" PRIi64 " - event id = %" PRIi32, sensor_data->timestamp, id);
        break;
    }
}

void i2c_master_init(void)
{
    const i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = I2C_MASTER_SDA_IO,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_io_num = I2C_MASTER_SCL_IO,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = I2C_MASTER_FREQ_HZ,
    };
    printf("requesting i2c bus handle\n");
    my_i2c_bus_handle = i2c_bus_create(I2C_MASTER_NUM, &conf);
    printf("i2c bus handle acquired\n");

}

void init_aht20()
{
    /*create sensor based on sensorID*/
    sensor_handle_t sensor_handle = NULL;
    sensor_config_t sensor_config = {
        .bus = my_i2c_bus_handle,            /*which bus sensors will connect to*/
        .type = HUMITURE_ID,               /*sensor type*/
        .addr = AHT20_ADDRESS_LOW,     /*sensor addr*/
        .mode = MODE_POLLING,              /*data acquire mode*/
        .min_delay = SENSOR_PERIOD         /*data acquire period*/
    };

    /*create a sensor with specific sensor_id and configurations*/
    iot_sensor_create("aht20", &sensor_config, &sensor_handle);

    /*register handler with sensor's handle*/
    iot_sensor_handler_register(sensor_handle, sensor_event_handler, NULL);

    /*start a sensor, data ready events will be posted once data acquired successfully*/
    iot_sensor_start(sensor_handle);

}

void app_main(void)
{
    //initialize i2c master
    i2c_master_init();

    // user defined function for aht20 initialization
    init_aht20();

    while (1) {
        vTaskDelay(1000);
    }
}
