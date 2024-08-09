/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#define SET_BITS(VALUE, MASK, SHIFT) ((VALUE & MASK) << SHIFT)

typedef enum {
    CONFIG1_REGISTER_ADDR = 0x90,
    CONFIG2_REGISTER_ADDR,
    CONFIG3_REGISTER_ADDR,
    CONFIG4_REGISTER_ADDR,
    CONFIG5_REGISTER_ADDR,
    CONFIG6_REGISTER_ADDR,
    CONFIG7_REGISTER_ADDR,
} drv10987_config_register_addr_t;

typedef enum {
    EEPROM_NO_READY = 0,
    EEPROM_READY,
} drv10987_eeprom_status_t;

typedef union {
    uint16_t result;
    struct {
        uint16_t rm_value : 4;
        uint16_t rm_shift : 3;
        uint16_t clk_cycle_adjust : 1;
        uint16_t fg_cycle : 4;
        uint16_t fg_open_loop_sel : 2;
        uint16_t ssm_config : 2;
    };
} drv10987_config1_t;

typedef union {
    uint16_t result;
    struct {
        uint16_t tctrladv : 4;
        uint16_t tctrladv_shift : 3;
        uint16_t commadv_mode : 1;
        uint16_t kt_value : 4;
        uint16_t kt_shift : 4;
    };
} drv10987_config2_t;

typedef union {
    uint16_t result;
    struct {
        uint16_t brkdone_thr : 3;
        uint16_t oplcurr_rt : 3;
        uint16_t openl_curr : 2;
        uint16_t rvsd_thr : 2;
        uint16_t rvsd_en : 1;
        uint16_t isd_en : 1;
        uint16_t bemf_hys : 1;
        uint16_t brkcur_thrsel : 1;
        uint16_t isd_thr : 2;
    };
} drv10987_config3_t;

typedef union {
    uint16_t result;
    struct {
        uint16_t align_time : 3;
        uint16_t op2cls_thr : 5;
        uint16_t st_accel : 3;
        uint16_t st_accel2 : 3;
        uint16_t accel_range_sel : 2;
    };
} drv10987_config4_t;

typedef union {
    uint16_t result;
    struct {
        uint16_t ipdashwi_limit : 1;
        uint16_t hwi_limit_thr : 3;
        uint16_t swi_limit_thr : 4;
        uint16_t lock_en0 : 1;
        uint16_t lock_en1 : 1;
        uint16_t lock_en2 : 1;
        uint16_t lock_en3 : 1;
        uint16_t lock_en4 : 1;
        uint16_t lock_en5 : 1;
        uint16_t ot_warning_limit : 2;
    };
} drv10987_config5_t;

typedef union {
    uint16_t result;
    struct {
        uint16_t slew_rate : 2;
        uint16_t duty_cycle_limit : 2;
        uint16_t cl_slp_accel : 3;
        uint16_t cl_op_dis : 1;
        uint16_t ipd_rl_smd : 1;
        uint16_t avs_mmd : 1;
        uint16_t avs_en : 1;
        uint16_t avs_ind_en : 1;
        uint16_t kt_lck_thr : 2;
        uint16_t pwm_freq : 1;
        uint16_t spd_ctrl_md : 1;
    };
} drv10987_config6_t;

typedef union {
    uint16_t result;
    struct {
        uint16_t dead_time : 5;
        uint16_t ctrl_coef : 3;
        uint16_t ipd_clk : 2;
        uint16_t ipd_curr_thr : 4;
        uint16_t ipd_advc_ag : 2;
    };
} drv10987_config7_t;

typedef struct {
    drv10987_config1_t config1;
    drv10987_config2_t config2;
    drv10987_config3_t config3;
    drv10987_config4_t config4;
    drv10987_config5_t config5;
    drv10987_config6_t config6;
    drv10987_config7_t config7;
} drv10987_config_reg_t;

#define DEFAULT_DRV10987_CONFIG1  \
    {                             \
        .rm_value = 0x0D,         \
        .rm_shift = 0x03,         \
        .clk_cycle_adjust = 0x00, \
        .fg_cycle = 0x00,         \
        .fg_open_loop_sel = 0x00, \
        .ssm_config = 0x00}

#define DEFAULT_DRV10987_CONFIG2 \
    {                            \
        .tctrladv = 0x00,        \
        .tctrladv_shift = 0x00,  \
        .commadv_mode = 0x00,    \
        .kt_value = 0x09,        \
        .kt_shift = 0x03}

#define DEFAULT_DRV10987_CONFIG3 \
    {                            \
        .brkdone_thr = 0x00,     \
        .oplcurr_rt = 0x00,      \
        .openl_curr = 0x03,      \
        .rvsd_thr = 0X00,        \
        .rvsd_en = 0X00,         \
        .isd_en = 0X00,          \
        .bemf_hys = 0x00,        \
        .brkcur_thrsel = 0x00,   \
        .isd_thr = 0x00}

#define DEFAULT_DRV10987_CONFIG4 \
    {                            \
        .align_time = 0X02,      \
        .op2cls_thr = 0x11,      \
        .st_accel = 0x07,        \
        .st_accel2 = 0x06,       \
        .accel_range_sel = 0X00}

#define DEFAULT_DRV10987_CONFIG5 \
    {                            \
        .ipdashwi_limit = 0x01,  \
        .hwi_limit_thr = 0X07,   \
        .swi_limit_thr = 0X00,   \
        .lock_en0 = 0x01,        \
        .lock_en1 = 0X01,        \
        .lock_en2 = 0x00,        \
        .lock_en3 = 0x01,        \
        .lock_en4 = 0x01,        \
        .lock_en5 = 0x01,        \
        .ot_warning_limit = 0x00}

#define DEFAULT_DRV10987_CONFIG6  \
    {                             \
        .slew_rate = 0x00,        \
        .duty_cycle_limit = 0x00, \
        .cl_slp_accel = 0x04,     \
        .cl_op_dis = 0x00,        \
        .ipd_rl_smd = 0x00,       \
        .avs_mmd = 0x00,          \
        .avs_en = 0x00,           \
        .avs_ind_en = 0x01,       \
        .kt_lck_thr = 0x03,       \
        .pwm_freq = 0x01,         \
        .spd_ctrl_md = 0x00}

#define DEFAULT_DRV10987_CONFIG7 \
    {                            \
        .dead_time = 0x1A,       \
        .ctrl_coef = 0x03,       \
        .ipd_clk = 0x00,         \
        .ipd_curr_thr = 0x00,    \
        .ipd_advc_ag = 0x00}

#define MTR_DIS_MASK 0x01
#define EEACCMODE_MASK 0x03
#define EEWRNEN_MASK 0x01
#define EEREFRESH_MASK 0x01
#define SHADOWREGEN_MASK 0x01

#define EECTRL_REGISTER_VALUE(MTR_DIS) \
    (SET_BITS(MTR_DIS, MTR_DIS_MASK, 15))

#define EEPROM_PROGRAMMING5_REGISTER_VALUE(EEACCMODE, EEWRNEN, EEREFRESH, SHADOWREGEN) \
    (SET_BITS(EEACCMODE, EEACCMODE_MASK, 0) |                                          \
     SET_BITS(EEWRNEN, EEWRNEN_MASK, 2) |                                              \
     SET_BITS(EEREFRESH, EEREFRESH_MASK, 8) |                                          \
     SET_BITS(SHADOWREGEN, SHADOWREGEN_MASK, 12))
