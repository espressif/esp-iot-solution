/*
 * SPDX-FileCopyrightText: 2023-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "Arduino.h"
#include "Stream.h"


// private method to read stream with timeout
int Stream::timedRead()
{
    int c;
    _startMillis = millis();
    do {
        c = read();
        if(c >= 0) {
            return c;
        }
    } while(millis() - _startMillis < _timeout);
    return -1;     // -1 indicates timeout
}

size_t Stream::readBytes(char *buffer, size_t length)
{
    size_t count = 0;
    while(count < length) {
        int c = timedRead();
        if(c < 0) {
            break;
        }
        *buffer++ = (char) c;
        count++;
    }
    return count;
}