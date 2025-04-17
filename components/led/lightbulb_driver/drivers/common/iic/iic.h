/*
 * SPDX-FileCopyrightText: 2022-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#if __has_include("esp_idf_version.h")
#include "esp_idf_version.h"
#endif

// Need to include this commit: https://github.com/espressif/esp-idf/commit/a245a316a518215c5655ffd2fe24fc920044b094
#if (((ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 2, 6)) && (ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(5, 3, 0))) || \
    ((ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 3, 3)) && (ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(5, 4, 0))) || \
    (ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 4, 1))) && \
    (CONFIG_LB_ENABLE_NEW_IIC_DRIVER)
#define ENABLE_NEW_IIC_DRIVER 1
#else
#define ENABLE_NEW_IIC_DRIVER 0
#endif

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>
#include "freertos/timers.h"
#include "driver_utils.h"
#if ENABLE_NEW_IIC_DRIVER
#include "driver/i2c_master.h"
#else
#include "driver/i2c.h"
#endif

esp_err_t iic_driver_init(i2c_port_t i2c_master_num, gpio_num_t sda_io_num, gpio_num_t scl_io_num, uint32_t clk_freq_hz);
esp_err_t iic_driver_write(uint8_t addr, uint8_t *data_wr, size_t size);
esp_err_t iic_driver_send_task_create(void);
esp_err_t iic_driver_task_destroy(void);
esp_err_t iic_driver_deinit(void);
