/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <stdint.h>

typedef union {
    uint16_t code;
    struct {
        uint16_t hardware_current_limit : 1;
        uint16_t speed_abnormal : 1;
        uint16_t kt_abnormal : 1;
        uint16_t no_motor : 1;
        uint16_t stuck_in_open_loop : 1;
        uint16_t stuck_in_closed_loop : 1;
        uint16_t reserved : 1;
        uint16_t ldo_3v3_undervoltage : 1;
        uint16_t vcc_undervoltage : 1;
        uint16_t vreg_undervoltage : 1;
        uint16_t charge_pump_undervoltage : 1;
        uint16_t overcurrent : 1;
        uint16_t vreg_overcurrent : 1;
        uint16_t vcc_upper_limit : 1;
        uint16_t temperature_over_warning_limit : 1;
        uint16_t temperature_over_limit : 1;
    };
} drv10987_fault_t;
