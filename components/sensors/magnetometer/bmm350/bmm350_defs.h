/**
* Copyright (c) 2023 Bosch Sensortec GmbH. All rights reserved.
*
* BSD-3-Clause
*
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions are met:
*
* 1. Redistributions of source code must retain the above copyright
*    notice, this list of conditions and the following disclaimer.
*
* 2. Redistributions in binary form must reproduce the above copyright
*    notice, this list of conditions and the following disclaimer in the
*    documentation and/or other materials provided with the distribution.
*
* 3. Neither the name of the copyright holder nor the names of its
*    contributors may be used to endorse or promote products derived from
*    this software without specific prior written permission.
*
* THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
* "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
* LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
* FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
* COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
* INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
* (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
* SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
* HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
* STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
* IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
* POSSIBILITY OF SUCH DAMAGE.
*
* @file       bmm350_defs.h
* @date       2023-05-26
* @version    v1.4.0
*
*/

#ifndef _BMM350_DEFS_H
#define _BMM350_DEFS_H

/*************************** Header files *******************************/

#ifdef __KERNEL__
#include <linux/types.h>
#include <linux/kernel.h>
#else
#include <stdint.h>
#include <stddef.h>
#endif

/***************************** Common Macros *****************************/

#ifdef __KERNEL__
#if !defined(UINT8_C) && !defined(INT8_C)
#define INT8_C(x)    S8_C(x)
#define UINT8_C(x)   U8_C(x)
#endif

#if !defined(UINT16_C) && !defined(INT16_C)
#define INT16_C(x)   S16_C(x)
#define UINT16_C(x)  U16_C(x)
#endif

#if !defined(INT32_C) && !defined(UINT32_C)
#define INT32_C(x)   S32_C(x)
#define UINT32_C(x)  U32_C(x)
#endif

#if !defined(INT64_C) && !defined(UINT64_C)
#define INT64_C(x)   S64_C(x)
#define UINT64_C(x)  U64_C(x)
#endif
#endif

/*! C standard macros */
#ifndef NULL
#ifdef __cplusplus
#define NULL         0
#else
#define NULL         ((void *) 0)
#endif
#endif

/******************************************************************************/
/*! @name        Compiler switch macros Definitions                           */
/******************************************************************************/

/************************* General Macro definitions ***************************/

/* Macro to SET and GET BITS of a register*/
#define BMM350_SET_BITS(reg_data, bitname, data) \
    ((reg_data & ~(bitname##_MSK)) | \
     ((data << bitname##_POS) & bitname##_MSK))

#define BMM350_GET_BITS(reg_data, bitname)          ((reg_data & (bitname##_MSK)) >> (bitname##_POS))

#define BMM350_GET_BITS_POS_0(reg_data, bitname)    (reg_data & (bitname##_MSK))

#define BMM350_SET_BITS_POS_0(reg_data, bitname, data) \
    ((reg_data & ~(bitname##_MSK)) | \
     (data & bitname##_MSK))

#ifndef BMM350_INTF_RET_TYPE
#define BMM350_INTF_RET_TYPE                        int8_t
#endif

/*! Chip id of BMM350 */
#define BMM350_CHIP_ID                              UINT8_C(0x33)

/*! Variant ID of BMM350 */
#define BMM350_MIN_VAR                              UINT8_C(0x10)

/************************* Sensor interface success code **************************/
#define BMM350_INTF_RET_SUCCESS                     INT8_C(0)

/************************* API success code **************************/
#define BMM350_OK                                   INT8_C(0)

/* API error codes */
#define BMM350_E_NULL_PTR                           INT8_C(-1)
#define BMM350_E_COM_FAIL                           INT8_C(-2)
#define BMM350_E_DEV_NOT_FOUND                      INT8_C(-3)
#define BMM350_E_INVALID_CONFIG                     INT8_C(-4)
#define BMM350_E_BAD_PAD_DRIVE                      INT8_C(-5)
#define BMM350_E_RESET_UNFINISHED                   INT8_C(-6)
#define BMM350_E_INVALID_INPUT                      INT8_C(-7)
#define BMM350_E_SELF_TEST_INVALID_AXIS             INT8_C(-8)
#define BMM350_E_OTP_BOOT                           INT8_C(-9)
#define BMM350_E_OTP_PAGE_RD                        INT8_C(-10)
#define BMM350_E_OTP_PAGE_PRG                       INT8_C(-11)
#define BMM350_E_OTP_SIGN                           INT8_C(-12)
#define BMM350_E_OTP_INV_CMD                        INT8_C(-13)
#define BMM350_E_OTP_UNDEFINED                      INT8_C(-14)
#define BMM350_E_ALL_AXIS_DISABLED                  INT8_C(-15)
#define BMM350_E_PMU_CMD_VALUE                      INT8_C(-16)

#define BMM350_NO_ERROR                             UINT8_C(0)

/************************* Sensor delay time settings in microseconds **************************/
#define BMM350_SOFT_RESET_DELAY                     UINT32_C(24000)
#define BMM350_MAGNETIC_RESET_DELAY                 UINT32_C(40000)
#define BMM350_START_UP_TIME_FROM_POR               UINT32_C(3000)

#define BMM350_GOTO_SUSPEND_DELAY                   UINT32_C(6000)
#define BMM350_SUSPEND_TO_NORMAL_DELAY              UINT32_C(38000)

#define BMM350_SUS_TO_FORCEDMODE_NO_AVG_DELAY       UINT32_C(15000)
#define BMM350_SUS_TO_FORCEDMODE_AVG_2_DELAY        UINT32_C(17000)
#define BMM350_SUS_TO_FORCEDMODE_AVG_4_DELAY        UINT32_C(20000)
#define BMM350_SUS_TO_FORCEDMODE_AVG_8_DELAY        UINT32_C(28000)

#define BMM350_SUS_TO_FORCEDMODE_FAST_NO_AVG_DELAY  UINT32_C(4000)
#define BMM350_SUS_TO_FORCEDMODE_FAST_AVG_2_DELAY   UINT32_C(5000)
#define BMM350_SUS_TO_FORCEDMODE_FAST_AVG_4_DELAY   UINT32_C(9000)
#define BMM350_SUS_TO_FORCEDMODE_FAST_AVG_8_DELAY   UINT32_C(16000)

#define BMM350_UPD_OAE_DELAY                        UINT16_C(1000)

#define BMM350_BR_DELAY                             UINT16_C(14000)
#define BMM350_FGR_DELAY                            UINT16_C(18000)

/************************ Length macros ************************/
#define BMM350_OTP_DATA_LENGTH                      UINT8_C(32)
#define BMM350_READ_BUFFER_LENGTH                   UINT8_C(127)
#define BMM350_MAG_TEMP_DATA_LEN                    UINT8_C(12)

/************************ Averaging macros **********************/
#define BMM350_AVG_NO_AVG                           UINT8_C(0x0)
#define BMM350_AVG_2                                UINT8_C(0x1)
#define BMM350_AVG_4                                UINT8_C(0x2)
#define BMM350_AVG_8                                UINT8_C(0x3)

/******************************* ODR **************************/
#define BMM350_ODR_400HZ                            UINT8_C(0x2)
#define BMM350_ODR_200HZ                            UINT8_C(0x3)
#define BMM350_ODR_100HZ                            UINT8_C(0x4)
#define BMM350_ODR_50HZ                             UINT8_C(0x5)
#define BMM350_ODR_25HZ                             UINT8_C(0x6)
#define BMM350_ODR_12_5HZ                           UINT8_C(0x7)
#define BMM350_ODR_6_25HZ                           UINT8_C(0x8)
#define BMM350_ODR_3_125HZ                          UINT8_C(0x9)
#define BMM350_ODR_1_5625HZ                         UINT8_C(0xA)

/********************* Power modes *************************/
#define BMM350_PMU_CMD_SUS                          UINT8_C(0x00)
#define BMM350_PMU_CMD_NM                           UINT8_C(0x01)
#define BMM350_PMU_CMD_UPD_OAE                      UINT8_C(0x02)
#define BMM350_PMU_CMD_FM                           UINT8_C(0x03)
#define BMM350_PMU_CMD_FM_FAST                      UINT8_C(0x04)
#define BMM350_PMU_CMD_FGR                          UINT8_C(0x05)
#define BMM350_PMU_CMD_FGR_FAST                     UINT8_C(0x06)
#define BMM350_PMU_CMD_BR                           UINT8_C(0x07)
#define BMM350_PMU_CMD_BR_FAST                      UINT8_C(0x08)
#define BMM350_PMU_CMD_NM_TC                        UINT8_C(0x09)

#define BMM350_PMU_STATUS_0                         UINT8_C(0x0)

#define BMM350_DISABLE                              UINT8_C(0x0)
#define BMM350_ENABLE                               UINT8_C(0x1)

#define BMM350_CMD_NOP                              UINT8_C(0x0)
#define BMM350_CMD_SOFTRESET                        UINT8_C(0xB6)

#define BMM350_TARGET_PAGE_PAGE0                    UINT8_C(0x0)
#define BMM350_TARGET_PAGE_PAGE1                    UINT8_C(0x1)

#define BMM350_INT_MODE_LATCHED                     UINT8_C(0x1)
#define BMM350_INT_MODE_PULSED                      UINT8_C(0x0)

#define BMM350_INT_POL_ACTIVE_HIGH                  UINT8_C(0x1)
#define BMM350_INT_POL_ACTIVE_LOW                   UINT8_C(0x0)

#define BMM350_INT_OD_PUSHPULL                      UINT8_C(0x1)
#define BMM350_INT_OD_OPENDRAIN                     UINT8_C(0x0)

#define BMM350_INT_OUTPUT_EN_OFF                    UINT8_C(0x0)
#define BMM350_INT_OUTPUT_EN_ON                     UINT8_C(0x1)

#define BMM350_INT_DRDY_EN                          UINT8_C(0x1)
#define BMM350_INT_DRDY_DIS                         UINT8_C(0x0)

#define BMM350_MR_MR1K8                             UINT8_C(0x0)
#define BMM350_MR_MR2K1                             UINT8_C(0x1)
#define BMM350_MR_MR1K5                             UINT8_C(0x2)
#define BMM350_MR_MR0K6                             UINT8_C(0x3)

#define BMM350_SEL_DTB1X_PAD_PAD_INT                UINT8_C(0x0)
#define BMM350_SEL_DTB1X_PAD_PAD_BYP                UINT8_C(0x1)

#define BMM350_TMR_TST_HIZ_VTMR_VTMR_ON             UINT8_C(0x0)
#define BMM350_TMR_TST_HIZ_VTMR_VTMR_HIZ            UINT8_C(0x1)

#define BMM350_LSB_MASK                             UINT16_C(0x00FF)
#define BMM350_MSB_MASK                             UINT16_C(0xFF00)

/********************** Pad drive strength ************************/
#define BMM350_PAD_DRIVE_WEAKEST                    UINT8_C(0)
#define BMM350_PAD_DRIVE_STRONGEST                  UINT8_C(7)

/********************** I2C Register Addresses ************************/

/*! Register to set I2C address to LOW */
#define BMM350_I2C_ADSEL_SET_LOW                    UINT8_C(0x14)

/*! Register to set I2C address to HIGH */
#define BMM350_I2C_ADSEL_SET_HIGH                   UINT8_C(0x15)

#define BMM350_DUMMY_BYTES                          UINT8_C(2)

/********************** Register Addresses ************************/

#define BMM350_REG_CHIP_ID                          UINT8_C(0x00)
#define BMM350_REG_REV_ID                           UINT8_C(0x01)
#define BMM350_REG_ERR_REG                          UINT8_C(0x02)
#define BMM350_REG_PAD_CTRL                         UINT8_C(0x03)
#define BMM350_REG_PMU_CMD_AGGR_SET                 UINT8_C(0x04)
#define BMM350_REG_PMU_CMD_AXIS_EN                  UINT8_C(0x05)
#define BMM350_REG_PMU_CMD                          UINT8_C(0x06)
#define BMM350_REG_PMU_CMD_STATUS_0                 UINT8_C(0x07)
#define BMM350_REG_PMU_CMD_STATUS_1                 UINT8_C(0x08)
#define BMM350_REG_I3C_ERR                          UINT8_C(0x09)
#define BMM350_REG_I2C_WDT_SET                      UINT8_C(0x0A)
#define BMM350_REG_TRSDCR_REV_ID                    UINT8_C(0x0D)
#define BMM350_REG_TC_SYNC_TU                       UINT8_C(0x21)
#define BMM350_REG_TC_SYNC_ODR                      UINT8_C(0x22)
#define BMM350_REG_TC_SYNC_TPH_1                    UINT8_C(0x23)
#define BMM350_REG_TC_SYNC_TPH_2                    UINT8_C(0x24)
#define BMM350_REG_TC_SYNC_DT                       UINT8_C(0x25)
#define BMM350_REG_TC_SYNC_ST_0                     UINT8_C(0x26)
#define BMM350_REG_TC_SYNC_ST_1                     UINT8_C(0x27)
#define BMM350_REG_TC_SYNC_ST_2                     UINT8_C(0x28)
#define BMM350_REG_TC_SYNC_STATUS                   UINT8_C(0x29)
#define BMM350_REG_INT_CTRL                         UINT8_C(0x2E)
#define BMM350_REG_INT_CTRL_IBI                     UINT8_C(0x2F)
#define BMM350_REG_INT_STATUS                       UINT8_C(0x30)
#define BMM350_REG_MAG_X_XLSB                       UINT8_C(0x31)
#define BMM350_REG_MAG_X_LSB                        UINT8_C(0x32)
#define BMM350_REG_MAG_X_MSB                        UINT8_C(0x33)
#define BMM350_REG_MAG_Y_XLSB                       UINT8_C(0x34)
#define BMM350_REG_MAG_Y_LSB                        UINT8_C(0x35)
#define BMM350_REG_MAG_Y_MSB                        UINT8_C(0x36)
#define BMM350_REG_MAG_Z_XLSB                       UINT8_C(0x37)
#define BMM350_REG_MAG_Z_LSB                        UINT8_C(0x38)
#define BMM350_REG_MAG_Z_MSB                        UINT8_C(0x39)
#define BMM350_REG_TEMP_XLSB                        UINT8_C(0x3A)
#define BMM350_REG_TEMP_LSB                         UINT8_C(0x3B)
#define BMM350_REG_TEMP_MSB                         UINT8_C(0x3C)
#define BMM350_REG_SENSORTIME_XLSB                  UINT8_C(0x3D)
#define BMM350_REG_SENSORTIME_LSB                   UINT8_C(0x3E)
#define BMM350_REG_SENSORTIME_MSB                   UINT8_C(0x3F)
#define BMM350_REG_OTP_CMD_REG                      UINT8_C(0x50)
#define BMM350_REG_OTP_DATA_MSB_REG                 UINT8_C(0x52)
#define BMM350_REG_OTP_DATA_LSB_REG                 UINT8_C(0x53)
#define BMM350_REG_OTP_STATUS_REG                   UINT8_C(0x55)
#define BMM350_REG_TMR_SELFTEST_USER                UINT8_C(0x60)
#define BMM350_REG_CTRL_USER                        UINT8_C(0x61)
#define BMM350_REG_CMD                              UINT8_C(0x7E)

/*********************** Macros for OVWR ***************************/
#define BMM350_REG_OVWR_VALUE_ANA_0                 UINT8_C(0x3A)
#define BMM350_REG_OVWR_EN_ANA_0                    UINT8_C(0x3B)

/*********************** Macros for bit masking ***************************/

#define BMM350_CHIP_ID_OTP_MSK                      UINT8_C(0xf)
#define BMM350_CHIP_ID_OTP_POS                      UINT8_C(0x0)
#define BMM350_CHIP_ID_FIXED_MSK                    UINT8_C(0xf0)
#define BMM350_CHIP_ID_FIXED_POS                    UINT8_C(0x4)
#define BMM350_REV_ID_MAJOR_MSK                     UINT8_C(0xf0)
#define BMM350_REV_ID_MAJOR_POS                     UINT8_C(0x4)
#define BMM350_REV_ID_MINOR_MSK                     UINT8_C(0xf)
#define BMM350_REV_ID_MINOR_POS                     UINT8_C(0x0)
#define BMM350_PMU_CMD_ERROR_MSK                    UINT8_C(0x1)
#define BMM350_PMU_CMD_ERROR_POS                    UINT8_C(0x0)
#define BMM350_BOOT_UP_ERROR_MSK                    UINT8_C(0x2)
#define BMM350_BOOT_UP_ERROR_POS                    UINT8_C(0x1)
#define BMM350_DRV_MSK                              UINT8_C(0x7)
#define BMM350_DRV_POS                              UINT8_C(0x0)
#define BMM350_AVG_MSK                              UINT8_C(0x30)
#define BMM350_AVG_POS                              UINT8_C(0x4)
#define BMM350_ODR_MSK                              UINT8_C(0xf)
#define BMM350_ODR_POS                              UINT8_C(0x0)
#define BMM350_PMU_CMD_MSK                          UINT8_C(0xf)
#define BMM350_PMU_CMD_POS                          UINT8_C(0x0)
#define BMM350_EN_X_MSK                             UINT8_C(0x01)
#define BMM350_EN_X_POS                             UINT8_C(0x0)
#define BMM350_EN_Y_MSK                             UINT8_C(0x02)
#define BMM350_EN_Y_POS                             UINT8_C(0x1)
#define BMM350_EN_Z_MSK                             UINT8_C(0x04)
#define BMM350_EN_Z_POS                             UINT8_C(0x2)
#define BMM350_EN_XYZ_MSK                           UINT8_C(0x7)
#define BMM350_EN_XYZ_POS                           UINT8_C(0x0)
#define BMM350_PMU_CMD_BUSY_MSK                     UINT8_C(0x1)
#define BMM350_PMU_CMD_BUSY_POS                     UINT8_C(0x0)
#define BMM350_ODR_OVWR_MSK                         UINT8_C(0x2)
#define BMM350_ODR_OVWR_POS                         UINT8_C(0x1)
#define BMM350_AVG_OVWR_MSK                         UINT8_C(0x4)
#define BMM350_AVG_OVWR_POS                         UINT8_C(0x2)
#define BMM350_PWR_MODE_IS_NORMAL_MSK               UINT8_C(0x8)
#define BMM350_PWR_MODE_IS_NORMAL_POS               UINT8_C(0x3)
#define BMM350_CMD_IS_ILLEGAL_MSK                   UINT8_C(0x10)
#define BMM350_CMD_IS_ILLEGAL_POS                   UINT8_C(0x4)
#define BMM350_PMU_CMD_VALUE_MSK                    UINT8_C(0xE0)
#define BMM350_PMU_CMD_VALUE_POS                    UINT8_C(0x5)
#define BMM350_PMU_ODR_S_MSK                        UINT8_C(0xf)
#define BMM350_PMU_ODR_S_POS                        UINT8_C(0x0)
#define BMM350_PMU_AVG_S_MSK                        UINT8_C(0x30)
#define BMM350_PMU_AVG_S_POS                        UINT8_C(0x4)
#define BMM350_I3C_ERROR_0_MSK                      UINT8_C(0x1)
#define BMM350_I3C_ERROR_0_POS                      UINT8_C(0x0)
#define BMM350_I3C_ERROR_3_MSK                      UINT8_C(0x8)
#define BMM350_I3C_ERROR_3_POS                      UINT8_C(0x3)
#define BMM350_I2C_WDT_EN_MSK                       UINT8_C(0x1)
#define BMM350_I2C_WDT_EN_POS                       UINT8_C(0x0)
#define BMM350_I2C_WDT_SEL_MSK                      UINT8_C(0x2)
#define BMM350_I2C_WDT_SEL_POS                      UINT8_C(0x1)
#define BMM350_TRSDCR_REV_ID_OTP_MSK                UINT8_C(0x3)
#define BMM350_TRSDCR_REV_ID_OTP_POS                UINT8_C(0x0)
#define BMM350_TRSDCR_REV_ID_FIXED_MSK              UINT8_C(0xfc)
#define BMM350_TRSDCR_REV_ID_FIXED_POS              UINT8_C(0x2)
#define BMM350_PAGING_EN_MSK                        UINT8_C(0x80)
#define BMM350_PAGING_EN_POS                        UINT8_C(0x7)
#define BMM350_DRDY_DATA_REG_MSK                    UINT8_C(0x4)
#define BMM350_DRDY_DATA_REG_POS                    UINT8_C(0x2)
#define BMM350_INT_MODE_MSK                         UINT8_C(0x1)
#define BMM350_INT_MODE_POS                         UINT8_C(0x0)
#define BMM350_INT_POL_MSK                          UINT8_C(0x2)
#define BMM350_INT_POL_POS                          UINT8_C(0x1)
#define BMM350_INT_OD_MSK                           UINT8_C(0x4)
#define BMM350_INT_OD_POS                           UINT8_C(0x2)
#define BMM350_INT_OUTPUT_EN_MSK                    UINT8_C(0x8)
#define BMM350_INT_OUTPUT_EN_POS                    UINT8_C(0x3)
#define BMM350_DRDY_DATA_REG_EN_MSK                 UINT8_C(0x80)
#define BMM350_DRDY_DATA_REG_EN_POS                 UINT8_C(0x7)
#define BMM350_DRDY_INT_MAP_TO_IBI_MSK              UINT8_C(0x1)
#define BMM350_DRDY_INT_MAP_TO_IBI_POS              UINT8_C(0x0)
#define BMM350_CLEAR_DRDY_INT_STATUS_UPON_IBI_MSK   UINT8_C(0x10)
#define BMM350_CLEAR_DRDY_INT_STATUS_UPON_IBI_POS   UINT8_C(0x4)
#define BMM350_TC_SYNC_TU_MSK                       UINT8_C(0xff)
#define BMM350_TC_SYNC_ODR_MSK                      UINT8_C(0xff)
#define BMM350_TC_SYNC_TPH_1_MSK                    UINT8_C(0xff)
#define BMM350_TC_SYNC_TPH_2_MSK                    UINT8_C(0xff)
#define BMM350_TC_SYNC_DT_MSK                       UINT8_C(0xff)
#define BMM350_TC_SYNC_ST_0_MSK                     UINT8_C(0xff)
#define BMM350_TC_SYNC_ST_1_MSK                     UINT8_C(0xff)
#define BMM350_TC_SYNC_ST_2_MSK                     UINT8_C(0xff)
#define BMM350_CFG_FORCE_SOSC_EN_MSK                UINT8_C(0x4)
#define BMM350_CFG_FORCE_SOSC_EN_POS                UINT8_C(0x2)
#define BMM350_ST_IGEN_EN_MSK                       UINT8_C(0x1)
#define BMM350_ST_IGEN_EN_POS                       UINT8_C(0x0)
#define BMM350_ST_N_MSK                             UINT8_C(0x2)
#define BMM350_ST_N_POS                             UINT8_C(0x1)
#define BMM350_ST_P_MSK                             UINT8_C(0x4)
#define BMM350_ST_P_POS                             UINT8_C(0x2)
#define BMM350_IST_EN_X_MSK                         UINT8_C(0x8)
#define BMM350_IST_EN_X_POS                         UINT8_C(0x3)
#define BMM350_IST_EN_Y_MSK                         UINT8_C(0x10)
#define BMM350_IST_EN_Y_POS                         UINT8_C(0x4)
#define BMM350_CFG_SENS_TIM_AON_MSK                 UINT8_C(0x1)
#define BMM350_CFG_SENS_TIM_AON_POS                 UINT8_C(0x0)
#define BMM350_DATA_X_7_0_MSK                       UINT8_C(0xff)
#define BMM350_DATA_X_7_0_POS                       UINT8_C(0x0)
#define BMM350_DATA_X_15_8_MSK                      UINT8_C(0xff)
#define BMM350_DATA_X_15_8_POS                      UINT8_C(0x0)
#define BMM350_DATA_X_23_16_MSK                     UINT8_C(0xff)
#define BMM350_DATA_X_23_16_POS                     UINT8_C(0x0)
#define BMM350_DATA_Y_7_0_MSK                       UINT8_C(0xff)
#define BMM350_DATA_Y_7_0_POS                       UINT8_C(0x0)
#define BMM350_DATA_Y_15_8_MSK                      UINT8_C(0xff)
#define BMM350_DATA_Y_15_8_POS                      UINT8_C(0x0)
#define BMM350_DATA_Y_23_16_MSK                     UINT8_C(0xff)
#define BMM350_DATA_Y_23_16_POS                     UINT8_C(0x0)
#define BMM350_DATA_Z_7_0_MSK                       UINT8_C(0xff)
#define BMM350_DATA_Z_7_0_POS                       UINT8_C(0x0)
#define BMM350_DATA_Z_15_8_MSK                      UINT8_C(0xff)
#define BMM350_DATA_Z_15_8_POS                      UINT8_C(0x0)
#define BMM350_DATA_Z_23_16_MSK                     UINT8_C(0xff)
#define BMM350_DATA_Z_23_16_POS                     UINT8_C(0x0)
#define BMM350_DATA_T_7_0_MSK                       UINT8_C(0xff)
#define BMM350_DATA_T_7_0_POS                       UINT8_C(0x0)
#define BMM350_DATA_T_15_8_MSK                      UINT8_C(0xff)
#define BMM350_DATA_T_15_8_POS                      UINT8_C(0x0)
#define BMM350_DATA_T_23_16_MSK                     UINT8_C(0xff)
#define BMM350_DATA_T_23_16_POS                     UINT8_C(0x0)
#define BMM350_DATA_ST_7_0_MSK                      UINT8_C(0xff)
#define BMM350_DATA_ST_7_0_POS                      UINT8_C(0x0)
#define BMM350_DATA_ST_15_8_MSK                     UINT8_C(0xff)
#define BMM350_DATA_ST_15_8_POS                     UINT8_C(0x0)
#define BMM350_DATA_ST_23_16_MSK                    UINT8_C(0xff)
#define BMM350_DATA_ST_23_16_POS                    UINT8_C(0x0)
#define BMM350_SIGN_INVERT_T_MSK                    UINT8_C(0x10)
#define BMM350_SIGN_INVERT_T_POS                    UINT8_C(0x4)
#define BMM350_SIGN_INVERT_X_MSK                    UINT8_C(0x20)
#define BMM350_SIGN_INVERT_X_POS                    UINT8_C(0x5)
#define BMM350_SIGN_INVERT_Y_MSK                    UINT8_C(0x40)
#define BMM350_SIGN_INVERT_Y_POS                    UINT8_C(0x6)
#define BMM350_SIGN_INVERT_Z_MSK                    UINT8_C(0x80)
#define BMM350_SIGN_INVERT_Z_POS                    UINT8_C(0x7)
#define BMM350_DIS_BR_NM_MSK                        UINT8_C(0x1)
#define BMM350_DIS_BR_NM_POS                        UINT8_C(0x0)
#define BMM350_DIS_FGR_NM_MSK                       UINT8_C(0x2)
#define BMM350_DIS_FGR_NM_POS                       UINT8_C(0x1)
#define BMM350_DIS_CRST_AT_ALL_MSK                  UINT8_C(0x4)
#define BMM350_DIS_CRST_AT_ALL_POS                  UINT8_C(0x2)
#define BMM350_DIS_BR_FM_MSK                        UINT8_C(0x8)
#define BMM350_DIS_BR_FM_POS                        UINT8_C(0x3)
#define BMM350_FRC_EN_BUFF_MSK                      UINT8_C(0x1)
#define BMM350_FRC_EN_BUFF_POS                      UINT8_C(0x0)
#define BMM350_FRC_INA_EN1_MSK                      UINT8_C(0x2)
#define BMM350_FRC_INA_EN1_POS                      UINT8_C(0x1)
#define BMM350_FRC_INA_EN2_MSK                      UINT8_C(0x4)
#define BMM350_FRC_INA_EN2_POS                      UINT8_C(0x2)
#define BMM350_FRC_ADC_EN_MSK                       UINT8_C(0x8)
#define BMM350_FRC_ADC_EN_POS                       UINT8_C(0x3)
#define BMM350_FRC_INA_RST_MSK                      UINT8_C(0x10)
#define BMM350_FRC_INA_RST_POS                      UINT8_C(0x4)
#define BMM350_FRC_ADC_RST_MSK                      UINT8_C(0x20)
#define BMM350_FRC_ADC_RST_POS                      UINT8_C(0x5)
#define BMM350_FRC_INA_XSEL_MSK                     UINT8_C(0x1)
#define BMM350_FRC_INA_XSEL_POS                     UINT8_C(0x0)
#define BMM350_FRC_INA_YSEL_MSK                     UINT8_C(0x2)
#define BMM350_FRC_INA_YSEL_POS                     UINT8_C(0x1)
#define BMM350_FRC_INA_ZSEL_MSK                     UINT8_C(0x4)
#define BMM350_FRC_INA_ZSEL_POS                     UINT8_C(0x2)
#define BMM350_FRC_ADC_TEMP_EN_MSK                  UINT8_C(0x8)
#define BMM350_FRC_ADC_TEMP_EN_POS                  UINT8_C(0x3)
#define BMM350_FRC_TSENS_EN_MSK                     UINT8_C(0x10)
#define BMM350_FRC_TSENS_EN_POS                     UINT8_C(0x4)
#define BMM350_DSENS_FM_MSK                         UINT8_C(0x20)
#define BMM350_DSENS_FM_POS                         UINT8_C(0x5)
#define BMM350_DSENS_SEL_MSK                        UINT8_C(0x40)
#define BMM350_DSENS_SEL_POS                        UINT8_C(0x6)
#define BMM350_DSENS_SHORT_MSK                      UINT8_C(0x80)
#define BMM350_DSENS_SHORT_POS                      UINT8_C(0x7)
#define BMM350_ERR_MISS_BR_DONE_MSK                 UINT8_C(0x1)
#define BMM350_ERR_MISS_BR_DONE_POS                 UINT8_C(0x0)
#define BMM350_ERR_MISS_FGR_DONE_MSK                UINT8_C(0x2)
#define BMM350_ERR_MISS_FGR_DONE_POS                UINT8_C(0x1)
#define BMM350_TST_CHAIN_LN_MODE_MSK                UINT8_C(0x1)
#define BMM350_TST_CHAIN_LN_MODE_POS                UINT8_C(0x0)
#define BMM350_TST_CHAIN_LP_MODE_MSK                UINT8_C(0x2)
#define BMM350_TST_CHAIN_LP_MODE_POS                UINT8_C(0x1)
#define BMM350_EN_OVWR_TMR_IF_MSK                   UINT8_C(0x1)
#define BMM350_EN_OVWR_TMR_IF_POS                   UINT8_C(0x0)
#define BMM350_TMR_CKTRIGB_MSK                      UINT8_C(0x2)
#define BMM350_TMR_CKTRIGB_POS                      UINT8_C(0x1)
#define BMM350_TMR_DO_BR_MSK                        UINT8_C(0x4)
#define BMM350_TMR_DO_BR_POS                        UINT8_C(0x2)
#define BMM350_TMR_DO_FGR_MSK                       UINT8_C(0x18)
#define BMM350_TMR_DO_FGR_POS                       UINT8_C(0x3)
#define BMM350_TMR_EN_OSC_MSK                       UINT8_C(0x80)
#define BMM350_TMR_EN_OSC_POS                       UINT8_C(0x7)
#define BMM350_VCM_TRIM_X_MSK                       UINT8_C(0x1f)
#define BMM350_VCM_TRIM_X_POS                       UINT8_C(0x0)
#define BMM350_VCM_TRIM_Y_MSK                       UINT8_C(0x1f)
#define BMM350_VCM_TRIM_Y_POS                       UINT8_C(0x0)
#define BMM350_VCM_TRIM_Z_MSK                       UINT8_C(0x1f)
#define BMM350_VCM_TRIM_Z_POS                       UINT8_C(0x0)
#define BMM350_VCM_TRIM_DSENS_MSK                   UINT8_C(0x1f)
#define BMM350_VCM_TRIM_DSENS_POS                   UINT8_C(0x0)
#define BMM350_TWLB_MSK                             UINT8_C(0x30)
#define BMM350_TWLB_POS                             UINT8_C(0x4)
#define BMM350_PRG_PLS_TIM_MSK                      UINT8_C(0x30)
#define BMM350_PRG_PLS_TIM_POS                      UINT8_C(0x4)
#define BMM350_OTP_OVWR_EN_MSK                      UINT8_C(0x1)
#define BMM350_OTP_OVWR_EN_POS                      UINT8_C(0x0)
#define BMM350_OTP_MEM_CLK_MSK                      UINT8_C(0x2)
#define BMM350_OTP_MEM_CLK_POS                      UINT8_C(0x1)
#define BMM350_OTP_MEM_CS_MSK                       UINT8_C(0x4)
#define BMM350_OTP_MEM_CS_POS                       UINT8_C(0x2)
#define BMM350_OTP_MEM_PGM_MSK                      UINT8_C(0x8)
#define BMM350_OTP_MEM_PGM_POS                      UINT8_C(0x3)
#define BMM350_OTP_MEM_RE_MSK                       UINT8_C(0x10)
#define BMM350_OTP_MEM_RE_POS                       UINT8_C(0x4)
#define BMM350_SAMPLE_RDATA_PLS_MSK                 UINT8_C(0x80)
#define BMM350_SAMPLE_RDATA_PLS_POS                 UINT8_C(0x7)
#define BMM350_CFG_FW_MSK                           UINT8_C(0x1)
#define BMM350_CFG_FW_POS                           UINT8_C(0x0)
#define BMM350_EN_BR_X_MSK                          UINT8_C(0x2)
#define BMM350_EN_BR_X_POS                          UINT8_C(0x1)
#define BMM350_EN_BR_Y_MSK                          UINT8_C(0x4)
#define BMM350_EN_BR_Y_POS                          UINT8_C(0x2)
#define BMM350_EN_BR_Z_MSK                          UINT8_C(0x8)
#define BMM350_EN_BR_Z_POS                          UINT8_C(0x3)
#define BMM350_CFG_PAUSE_TIME_MSK                   UINT8_C(0x30)
#define BMM350_CFG_PAUSE_TIME_POS                   UINT8_C(0x4)
#define BMM350_CFG_FGR_PLS_DUR_MSK                  UINT8_C(0xf)
#define BMM350_CFG_FGR_PLS_DUR_POS                  UINT8_C(0x0)
#define BMM350_CFG_BR_Z_ORDER_MSK                   UINT8_C(0x10)
#define BMM350_CFG_BR_Z_ORDER_POS                   UINT8_C(0x4)
#define BMM350_CFG_BR_XY_CHOP_MSK                   UINT8_C(0x20)
#define BMM350_CFG_BR_XY_CHOP_POS                   UINT8_C(0x5)
#define BMM350_CFG_BR_PLS_DUR_MSK                   UINT8_C(0xc0)
#define BMM350_CFG_BR_PLS_DUR_POS                   UINT8_C(0x6)
#define BMM350_ENABLE_BR_FGR_TEST_MSK               UINT8_C(0x1)
#define BMM350_ENABLE_BR_FGR_TEST_POS               UINT8_C(0x0)
#define BMM350_SEL_AXIS_MSK                         UINT8_C(0xe)
#define BMM350_SEL_AXIS_POS                         UINT8_C(0x1)
#define BMM350_TMR_CFG_TEST_CLK_EN_MSK              UINT8_C(0x10)
#define BMM350_TMR_CFG_TEST_CLK_EN_POS              UINT8_C(0x4)
#define BMM350_TEST_VAL_BITS_7DOWNTO0_MSK           UINT8_C(0xff)
#define BMM350_TEST_VAL_BITS_7DOWNTO0_POS           UINT8_C(0x0)
#define BMM350_TEST_VAL_BITS_8_MSK                  UINT8_C(0x1)
#define BMM350_TEST_VAL_BITS_8_POS                  UINT8_C(0x0)
#define BMM350_TEST_P_SAMPLE_MSK                    UINT8_C(0x2)
#define BMM350_TEST_P_SAMPLE_POS                    UINT8_C(0x1)
#define BMM350_TEST_N_SAMPLE_MSK                    UINT8_C(0x4)
#define BMM350_TEST_N_SAMPLE_POS                    UINT8_C(0x2)
#define BMM350_TEST_APPLY_TO_REM_MSK                UINT8_C(0x10)
#define BMM350_TEST_APPLY_TO_REM_POS                UINT8_C(0x4)
#define BMM350_UFO_TRM_OSC_RANGE_MSK                UINT8_C(0xf)
#define BMM350_UFO_TRM_OSC_RANGE_POS                UINT8_C(0x0)
#define BMM350_ISO_CHIP_ID_MSK                      UINT8_C(0x78)
#define BMM350_ISO_CHIP_ID_POS                      UINT8_C(0x3)
#define BMM350_ISO_I2C_DEV_ID_MSK                   UINT8_C(0x80)
#define BMM350_ISO_I2C_DEV_ID_POS                   UINT8_C(0x7)
#define BMM350_I3C_FREQ_BITS_1DOWNTO0_MSK           UINT8_C(0xc)
#define BMM350_I3C_FREQ_BITS_1DOWNTO0_POS           UINT8_C(0x2)
#define BMM350_I3C_IBI_MDB_SEL_MSK                  UINT8_C(0x10)
#define BMM350_I3C_IBI_MDB_SEL_POS                  UINT8_C(0x4)
#define BMM350_TC_ASYNC_EN_MSK                      UINT8_C(0x20)
#define BMM350_TC_ASYNC_EN_POS                      UINT8_C(0x5)
#define BMM350_TC_SYNC_EN_MSK                       UINT8_C(0x40)
#define BMM350_TC_SYNC_EN_POS                       UINT8_C(0x6)
#define BMM350_I3C_SCL_GATING_EN_MSK                UINT8_C(0x80)
#define BMM350_I3C_SCL_GATING_EN_POS                UINT8_C(0x7)
#define BMM350_I3C_INACCURACY_BITS_6DOWNTO0_MSK     UINT8_C(0x7f)
#define BMM350_I3C_INACCURACY_BITS_6DOWNTO0_POS     UINT8_C(0x0)
#define BMM350_EST_EN_X_MSK                         UINT8_C(0x1)
#define BMM350_EST_EN_X_POS                         UINT8_C(0x0)
#define BMM350_EST_EN_Y_MSK                         UINT8_C(0x2)
#define BMM350_EST_EN_Y_POS                         UINT8_C(0x1)
#define BMM350_CRST_DIS_MSK                         UINT8_C(0x4)
#define BMM350_CRST_DIS_POS                         UINT8_C(0x2)
#define BMM350_BR_TFALL_MSK                         UINT8_C(0x7)
#define BMM350_BR_TFALL_POS                         UINT8_C(0x0)
#define BMM350_BR_TRISE_MSK                         UINT8_C(0x70)
#define BMM350_BR_TRISE_POS                         UINT8_C(0x4)
#define BMM350_TMR_SOFT_START_DIS_MSK               UINT8_C(0x80)
#define BMM350_TMR_SOFT_START_DIS_POS               UINT8_C(0x7)
#define BMM350_FOSC_LOW_RANGE_MSK                   UINT8_C(0x80)
#define BMM350_FOSC_LOW_RANGE_POS                   UINT8_C(0x7)
#define BMM350_VCRST_TRIM_FG_MSK                    UINT8_C(0x3f)
#define BMM350_VCRST_TRIM_FG_POS                    UINT8_C(0x0)
#define BMM350_VCRST_TRIM_BR_MSK                    UINT8_C(0x3f)
#define BMM350_VCRST_TRIM_BR_POS                    UINT8_C(0x0)
#define BMM350_BG_TRIM_VRP_MSK                      UINT8_C(0xc0)
#define BMM350_BG_TRIM_VRP_POS                      UINT8_C(0x6)
#define BMM350_BG_TRIM_TC_MSK                       UINT8_C(0xf)
#define BMM350_BG_TRIM_TC_POS                       UINT8_C(0x0)
#define BMM350_BG_TRIM_VRA_MSK                      UINT8_C(0xf0)
#define BMM350_BG_TRIM_VRA_POS                      UINT8_C(0x4)
#define BMM350_BG_TRIM_VRD_MSK                      UINT8_C(0xf)
#define BMM350_BG_TRIM_VRD_POS                      UINT8_C(0x0)
#define BMM350_OVWR_REF_IB_EN_MSK                   UINT8_C(0x10)
#define BMM350_OVWR_REF_IB_EN_POS                   UINT8_C(0x4)
#define BMM350_OVWR_VDDA_EN_MSK                     UINT8_C(0x20)
#define BMM350_OVWR_VDDA_EN_POS                     UINT8_C(0x5)
#define BMM350_OVWR_VDDP_EN_MSK                     UINT8_C(0x40)
#define BMM350_OVWR_VDDP_EN_POS                     UINT8_C(0x6)
#define BMM350_OVWR_VDDS_EN_MSK                     UINT8_C(0x80)
#define BMM350_OVWR_VDDS_EN_POS                     UINT8_C(0x7)
#define BMM350_REF_IB_EN_MSK                        UINT8_C(0x10)
#define BMM350_REF_IB_EN_POS                        UINT8_C(0x4)
#define BMM350_VDDA_EN_MSK                          UINT8_C(0x20)
#define BMM350_VDDA_EN_POS                          UINT8_C(0x5)
#define BMM350_VDDP_EN_MSK                          UINT8_C(0x40)
#define BMM350_VDDP_EN_POS                          UINT8_C(0x6)
#define BMM350_VDDS_EN_MSK                          UINT8_C(0x80)
#define BMM350_VDDS_EN_POS                          UINT8_C(0x7)
#define BMM350_OVWR_OTP_PROG_VDD_SW_EN_MSK          UINT8_C(0x8)
#define BMM350_OVWR_OTP_PROG_VDD_SW_EN_POS          UINT8_C(0x3)
#define BMM350_OVWR_EN_MFE_BG_FILT_BYPASS_MSK       UINT8_C(0x10)
#define BMM350_OVWR_EN_MFE_BG_FILT_BYPASS_POS       UINT8_C(0x4)
#define BMM350_OTP_PROG_VDD_SW_EN_MSK               UINT8_C(0x8)
#define BMM350_OTP_PROG_VDD_SW_EN_POS               UINT8_C(0x3)
#define BMM350_CP_COMP_CRST_EN_TM_MSK               UINT8_C(0x10)
#define BMM350_CP_COMP_CRST_EN_TM_POS               UINT8_C(0x4)
#define BMM350_CP_COMP_VDD_EN_TM_MSK                UINT8_C(0x20)
#define BMM350_CP_COMP_VDD_EN_TM_POS                UINT8_C(0x5)
#define BMM350_CP_INTREFS_EN_TM_MSK                 UINT8_C(0x40)
#define BMM350_CP_INTREFS_EN_TM_POS                 UINT8_C(0x6)
#define BMM350_ADC_LOCAL_CHOP_EN_MSK                UINT8_C(0x20)
#define BMM350_ADC_LOCAL_CHOP_EN_POS                UINT8_C(0x5)
#define BMM350_INA_MODE_MSK                         UINT8_C(0x40)
#define BMM350_INA_MODE_POS                         UINT8_C(0x6)
#define BMM350_VDDD_EXT_EN_MSK                      UINT8_C(0x20)
#define BMM350_VDDD_EXT_EN_POS                      UINT8_C(0x5)
#define BMM350_VDDP_EXT_EN_MSK                      UINT8_C(0x80)
#define BMM350_VDDP_EXT_EN_POS                      UINT8_C(0x7)
#define BMM350_ADC_DSENS_EN_MSK                     UINT8_C(0x10)
#define BMM350_ADC_DSENS_EN_POS                     UINT8_C(0x4)
#define BMM350_DSENS_EN_MSK                         UINT8_C(0x20)
#define BMM350_DSENS_EN_POS                         UINT8_C(0x5)
#define BMM350_OTP_TM_CLVWR_EN_MSK                  UINT8_C(0x40)
#define BMM350_OTP_TM_CLVWR_EN_POS                  UINT8_C(0x6)
#define BMM350_OTP_VDDP_DIS_MSK                     UINT8_C(0x80)
#define BMM350_OTP_VDDP_DIS_POS                     UINT8_C(0x7)
#define BMM350_FORCE_HIGH_VREF_IREF_OK_MSK          UINT8_C(0x10)
#define BMM350_FORCE_HIGH_VREF_IREF_OK_POS          UINT8_C(0x4)
#define BMM350_FORCE_HIGH_FOSC_OK_MSK               UINT8_C(0x20)
#define BMM350_FORCE_HIGH_FOSC_OK_POS               UINT8_C(0x5)
#define BMM350_FORCE_HIGH_MFE_BG_RDY_MSK            UINT8_C(0x40)
#define BMM350_FORCE_HIGH_MFE_BG_RDY_POS            UINT8_C(0x6)
#define BMM350_FORCE_HIGH_MFE_VTMR_RDY_MSK          UINT8_C(0x80)
#define BMM350_FORCE_HIGH_MFE_VTMR_RDY_POS          UINT8_C(0x7)
#define BMM350_ERR_END_OF_RECHARGE_MSK              UINT8_C(0x1)
#define BMM350_ERR_END_OF_RECHARGE_POS              UINT8_C(0x0)
#define BMM350_ERR_END_OF_DISCHARGE_MSK             UINT8_C(0x2)
#define BMM350_ERR_END_OF_DISCHARGE_POS             UINT8_C(0x1)
#define BMM350_CP_TMX_DIGTP_SEL_MSK                 UINT8_C(0x7)
#define BMM350_CP_TMX_DIGTP_SEL_POS                 UINT8_C(0x0)
#define BMM350_CP_CPOSC_EN_TM_MSK                   UINT8_C(0x80)
#define BMM350_CP_CPOSC_EN_TM_POS                   UINT8_C(0x7)
#define BMM350_TST_ATM1_CFG_MSK                     UINT8_C(0x3f)
#define BMM350_TST_ATM1_CFG_POS                     UINT8_C(0x0)
#define BMM350_TST_TB1_EN_MSK                       UINT8_C(0x80)
#define BMM350_TST_TB1_EN_POS                       UINT8_C(0x7)
#define BMM350_TST_ATM2_CFG_MSK                     UINT8_C(0x1f)
#define BMM350_TST_ATM2_CFG_POS                     UINT8_C(0x0)
#define BMM350_TST_TB2_EN_MSK                       UINT8_C(0x80)
#define BMM350_TST_TB2_EN_POS                       UINT8_C(0x7)
#define BMM350_REG_DTB1X_SEL_MSK                    UINT8_C(0x7f)
#define BMM350_REG_DTB1X_SEL_POS                    UINT8_C(0x0)
#define BMM350_SEL_DTB1X_PAD_MSK                    UINT8_C(0x80)
#define BMM350_SEL_DTB1X_PAD_POS                    UINT8_C(0x7)
#define BMM350_REG_DTB2X_SEL_MSK                    UINT8_C(0x7f)
#define BMM350_REG_DTB2X_SEL_POS                    UINT8_C(0x0)
#define BMM350_TMR_TST_CFG_MSK                      UINT8_C(0x7f)
#define BMM350_TMR_TST_CFG_POS                      UINT8_C(0x0)
#define BMM350_TMR_TST_HIZ_VTMR_MSK                 UINT8_C(0x80)
#define BMM350_TMR_TST_HIZ_VTMR_POS                 UINT8_C(0x7)

/****************************** OTP MACROS ***************************/
#define BMM350_OTP_CMD_DIR_READ                     UINT8_C(0x20)
#define BMM350_OTP_CMD_DIR_PRGM_1B                  UINT8_C(0x40)
#define BMM350_OTP_CMD_DIR_PRGM                     UINT8_C(0x60)
#define BMM350_OTP_CMD_PWR_OFF_OTP                  UINT8_C(0x80)
#define BMM350_OTP_CMD_EXT_READ                     UINT8_C(0xA0)
#define BMM350_OTP_CMD_EXT_PRGM                     UINT8_C(0xE0)
#define BMM350_OTP_CMD_MSK                          UINT8_C(0xE0)
#define BMM350_OTP_WORD_ADDR_MSK                    UINT8_C(0x1F)

#define BMM350_OTP_STATUS_ERROR_MSK                 UINT8_C(0xE0)
#define BMM350_OTP_STATUS_ERROR(val)                (val & BMM350_OTP_STATUS_ERROR_MSK)
#define BMM350_OTP_STATUS_NO_ERROR                  UINT8_C(0x00)
#define BMM350_OTP_STATUS_BOOT_ERR                  UINT8_C(0x20)
#define BMM350_OTP_STATUS_PAGE_RD_ERR               UINT8_C(0x40)
#define BMM350_OTP_STATUS_PAGE_PRG_ERR              UINT8_C(0x60)
#define BMM350_OTP_STATUS_SIGN_ERR                  UINT8_C(0x80)
#define BMM350_OTP_STATUS_INV_CMD_ERR               UINT8_C(0xA0)
#define BMM350_OTP_STATUS_CMD_DONE                  UINT8_C(0x01)

/****************************** OTP indices ***************************/
#define BMM350_TEMP_OFF_SENS                        UINT8_C(0x0D)

#define BMM350_MAG_OFFSET_X                         UINT8_C(0x0E)
#define BMM350_MAG_OFFSET_Y                         UINT8_C(0x0F)
#define BMM350_MAG_OFFSET_Z                         UINT8_C(0x10)

#define BMM350_MAG_SENS_X                           UINT8_C(0x10)
#define BMM350_MAG_SENS_Y                           UINT8_C(0x11)
#define BMM350_MAG_SENS_Z                           UINT8_C(0x11)

#define BMM350_MAG_TCO_X                            UINT8_C(0x12)
#define BMM350_MAG_TCO_Y                            UINT8_C(0x13)
#define BMM350_MAG_TCO_Z                            UINT8_C(0x14)

#define BMM350_MAG_TCS_X                            UINT8_C(0x12)
#define BMM350_MAG_TCS_Y                            UINT8_C(0x13)
#define BMM350_MAG_TCS_Z                            UINT8_C(0x14)

#define BMM350_MAG_DUT_T_0                          UINT8_C(0x18)

#define BMM350_CROSS_X_Y                            UINT8_C(0x15)
#define BMM350_CROSS_Y_X                            UINT8_C(0x15)
#define BMM350_CROSS_Z_X                            UINT8_C(0x16)
#define BMM350_CROSS_Z_Y                            UINT8_C(0x16)

#define BMM350_SENS_CORR_Y                          (0.01f)
#define BMM350_TCS_CORR_Z                           (0.0001f)

/**************************** Signed bit macros **********************/
#define BMM350_SIGNED_8_BIT                         UINT8_C(8)
#define BMM350_SIGNED_12_BIT                        UINT8_C(12)
#define BMM350_SIGNED_16_BIT                        UINT8_C(16)
#define BMM350_SIGNED_21_BIT                        UINT8_C(21)
#define BMM350_SIGNED_24_BIT                        UINT8_C(24)

/**************************** Self-test macros **********************/
#define BMM350_SELF_TEST_DISABLE                    UINT8_C(0x00)
#define BMM350_SELF_TEST_POS_X                      UINT8_C(0x0D)
#define BMM350_SELF_TEST_NEG_X                      UINT8_C(0x0B)
#define BMM350_SELF_TEST_POS_Y                      UINT8_C(0x15)
#define BMM350_SELF_TEST_NEG_Y                      UINT8_C(0x13)

#define BMM350_X_FM_XP_UST_MAX_LIMIT                INT16_C(150)
#define BMM350_X_FM_XP_UST_MIN_LIMIT                INT16_C(50)

#define BMM350_X_FM_XN_UST_MAX_LIMIT                INT16_C(-50)
#define BMM350_X_FM_XN_UST_MIN_LIMIT                INT16_C(-150)

#define BMM350_Y_FM_YP_UST_MAX_LIMIT                INT16_C(150)
#define BMM350_Y_FM_YP_UST_MIN_LIMIT                INT16_C(50)

#define BMM350_Y_FM_YN_UST_MAX_LIMIT                INT16_C(-50)
#define BMM350_Y_FM_YN_UST_MIN_LIMIT                INT16_C(-150)

/**************************** PMU command status 0 macros **********************/
#define BMM350_PMU_CMD_STATUS_0_SUS                 UINT8_C(0x00)
#define BMM350_PMU_CMD_STATUS_0_NM                  UINT8_C(0x01)
#define BMM350_PMU_CMD_STATUS_0_UPD_OAE             UINT8_C(0x02)
#define BMM350_PMU_CMD_STATUS_0_FM                  UINT8_C(0x03)
#define BMM350_PMU_CMD_STATUS_0_FM_FAST             UINT8_C(0x04)
#define BMM350_PMU_CMD_STATUS_0_FGR                 UINT8_C(0x05)
#define BMM350_PMU_CMD_STATUS_0_FGR_FAST            UINT8_C(0x06)
#define BMM350_PMU_CMD_STATUS_0_BR                  UINT8_C(0x07)
#define BMM350_PMU_CMD_STATUS_0_BR_FAST             UINT8_C(0x07)

/****************************** Enumerators ***************************/
enum bmm350_interrupt_enable_disable {
    BMM350_DISABLE_INTERRUPT = BMM350_DISABLE,
    BMM350_ENABLE_INTERRUPT = BMM350_ENABLE
};

enum bmm350_power_modes {
    BMM350_SUSPEND_MODE = BMM350_PMU_CMD_SUS,
    BMM350_NORMAL_MODE = BMM350_PMU_CMD_NM,
    BMM350_FORCED_MODE = BMM350_PMU_CMD_FM,
    BMM350_FORCED_MODE_FAST = BMM350_PMU_CMD_FM_FAST
};

enum bmm350_data_rates {
    BMM350_DATA_RATE_400HZ    = BMM350_ODR_400HZ,
    BMM350_DATA_RATE_200HZ    = BMM350_ODR_200HZ,
    BMM350_DATA_RATE_100HZ    = BMM350_ODR_100HZ,
    BMM350_DATA_RATE_50HZ     = BMM350_ODR_50HZ,
    BMM350_DATA_RATE_25HZ     = BMM350_ODR_25HZ,
    BMM350_DATA_RATE_12_5HZ   = BMM350_ODR_12_5HZ,
    BMM350_DATA_RATE_6_25HZ   = BMM350_ODR_6_25HZ,
    BMM350_DATA_RATE_3_125HZ  = BMM350_ODR_3_125HZ,
    BMM350_DATA_RATE_1_5625HZ = BMM350_ODR_1_5625HZ
};

enum bmm350_magreset_type {
    BMM350_FLUXGUIDE_9MS = BMM350_PMU_CMD_FGR,
    BMM350_FLUXGUIDE_FAST = BMM350_PMU_CMD_FGR_FAST,
    BMM350_BITRESET_9MS = BMM350_PMU_CMD_BR,
    BMM350_BITRESET_FAST = BMM350_PMU_CMD_BR_FAST,
    BMM350_NOMAGRESET = UINT8_C(127)
};

enum bmm350_intr_en_dis {
    BMM350_INTR_DISABLE = BMM350_DISABLE,
    BMM350_INTR_ENABLE = BMM350_ENABLE
};

enum bmm350_intr_map {
    BMM350_UNMAP_FROM_PIN = BMM350_DISABLE,
    BMM350_MAP_TO_PIN = BMM350_ENABLE
};
enum bmm350_intr_latch {
    BMM350_PULSED = BMM350_INT_MODE_PULSED,
    BMM350_LATCHED = BMM350_INT_MODE_LATCHED
};

enum bmm350_intr_polarity {
    BMM350_ACTIVE_LOW = BMM350_INT_POL_ACTIVE_LOW,
    BMM350_ACTIVE_HIGH = BMM350_INT_POL_ACTIVE_HIGH
};

enum bmm350_intr_drive {
    BMM350_INTR_OPEN_DRAIN = BMM350_INT_OD_OPENDRAIN,
    BMM350_INTR_PUSH_PULL = BMM350_INT_OD_PUSHPULL
};

enum bmm350_drdy_int_map_to_ibi {
    BMM350_IBI_DISABLE = BMM350_DISABLE,
    BMM350_IBI_ENABLE = BMM350_ENABLE
};

enum bmm350_clear_drdy_int_status_upon_ibi {
    BMM350_NOCLEAR_ON_IBI = BMM350_DISABLE,
    BMM350_CLEAR_ON_IBI = BMM350_ENABLE
};

enum bmm350_i2c_wdt_en {
    BMM350_I2C_WDT_DIS = BMM350_DISABLE,
    BMM350_I2C_WDT_EN = BMM350_ENABLE
};

enum bmm350_i2c_wdt_sel {
    BMM350_I2C_WDT_SEL_SHORT = BMM350_DISABLE,
    BMM350_I2C_WDT_SEL_LONG = BMM350_ENABLE
};

enum bmm350_performance_parameters {
    BMM350_NO_AVERAGING = BMM350_AVG_NO_AVG,
    BMM350_AVERAGING_2 = BMM350_AVG_2,
    BMM350_AVERAGING_4 = BMM350_AVG_4,
    BMM350_AVERAGING_8 = BMM350_AVG_8,
    /*lint -e849*/
    BMM350_ULTRALOWNOISE = BMM350_AVG_8,
    BMM350_LOWNOISE = BMM350_AVG_4,
    BMM350_REGULARPOWER = BMM350_AVG_2,
    BMM350_LOWPOWER = BMM350_AVG_NO_AVG
};

enum bmm350_st_igen_en {
    BMM350_ST_IGEN_DIS = BMM350_DISABLE,
    BMM350_ST_IGEN_EN = BMM350_ENABLE
};

enum bmm350_st_n {
    BMM350_ST_N_DIS = BMM350_DISABLE,
    BMM350_ST_N_EN = BMM350_ENABLE
};

enum bmm350_st_p {
    BMM350_ST_P_DIS = BMM350_DISABLE,
    BMM350_ST_P_EN = BMM350_ENABLE
};

enum bmm350_ist_en_x {
    BMM350_IST_X_DIS = BMM350_DISABLE,
    BMM350_IST_X_EN = BMM350_ENABLE
};

enum bmm350_ist_en_y {
    BMM350_IST_Y_DIS = BMM350_DISABLE,
    BMM350_IST_Y_EN = BMM350_ENABLE
};

enum bmm350_ctrl_user {
    BMM350_CFG_SENS_TIM_AON_DIS = BMM350_DISABLE,
    BMM350_CFG_SENS_TIM_AON_EN = BMM350_ENABLE
};

enum bmm350_x_axis_en_dis {
    BMM350_X_DIS = BMM350_DISABLE,
    BMM350_X_EN = BMM350_ENABLE
};

enum bmm350_y_axis_en_dis {
    BMM350_Y_DIS = BMM350_DISABLE,
    BMM350_Y_EN = BMM350_ENABLE
};

enum bmm350_z_axis_en_dis {
    BMM350_Z_DIS = BMM350_DISABLE,
    BMM350_Z_EN = BMM350_ENABLE
};

/******************************************************************************/
/*! @name           Function Pointers                             */
/******************************************************************************/

/*!
 * @brief Bus communication function pointer which should be mapped to
 * the platform specific read functions of the user
 *
 * @param[in] reg_addr       : Register address from which data is read.
 * @param[out] reg_data      : Pointer to data buffer where read data is stored.
 * @param[in] len            : Number of bytes of data to be read.
 * @param[in, out] intf_ptr  : Void pointer that can enable the linking of descriptors
 *                             for interface related call backs.
 *
 * retval = 0 -> Success
 * retval < 0 -> Failure
 *
 */
typedef BMM350_INTF_RET_TYPE (*bmm350_read_fptr_t)(uint8_t reg_addr, uint8_t *reg_data, uint32_t len, void *intf_ptr);

/*!
 * @brief Bus communication function pointer which should be mapped to
 * the platform specific write functions of the user
 *
 * @param[in] reg_addr      : Register address to which the data is written.
 * @param[in] reg_data      : Pointer to data buffer in which data to be written
 *                            is stored.
 * @param[in] len           : Number of bytes of data to be written.
 * @param[in, out] intf_ptr : Void pointer that can enable the linking of descriptors
 *                            for interface related call backs
 *
 * retval = 0 -> Success
 * retval < 0 -> Failure
 *
 */
typedef BMM350_INTF_RET_TYPE (*bmm350_write_fptr_t)(uint8_t reg_addr, const uint8_t *reg_data, uint32_t len,
        void *intf_ptr);

/*!
 * @brief Delay function pointer which should be mapped to
 * delay function of the user
 *
 * @param[in] period              : Delay in microseconds.
 * @param[in, out] intf_ptr       : Void pointer that can enable the linking of descriptors
 *                                  for interface related call backs
 *
 */
typedef void (*bmm350_delay_us_fptr_t)(uint32_t period, void *intf_ptr);

/* Pre-declaration */
struct bmm350_dev;

/*!
 * @brief Function pointer for the magnetic reset and wait override
 *
 * @param[in]  dev              : Structure instance of bmm350_dev.
 * @return Result of API execution status
 *  @retval = 0 -> Success
 *  @retval < 0 -> Error
 */
typedef int8_t (*bmm350_mraw_override_t)(struct bmm350_dev *dev);

/*************************  STRUCTURE DEFINITIONS *************************/

/*!
 * @brief bmm350 un-compensated (raw) magnetometer data, signed integer
 */
struct bmm350_raw_mag_data {
    /*! Raw mag X data */
    int32_t raw_xdata;

    /*! Raw mag Y data */
    int32_t raw_ydata;

    /*! Raw mag Z data */
    int32_t raw_zdata;

    /*! Raw mag temperature value */
    int32_t raw_data_t;
};

/*!
 * @brief bmm350 compensated magnetometer data and temperature data
 */
struct bmm350_mag_temp_data {
    /*! Compensated mag X data */
    float x;

    /*! Compensated mag Y data */
    float y;

    /*! Compensated mag Z data */
    float z;

    /*! Temperature */
    float temperature;
};

/*!
 * @brief bmm350 magnetometer dut offset coefficient structure
 */
struct bmm350_dut_offset_coef {
    /*! Temperature offset */
    float t_offs;

    /*! Offset x-axis */
    float offset_x;

    /*! Offset y-axis */
    float offset_y;

    /*! Offset z-axis */
    float offset_z;
};

/*!
 * @brief bmm350 magnetometer dut sensitivity coefficient structure
 */
struct bmm350_dut_sensit_coef {
    /*! Temperature sensitivity */
    float t_sens;

    /*! Sensitivity x-axis */
    float sens_x;

    /*! Sensitivity y-axis */
    float sens_y;

    /*! Sensitivity z-axis */
    float sens_z;
};

/*!
 * @brief bmm350 magnetometer dut tco structure
 */
struct bmm350_dut_tco {
    float tco_x;
    float tco_y;
    float tco_z;
};

/*!
 * @brief bmm350 magnetometer dut tcs structure
 */
struct bmm350_dut_tcs {
    float tcs_x;
    float tcs_y;
    float tcs_z;
};

/*!
 * @brief bmm350 magnetometer cross axis compensation structure
 */
struct bmm350_cross_axis {
    float cross_x_y;
    float cross_y_x;
    float cross_z_x;
    float cross_z_y;
};

/*!
 * @brief bmm350 magnetometer compensate structure
 */
struct bmm350_mag_compensate {
    /*! Structure to store dut offset coefficient */
    struct bmm350_dut_offset_coef dut_offset_coef;

    /*! Structure to store dut sensitivity coefficient */
    struct bmm350_dut_sensit_coef dut_sensit_coef;

    /*! Structure to store dut tco */
    struct bmm350_dut_tco dut_tco;

    /*! Structure to store dut tcs */
    struct bmm350_dut_tcs dut_tcs;

    /*! Initialize T0_reading parameter */
    float dut_t0;

    /*! Structure to define cross axis compensation */
    struct bmm350_cross_axis cross_axis;
};

/*!
 * @brief bmm350 device structure
 */
struct bmm350_dev {
    /*!
     * The interface pointer is used to enable the user
     * to link their interface descriptors for reference during the
     * implementation of the read and write interfaces to the
     * hardware.
     */
    void *intf_ptr;

    /*! Chip Id of BMM350 */
    uint8_t chip_id;

    /*! Bus read function pointer */
    bmm350_read_fptr_t read;

    /*! Bus write function pointer */
    bmm350_write_fptr_t write;

    /*! delay(in us) function pointer */
    bmm350_delay_us_fptr_t delay_us;

    /*! To store interface pointer error */
    BMM350_INTF_RET_TYPE intf_rslt;

    /*! Variable to store status of axes enabled */
    uint8_t axis_en;

    /*! Structure for mag compensate */
    struct bmm350_mag_compensate mag_comp;

    /*! Array to store OTP data */
    uint16_t otp_data[BMM350_OTP_DATA_LENGTH];

    /*! Variant ID */
    uint8_t var_id;

    /*! Magnetic reset and wait override */
    bmm350_mraw_override_t mraw_override;
};

/*!
 * @brief bmm350 self-test structure
 */
struct bmm350_self_test {
    /* Variable to store self-test data on x-axis */
    float out_ust_x;

    /* Variable to store self-test data on y-axis */
    float out_ust_y;
};

/*!
 * @brief bmm350 PMU command status 0 structure
 */
struct bmm350_pmu_cmd_status_0 {
    /*! The previous PMU CMD is still in processing */
    uint8_t pmu_cmd_busy;

    /*! The previous PMU_CMD_AGGR_SET.odr has been overwritten */
    uint8_t odr_ovwr;

    /*! The previous PMU_CMD_AGGR_SET.avg has been overwritten */
    uint8_t avr_ovwr;

    /*! The chip is in normal power mode */
    uint8_t pwr_mode_is_normal;

    /*! CMD value is not allowed */
    uint8_t cmd_is_illegal;

    /*! Stores the latest PMU_CMD code processed */
    uint8_t pmu_cmd_value;
};

#endif /* _BMM350_DEFS_H */
