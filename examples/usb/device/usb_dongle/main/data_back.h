/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdbool.h>
#include <stddef.h>

#define ENABLE_FLUSH  1
#define DISABLE_FLUSH 0

void esp_data_back(void* data_buf, size_t length, bool flush);
