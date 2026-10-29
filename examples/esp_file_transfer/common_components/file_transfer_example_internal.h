/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "esp_err.h"
#include "esp_gmp.h"

esp_err_t file_transfer_example_storage_init(void);
esp_err_t file_transfer_example_console_init(void);
bool file_transfer_example_runtime_lock(void);
void file_transfer_example_runtime_unlock(void);
bool file_transfer_example_runtime_ready_locked(void);
esp_gmp_link_t file_transfer_example_runtime_link_locked(void);
