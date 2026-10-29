/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint32_t report_id;
    union {
        struct {
            uint8_t modifier;
            uint8_t reserved;
            uint8_t keycode[15];
        } keyboard_full_key_report;
        struct {
            uint8_t modifier;
            uint8_t reserved;
            uint8_t keycode[6];
        } keyboard_report;
        struct {
            uint16_t keycode;
        } consumer_report;
    };
} hid_report_t;

#ifdef __cplusplus
}
#endif
