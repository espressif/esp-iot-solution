/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef __ONENET_H__
#define __ONENET_H__
#include "mqtt_client.h"
#define ONENET_HOST         "183.230.40.39"
#define ONENET_PORT         (6002)

/* Here needs to be changed according to your oneONET configure. */
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

void onenet_start(esp_mqtt_client_handle_t client);

void onenet_stop(esp_mqtt_client_handle_t client);

#endif /* __ONENET_H__ */
