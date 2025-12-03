/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include "esp_err.h"
#include "esp32_port.h"
#include "esp_loader.h"

/**
 * @brief Initialize the serial flasher
 *
 * @param config Serial flasher configuration
 * @return esp_err_t ESP_OK if successful, otherwise ESP_FAIL
 */
esp_err_t app_serial_flasher_init(const loader_esp32_config_t *config);
