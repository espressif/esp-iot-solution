// Copyright 2020-2022 Espressif Systems (Shanghai) PTE LTD
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "driver/i2c.h"
#pragma once

esp_err_t iic_driver_init(i2c_port_t i2c_master_num, gpio_num_t sda_io_num, gpio_num_t scl_io_num, uint32_t clk_freq_hz);
esp_err_t iic_driver_write(uint8_t addr, uint8_t *data_wr, size_t size);
esp_err_t iic_driver_send_task_create(void);
esp_err_t iic_driver_task_destroy(void);
esp_err_t iic_driver_deinit(void);
