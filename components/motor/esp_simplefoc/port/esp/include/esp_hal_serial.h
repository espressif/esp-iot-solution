/*
 * SPDX-FileCopyrightText: 2023-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include "Stream.h"

#include "driver/gpio.h"
#include "driver/uart.h"
#include "soc/soc_caps.h"

/**
 * ESP32-S-Devkitc default serial port: tx:1 rx:3
 * ESP32-S3-Devkitc default serial port: tx:43 rx:44
 */

#if CONFIG_IDF_TARGET_ESP32
#define TX0 1
#define RX0 3
#elif CONFIG_IDF_TARGET_ESP32S3
#define TX0 43
#define RX0 44
#endif

class HardwareSerial : public Stream
{
public:
    HardwareSerial();
    ~HardwareSerial();
    void begin(unsigned long baud, uart_port_t uart_num = CONFIG_ESP_CONSOLE_UART_NUM, int tx_io = TX0, int rx_io = RX0, uart_word_length_t wordLength = UART_DATA_8_BITS, uart_parity_t parity = UART_PARITY_DISABLE, uart_stop_bits_t stopBits = UART_STOP_BITS_1, int8_t rxPin = -1, int8_t txPin = -1, bool invert = false, unsigned long timeout_ms = 20000UL, uint8_t rxfifo_full_thrhd = 112);
    void end();
    int available(void);
    int peek(void);
    int getRxBuffer(void);
    uart_port_t getUartPort();
    size_t write(uint8_t);
    size_t write(const uint8_t *buffer, size_t size);
    inline size_t write(const char *buffer, size_t size)
    {
        return write((uint8_t *)buffer, size);
    }
    inline size_t write(const char *s)
    {
        return write((uint8_t *)s, strlen(s));
    }
    inline size_t write(unsigned long n)
    {
        return write((uint8_t)n);
    }
    inline size_t write(long n)
    {
        return write((uint8_t)n);
    }
    inline size_t write(unsigned int n)
    {
        return write((uint8_t)n);
    }
    inline size_t write(int n)
    {
        return write((uint8_t)n);
    }

    int read(void);
    size_t read(uint8_t *buffer, size_t size);
    size_t readBytes(uint8_t *buffer, size_t length);
    size_t readBytes(char *buffer, size_t length)
    {
        return readBytes((uint8_t *)buffer, length);
    }

private:
    int8_t _rxPin, _txPin, _ctsPin, _rtsPin;
    uart_port_t _uart_num;
    size_t _rxBufferSize;
    size_t _txBufferSize;
    uart_config_t _uart_config;
};

extern HardwareSerial Serial;
