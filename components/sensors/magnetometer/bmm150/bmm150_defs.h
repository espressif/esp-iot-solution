/**
* Copyright (c) 2020 Bosch Sensortec GmbH. All rights reserved.
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
* @file       bmm150_defs.h
* @date       2020-06-03
* @version    v2.0.0
*
*/

/*! \file bmm150_defs.h */

#ifndef _BMM150_DEFS_H
#define _BMM150_DEFS_H

/******************************************************************************/
/*! @name       Header includes                           */
/******************************************************************************/
#ifdef __KERNEL__
#include <linux/types.h>
#else
#include <stdint.h>
#include <stddef.h>
#endif

/******************************************************************************/
/*! @name       Common macros                         */
/******************************************************************************/
#ifdef __KERNEL__
#if (LONG_MAX) > 0x7fffffff
#define __have_long64  1
#elif (LONG_MAX) == 0x7fffffff
#define __have_long32  1
#endif
#endif

#if !defined(UINT8_C)
#define INT8_C(x)      x
#if (INT_MAX) > 0x7f
#define UINT8_C(x)     x
#else
#define UINT8_C(x)     x##U
#endif
#endif

#if !defined(UINT16_C)
#define INT16_C(x)     x
#if (INT_MAX) > 0x7fff
#define UINT16_C(x)    x
#else
#define UINT16_C(x)    x##U
#endif
#endif

#if !defined(INT32_C) && !defined(UINT32_C)
#if __have_long32
#define INT32_C(x)     x##L
#define UINT32_C(x)    x##UL
#else
#define INT32_C(x)     x
#define UINT32_C(x)    x##U
#endif
#endif

#if !defined(INT64_C) && !defined(UINT64_C)
#if __have_long64
#define INT64_C(x)     x##L
#define UINT64_C(x)    x##UL
#else
#define INT64_C(x)     x##LL
#define UINT64_C(x)    x##ULL
#endif
#endif

/*! @name C standard macros */
#ifndef NULL
#ifdef __cplusplus
#define NULL           0
#else
#define NULL           ((void *) 0)
#endif
#endif

/******************************************************************************/
/*! @name        Compiler switch macros Definitions                */
/******************************************************************************/
#ifndef BMM150_USE_FLOATING_POINT /*< Check if floating point (using BMM150_USE_FLOATING_POINT) is enabled */
#ifndef BMM150_USE_FIXED_POINT /*< If floating point is not enabled then enable BMM150_USE_FIXED_POINT */
#define BMM150_USE_FIXED_POINT
#endif
#endif

/******************************************************************************/
/*! @name        General Macro Definitions                */
/******************************************************************************/
/*! @name API success code */
#define BMM150_OK                                 INT8_C(0)

/*! @name To define TRUE or FALSE */
#define BMM150_TRUE                               UINT8_C(1)
#define BMM150_FALSE                              UINT8_C(0)

/*! @name API error codes */
#define BMM150_E_NULL_PTR                         INT8_C(-1)
#define BMM150_E_DEV_NOT_FOUND                    INT8_C(-2)
#define BMM150_E_INVALID_CONFIG                   INT8_C(-3)
#define BMM150_E_COM_FAIL                         INT8_C(-4)

/*! @name API warning codes */
#define BMM150_W_NORMAL_SELF_TEST_YZ_FAIL         INT8_C(1)
#define BMM150_W_NORMAL_SELF_TEST_XZ_FAIL         INT8_C(2)
#define BMM150_W_NORMAL_SELF_TEST_Z_FAIL          INT8_C(3)
#define BMM150_W_NORMAL_SELF_TEST_XY_FAIL         INT8_C(4)
#define BMM150_W_NORMAL_SELF_TEST_Y_FAIL          INT8_C(5)
#define BMM150_W_NORMAL_SELF_TEST_X_FAIL          INT8_C(6)
#define BMM150_W_NORMAL_SELF_TEST_XYZ_FAIL        INT8_C(7)
#define BMM150_W_ADV_SELF_TEST_FAIL               INT8_C(8)

/*! @name CHIP ID & SOFT RESET VALUES      */
#define BMM150_CHIP_ID                            UINT8_C(0x32)
#define BMM150_SET_SOFT_RESET                     UINT8_C(0x82)

/*! @name POWER MODE DEFINTIONS      */
#define BMM150_POWERMODE_NORMAL                   UINT8_C(0x00)
#define BMM150_POWERMODE_FORCED                   UINT8_C(0x01)
#define BMM150_POWERMODE_SLEEP                    UINT8_C(0x03)
#define BMM150_POWERMODE_SUSPEND                  UINT8_C(0x04)

/*! @name Power mode settings  */
#define BMM150_POWER_CNTRL_DISABLE                UINT8_C(0x00)
#define BMM150_POWER_CNTRL_ENABLE                 UINT8_C(0x01)

/*! @name I2C ADDRESS       */
#define BMM150_DEFAULT_I2C_ADDRESS                UINT8_C(0x10)
#define BMM150_I2C_ADDRESS_CSB_LOW_SDO_HIGH       UINT8_C(0x11)
#define BMM150_I2C_ADDRESS_CSB_HIGH_SDO_LOW       UINT8_C(0x12)
#define BMM150_I2C_ADDRESS_CSB_HIGH_SDO_HIGH      UINT8_C(0x13)

/*! @name Sensor delay time settings  */
#define BMM150_DELAY_SOFT_RESET                   UINT8_C(1000)
#define BMM150_DELAY_NORMAL_SELF_TEST             UINT8_C(2000)
#define BMM150_START_UP_TIME                      UINT8_C(3000)
#define BMM150_DELAY_ADV_SELF_TEST                UINT8_C(4000)

/*! @name ENABLE/DISABLE DEFINITIONS  */
#define BMM150_XYZ_CHANNEL_ENABLE                 UINT8_C(0x00)
#define BMM150_XYZ_CHANNEL_DISABLE                UINT8_C(0x07)

/*! @name Register Address */
#define BMM150_REG_CHIP_ID                        UINT8_C(0x40)
#define BMM150_REG_DATA_X_LSB                     UINT8_C(0x42)
#define BMM150_REG_DATA_READY_STATUS              UINT8_C(0x48)
#define BMM150_REG_INTERRUPT_STATUS               UINT8_C(0x4A)
#define BMM150_REG_POWER_CONTROL                  UINT8_C(0x4B)
#define BMM150_REG_OP_MODE                        UINT8_C(0x4C)
#define BMM150_REG_INT_CONFIG                     UINT8_C(0x4D)
#define BMM150_REG_AXES_ENABLE                    UINT8_C(0x4E)
#define BMM150_REG_LOW_THRESHOLD                  UINT8_C(0x4F)
#define BMM150_REG_HIGH_THRESHOLD                 UINT8_C(0x50)
#define BMM150_REG_REP_XY                         UINT8_C(0x51)
#define BMM150_REG_REP_Z                          UINT8_C(0x52)

/*! @name Macros to select the sensor settings to be set by the user
 * These values are internal for API implementation. Don't relate this to
 * data sheet.
 */
#define BMM150_SEL_DATA_RATE                      UINT16_C(1)
#define BMM150_SEL_CONTROL_MEASURE                UINT16_C(1 << 1)
#define BMM150_SEL_XY_REP                         UINT16_C(1 << 2)
#define BMM150_SEL_Z_REP                          UINT16_C(1 << 3)
#define BMM150_SEL_DRDY_PIN_EN                    UINT16_C(1 << 4)
#define BMM150_SEL_INT_PIN_EN                     UINT16_C(1 << 5)
#define BMM150_SEL_DRDY_POLARITY                  UINT16_C(1 << 6)
#define BMM150_SEL_INT_LATCH                      UINT16_C(1 << 7)
#define BMM150_SEL_INT_POLARITY                   UINT16_C(1 << 8)
#define BMM150_SEL_DATA_OVERRUN_INT               UINT16_C(1 << 9)
#define BMM150_SEL_OVERFLOW_INT                   UINT16_C(1 << 10)
#define BMM150_SEL_HIGH_THRESHOLD_INT             UINT16_C(1 << 11)
#define BMM150_SEL_LOW_THRESHOLD_INT              UINT16_C(1 << 12)
#define BMM150_SEL_LOW_THRESHOLD_SETTING          UINT16_C(1 << 13)
#define BMM150_SEL_HIGH_THRESHOLD_SETTING         UINT16_C(1 << 14)

/*! @name DATA RATE DEFINITIONS  */
#define BMM150_DATA_RATE_10HZ                     UINT8_C(0x00)
#define BMM150_DATA_RATE_02HZ                     UINT8_C(0x01)
#define BMM150_DATA_RATE_06HZ                     UINT8_C(0x02)
#define BMM150_DATA_RATE_08HZ                     UINT8_C(0x03)
#define BMM150_DATA_RATE_15HZ                     UINT8_C(0x04)
#define BMM150_DATA_RATE_20HZ                     UINT8_C(0x05)
#define BMM150_DATA_RATE_25HZ                     UINT8_C(0x06)
#define BMM150_DATA_RATE_30HZ                     UINT8_C(0x07)
#define BMM150_ODR_MAX                            UINT8_C(0x07)
#define BMM150_ODR_MSK                            UINT8_C(0x38)
#define BMM150_ODR_POS                            UINT8_C(0x03)

/*! @name TRIM REGISTERS      */
/* Trim Extended Registers */
#define BMM150_DIG_X1                             UINT8_C(0x5D)
#define BMM150_DIG_Y1                             UINT8_C(0x5E)
#define BMM150_DIG_Z4_LSB                         UINT8_C(0x62)
#define BMM150_DIG_Z4_MSB                         UINT8_C(0x63)
#define BMM150_DIG_X2                             UINT8_C(0x64)
#define BMM150_DIG_Y2                             UINT8_C(0x65)
#define BMM150_DIG_Z2_LSB                         UINT8_C(0x68)
#define BMM150_DIG_Z2_MSB                         UINT8_C(0x69)
#define BMM150_DIG_Z1_LSB                         UINT8_C(0x6A)
#define BMM150_DIG_Z1_MSB                         UINT8_C(0x6B)
#define BMM150_DIG_XYZ1_LSB                       UINT8_C(0x6C)
#define BMM150_DIG_XYZ1_MSB                       UINT8_C(0x6D)
#define BMM150_DIG_Z3_LSB                         UINT8_C(0x6E)
#define BMM150_DIG_Z3_MSB                         UINT8_C(0x6F)
#define BMM150_DIG_XY2                            UINT8_C(0x70)
#define BMM150_DIG_XY1                            UINT8_C(0x71)

/*! @name Threshold interrupt setting macros for x,y,z axes selection */
#define BMM150_THRESHOLD_X                        UINT8_C(0x06)
#define BMM150_THRESHOLD_Y                        UINT8_C(0x05)
#define BMM150_THRESHOLD_Z                        UINT8_C(0x03)

#define BMM150_HIGH_THRESHOLD_INT_MSK             UINT8_C(0x38)
#define BMM150_HIGH_THRESHOLD_INT_POS             UINT8_C(0x03)
#define BMM150_LOW_THRESHOLD_INT_MSK              UINT8_C(0x07)

/*! @name User configurable interrupt setting macros */
#define BMM150_INT_ENABLE                         UINT8_C(0x01)
#define BMM150_INT_DISABLE                        UINT8_C(0x00)
#define BMM150_ACTIVE_HIGH_POLARITY               UINT8_C(0x01)
#define BMM150_ACTIVE_LOW_POLARITY                UINT8_C(0x00)
#define BMM150_LATCHED                            UINT8_C(0x01)
#define BMM150_NON_LATCHED                        UINT8_C(0x00)

/*! @name Interrupt status */
#define BMM150_INT_THRESHOLD_X_LOW                UINT16_C(0x0001)
#define BMM150_INT_THRESHOLD_Y_LOW                UINT16_C(0x0002)
#define BMM150_INT_THRESHOLD_Z_LOW                UINT16_C(0x0004)
#define BMM150_INT_THRESHOLD_X_HIGH               UINT16_C(0x0008)
#define BMM150_INT_THRESHOLD_Y_HIGH               UINT16_C(0x0010)
#define BMM150_INT_THRESHOLD_Z_HIGH               UINT16_C(0x0020)
#define BMM150_INT_DATA_OVERFLOW                  UINT16_C(0x0040)
#define BMM150_INT_DATA_OVERRUN                   UINT16_C(0x0080)
#define BMM150_INT_DATA_READY                     UINT16_C(0x0100)

#define BMM150_DRDY_EN_MSK                        UINT8_C(0x80)
#define BMM150_DRDY_EN_POS                        UINT8_C(0x07)
#define BMM150_DRDY_POLARITY_MSK                  UINT8_C(0x04)
#define BMM150_DRDY_POLARITY_POS                  UINT8_C(0x02)
#define BMM150_INT_PIN_EN_MSK                     UINT8_C(0x40)
#define BMM150_INT_PIN_EN_POS                     UINT8_C(0x06)
#define BMM150_INT_LATCH_MSK                      UINT8_C(0x02)
#define BMM150_INT_LATCH_POS                      UINT8_C(0x01)
#define BMM150_INT_POLARITY_MSK                   UINT8_C(0x01)
#define BMM150_DRDY_STATUS_MSK                    UINT8_C(0x01)

/*! @name Interrupt status macros */
#define BMM150_INT_ASSERTED_DRDY                  UINT16_C(0x0100)
#define BMM150_INT_ASSERTED_LOW_THRES             UINT16_C(0x0007)
#define BMM150_INT_ASSERTED_HIGH_THRES            UINT16_C(0x0380)

/*! @name Power control bit macros */
#define BMM150_PWR_CNTRL_MSK                      UINT8_C(0x01)
#define BMM150_CONTROL_MEASURE_MSK                UINT8_C(0x38)
#define BMM150_CONTROL_MEASURE_POS                UINT8_C(0x03)
#define BMM150_POWER_CONTROL_BIT_MSK              UINT8_C(0x01)
#define BMM150_POWER_CONTROL_BIT_POS              UINT8_C(0x00)

/*! @name Data macros */
#define BMM150_DATA_X_MSK                         UINT8_C(0xF8)
#define BMM150_DATA_X_POS                         UINT8_C(0x03)

#define BMM150_DATA_Y_MSK                         UINT8_C(0xF8)
#define BMM150_DATA_Y_POS                         UINT8_C(0x03)

#define BMM150_DATA_Z_MSK                         UINT8_C(0xFE)
#define BMM150_DATA_Z_POS                         UINT8_C(0x01)

#define BMM150_DATA_RHALL_MSK                     UINT8_C(0xFC)
#define BMM150_DATA_RHALL_POS                     UINT8_C(0x02)

#define BMM150_DATA_OVERRUN_INT_MSK               UINT8_C(0x80)
#define BMM150_DATA_OVERRUN_INT_POS               UINT8_C(0x07)

#define BMM150_OVERFLOW_INT_MSK                   UINT8_C(0x40)
#define BMM150_OVERFLOW_INT_POS                   UINT8_C(0x06)

/*! @name OVERFLOW DEFINITIONS  */
#define BMM150_OVERFLOW_ADCVAL_XYAXES_FLIP        INT16_C(-4096)
#define BMM150_OVERFLOW_ADCVAL_ZAXIS_HALL         INT16_C(-16384)
#define BMM150_OVERFLOW_OUTPUT                    INT16_C(-32768)
#define BMM150_NEGATIVE_SATURATION_Z              INT16_C(-32767)
#define BMM150_POSITIVE_SATURATION_Z              INT16_C(32767)
#ifdef BMM150_USE_FLOATING_POINT
#define BMM150_OVERFLOW_OUTPUT_FLOAT              0.0f
#endif

/*! @name PRESET MODE DEFINITIONS  */
#define BMM150_PRESETMODE_LOWPOWER                UINT8_C(0x01)
#define BMM150_PRESETMODE_REGULAR                 UINT8_C(0x02)
#define BMM150_PRESETMODE_HIGHACCURACY            UINT8_C(0x03)
#define BMM150_PRESETMODE_ENHANCED                UINT8_C(0x04)

#define BMM150_OP_MODE_MSK                        UINT8_C(0x06)
#define BMM150_OP_MODE_POS                        UINT8_C(0x01)

/*! @name PRESET MODES - REPETITIONS-XY RATES */
#define BMM150_REPXY_LOWPOWER                     UINT8_C(0x01)
#define BMM150_REPXY_REGULAR                      UINT8_C(0x04)
#define BMM150_REPXY_ENHANCED                     UINT8_C(0x07)
#define BMM150_REPXY_HIGHACCURACY                 UINT8_C(0x17)

/*! @name PRESET MODES - REPETITIONS-Z RATES */
#define BMM150_REPZ_LOWPOWER                      UINT8_C(0x01)
#define BMM150_REPZ_REGULAR                       UINT8_C(0x07)
#define BMM150_REPZ_ENHANCED                      UINT8_C(0x0D)
#define BMM150_REPZ_HIGHACCURACY                  UINT8_C(0x29)

/*! @name Self test settings */
#define BMM150_DISABLE_XY_AXIS                    UINT8_C(0x03)
#define BMM150_SELF_TEST_REP_Z                    UINT8_C(0x04)

/*! @name Self test selection macros */
#define BMM150_SELF_TEST_NORMAL                   UINT8_C(0)
#define BMM150_SELF_TEST_ADVANCED                 UINT8_C(1)

/*! @name Advanced self-test current settings */
#define BMM150_DISABLE_SELF_TEST_CURRENT          UINT8_C(0x00)
#define BMM150_ENABLE_NEGATIVE_CURRENT            UINT8_C(0x02)
#define BMM150_ENABLE_POSITIVE_CURRENT            UINT8_C(0x03)

/*! @name Normal self-test status */
#define BMM150_SELF_TEST_STATUS_XYZ_FAIL          UINT8_C(0x00)
#define BMM150_SELF_TEST_STATUS_SUCCESS           UINT8_C(0x07)

#define BMM150_SELF_TEST_MSK                      UINT8_C(0x01)
#define BMM150_ADV_SELF_TEST_MSK                  UINT8_C(0xC0)
#define BMM150_ADV_SELF_TEST_POS                  UINT8_C(0x06)

/*! @name Register read lengths  */
#define BMM150_LEN_SELF_TEST                      UINT8_C(5)
#define BMM150_LEN_SETTING_DATA                   UINT8_C(8)
#define BMM150_LEN_XYZR_DATA                      UINT8_C(8)

/*! @name Boundary check macros */
#define BMM150_BOUNDARY_MAXIMUM                   UINT8_C(0)
#define BMM150_BOUNDARY_MINIMUM                   UINT8_C(1)

/*! @name Macro to SET and GET BITS of a register*/
#define BMM150_SET_BITS(reg_data, bitname, data) \
    ((reg_data & ~(bitname##_MSK)) | \
     ((data << bitname##_POS) & bitname##_MSK))

#define BMM150_GET_BITS(reg_data, bitname)        ((reg_data & (bitname##_MSK)) >> \
                                                   (bitname##_POS))

#define BMM150_SET_BITS_POS_0(reg_data, bitname, data) \
    ((reg_data & ~(bitname##_MSK)) | \
     (data & bitname##_MSK))

#define BMM150_GET_BITS_POS_0(reg_data, bitname)  (reg_data & (bitname##_MSK))

/********************************************************/

/*!
 * @brief Interface selection Enums
 */
enum bmm150_intf {
    /*! SPI interface */
    BMM150_SPI_INTF,
    /*! I2C interface */
    BMM150_I2C_INTF
};

/******************************************************************************/
/*! @name           Function Pointers                             */
/******************************************************************************/
#ifndef BMM150_INTF_RET_TYPE
#define BMM150_INTF_RET_TYPE     int8_t
#endif

#ifndef BMM150_INTF_RET_SUCCESS
#define BMM150_INTF_RET_SUCCESS  INT8_C(0)
#endif

/*!
 * @brief Bus communication function pointer which should be mapped to
 * the platform specific read functions of the user
 *
 * @param[in]     reg_addr : 8bit register address of the sensor
 * @param[out]    reg_data : Data from the specified address
 * @param[in]     length   : Length of the reg_data array
 * @param[in,out] intf_ptr : Void pointer that can enable the linking of descriptors
 *                           for interface related callbacks
 * @retval 0 for Success
 * @retval Non-zero for Failure
 */
typedef BMM150_INTF_RET_TYPE (*bmm150_read_fptr_t)(uint8_t reg_addr, uint8_t *reg_data, uint32_t length,
                                                   void *intf_ptr);

/*!
 * @brief Bus communication function pointer which should be mapped to
 * the platform specific write functions of the user
 *
 * @param[in]     reg_addr : 8bit register address of the sensor
 * @param[out]    reg_data : Data to the specified address
 * @param[in]     length   : Length of the reg_data array
 * @param[in,out] intf_ptr : Void pointer that can enable the linking of descriptors
 *                           for interface related callbacks
 * @retval 0 for Success
 * @retval Non-zero for Failure
 *
 */
typedef BMM150_INTF_RET_TYPE (*bmm150_write_fptr_t)(uint8_t reg_addr, const uint8_t *reg_data, uint32_t length,
                                                    void *intf_ptr);

/*!
 * @brief Delay function pointer which should be mapped to
 * delay function of the user
 *
 * @param period           : The time period in microseconds
 * @param[in,out] intf_ptr : Void pointer that can enable the linking of descriptors
 *                           for interface related callbacks
 */
typedef void (*bmm150_delay_us_fptr_t)(uint32_t period, void *intf_ptr);

/******************************************************************************/
/*!  @name         Structure Declarations                             */
/******************************************************************************/

/*!
 * @brief bmm150 trim data structure
 */
struct bmm150_trim_registers
{
    /*! trim x1 data */
    int8_t dig_x1;

    /*! trim y1 data */
    int8_t dig_y1;

    /*! trim x2 data */
    int8_t dig_x2;

    /*! trim y2 data */
    int8_t dig_y2;

    /*! trim z1 data */
    uint16_t dig_z1;

    /*! trim z2 data */
    int16_t dig_z2;

    /*! trim z3 data */
    int16_t dig_z3;

    /*! trim z4 data */
    int16_t dig_z4;

    /*! trim xy1 data */
    uint8_t dig_xy1;

    /*! trim xy2 data */
    int8_t dig_xy2;

    /*! trim xyz1 data */
    uint16_t dig_xyz1;
};

/*!
 * @brief bmm150 interrupt pin settings
 */
struct bmm150_int_ctrl_settings
{
    /*! Data ready interrupt enable */
    uint8_t drdy_pin_en;

    /*! Threshold and overflow interrupts enable */
    uint8_t int_pin_en;

    /*! Data ready interrupt polarity Active high/low */
    uint8_t drdy_polarity;

    /*! Interrupt pin - Latched or Non-latched */
    uint8_t int_latch;

    /*! Interrupt polarity Active high/low */
    uint8_t int_polarity;

    /*! Data overrun interrupt enable */
    uint8_t data_overrun_en;

    /*! Overflow interrupt enable */
    uint8_t overflow_int_en;

    /*! high interrupt enable/disable axis selection */
    uint8_t high_int_en;

    /*! low interrupt enable/disable axis selection */
    uint8_t low_int_en;

    /*! low threshold limit */
    uint8_t low_threshold;

    /*! high threshold limit */
    uint8_t high_threshold;
};

/*!
 * @brief bmm150 sensor settings
 */
struct bmm150_settings
{
    /*! Control measurement of XYZ axes */
    uint8_t xyz_axes_control;

    /*! Power mode of sensor */
    uint8_t pwr_mode;

    /*! Data rate value (ODR) */
    uint8_t data_rate;

    /*! XY Repetitions */
    uint8_t xy_rep;

    /*! Z Repetitions */
    uint8_t z_rep;

    /*! Preset mode of sensor */
    uint8_t preset_mode;

    /*! Interrupt configuration settings */
    struct bmm150_int_ctrl_settings int_settings;
};

/*!
 * @brief bmm150 un-compensated (raw) magnetometer data
 */
struct bmm150_raw_mag_data
{
    /*! Raw mag X data */
    int16_t raw_datax;

    /*! Raw mag Y data */
    int16_t raw_datay;

    /*! Raw mag Z data */
    int16_t raw_dataz;

    /*! Raw mag resistance value */
    uint16_t raw_data_r;
};

#ifdef BMM150_USE_FLOATING_POINT

/*!
 * @brief bmm150 compensated magnetometer data in float
 */
struct bmm150_mag_data
{
    /*! compensated mag X data */
    float x;

    /*! compensated mag Y data */
    float y;

    /*! compensated mag Z data */
    float z;
};

#else

/*!
 * @brief bmm150 compensated magnetometer data in int16_t format
 */
struct bmm150_mag_data
{
    /*! compensated mag X data */
    int16_t x;

    /*! compensated mag Y data */
    int16_t y;

    /*! compensated mag Z data */
    int16_t z;
};

#endif

/*!
 * @brief bmm150 device structure
 */
struct bmm150_dev
{
    /*! Chip Id */
    uint8_t chip_id;

    /*! SPI/I2C Interface */
    enum bmm150_intf intf;

    /*!
     * The interface pointer is used to enable the user
     * to link their interface descriptors for reference during the
     * implementation of the read and write interfaces to the
     * hardware.
     */
    void *intf_ptr;

    /*! Variable that holds result of read/write function */
    BMM150_INTF_RET_TYPE intf_rslt;

    /*! Bus read function pointer */
    bmm150_read_fptr_t read;

    /*! Bus write function pointer */
    bmm150_write_fptr_t write;

    /*! delay(in us) function pointer */
    bmm150_delay_us_fptr_t delay_us;

    /*! Trim registers */
    struct bmm150_trim_registers trim_data;

    /*! Interrupt status */
    uint16_t int_status;

    /*! Power control bit value */
    uint8_t pwr_cntrl_bit;
};

#endif /* BMM150_DEFS_H_ */
