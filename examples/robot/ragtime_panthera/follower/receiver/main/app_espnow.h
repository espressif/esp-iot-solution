/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include "driver/uart.h"

typedef struct {
    uint8_t *data;
    int len;
} espnow_recv_data_t;

/**
 * @brief Initialize the ESPNOW library
 *
 */
void app_espnow_init();
