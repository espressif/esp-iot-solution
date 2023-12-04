/*
 * SPDX-FileCopyrightText: 2022-2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "driver/i2c.h"
#pragma once

esp_err_t iic_driver_init(i2c_port_t i2c_master_num, gpio_num_t sda_io_num, gpio_num_t scl_io_num, uint32_t clk_freq_hz);
esp_err_t iic_driver_write(uint8_t addr, uint8_t *data_wr, size_t size);
esp_err_t iic_driver_send_task_create(void);
esp_err_t iic_driver_task_destroy(void);
esp_err_t iic_driver_deinit(void);
