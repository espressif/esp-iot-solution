/*
 * SPDX-FileCopyrightText: 2025-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include "esp_err.h"
#include "driver/gpio.h"
#include "driver/uart.h"

/** UART connection to the target chip. */
typedef struct {
    uint32_t baud_rate;         /*!< Initial baud rate */
    uart_port_t uart_port;      /*!< UART peripheral connected to the target */
    gpio_num_t uart_rx_pin;     /*!< UART RX pin */
    gpio_num_t uart_tx_pin;     /*!< UART TX pin */
    gpio_num_t reset_pin;       /*!< GPIO controlling the target reset (EN) pin */
    gpio_num_t boot_pin;        /*!< GPIO controlling the target BOOT (GPIO0) pin */
} app_serial_flasher_config_t;

/** Flash the embedded receiver firmware over UART. */
esp_err_t app_serial_flasher_init(const app_serial_flasher_config_t *config);
