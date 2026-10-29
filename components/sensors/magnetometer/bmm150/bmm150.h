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
* @file       bmm150.h
* @date       2020-06-03
* @version    v2.0.0
*
*/

/*!
 * @defgroup bmm150 BMM150
 */

#ifndef _BMM150_H
#define _BMM150_H

/*! CPP guard */
#ifdef __cplusplus
extern "C" {
#endif

/********************************************************************/
/* header files */

#include "bmm150_defs.h"

/********************************************************************/
/* (extern) variable declarations */
/********************************************************************/
/* function prototype declarations */

/**
 * \ingroup bmm150
 * \defgroup bmm150ApiInit Initialization
 * @brief Initialize the sensor and device structure
 */

/*!
 * \ingroup bmm150ApiInit
 * \page bmm150_api_bmm150_init bmm150_init
 * \code
 * int8_t bmm150_init(struct bmm150_dev *dev);
 * \endcode
 * @details This API is the entry point, Call this API before using other APIs.
 *  This API reads the chip-id of the sensor which is the first step to
 *  verify the sensor and also it configures the read mechanism of SPI and
 *  I2C interface.
 *
 *  @param[in,out] dev : Structure instance of bmm150_dev
 *  @note : Refer user guide for detailed info.
 *
 *  @return Result of API execution status
 * @retval 0 -> Success
 * @retval >0 -> Warning
 * @retval <0 -> Fail
 */
int8_t bmm150_init(struct bmm150_dev *dev);

/**
 * \ingroup bmm150
 * \defgroup bmm150ApiRegs Registers
 * @brief Read / Write data to the given register address of the sensor
 */

/*!
 * \ingroup bmm150ApiRegs
 * \page bmm150_api_bmm150_set_regs bmm150_set_regs
 * \code
 * int8_t bmm150_set_regs(uint8_t reg_addr, const uint8_t *reg_data, uint32_t len, struct bmm150_dev *dev);
 * \endcode
 * @details This API writes the given data to the register address
 * of the sensor.
 *
 * @param[in] reg_addr : Register address from where the data to be written.
 * @param[in] reg_data : Pointer to data buffer which is to be written
 *                       in the reg_addr of sensor.
 * @param[in] len      : No of bytes of data to write..
 * @param[in] dev      : Structure instance of bmm150_dev.
 *
 * @return Result of API execution status
 * @retval 0 -> Success
 * @retval >0 -> Warning
 * @retval <0 -> Fail
 */
int8_t bmm150_set_regs(uint8_t reg_addr, const uint8_t *reg_data, uint32_t len, struct bmm150_dev *dev);

/*!
 * \ingroup bmm150ApiRegs
 * \page bmm150_api_bmm150_get_regs bmm150_get_regs
 * \code
 * int8_t bmm150_get_regs(uint8_t reg_addr, uint8_t *reg_data, uint32_t len, struct bmm150_dev *dev)
 * \endcode
 * @details This API reads the data from the given register address of sensor.
 *
 * @param[in] reg_addr  : Register address from where the data to be read
 * @param[out] reg_data : Pointer to data buffer to store the read data.
 * @param[in] len       : No of bytes of data to be read.
 * @param[in] dev       : Structure instance of bmm150_dev.
 *
 * @return Result of API execution status
 * @retval 0 -> Success
 * @retval >0 -> Warning
 * @retval <0 -> Fail
 */
int8_t bmm150_get_regs(uint8_t reg_addr, uint8_t *reg_data, uint32_t len, struct bmm150_dev *dev);

/**
 * \ingroup bmm150
 * \defgroup bmm150ApiSoftreset Soft reset
 * @brief Perform soft reset of the sensor
 */

/*!
 * \ingroup bmm150ApiSoftreset
 * \page bmm150_api_bmm150_soft_reset bmm150_soft_reset
 * \code
 * int8_t bmm150_soft_reset(struct bmm150_dev *dev);
 * \endcode
 * @details This API is used to perform soft-reset of the sensor
 * where all the registers are reset to their default values except 0x4B.
 *
 * @param[in] dev       : Structure instance of bmm150_dev.
 *
 * @return Result of API execution status
 * @retval 0 -> Success
 * @retval >0 -> Warning
 * @retval <0 -> Fail
 */
int8_t bmm150_soft_reset(struct bmm150_dev *dev);

/**
 * \ingroup bmm150
 * \defgroup bmm150ApiPowermode Power mode
 * @brief Set / Get power mode of the sensor
 */

/*!
 * \ingroup bmm150ApiPowermode
 * \page bmm150_api_bmm150_set_op_mode bmm150_set_op_mode
 * \code
 * int8_t bmm150_set_op_mode(const struct bmm150_settings *settings, struct bmm150_dev *dev);
 * \endcode
 * @details This API is used to set the power mode of the sensor.
 *
 * @param[in] settings  : Structure instance of bmm150_settings.
 * @param[in] dev       : Structure instance of bmm150_dev.
 *
 *@verbatim
 *  settings->pwr_mode     |  Power mode
 * ------------------------|-----------------------
 *   0x00                  |   BMM150_POWERMODE_NORMAL
 *   0x01                  |   BMM150_POWERMODE_FORCED
 *   0x03                  |   BMM150_POWERMODE_SLEEP
 *   0x04                  |   BMM150_POWERMODE_SUSPEND
 *@endverbatim
 *
 * @return Result of API execution status
 * @retval 0 -> Success
 * @retval >0 -> Warning
 * @retval <0 -> Fail
 */
int8_t bmm150_set_op_mode(const struct bmm150_settings *settings, struct bmm150_dev *dev);

/*!
 * \ingroup bmm150ApiPowermode
 * \page bmm150_api_bmm150_get_op_mode bmm150_get_op_mode
 * \code
 * int8_t bmm150_get_op_mode(uint8_t *op_mode, struct bmm150_dev *dev);
 * \endcode
 * @details This API is used to get the power mode of the sensor.
 *
 * @param[out] op_mode  : power mode of the sensor.
 * @param[in] dev       : Structure instance of bmm150_dev.
 *
 *@verbatim
 *   op_mode     |  Power mode
 *  -------------|-----------------------
 *   0x00        |   BMM150_POWERMODE_NORMAL
 *   0x01        |   BMM150_POWERMODE_FORCED
 *   0x03        |   BMM150_POWERMODE_SLEEP
 *   0x04        |   BMM150_POWERMODE_SUSPEND
 *@endverbatim
 *
 * @return Result of API execution status
 * @retval 0 -> Success
 * @retval >0 -> Warning
 * @retval <0 -> Fail
 */
int8_t bmm150_get_op_mode(uint8_t *op_mode, struct bmm150_dev *dev);

/**
 * \ingroup bmm150
 * \defgroup bmm150ApiPresetmode Preset mode
 * @brief Set preset mode of the sensor
 */

/*!
 * \ingroup bmm150ApiPresetmode
 * \page bmm150_api_bmm150_set_presetmode bmm150_set_presetmode
 * \code
 * int8_t bmm150_set_presetmode(struct bmm150_settings *settings, struct bmm150_dev *dev);
 * \endcode
 * @details This API is used to set the preset mode of the sensor.
 *
 * @param[in] settings  : Structure instance of bmm150_settings.
 * @param[in] dev       : Structure instance of bmm150_dev.
 *
 *@verbatim
 *   settings->preset_mode      |  Preset mode
 *  ---------------------------|----------------------------------
 *   0x01                      |   BMM150_PRESETMODE_LOWPOWER
 *   0x02                      |   BMM150_PRESETMODE_REGULAR
 *   0x03                      |   BMM150_PRESETMODE_HIGHACCURACY
 *   0x04                      |   BMM150_PRESETMODE_ENHANCED
 *@endverbatim
 *
 * @return Result of API execution status
 * @retval 0 -> Success
 * @retval >0 -> Warning
 * @retval <0 -> Fail
 */
int8_t bmm150_set_presetmode(struct bmm150_settings *settings, struct bmm150_dev *dev);

/**
 * \ingroup bmm150
 * \defgroup bmm150ApiSensorSettings Sensor Settings
 * @brief Set / Get sensor settings of the sensor
 */

/*!
 * \ingroup bmm150ApiSensorSettings
 * \page bmm150_api_bmm150_set_sensor_settings bmm150_set_sensor_settings
 * \code
 * int8_t bmm150_set_sensor_settings(uint16_t desired_settings, const struct bmm150_settings *settings, struct bmm150_dev *dev);
 * \endcode
 * @details This API sets the sensor settings based on the desired_settings
 * and the dev structure configuration
 *
 * @param[in] desired_settings  : Selection macro for selecting the setting.
 * @param[in] settings          : Structure instance of bmm150_settings.
 * @param[in] dev               : Structure instance of bmm150_dev.
 *
 * @note Assign the sensor setting macros (multiple macros can be
 * set by doing a bitwise-OR operation) to the desired_settings parameter
 * of this API to perform the corresponding setting.
 *
 * @note threshold interrupt for each axes are set by using bitwise AND
 * operation of the following macros
 *  - BMM150_THRESHOLD_X
 *  - BMM150_THRESHOLD_Y
 *  - BMM150_THRESHOLD_Z
 *
 *@verbatim
 *   desired_settings  |  Selected sensor setting macros
 *  -------------------|--------------------------------
 *   0x0001            |  BMM150_SEL_DATA_RATE
 *   0x0002            |  BMM150_SEL_CONTROL_MEASURE
 *   0x0004            |  BMM150_SEL_XY_REP
 *   0x0008            |  BMM150_SEL_Z_REP
 *   0x0010            |  BMM150_SEL_DRDY_PIN_EN
 *   0x0020            |  BMM150_SEL_INT_PIN_EN
 *   0x0040            |  BMM150_SEL_DRDY_POLARITY
 *   0x0080            |  BMM150_SEL_INT_LATCH
 *   0x0100            |  BMM150_SEL_INT_POLARITY
 *   0x0200            |  BMM150_SEL_DATA_OVERRUN_INT
 *   0x0400            |  BMM150_SEL_OVERFLOW_INT
 *   0x0800            |  BMM150_SEL_HIGH_THRESHOLD_INT
 *   0x1000            |  BMM150_SEL_LOW_THRESHOLD_INT
 *   0x2000            |  BMM150_SEL_LOW_THRESHOLD_SETTING
 *   0x4000            |  BMM150_SEL_HIGH_THRESHOLD_SETTING
 *@endverbatim
 *
 * @return Result of API execution status
 * @retval 0 -> Success
 * @retval >0 -> Warning
 * @retval <0 -> Fail
 */
int8_t bmm150_set_sensor_settings(uint16_t desired_settings,
                                  const struct bmm150_settings *settings,
                                  struct bmm150_dev *dev);

/*!
 * \ingroup bmm150ApiSensorSettings
 * \page bmm150_api_bmm150_get_sensor_settings bmm150_get_sensor_settings
 * \code
 * int8_t bmm150_get_sensor_settings(struct bmm150_settings *settings, struct bmm150_dev *dev);
 * \endcode
 * @details This API gets all the sensor settings and updates the dev structure
 *
 * @param[in] settings  : Structure instance of bmm150_settings.
 * @param[in] dev       : Structure instance of bmm150_dev.
 *
 * @return Result of API execution status
 * @retval 0 -> Success
 * @retval >0 -> Warning
 * @retval <0 -> Fail
 */
int8_t bmm150_get_sensor_settings(struct bmm150_settings *settings, struct bmm150_dev *dev);

/**
 * \ingroup bmm150
 * \defgroup bmm150ApiMagData Read magnetometer data
 * @brief Read magnetometer data
 */

/*!
 * \ingroup bmm150ApiMagData
 * \page bmm150_api_bmm150_read_mag_data bmm150_read_mag_data
 * \code
 * int8_t bbmm150_read_mag_data(struct bmm150_mag_data *mag_data, struct bmm150_dev *dev);
 * \endcode
 * @details This API reads the magnetometer data from registers 0x42 to 0x49
 * and updates the dev structure with compensated mag data in micro-tesla
 *
 * @param[in] mag_data  : Structure instance of bmm150_mag_data.
 * @param[in,out] dev   :   Structure instance of bmm150_dev.
 *
 * @note The output mag data can be obtained either in int16_t or float format
 *       using this API.
 * @note Enable the macro "BMM150_USE_FLOATING_POINT" in the bmm150_defs.h
 *       file and call this API to get the mag data in float,
 *       disable this macro to get the mag data in int16_t format
 *
 *@verbatim
 *  Mag data output(micro-tesla)   |  Mag data in dev structure(int16_t/float)
 * --------------------------------|------------------------------------------
 *   X-axis data                   | mag_data->x
 *   Y-axis data                   | mag_data->y
 *   Z-axis data                   | mag_data->z
 *@endverbatim
 *
 * @return Result of API execution status
 * @retval 0 -> Success
 * @retval >0 -> Warning
 * @retval <0 -> Fail
 */
int8_t bmm150_read_mag_data(struct bmm150_mag_data *mag_data, struct bmm150_dev *dev);

/**
 * \ingroup bmm150
 * \defgroup bmm150ApiSelftest Self test
 * @brief Perform self test
 */

/*!
 * \ingroup bmm150ApiSelftest
 * \page bmm150_api_bmm150_perform_self_test bmm150_perform_self_test
 * \code
 * int8_t bmm150_perform_self_test(uint8_t self_test_mode, struct bmm150_dev *dev);
 * \endcode
 * @details This API is used to perform the complete self test
 * (both normal and advanced) for the BMM150 sensor
 *
 * @param[in] self_test_mode    : The type of self test to be performed
 * @param[in] dev               : Structure instance of bmm150_dev.
 *
 *@verbatim
 *   self_test_mode    |  Self test enabled
 * --------------------|--------------------------
 *      0              | BMM150_SELF_TEST_NORMAL
 *      1              | BMM150_SELF_TEST_ADVANCED
 *@endverbatim
 *
 * @note The return value of this API gives us the result of self test.
 *
 * @note Performing advanced self test does soft reset of the sensor, User can
 * set the desired settings after performing the advanced self test.
 *
 * @return Result of API execution status and self test result.
 * @retval  0       BMM150_OK
 * @retval  1       BMM150_W_NORMAL_SELF_TEST_YZ_FAIL
 * @retval  2       BMM150_W_NORMAL_SELF_TEST_XZ_FAIL
 * @retval  3       BMM150_W_NORMAL_SELF_TEST_Z_FAIL
 * @retval  4       BMM150_W_NORMAL_SELF_TEST_XY_FAIL
 * @retval  5       BMM150_W_NORMAL_SELF_TEST_Y_FAIL
 * @retval  6       BMM150_W_NORMAL_SELF_TEST_X_FAIL
 * @retval  7       BMM150_W_NORMAL_SELF_TEST_XYZ_FAIL
 * @retval  8       BMM150_W_ADV_SELF_TEST_FAIL
 */
int8_t bmm150_perform_self_test(uint8_t self_test_mode, struct bmm150_dev *dev);

/**
 * \ingroup bmm150
 * \defgroup bmm150ApiInt Interrupt status
 * @brief Obtain interrupt staus flags
 */

/*!
 * \ingroup bmm150ApiInt
 * \page bmm150_api_bmm150_get_interrupt_status bmm150_get_interrupt_status
 * \code
 * int8_t bmm150_get_interrupt_status(struct bmm150_dev *dev);
 * \endcode
 * @details This API obtains the status flags of all interrupt
 * which is used to check for the assertion of interrupts
 *
 * @param[in,out] dev       : Structure instance of bmm150_dev.
 *
 * @note The status flags of all the interrupts are stored in the
 * dev->int_status.
 *
 * @note The value of dev->int_status is performed a bitwise AND operation
 * with predefined interrupt status macros to find the interrupt status
 * which is either set or reset.
 *
 * Ex.
 * if (dev->int_status & BMM150_INT_ASSERTED_DRDY)
 * {
 *  Occurrence of data ready interrupt
 * } else {
 *  No interrupt occurred
 * }
 *
 * @return Result of API execution status and self test result.
 * @retval 0 -> Success
 * @retval >0 -> Warning
 * @retval <0 -> Fail
 */
int8_t bmm150_get_interrupt_status(struct bmm150_dev *dev);

/**
 * \ingroup bmm150
 * \defgroup bmm150ApiAux Compensate magnetometer data
 * @brief Compensation of magnetometer data
 */

/*!
 * \ingroup bmm150ApiAux
 * \page bmm150_api_bmm150_aux_mag_data bmm150_aux_mag_data
 * \code
 * int8_t bmm150_aux_mag_data(uint8_t *aux_data, struct bmm150_mag_data *mag_data, const struct bmm150_dev *dev);
 * \endcode
 * @details This API is used to compensate the raw mag data
 *
 * @param[in] aux_data   : Raw mag data obtained from BMI160 registers
 * @param[in] mag_data   : Structure instance of bmm150_mag_data.
 * @param[in,out] dev    : Structure instance of bmm150_dev.
 *
 * @return Result of API execution status and self test result.
 * @retval 0 -> Success
 * @retval >0 -> Warning
 * @retval <0 -> Fail
 */
int8_t bmm150_aux_mag_data(uint8_t *aux_data, struct bmm150_mag_data *mag_data, const struct bmm150_dev *dev);

#ifdef __cplusplus
}
#endif /* End of CPP guard */

#endif /* _BMM150_H */
