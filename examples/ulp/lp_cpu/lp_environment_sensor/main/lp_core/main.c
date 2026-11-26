/*
 * SPDX-FileCopyrightText: 2023-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <stdbool.h>
#include "ulp_lp_core_utils.h"
#include "ulp_lp_core_i2c.h"
#include "sdkconfig.h"
#ifdef CONFIG_ULP_UART_PRINT
#include "ulp_lp_core_print.h"
#endif

#define AHT21B_SENSOR_I2C_ADDR          0x38
#define AHT21B_STATUS_BUSY              0x80
#define AHT21B_STATUS_CALIBRATED        0x08
#define WEATHER_UPDATE_INTERVAL_TIMES   (3 * (3600 / CONFIG_LP_CPU_WAKEUP_TIME_SECOND)) // Update interval is 3 hours
#define LP_I2C_TRANS_TIMEOUT_CYCLES     5000
#define AHT21B_MEASUREMENT_TIME_US      80000
#define TEMP_CHANGE_THRESHOLD           0.5
#define HUM_CHANGE_THRESHOLD            5.0

uint8_t sensor_data_report = 0;
volatile uint32_t sensor_data_valid = 0;
uint32_t temp = 0;
uint32_t hum = 0;
float temp_last = 0.0;
float hum_last = 0.0;
#if CONFIG_UPDATE_WEATHER_DATA
uint32_t measure_times = 0;
uint32_t weather_data_update = 0;
#endif

static uint8_t aht21b_crc8(const uint8_t *data, size_t length)
{
    uint8_t crc = 0xFF;
    for (size_t i = 0; i < length; i++) {
        crc ^= data[i];
        for (uint8_t bit = 0; bit < 8; bit++) {
            crc = (crc & 0x80) ? (crc << 1) ^ 0x31 : crc << 1;
        }
    }
    return crc;
}

static bool lp_read_aht21b_data(float *temperature, float *humidity)
{
    uint8_t data_write[3] = { 0xAC, 0x33, 0x00 };
    if (lp_core_i2c_master_write_to_device(LP_I2C_NUM_0, AHT21B_SENSOR_I2C_ADDR,
                                            data_write, sizeof(data_write),
                                            LP_I2C_TRANS_TIMEOUT_CYCLES) != ESP_OK) {
        return false;
    }

    /* AHT21B needs up to 80 ms to complete a triggered measurement. */
    ulp_lp_core_delay_us(AHT21B_MEASUREMENT_TIME_US);

    uint8_t buf[7] = { 0 };
    if (lp_core_i2c_master_read_from_device(LP_I2C_NUM_0, AHT21B_SENSOR_I2C_ADDR,
                                             buf, sizeof(buf),
                                             LP_I2C_TRANS_TIMEOUT_CYCLES) != ESP_OK) {
        return false;
    }
    /* Accept only an idle, calibrated frame with a valid CRC. */
    if ((buf[0] & AHT21B_STATUS_BUSY) != 0 ||
        (buf[0] & AHT21B_STATUS_CALIBRATED) == 0 ||
        aht21b_crc8(buf, 6) != buf[6]) {
        return false;
    }

    uint32_t raw_data = ((uint32_t)buf[1] << 12) |
                        ((uint32_t)buf[2] << 4) |
                        (buf[3] >> 4);
    *humidity = raw_data * 100.0f / 1048576;

    raw_data = ((uint32_t)(buf[3] & 0x0F) << 16) |
               ((uint32_t)buf[4] << 8) |
               buf[5];
    *temperature = raw_data * 200.0f / 1048576 - 50;
    return true;
}

int main(void)
{
#if CONFIG_UPDATE_WEATHER_DATA
    bool weather_data_pending = false;
    measure_times++;
    /* Wake up the CPU every 3 hours to fetch weather information. */
    if (measure_times == WEATHER_UPDATE_INTERVAL_TIMES) {
        weather_data_update = 1;
        weather_data_pending = true;
    }
#endif
    if (sensor_data_report == 0) {
        float sample_temp = 0.0f;
        float sample_hum = 0.0f;
        sensor_data_valid = 0;
        if (lp_read_aht21b_data(&sample_temp, &sample_hum)) {
            float *temp_f = (float *)&temp;
            float *hum_f = (float *)&hum;
            *temp_f = sample_temp;
            *hum_f = sample_hum;
            sensor_data_valid = 1;
            if (fabsf(sample_temp - temp_last) > TEMP_CHANGE_THRESHOLD ||
                fabsf(sample_hum - hum_last) > HUM_CHANGE_THRESHOLD) {
                temp_last = sample_temp;
                hum_last = sample_hum;
                sensor_data_report = 1;
#ifdef CONFIG_ULP_UART_PRINT
                lp_core_printf("temp: %d %d, hum: %d %d\r\n", (int)sample_temp, (int)temp_last,
                               (int)sample_hum, (int)hum_last);
#endif
#if CONFIG_UPDATE_WEATHER_DATA
                weather_data_pending = false;
#endif
                ulp_lp_core_wakeup_main_processor();
            }
        }
    }
#if CONFIG_UPDATE_WEATHER_DATA
    if (weather_data_pending) {
        ulp_lp_core_wakeup_main_processor();
    }
#endif
    return 0;
}
