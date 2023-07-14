/*
 * SPDX-FileCopyrightText: 2023-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <inttypes.h>
#include "Print.h"

class Stream : public Print
{
protected:
    unsigned long _timeout;     // number of milliseconds to wait for the next char before aborting timed read
    unsigned long _startMillis; // used for timeout measurement
    int timedRead();            // private method to read stream with timeout

public:
    virtual int available() = 0;
    virtual int read() = 0;

    Stream() : _startMillis(0)
    {
        _timeout = 1000;
    }
    virtual ~Stream() {}

    void setTimeout(unsigned long timeout); // sets maximum milliseconds to wait for stream data, default is 1 second
    unsigned long getTimeout(void);

    virtual size_t readBytes(char *buffer, size_t length); // read chars from stream into buffer
    virtual size_t readBytes(uint8_t *buffer, size_t length)
    {
        return readBytes((char *)buffer, length);
    }
};
