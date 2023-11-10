/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

/**
 * @brief chip information definition.
 */
#define CHIP_NAME                 "MoreSense AT581X"    /*!< chip name */

/**
 * @brief Select gain for AT581X.
 *
 * Range: 0x0000 ~ 0x1100 (the larger the value, the smaller the gain)
 * Step: 3db
 * Recommended: GAIN_3 ~ GAIN_9
 */
typedef enum {
    AT581X_STAGE_GAIN_0  = 0,
    AT581X_STAGE_GAIN_1,
    AT581X_STAGE_GAIN_2,
    AT581X_STAGE_GAIN_3,
    AT581X_STAGE_GAIN_4,
    AT581X_STAGE_GAIN_5,
    AT581X_STAGE_GAIN_6,
    AT581X_STAGE_GAIN_7,
    AT581X_STAGE_GAIN_8,
    AT581X_STAGE_GAIN_9,
    AT581X_STAGE_GAIN_A,
    AT581X_STAGE_GAIN_B,
    AT581X_STAGE_GAIN_C,
} at581x_gain_t;

/*!< Power consumption configuration table. */
#define AT581X_POWER_48uA          (0x00<<4 | 0x01)
#define AT581X_POWER_56uA          (0x01<<4 | 0x01)
#define AT581X_POWER_63uA          (0x02<<4 | 0x01)
#define AT581X_POWER_70uA          (0x03<<4 | 0x01)
#define AT581X_POWER_77A_uA        (0x04<<4 | 0x01)
#define AT581X_POWER_91uA          (0x05<<4 | 0x01)
#define AT581X_POWER_105uA         (0x06<<4 | 0x01)
#define AT581X_POWER_115uA         (0x07<<4 | 0x01)

#define AT581X_POWER_40uA          (0x00<<4 | 0x03)
#define AT581X_POWER_44uA          (0x01<<4 | 0x03)
#define AT581X_POWER_47uA          (0x02<<4 | 0x03)
#define AT581X_POWER_51uA          (0x03<<4 | 0x03)
#define AT581X_POWER_54uA          (0x04<<4 | 0x03)
#define AT581X_POWER_61uA          (0x05<<4 | 0x03)
#define AT581X_POWER_68uA          (0x06<<4 | 0x03)
#define AT581X_POWER_77B_uA        (0x07<<4 | 0x03)

/*!< Frequency Configuration table. */
#define AT581X_FREQ_0X5F_5696MHZ    0x40
#define AT581X_FREQ_0X60_5696MHZ    0x9d

#define AT581X_FREQ_0X5F_5715MHZ    0x41
#define AT581X_FREQ_0X60_5715MHZ    0x9d

#define AT581X_FREQ_0X5F_5730MHZ    0x42
#define AT581X_FREQ_0X60_5730MHZ    0x9d

#define AT581X_FREQ_0X5F_5748MHZ    0x43
#define AT581X_FREQ_0X60_5748MHZ    0x9d

#define AT581X_FREQ_0X5F_5765MHZ    0x44
#define AT581X_FREQ_0X60_5765MHZ    0x9d

#define AT581X_FREQ_0X5F_5784MHZ    0x45
#define AT581X_FREQ_0X60_5784MHZ    0x9d

#define AT581X_FREQ_0X5F_5800MHZ    0x46
#define AT581X_FREQ_0X60_5800MHZ    0x9d

#define AT581X_FREQ_0X5F_5819MHZ    0x47
#define AT581X_FREQ_0X60_5819MHZ    0x9d

#define AT581X_FREQ_0X5F_5836MHZ    0x40
#define AT581X_FREQ_0X60_5836MHZ    0x9e

#define AT581X_FREQ_0X5F_5851MHZ    0x41
#define AT581X_FREQ_0X60_5851MHZ    0x9e

#define AT581X_FREQ_0X5F_5869MHZ    0x42
#define AT581X_FREQ_0X60_5869MHZ    0x9e

#define AT581X_FREQ_0X5F_5888MHZ    0x43
#define AT581X_FREQ_0X60_5888MHZ    0x9e

/*!< Value for RF and analog modules switch. */
#define AT581X_RX_RF2_OFF           0x46
#define AT581X_RX_RF2_ON            0x45

#define AT581X_RX_RF1_OFF           0xAA
#define AT581X_RX_RF1_ON            0x55

#define AT581X_RF_CTRL_OFF          0x50
#define AT581X_RF_CTRL_ON           0xA0

/*!< Value for register AT581X_RSTN_SW. */
#define AT581X_RSTN_SW_SOFT_RESET   0x00
#define AT581X_RSTN_SW_RELEASE      0x01

/*!< Value for register AT581X_CNF_LIGHT_DR. */
#define AT581X_CNF_LIGHT_OFF        0x4A
#define AT581X_CNF_LIGHT_ON         0x42

/*!< Registers of software reset. */
#define AT581X_RSTN_SW              0x00

/*!< Registers of self test time after poweron. Default: 3s */
#define AT581X_TIME_SELF_CK_1       0x38    /*!< Time_self_ck_1 Bit<7:0> */
#define AT581X_TIME_SELF_CK_2       0x39    /*!< Time_self_ck_2 Bit<15:8> */

/*!< Registers of Minimum lighting time. Default: 500ms */
#define AT581X_TIME_FLAG_BASE_1      0x3D   /*!< Time_flag_base_1 Bit<7:0> */
#define AT581X_TIME_FLAG_BASE_2      0x3E   /*!< Time_flag_base_2 Bit<15:8> */
#define AT581X_TIME_FLAG_BASE_3      0x3F   /*!< Time_flag_base_3 Bit<23:16> */
#define AT581X_TIME_FLAG_BASE_4      0x40   /*!< Time_flag_base_4 Bit<31:24> */

/*!< Registers of Lighting delay time. Unit: ms */
#define AT581X_TIME_FLAG_OUT_CTRL   0x41    /*!< Time_flag_out_ctrl 0x01 */
#define AT581X_TIME_FLAG_OUT_1      0x42    /*!< Time_flag_out_1 Bit<7:0> */
#define AT581X_TIME_FLAG_OUT_2      0x43    /*!< Time_flag_out_2 Bit<15:8> */
#define AT581X_TIME_FLAG_OUT_3      0x44    /*!< Time_flag_out_3 Bit<23:16> */
#define AT581X_TIME_FLAG_OUT_4      0x45    /*!< Time_flag_out_4 Bit<31:24> */

/*!< Registers of lights-off protection time. Default: 1000ms */
#define AT581X_TIME_UNDET_PORT_1    0x4E    /*!< Time_undet_prot_1 Bit<7:0> */
#define AT581X_TIME_UNDET_PORT_2    0x4F    /*!< Time_undet_prot_2 Bit<15:8> */

/*!< Registers of receive gain. */
#define AT581X_RX_TIA_AGIN_REG      0x5C    /*!< Bit<3> Rx_tia_gain_dr,1: enable. Bit<7:4> Rx_tia_gain_reg (0000 ~ 1100) */

#define AT581X_TIA_GAIN_DR_BIT      (3)
#define AT581X_TIA_GAIN_REF_BIT     (4)

/*!< Registers of sensing distance. Range: 0-1023 */
#define AT581X_CNF_POWER_TH_DR      0x68    /*!< Bit<6> 1:set thr from reg */
#define AT581X_CNF_POWER_TH_REG     0x67    /*!< Bit<7> 1:set thr_1 */

#define AT581X_POWER_TH_DR_BIT      (6)
#define AT581X_POWER_TH_REG_BIT     (7)

#define AT581X_SIGNAL_DET_THR_1_1   0x10    /*!< Signal_det_thr_1_1. Bit7~0 of thr */
#define AT581X_SIGNAL_DET_THR_1_2   0x11    /*!< Signal_det_thr_1_2. Bit15~8 of thr */

/*!< Registers of power consumption. */
#define AT581X_CNF_WORK_TIME_REG    0x67    /*!< <3> Cnf_work_time_dr 1: enable. <2:0> Cnf_work_time_reg (000 ~ 111) */
#define AT581X_CNF_BURST_TIME_REG   0x68    /*!< <5> Cnf_burst_time_dr 1: enable. <4:3> Cnf_burst_time_reg (00 ~ 11) */

#define AT581X_WORK_TIME_DR_BIT     (3)
#define AT581X_WORK_TIME_REG_BIT    (0)
#define AT581X_BURST_TIME_DR_BIT    (5)
#define AT581X_BURST_TIME_REG_BIT   (3)

/*!< Registers of RF transmission frequency. */
#define AT581X_VCO_CAP_DR           0x61    /*!< Bit<1:0> Vco_cap_dr. 10: enable */
#define AT581X_VCO_1                0x5F    /*!< Bit<2:0> Vco1. Refer to "Frequency Configuration table" */
#define AT581X_VCO_2                0x60    /*!< Bit<5> Vco2. Refer to "Frequency Configuration table" */

#define AT581X_VCO_CAP_DR_BIT       (0)

/*!< Registers of RF and analog modules switch. */
#define AT581X_RX_RF2               0x5D    /*!< Refer to "Value for RF and analog modules switch" */
#define AT581X_RX_RF1               0x62    /*!< Refer to "Value for RF and analog modules switch" */
#define AT581X_RF_CTRL              0x51    /*!< Refer to "Value for RF and analog modules switch" */

/*!< Registers of detect parameters. */
#define AT581X_POWER_DET_WIN_LEN    0x31    /*!< Number of windows for each detection. Default: 4 */
#define AT581X_POWER_DET_WIN_THR    0x32    /*!< Number of windows required for triggering. Default: 3 */
#define AT581X_POWER_DET_LEN        0x0E    /*!< Single window length. Default: 0x0a */

/*!< Registers of light sensor. */
#define AT581X_CNF_LIGHT_DR         0x66    /*!< on/off light sensor. */
#define AT581X_LIGHT_REF_LOW        0x34    /*!< low 8bit of Light_ref_low. */
#define AT581X_LIGHT_REF_HIGH       0x35    /*!< low 8bit of Light_ref_high. */
#define AT581X_LIGHT_REF            0x36

#define AT581X_LIGHT_REF_LOW_BIT    (0)     /*!< AT581X_LIGHT_REF, high 2bit of Light_ref_low. */
#define AT581X_LIGHT_REF_HIGH_BIT   (2)     /*!< AT581X_LIGHT_REF, high 2bit of Light_ref_high. */
#define AT581X_LIGHT_INV_BIT        (4)     /*!< AT581X_LIGHT_REF, 0: Default. 1: reverse. */

