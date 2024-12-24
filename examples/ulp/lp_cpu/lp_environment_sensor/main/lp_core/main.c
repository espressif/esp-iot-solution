/*
 * SPDX-FileCopyrightText: 2023-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "ulp_lp_core_utils.h"
#include "ulp_lp_core_i2c.h"
#include "sdkconfig.h"
#ifdef CONFIG_ULP_UART_PRINT
#include "ulp_lp_core_print.h"
#endif

#define AHT30_SENSOR_I2C_ADDR           0x38
#define WEATHER_UPDATE_INTERVAL_TIMES   (3 * (3600 / CONFIG_LP_CPU_WAKEUP_TIME_SECOND)) // Update interval is 3 hours
#define LP_I2C_TRANS_TIMEOUT_CYCLES     5000
#define LP_I2C_TRANS_WAIT_FOREVER       -1
#define TEMP_CHANGE_THRESHOLD           0.5
#define HUM_CHANGE_THRESHOLD            5.0

uint8_t sensor_data_report = 0;
uint32_t temp = 0;
uint32_t hum = 0;
float temp_last = 0.0;
float hum_last = 0.0;
#if CONFIG_UPDATE_WEATHER_DATA
uint32_t measure_times = 0;
uint32_t weather_data_update = 0;
#endif

static void lp_read_aht30_data(float *temperature, float *humidity)
{
    uint8_t data_write[3] = { 0xAC, 0x33, 0x00 };
    lp_core_i2c_master_write_to_device(LP_I2C_NUM_0, AHT30_SENSOR_I2C_ADDR, data_write, sizeof(data_write), LP_I2C_TRANS_WAIT_FOREVER);

    uint32_t raw_data;
    uint8_t buf[7] = { 0 };
    lp_core_i2c_master_read_from_device(LP_I2C_NUM_0, AHT30_SENSOR_I2C_ADDR, buf, sizeof(buf), LP_I2C_TRANS_TIMEOUT_CYCLES);

    raw_data = buf[1];
    raw_data = raw_data << 8;
    raw_data += buf[2];
    raw_data = raw_data << 8;
    raw_data += buf[3];
    raw_data = raw_data >> 4;
    *humidity = raw_data * 100.0f / 1048576;

    raw_data = buf[3] & 0x0F;
    raw_data = raw_data << 8;
    raw_data += buf[4];
    raw_data = raw_data << 8;
    raw_data += buf[5];
    *temperature = raw_data * 200.0f / 1048576 - 50;
}

int main(void)
{
#if CONFIG_UPDATE_WEATHER_DATA
    measure_times++;
    /* Wake up the CPU every 3 hours to fetch weather information. */
    if (measure_times == WEATHER_UPDATE_INTERVAL_TIMES) {
        ulp_lp_core_wakeup_main_processor();
        weather_data_update = 1;
    }
#endif
    if (sensor_data_report == 0) {
        int temp_int = 0;
        float *temp_f = (float *)&temp;
        float *hum_f = (float *)&hum;
        lp_read_aht30_data(temp_f, hum_f);
        /* Wake up the CPU to report temperature and humidity data
         * when the temperature changes by 0.5 degree Celsius or the humidity changes by 5.
         */
        if (fabs(*temp_f - temp_last) > TEMP_CHANGE_THRESHOLD) {
#ifdef CONFIG_ULP_UART_PRINT
            lp_core_printf("temp: %d %d, hum: %d %d\r\n", (int)temp, temp_last, (int)hum, hum_last);
#endif
            ulp_lp_core_wakeup_main_processor();
            temp_last = *temp_f;
            hum_last = *hum_f;
            sensor_data_report = 1;
        }
    }
    return 0;
}
