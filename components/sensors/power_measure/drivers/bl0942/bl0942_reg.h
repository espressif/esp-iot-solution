/* SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
* SPDX-License-Identifier: Apache-2.0
*/

#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Register address map (24-bit registers) */
enum bl0942_reg_addr {
    BL0942_ADDR_I_WAVE      = 0x01,
    BL0942_ADDR_V_WAVE      = 0x02,
    BL0942_ADDR_I_RMS       = 0x03,
    BL0942_ADDR_V_RMS       = 0x04,
    BL0942_ADDR_I_FAST_RMS  = 0x05,
    BL0942_ADDR_WATT        = 0x06,
    BL0942_ADDR_CF_CNT      = 0x07,
    BL0942_ADDR_FREQ        = 0x08,
    BL0942_ADDR_STATUS      = 0x09,
    BL0942_ADDR_I_RMSOS     = 0x12,
    BL0942_ADDR_WA_CREEP    = 0x14,
    BL0942_ADDR_I_FAST_TH   = 0x15,
    BL0942_ADDR_I_FAST_CYC  = 0x16,
    BL0942_ADDR_FREQ_CYC    = 0x17,
    BL0942_ADDR_OT_FUNX     = 0x18,
    BL0942_ADDR_MODE        = 0x19,
    BL0942_ADDR_GAIN_CR     = 0x1A,
    BL0942_ADDR_SOFT_RESET  = 0x1C,
    BL0942_ADDR_USR_WRPROT  = 0x1D,
};

/* Backward-compatible register macros (used by driver) */
#define BL0942_REG_I_WAVE       BL0942_ADDR_I_WAVE
#define BL0942_REG_V_WAVE       BL0942_ADDR_V_WAVE
#define BL0942_REG_I_RMS        BL0942_ADDR_I_RMS
#define BL0942_REG_V_RMS        BL0942_ADDR_V_RMS
#define BL0942_REG_I_FAST_RMS   BL0942_ADDR_I_FAST_RMS
#define BL0942_REG_WATT         BL0942_ADDR_WATT
#define BL0942_REG_CF_CNT       BL0942_ADDR_CF_CNT
#define BL0942_REG_FREQ         BL0942_ADDR_FREQ
#define BL0942_REG_STATUS       BL0942_ADDR_STATUS
#define BL0942_REG_I_RMSOS      BL0942_ADDR_I_RMSOS
#define BL0942_REG_WA_CREEP     BL0942_ADDR_WA_CREEP
#define BL0942_REG_I_FAST_TH    BL0942_ADDR_I_FAST_TH
#define BL0942_REG_I_FAST_CYC   BL0942_ADDR_I_FAST_CYC
#define BL0942_REG_FREQ_CYC     BL0942_ADDR_FREQ_CYC
#define BL0942_REG_OT_FUNX      BL0942_ADDR_OT_FUNX
#define BL0942_REG_MODE         BL0942_ADDR_MODE
#define BL0942_REG_GAIN_CR      BL0942_ADDR_GAIN_CR
#define BL0942_REG_SOFT_RESET   BL0942_ADDR_SOFT_RESET
#define BL0942_REG_USR_WRPROT   BL0942_ADDR_USR_WRPROT

/* MODE (0x19) bitfields (24-bit register, using lower 10 bits) */
typedef union {
    struct __attribute__((packed)) {
        uint32_t reserved0: 2;       /* [1:0] reserved */
        uint32_t cf_en: 1;           /* [2] */
        uint32_t rms_update_sel: 1;  /* [3] 0:400ms 1:800ms */
        uint32_t fast_rms_sel: 1;    /* [4] 0:SINC 1:HPF */
        uint32_t ac_freq_sel: 1;     /* [5] 0:50Hz 1:60Hz */
        uint32_t cf_cnt_clr_sel: 1;  /* [6] 0:off 1:on */
        uint32_t cf_cnt_add_sel: 1;  /* [7] 0:algebraic 1:absolute */
        uint32_t uart_rate_sel: 2;   /* [9:8] 00/01:ext pin, 10:19200, 11:38400 */
        uint32_t reserved1: 14;      /* [23:10] */
    } bit;
    uint32_t all; /* use lower 24 bits */
} bl0942_reg_mode_t;

/* OT_FUNX (0x18) bitfields */
typedef union {
    struct __attribute__((packed)) {
        uint32_t cf1_funx_sel: 2; /* [1:0] */
        uint32_t cf2_funx_sel: 2; /* [3:2] */
        uint32_t zx_funx_sel: 2; /* [5:4] */
        uint32_t reserved: 18;   /* [23:6] */
    } bit;
    uint32_t all;
} bl0942_reg_ot_funx_t;

/* STATUS (0x09) bitfields */
typedef union {
    struct __attribute__((packed)) {
        uint32_t cf_revp_f: 1;   /* [0] */
        uint32_t creep_f: 1;     /* [1] */
        uint32_t reserved0: 6;   /* [7:2] */
        uint32_t i_zx_lth_f: 1;  /* [8] */
        uint32_t v_zx_lth_f: 1;  /* [9] */
        uint32_t reserved1: 14;  /* [23:10] */
    } bit;
    uint32_t all;
} bl0942_reg_status_t;

/* GAIN_CR (0x1A) bitfields */
typedef union {
    struct __attribute__((packed)) {
        uint32_t gain_cr: 2;     /* [1:0] 00:1x 01:4x 10:16x 11:24x */
        uint32_t reserved: 22;   /* [23:2] */
    } bit;
    uint32_t all;
} bl0942_reg_gain_cr_t;

/* Aggregated register snapshot (optional) */
typedef struct {
    bl0942_reg_mode_t mode;
    bl0942_reg_ot_funx_t ot_funx;
    bl0942_reg_status_t status;
    bl0942_reg_gain_cr_t gain_cr;
    uint32_t i_rms;
    uint32_t v_rms;
    uint32_t i_fast_rms;
    uint32_t watt;
    uint32_t cf_cnt;
    uint32_t freq;
} bl0942_reg_t;

/* OT_FUNX function encodings */
#define BL0942_FUNX_CF   0x0
#define BL0942_FUNX_OCP  0x1
#define BL0942_FUNX_VZX  0x2
#define BL0942_FUNX_IZX  0x3

#ifdef __cplusplus
}
#endif
