/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "esp_xiaozhi_info.h"

esp_err_t esp_xiaozhi_info_parse_response(const char *data, size_t length, esp_xiaozhi_chat_info_t *info);
esp_err_t esp_xiaozhi_info_build_activation(const esp_xiaozhi_chat_info_t *info,
                                            const esp_xiaozhi_chat_bootstrap_config_t *config, char **payload);
