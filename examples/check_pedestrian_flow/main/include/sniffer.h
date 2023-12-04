/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef __SNIFFER_H__
#define __SNIFFER_H__

typedef struct {
    uint8_t header[4];
    uint8_t dest_mac[6];
    uint8_t source_mac[6];
    uint8_t bssid[6];
    uint8_t payload[0];
} sniffer_payload_t;

typedef struct station_info {
    uint8_t  bssid[6];
    int8_t   rssi;
    uint8_t  channel;
    uint32_t timestamp;
    struct   station_info *next;
} station_info_t;

#endif
