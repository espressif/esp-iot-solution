/*
  * ESPRESSIF MIT License
  *
  * Copyright (c) 2017 <ESPRESSIF SYSTEMS (SHANGHAI) PTE LTD>
  *
  * Permission is hereby granted for use on ESPRESSIF SYSTEMS products only, in which case,
  * it is free of charge, to any person obtaining a copy of this software and associated
  * documentation files (the "Software"), to deal in the Software without restriction, including
  * without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense,
  * and/or sell copies of the Software, and to permit persons to whom the Software is furnished
  * to do so, subject to the following conditions:
  *
  * The above copyright notice and this permission notice shall be included in all copies or
  * substantial portions of the Software.
  *
  * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
  * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
  * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
  * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
  * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
  *
  */
#ifndef __ONENET_H__
#define __ONENET_H__

#define ONENET_HOST         "183.230.40.39"
#define ONENET_PORT         (6002)

/* Here needs to be changed accoding to your oneONET configure. */
#define ONENET_DEVICE_ID    CONFIG_DEVICE_ID                    // mqtt client id
#define ONENET_PROJECT_ID   CONFIG_PROJECT_ID                   // mqtt username
#define ONENET_AUTH_INFO    CONFIG_AUTH_INFO                    // mqtt password
#define ONENET_DATA_STREAM  CONFIG_DATA_STREAM                  // datastream name

#define ONENET_PUB_INTERVAL (60)                         // unit: s

/* Supported format when publish data point to oneNET */
enum mqtt_save_data_type {
    data_type_full_json = 0x01,
    data_type_bin = 0x02,
    data_type_simple_json_without_time = 0x03,
    data_type_simple_json_with_time = 0x04,
    data_type_string = 0x05,
    data_type_string_with_time = 0x06,
    data_type_float  = 0x07
};

void connected_cb(void *self, void *params);

void disconnected_cb(void *self, void *params);

void reconnect_cb(void *self, void *params);

void subscribe_cb(void *self, void *params);

void publish_cb(void *self, void *params);

void data_cb(void *self, void *params);

#endif /* __ONENET_H__ */
