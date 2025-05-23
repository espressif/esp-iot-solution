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
 * @date: May 2, 2025
 *
 * @Author: Rohan Jeet <jeetrohan92@gmail.com>
 *
 */
#include <stdio.h>
#include <stdbool.h>
#include <unistd.h>
#include "aht20.h"

//i2c configuration values
#define I2C_MASTER_SCL_IO           (22)    // SCL pin
#define I2C_MASTER_SDA_IO           (21)    // SDA pin
#define I2C_MASTER_NUM              I2C_NUM_0
#define I2C_MASTER_FREQ_HZ          (400000)  // I2C frequency

i2c_bus_handle_t my_i2c_bus_handle;

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

void read_aht20(void *my_aht20_handle)
{

    aht20_handle_t aht20_handle = (aht20_handle_t) my_aht20_handle; //retrieve the AHT20 device handle copy
    vTaskDelay(400 / portTICK_PERIOD_MS);
    for (int i = 0; i < 5; i++) {
        //read both humidity and temperature at once from device, using AHT20 device handle
        aht20_read_humiture(aht20_handle);
        //access the results stored in AHT20 device object, using the AHT20 device handle
        //other apis require user to explicitly pass variable address to hold data
        printf("tempertature = %.2fC  humidity = %.3f \n", aht20_handle->humiture.temperature, aht20_handle->humiture.humidity);
        vTaskDelay(2000 / portTICK_PERIOD_MS);
    }
    printf("end of demo\n");
    aht20_remove(&aht20_handle);
    i2c_bus_delete(&my_i2c_bus_handle);
    vTaskDelete(NULL);
}

void init_aht20()
{
    //create a AHT20 device object and receive a device handle for it
    aht20_handle_t aht20_handle =  aht20_create(my_i2c_bus_handle, AHT20_ADDRESS_LOW);

    printf("initializing aht20\n");
    //use the previously created AHT20 device handle for initializing the associated device
    while (aht20_init(aht20_handle) != ESP_OK) { // wait until it is initialized
        vTaskDelay(100 / portTICK_PERIOD_MS);
    }
    printf("\naht20 initialized\n");

    //creating a task to read from AHT20, passing the AHT20 device handle copy
    xTaskCreate(read_aht20, "aht20_tester", 2500, aht20_handle, 5, NULL);
}

void app_main(void)
{
    i2c_master_init(); //initialize i2c master
    init_aht20();    // user defined function for aht20 initialization
}
