/*
 * SPDX-FileCopyrightText: 2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */
/* Compressed OTA example, common declarations

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#pragma once

#include "esp_partition.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t example_storage_compressed_image(const esp_partition_t *update_partition);

#if CONFIG_EXAMPLE_BLE_DOWNLOAD

void ble_init(void);

#endif

#ifdef __cplusplus
}
#endif
