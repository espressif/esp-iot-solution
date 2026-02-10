/*
 * SPDX-FileCopyrightText: 2024-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "stdint.h"

#ifdef __cplusplus
extern "C" {
#endif

enum ina236_reg_addr {
    INA236_REG_CFG         = 0x00,
    INA236_REG_VSHUNT      = 0x01,
    INA236_REG_VBUS        = 0x02,
    INA236_REG_POWER       = 0x03,
    INA236_REG_CURRENT     = 0x04,
    INA236_REG_CALIBRATION = 0x05,
    INA236_REG_MASK        = 0x06,
    INA236_REG_ALERT_LIM   = 0x07,
    INA236_REG_MAF_ID      = 0x3E,
    INA236_REG_DEVID       = 0x3F,
};

typedef union {
    struct __attribute__((packed)) {
        uint16_t mode: 3;       //111 - Continuous shunt and bus voltage
        uint16_t vshct: 3;      //100 - 1100us
        uint16_t vbusct: 3;     //100 - 1100us
        uint16_t avg: 3;        //000 - 1
        uint16_t adcrange: 1;   //0
        uint16_t reserved: 2;
        uint16_t rst: 1;
    } bit;
    uint16_t all;
} ina236_reg_cfg_t;

typedef union {
    uint16_t vshunt;
} ina236_reg_vshunt_t;

typedef union {
    struct __attribute__((packed)) {
        uint16_t vbus: 15;
        uint16_t reserved: 1;
    } bit;
    uint16_t all;
} ina236_reg_vbus_t;

typedef union {
    uint16_t power;
} ina236_reg_power_t;

typedef union {
    uint16_t current;
} ina236_reg_current_t;

typedef union {
    struct __attribute__((packed)) {
        uint16_t shunt_cal: 15; //11 - 0.0564
        uint16_t reserved:   1;
    } bit;
    uint16_t all;
} ina236_reg_calibration_t;

typedef union {
    struct __attribute__((packed)) {
        uint16_t len:       1; //0
        uint16_t apol:      1; //0
        uint16_t ovf:       1; //0
        uint16_t cvrf:      1; //0
        uint16_t aff:       1; //0
        uint16_t memerror:  1; //0
        uint16_t reserved:  4; //0
        uint16_t cnvr:      1; //0
        uint16_t pol:       1; //0
        uint16_t bul:       1; //0
        uint16_t bol:       1; //0
        uint16_t sul:       1; //0
        uint16_t sol:       1; //0
    } bit;
    uint16_t all;
} ina236_reg_mask_t;

typedef union {
    uint16_t lim;
} ina236_reg_alert_lim_t;

typedef struct {
    ina236_reg_cfg_t cfg;
    ina236_reg_vshunt_t vshunt;
    ina236_reg_vbus_t vbus;
    ina236_reg_power_t power;
    ina236_reg_current_t current;
    ina236_reg_calibration_t calibration;
    ina236_reg_mask_t mask;
    ina236_reg_alert_lim_t lim;
    uint16_t maf_id;
    uint16_t devid_id;
} ina236_reg_t;

#ifdef __cplusplus
}
#endif
