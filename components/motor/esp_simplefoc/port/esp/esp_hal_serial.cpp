/*
 * SPDX-FileCopyrightText: 2023-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "esp_hal_serial.h"

HardwareSerial Serial;

/**
 * @description: HardwareSerial constructor.
 * @return {*}
 */
HardwareSerial::HardwareSerial()
{
    _rxPin = -1;
    _txPin = -1;
    _ctsPin = -1;
    _rtsPin = -1;
    _rxBufferSize = 256;
    _txBufferSize = 0;
}

/**
 * @description: HardwareSerial destructor.
 * @return {*}
 */
HardwareSerial::~HardwareSerial()
{

    end();
}

/**
 * @description: Return hardwareSerial rx buffer size.
 * @return {*}
 */
int HardwareSerial::getRxBuffer()
{
    return _rxBufferSize;
}

/**
 * @description: Return harewareSerial uart_port.
 * @return {*}
 */
uart_port_t HardwareSerial::getUartPort()
{
    return _uart_num;
}

/**
 * @description: HardwareSerial begin function. support separate setting of serial port and IO.
 * @param {unsigned long} baud
 * @param {uart_port_t} uart_num
 * @param {int} tx_io
 * @param {int} rx_io
 * @param {uart_word_length_t} wordLength
 * @param {uart_parity_t} parity
 * @param {uart_stop_bits_t} stopBits
 * @param {int8_t} rxPin
 * @param {int8_t} txPin
 * @param {bool} invert
 * @param {unsigned long} timeout_ms
 * @param {uint8_t} rxfifo_full_thrhd
 * @return {*}
 */
void HardwareSerial::begin(unsigned long baud, uart_port_t uart_num, int tx_io, int rx_io, uart_word_length_t wordLength, uart_parity_t parity, uart_stop_bits_t stopBits, int8_t rxPin, int8_t txPin, bool invert, unsigned long timeout_ms, uint8_t rxfifo_full_thrhd)
{
    _uart_num = uart_num;
    if (!uart_is_driver_installed(_uart_num))
    {
        _uart_config.baud_rate = baud;
        _uart_config.data_bits = wordLength;
        _uart_config.parity = parity;
        _uart_config.stop_bits = stopBits;
        _uart_config.flow_ctrl = UART_HW_FLOWCTRL_DISABLE;
        _uart_config.source_clk = UART_SCLK_DEFAULT;

        uart_driver_install(_uart_num, _rxBufferSize * 2, 0, 0, NULL, 0);
        uart_param_config(_uart_num, &_uart_config);
        uart_set_pin(_uart_num, tx_io, rx_io, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    }
}

/**
 * @description: Delete hardwareSerial
 * @return {*}
 */
void HardwareSerial::end()
{
    if (uart_is_driver_installed(_uart_num))
    {
        uart_driver_delete(_uart_num);
    }
}

/**
 * @description: Get hardwareSerial buffer data size.
 * @return {*}
 */
int HardwareSerial::available()
{
    size_t available;

    uart_get_buffered_data_len(_uart_num, &available);

    return available;
}

/**
 * @description: Empty function.
 * @return {*}
 */
int HardwareSerial::peek()
{
    return 1;
}

/**
 * @description: HardwareSerial write function.
 * @param {uint8_t} c
 * @return {*}
 */
size_t HardwareSerial::write(uint8_t c)
{
    uart_write_bytes(_uart_num, &c, 1);
    return 1;
}

/**
 * @description: HardwareSerial write function.
 * @param {uint8_t} *buffer
 * @param {size_t} size
 * @return {*}
 */
size_t HardwareSerial::write(const uint8_t *buffer, size_t size)
{
    uart_write_bytes(_uart_num, buffer, size);
    return size;
}

/**
 * @description: HardwareSerial read function.
 * @return {*}
 */
int HardwareSerial::read()
{
    uint8_t c = 0;
    if (uart_read_bytes(_uart_num, &c, 1, 10 / portTICK_PERIOD_MS) == 1)
    {
        return c;
    }
    return -1;
}

/**
 * @description: HardwareSerial read function.
 * @param {uint8_t} *buffer
 * @param {size_t} size
 * @return {*}
 */
size_t HardwareSerial::read(uint8_t *buffer, size_t size)
{
    return uart_read_bytes(_uart_num, buffer, size, 10 / portTICK_PERIOD_MS);
}

/**
 * @description: HardwareSerial read function.
 * @param {uint8_t} *buffer
 * @param {size_t} length
 * @return {*}
 */
size_t HardwareSerial::readBytes(uint8_t *buffer, size_t length)
{
    return uart_read_bytes(_uart_num, buffer, length, 10 / portTICK_PERIOD_MS);
}