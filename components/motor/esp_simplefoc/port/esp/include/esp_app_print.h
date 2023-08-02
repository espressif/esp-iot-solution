/*
 * SPDX-FileCopyrightText: 2023-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <stdint.h>
#include <stddef.h>
#include <string>
#include <sstream>
#include <string.h>

class Print {
public:
    /**
     * @brief Construct a new Print object
     *
     */
    Print() {}

    /**
     * @brief Destroy the Print object
     *
     */
    virtual ~Print() {}

    /**
     * @brief Write character.
     *
     * @return character size
     */
    virtual size_t write(uint8_t) = 0;

    /**
     * @brief Write buffer.
     *
     * @param str buffer
     * @return buffer size
     */
    size_t write(const char *str);

    /**
     * @brief Write buffer.
     *
     * @param buffer buffer
     * @param size buffer size
     * @return buffer size
     */
    virtual size_t write(const uint8_t *buffer, size_t size);

    /**
     * @brief Write buffer.
     *
     * @param buffer buffer
     * @param size buffer size
     * @return buffer size
     */
    size_t write(const char *buffer, size_t size);

    /**
     * @brief Print different types of data.
     *
     * @tparam T
     * @param value data
     * @return length of the converted string
     */
    template <typename T>
    size_t print(T value)
    {
        std::stringstream ss;
        ss << value;
        return write(ss.str().c_str());
    }

    /**
     * @brief Print data of type double.
     *
     * @return length of the converted string
     */
    size_t print(double, int = 2);

    /**
     * @brief Print different types of data and newline.
     *
     * @tparam T
     * @param value data
     * @return length of the converted string
     */
    template <typename T>
    size_t println(T value)
    {
        std::stringstream ss;
        ss << value << "\r\n";
        return write(ss.str().c_str());
    }

    /**
     * @brief Print data of type double.
     *
     * @return length of the converted string
     */
    size_t println(double, int = 2);

    /**
     * @brief Print newline.
     *
     * @return length of "\r\n"
     */
    size_t println(void);
};

class Stream : public Print {
protected:
    unsigned long _timeout;
    unsigned long _startMillis;
    int timedRead();

public:
    /**
     * @brief Whether the RX buffer has data.
     *
     * @return
     *     - 0 have data
     *     - 1 no data
     */
    virtual int available() = 0;

    /**
     * @brief Read serial data.
     *
     * @return data size
     */
    virtual int read() = 0;

    /**
     * @brief Construct a new Stream object.
     *
     */
    Stream();

    /**
     * @brief Destroy the Stream object.
     *
     */
    virtual ~Stream() {}

    /**
     * @brief Set timeout.
     *
     * @param timeout timeout
     */
    void setTimeout(unsigned long timeout);

    /**
     * @brief Get timeout.
     *
     * @return timeout
     */
    unsigned long getTimeout(void);

    /**
     * @brief Read bytes.
     *
     * @param buffer pointer to the buffer.
     * @param length data length
     * @return data length
     */
    virtual size_t readBytes(char *buffer, size_t length);

    /**
     * @brief Read bytes.
     *
     * @param buffer pointer to the buffer.
     * @param length data length
     * @return data length
     */
    virtual size_t readBytes(uint8_t *buffer, size_t length);
};
