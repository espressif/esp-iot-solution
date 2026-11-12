/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once
#include <stdbool.h>
bool esp_ptr_internal(const void *);
bool esp_ptr_dma_capable(const void *);

bool esp_ptr_dma_ext_capable(const void *);
