/*
 * SPDX-FileCopyrightText: 2023-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include "esp_app_print.h"

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

class HardwareSerial : public Stream {
public:
    /**
     * @brief Construct a new Hardware Serial object
     *
     */
    HardwareSerial();

    /**
     * @brief Destroy the Hardware Serial object
     *
     */
    ~HardwareSerial();

    /**
     * @brief HardwareSerial begin function, support separate setting of serial port and IO.
     *
     * @param baud uart baud rate.
     * @param uart_num uart port number, the max port number is (UART_NUM_MAX -1).
     * @param tx_io uart TX pin GPIO number.
     * @param rx_io uart RX pin GPIO number.
     * @param wordLength  uart word length constants
     * @param parity uart parity constants
     * @param stopBits uart stop bits number
     */
    void begin(unsigned long baud, uart_port_t uart_num = CONFIG_ESP_CONSOLE_UART_NUM, int tx_io = TX0, int rx_io = RX0, uart_word_length_t wordLength = UART_DATA_8_BITS, uart_parity_t parity = UART_PARITY_DISABLE, uart_stop_bits_t stopBits = UART_STOP_BITS_1);

    /**
     * @brief Delete hardwareSerial.
     *
     */
    void end();

    /**
     * @brief Whether the RX buffer has data.
     *
     * @return
     *     - 0 have data
     *     - 1 no data
     */
    int available(void);

    /**
     * @brief Empty Function.
     *
     * @return always return 1
     * @note this function does no processing.
     */
    int peek(void);

    /**
     * @brief Get rx buffer size.
     *
     * @return buffer size
     */
    int getRxBuffer(void);

    /**
     * @brief Get uart port.
     *
     * @return uart port
     */
    uart_port_t getUartPort();

    /**
     * @brief Send data to the UART port from a given character.
     *
     * @return
     *     - (-1) Parameter error
     *     - OTHERS (>=0) The number of bytes pushed to the TX FIFO
     */
    size_t write(uint8_t);

    /**
     * @brief Send data to the UART port from a given buffer and length.
     *
     * @param buffer data buffer address
     * @param size data length to send
     * @return
     *     - (-1) Parameter error
     *     - OTHERS (>=0) The number of bytes pushed to the TX FIFO
     */
    size_t write(const uint8_t *buffer, size_t size);

    /**
     * @brief Send data to the UART port from a given buffer and length.
     *
     * @param buffer data buffer address
     * @param size data length to send
     * @return
     *     - (-1) Parameter error
     *     - OTHERS (>=0) The number of bytes pushed to the TX FIFO
     */
    inline size_t write(const char *buffer, size_t size)
    {
        return write((uint8_t *)buffer, size);
    }

    /**
     * @brief Send data to the UART port from a given buffer.
     *
     * @param buffer data buffer address
     * @return
     *     - (-1) Parameter error
     *     - OTHERS (>=0) The number of bytes pushed to the TX FIFO
     */
    inline size_t write(const char *s)
    {
        return write((uint8_t *)s, strlen(s));
    }

    /**
     * @brief Send data to the UART port from a given data.
     *
     * @param n data
     * @return data length
     */
    inline size_t write(unsigned long n)
    {
        return write((uint8_t)n);
    }

    /**
     * @brief Send data to the UART port from a given data.
     *
     * @param n data
     * @return data length
     */
    inline size_t write(long n)
    {
        return write((uint8_t)n);
    }

    /**
     * @brief Send data to the UART port from a given data.
     *
     * @param n data
     * @return data length
     */
    inline size_t write(unsigned int n)
    {
        return write((uint8_t)n);
    }

    /**
     * @brief Send data to the UART port from a given data.
     *
     * @param n data
     * @return data length
     */
    inline size_t write(int n)
    {
        return write((uint8_t)n);
    }

    /**
     * @brief UART read byte from UART buffer.
     *
     * @return
     *     - (-1) Error
     *     - OTHERS (>=0) The number of bytes read from UART buffer
     */
    int read(void);

    /**
     * @brief UART read bytes from UART buffer.
     *
     * @param buffer pointer to the buffer
     * @param size data length
     * @return
     *     - (-1) Error
     *     - OTHERS (>=0) The number of bytes read from UART buffer
     */
    size_t read(uint8_t *buffer, size_t size);

    /**
     * @brief UART read bytes from UART buffer.
     *
     * @param buffer pointer to the buffer
     * @param length data length
     * @return
     *     - (-1) Error
     *     - OTHERS (>=0) The number of bytes read from UART buffer
     */
    size_t readBytes(uint8_t *buffer, size_t length);

    /**
     * @brief UART read bytes from UART buffer.
     *
     * @param buffer pointer to the buffer
     * @param length data length
     * @return
     *     - (-1) Error
     *     - OTHERS (>=0) The number of bytes read from UART buffer
     */
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
