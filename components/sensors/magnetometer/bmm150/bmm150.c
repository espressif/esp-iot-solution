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
* @file       bmm150.c
* @date       2020-06-03
* @version    v2.0.0
*
*/

/*! @file bmm150.c
 * @brief Sensor driver for BMM150 sensor */
#include "bmm150.h"

/************************** Internal macros *******************************/
/* Sensor ODR, Repetition and axes enable/disable settings */
#define MODE_SETTING_SEL                UINT16_C(0x000F)

/* Interrupt pin settings like polarity,latch and int_pin enable */
#define INTERRUPT_PIN_SETTING_SEL       UINT16_C(0x01F0)

/* Settings to enable/disable interrupts */
#define INTERRUPT_CONFIG_SEL            UINT16_C(0x1E00)

/* Interrupt settings for configuring threshold values */
#define INTERRUPT_THRESHOLD_CONFIG_SEL  UINT16_C(0x6000)

/********************** Static function declarations ************************/

/*!
 * @brief This internal API is used to validate the device pointer for
 * null conditions.
 *
 * @param[in] dev : Structure instance of bmm150_dev.
 *
 * @return Result of API execution status
 * @retval 0 -> Success
 * @retval >0 -> Warning
 * @retval <0 -> Fail
 */
static int8_t null_ptr_check(const struct bmm150_dev *dev);

/*!
 * @brief This internal API sets/resets the power control bit of 0x4B register.
 *
 * @param[in] pwrcntrl_bit : Variable used to select/deselect the suspend mode.
 * @param[in,out] dev      : Structure instance of bmm150_dev
 *
 * pwrcntrl_bit     |  power mode
 * -----------------|-------------------------
 * 0x00             |    Suspend mode
 * 0x01             |    Sleep/Active modes
 *
 * @return Result of API execution status
 * @retval 0 -> Success
 * @retval >0 -> Warning
 * @retval <0 -> Fail
 */
static int8_t set_power_control_bit(uint8_t pwrcntrl_bit, struct bmm150_dev *dev);

/*!
 * @brief This internal API reads the trim registers of the sensor and stores
 * the trim values in the "trim_data" of device structure.
 *
 * @param[in,out] dev      : Structure instance of bmm150_dev
 *
 * @return Result of API execution status
 * @retval 0 -> Success
 * @retval >0 -> Warning
 * @retval <0 -> Fail
 */
static int8_t read_trim_registers(struct bmm150_dev *dev);

/*!
 * @brief This internal API writes the op_mode value in the Opmode bits
 * (bits 1 and 2) of 0x4C register.
 *
 *  op_mode    |  Power mode
 * ------------|-----------------------
 *   0x00      |   BMM150_POWERMODE_NORMAL
 *   0x01      |   BMM150_POWERMODE_FORCED
 *   0x03      |   BMM150_POWERMODE_SLEEP
 *
 * @param[in,out] dev      : Structure instance of bmm150_dev
 *
 * @return Result of API execution status
 * @retval 0 -> Success
 * @retval >0 -> Warning
 * @retval <0 -> Fail
 */
static int8_t write_op_mode(uint8_t op_mode, struct bmm150_dev *dev);

/*!
 * @brief This internal API sets the device from suspend to sleep mode
 * by setting the power control bit to '1' of 0x4B register
 *
 * @param[in,out] dev      : Structure instance of bmm150_dev
 *
 * @return Result of API execution status
 * @retval 0 -> Success
 * @retval >0 -> Warning
 * @retval <0 -> Fail
 */
static int8_t suspend_to_sleep_mode(struct bmm150_dev *dev);

/*!
 * @brief This internal API sets the xy repetition value in the 0x51 register.
 *
 * @param[in] settings  : Structure instance of bmm150_settings.
 * @param[in,out] dev   : Structure instance of bmm150_dev
 *
 *     settings->xy_rep     |  nXY(XY Repetitions)
 * -------------------------|-----------------------
 *   0x00                   |   1
 *   0x01                   |   3
 *   0x02                   |   5
 *    .                     |   .
 *    .                     |   .
 *   0xFF                   |   511
 *
 * @note number of XY Repetitions nXY  = 1+2(settings->xy_rep)
 *
 * @return Result of API execution status
 * @retval 0 -> Success
 * @retval >0 -> Warning
 * @retval <0 -> Fail
 */
static int8_t set_xy_rep(const struct bmm150_settings *settings, struct bmm150_dev *dev);

/*!
 * @brief This internal API sets the z repetition value in the 0x52 register.
 *
 * @param[in] settings  : Structure instance of bmm150_settings.
 * @param[in,out] dev   : Structure instance of bmm150_dev
 *
 *     settings->z_rep      |  nZ(Z Repetitions)
 * -------------------------|-----------------------
 *   0x00                   |   1
 *   0x01                   |   2
 *   0x02                   |   3
 *    .                     |   .
 *    .                     |   .
 *   0xFF                   |   256
 *
 * @note number of Z Repetitions nZ  = 1+(settings->z_rep)
 *
 * @return Result of API execution status
 * @retval 0 -> Success
 * @retval >0 -> Warning
 * @retval <0 -> Fail
 */
static int8_t set_z_rep(const struct bmm150_settings *settings, struct bmm150_dev *dev);

/*!
 * @brief This internal API is used to set the output data rate of the sensor
 *
 * @param[in] settings  : Structure instance of bmm150_settings.
 * @param[in] dev       : Structure instance of bmm150_dev.
 *
 *    settings->data_rate   |   Data rate (ODR)
 * -------------------------|-----------------------
 *   0x00                   |  BMM150_DATA_RATE_10HZ
 *   0x01                   |  BMM150_DATA_RATE_02HZ
 *   0x02                   |  BMM150_DATA_RATE_06HZ
 *   0x03                   |  BMM150_DATA_RATE_08HZ
 *   0x04                   |  BMM150_DATA_RATE_15HZ
 *   0x05                   |  BMM150_DATA_RATE_20HZ
 *   0x06                   |  BMM150_DATA_RATE_25HZ
 *   0x07                   |  BMM150_DATA_RATE_30HZ
 *
 * @return Result of API execution status
 * @retval 0 -> Success
 * @retval >0 -> Warning
 * @retval <0 -> Fail
 */
static int8_t set_odr(const struct bmm150_settings *settings, struct bmm150_dev *dev);

/*!
 * @brief This internal API sets the preset mode ODR and repetition settings.
 * @param[in] settings : Structure instance of bmm150_settings
 * @param[in] dev      : Structure instance of bmm150_dev
 *
 *  API settings        |  Representation
 * ---------------------|------------------------------
 * settings->data_rate  | Output Data Rate (ODR)
 * settings->xy_rep     | XY repetition value
 * settings->z_rep      | Z-repetition value
 *
 *
 * @return Result of API execution status
 * @retval 0 -> Success
 * @retval >0 -> Warning
 * @retval <0 -> Fail
 */
static int8_t set_odr_xyz_rep(const struct bmm150_settings *settings, struct bmm150_dev *dev);

/*!
 * @brief This internal API is used to enable or disable the magnetic
 * measurement of x,y,z axes based on the value of xyz_axes_control.
 *
 * @param[in] settings  : Structure instance of bmm150_settings.
 * @param[in] dev       : Structure instance of bmm150_dev.
 *
 *  settings->xyz_axes_control | Measurement axes/channel
 *  ---------------------------|--------------------------
 *  Bit 0                      | X - Channel
 *  Bit 1                      | Y - Channel
 *  Bit 2                      | Z - Channel
 *
 * @note Setting 1 - Disables Channel measurement
 * @note Setting 0 - Enables Channel measurement
 *
 *   settings->xyz_axes_control   | Measurement axes Enabled/disabled
 * -------------------------------|------------------------------------
 *  0x01                          | Disables X axis (Y,Z axes enabled)
 *  0x02                          | Disables Y axis (X,Z axes enabled)
 *  0x04                          | Disables Z axis (X,Y axes enabled)
 *  0x07                          | Disables all X,Y,Z axes measurement
 *
 * @return Result of API execution status
 * @retval 0 -> Success
 * @retval >0 -> Warning
 * @retval <0 -> Fail
 */
static int8_t set_control_measurement_xyz(const struct bmm150_settings *settings, struct bmm150_dev *dev);

/*!
 * @brief This internal API is used to identify the settings which the user
 * wants to modify in the sensor.
 *
 * @param[in] sub_settings : Contains the settings subset to identify particular
 * group of settings which the user is interested to change.
 * @param[in] settings : Contains the user specified settings.
 *
 * @return Indicates whether user is interested to modify the settings which
 * are related to sub_settings.
 * @retval True -> User wants to modify this group of settings
 * @retval False -> User does not want to modify this group of settings
 */
static uint8_t are_settings_changed(uint16_t sub_settings, uint16_t settings);

/*!
 * @brief This API sets the ODR , measurement axes control ,
 * repetition values of xy,z.
 *
 * @param[in] desired_settings : Contains the settings which user wants to
 * change.
 * @param[in] settings         : Structure instance of bmm150_settings.
 * @param[in] dev              : Structure instance of bmm150_dev.
 *
 * @return Result of API execution status
 * @retval 0 -> Success
 * @retval >0 -> Warning
 * @retval <0 -> Fail
 */
static int8_t mode_settings(uint16_t desired_settings, const struct bmm150_settings *settings, struct bmm150_dev *dev);

/*!
 * @brief This internal API is used to parse and store the sensor
 * settings in the device structure
 *
 * @param[in] reg_data   : Pointer of an array consisting all sensor
 *                         setting data from 0x4B to 0x52 registers.
 * @param[in] settings   : Structure instance of bmm150_settings.
 *
 */
static void parse_setting(const uint8_t *reg_data, struct bmm150_settings *settings);

/*!
 * @brief This API is used to enable the interrupts and map them to the
 * corresponding interrupt pins and specify the pin characteristics like the
 * polarity , latch settings for the interrupt pins.
 *
 * @note The other interrupts can be latched or non-latched but,
 * Data ready interrupt is always cleared after reading out the data
 *
 * @param[in] desired_settings : Contains the settings which user wants to
 *                               change.
 * @param[in] settings         : Structure instance of bmm150_settings.
 * @param[in] dev              : Structure instance of bmm150_dev.
 *
 * @return Result of API execution status
 * @retval 0 -> Success
 * @retval >0 -> Warning
 * @retval <0 -> Fail
 */
static int8_t interrupt_pin_settings(uint16_t desired_settings,
                                     const struct bmm150_settings *settings,
                                     struct bmm150_dev *dev);

/*!
 * @brief This API is used to enable data overrun , overflow interrupts and
 * enable/disable high/low threshold interrupts for x,y,z axis based on the
 * threshold values set by the user in the High threshold (0x50) and
 * Low threshold (0x4F) registers.
 *
 * @param[in] desired_settings : Contains the settings which user wants to
 *                               change.
 * @param[in] settings         : Structure instance of bmm150_settings.
 * @param[in] dev              : Structure instance of bmm150_dev.
 *
 * @return Result of API execution status
 * @retval 0 -> Success
 * @retval >0 -> Warning
 * @retval <0 -> Fail
 */
static int8_t interrupt_config(uint16_t desired_settings, const struct bmm150_settings *settings,
                               struct bmm150_dev *dev);

/*!
 * @brief This API is used to write the user specified High/Low threshold value
 * as a reference to generate the high/low threshold interrupt.
 *
 * @param[in] desired_settings : Contains the settings which user wants to
 *                               change.
 * @param[in] settings         : Structure instance of bmm150_settings.
 * @param[in] dev              : Structure instance of bmm150_dev.
 *
 * @return Result of API execution status
 * @retval 0 -> Success
 * @retval >0 -> Warning
 * @retval <0 -> Fail
 */
static int8_t interrupt_threshold_settings(uint16_t desired_settings,
                                           const struct bmm150_settings *settings,
                                           struct bmm150_dev *dev);

#ifdef BMM150_USE_FLOATING_POINT

/*!
 * @brief This internal API is used to obtain the compensated
 * magnetometer X axis data in float.
 *
 * @param[in] mag_data_x     : The value of raw X data
 * @param[in] data_rhall     : The value of raw RHALL data
 * @param[in] dev            : Structure instance of bmm150_dev.
 *
 * @return Result of compensated X data value in float
 */
static float compensate_x(int16_t mag_data_x, uint16_t data_rhall, const struct bmm150_dev *dev);

/*!
 * @brief This internal API is used to obtain the compensated
 * magnetometer Y axis data in float.
 *
 * @param[in] mag_data_y     : The value of raw Y data
 * @param[in] data_rhall     : The value of raw RHALL data
 * @param[in] dev            : Structure instance of bmm150_dev.
 *
 * @return Result of compensated Y data value in float
 */
static float compensate_y(int16_t mag_data_y, uint16_t data_rhall, const struct bmm150_dev *dev);

/*!
 * @brief This internal API is used to obtain the compensated
 * magnetometer Z axis data in float.
 *
 * @param[in] mag_data_z     : The value of raw Z data
 * @param[in] data_rhall     : The value of raw RHALL data
 * @param[in] dev            : Structure instance of bmm150_dev.
 *
 * @return Result of compensated Z data value in float
 */
static float compensate_z(int16_t mag_data_z, uint16_t data_rhall, const struct bmm150_dev *dev);

#else

/*!
 * @brief This internal API is used to obtain the compensated
 * magnetometer X axis data in int16_t.
 *
 * @param[in] mag_data_x     : The value of raw X data
 * @param[in] data_rhall     : The value of raw RHALL data
 * @param[in] dev            : Structure instance of bmm150_dev.
 *
 * @return Result of compensated X data value in int16_t format
 */
static int16_t compensate_x(int16_t mag_data_x, uint16_t data_rhall, const struct bmm150_dev *dev);

/*!
 * @brief This internal API is used to obtain the compensated
 * magnetometer Y axis data in int16_t.
 *
 * @param[in] mag_data_y     : The value of raw Y data
 * @param[in] data_rhall     : The value of raw RHALL data
 * @param[in] dev            : Structure instance of bmm150_dev.
 *
 * @return Result of compensated Y data value in int16_t format
 */
static int16_t compensate_y(int16_t mag_data_y, uint16_t data_rhall, const struct bmm150_dev *dev);

/*!
 * @brief This internal API is used to obtain the compensated
 * magnetometer Z axis data in int16_t.
 *
 * @param[in] mag_data_z     : The value of raw Z data
 * @param[in] data_rhall     : The value of raw RHALL data
 * @param[in] dev            : Structure instance of bmm150_dev.
 *
 * @return Result of compensated Z data value in int16_t format
 */
static int16_t compensate_z(int16_t mag_data_z, uint16_t data_rhall, const struct bmm150_dev *dev);

#endif

/*!
 * @brief This internal API is used to perform the normal self test
 * of the sensor and return the self test result as return value
 *
 * @param[in] dev          : Structure instance of bmm150_dev.
 *
 * @return Result of API execution status
 * @retval 0 -> Success
 * @retval >0 -> Warning
 * @retval <0 -> Fail
 */
static int8_t perform_normal_self_test(struct bmm150_dev *dev);

/*!
 * @brief This internal API is used to enable the normal self test by setting
 * the Self Test bit (bit0) of the 0x4C register,
 * which triggers the start of self test
 *
 * @param[out] self_test_enable : The value of self test bit0 in 0x4C register
 * @param[in] dev               : Structure instance of bmm150_dev.
 *
 * @return Result of API execution status
 * @retval 0 -> Success
 * @retval >0 -> Warning
 * @retval <0 -> Fail
 */
static int8_t enable_normal_self_test(uint8_t *self_test_enable, struct bmm150_dev *dev);

/*!
 * @brief This internal API is used to validate the results of normal self test
 * by using the self test status available in the bit0 of registers 0x42,0x44
 * and 0x46.
 *
 * @param[in] dev       : Structure instance of bmm150_dev
 *
 * @return Result of API execution status
 * @retval 0 -> Success
 * @retval >0 -> Warning
 * @retval <0 -> Fail
 */
static int8_t validate_normal_self_test(struct bmm150_dev *dev);

/*!
 * @brief This internal API is used to perform advanced self test for Z axis
 *
 * @param[in] dev     : Structure instance of bmm150_dev
 *
 * @return Result of API execution status
 * @retval zero -> Success / +ve value -> Warning / -ve value -> Error
 *
 * Return value         | Status of self-test
 *----------------------|---------------------------
 *      0               | BMM150_OK
 *      8               | BMM150_W_ADV_SELF_TEST_FAIL
 */
static int8_t perform_adv_self_test(struct bmm150_dev *dev);

/*!
 * @brief This internal API is used to set the desired power mode ,
 * axes control and repetition settings for advanced self test
 *
 * @param[in] dev   :  Structure instance of bmm150_dev
 *
 * @return Result of API execution status
 * @retval 0 -> Success
 * @retval >0 -> Warning
 * @retval <0 -> Fail
 */
static int8_t adv_self_test_settings(struct bmm150_dev *dev);

/*!
 * @brief This internal API is used to set the positive or negative value of
 * self-test current and obtain the corresponding magnetometer z axis data
 *
 * @param[in] self_test_current  : Self test current either positive/negative
 * @param[out] data_z            : Z-axis Magnetometer data
 * @param[in] dev                : Structure instance of bmm150_dev
 *
 *  self_test_current      | Self-test current Direction
 *-------------------------|------------------------------
 *     0x03                | BMM150_ENABLE_POSITIVE_CURRENT
 *     0x02                | BMM150_ENABLE_NEGATIVE_CURRENT
 *
 * @return Result of API execution status
 * @retval 0 -> Success
 * @retval >0 -> Warning
 * @retval <0 -> Fail
 */
static int8_t adv_self_test_measurement(uint8_t self_test_current, int16_t *data_z, struct bmm150_dev *dev);

/*!
 * @brief This internal API is used to get the difference between the
 * Z axis mag data obtained by positive and negative self-test current
 * and validate whether the advanced self test is done successfully or not.
 *
 * @param[in] positive_data_z   : Z-axis Mag data by positive self-test current
 * @param[in] negative_data_z  : Z-axis Mag data by negative self-test current
 *
 *
 * @return Result of API execution status
 * @retval zero -> Success / +ve value -> Warning / -ve value -> Error
 *
 *  Return value        | Status of self-test
 *----------------------|---------------------------
 *      0               | BMM150_OK
 *      8               | BMM150_W_ADV_SELF_TEST_FAIL
 */
static int8_t validate_adv_self_test(int16_t positive_data_z, int16_t negative_data_z);

/*!
 * @brief This internal API is used to set the self test current value in
 * the Adv. ST bits (bit6 and bit7) of 0x4C register
 *
 * @param[in] self_test_current  : Self test current value (+ve/-ve)
 * @param[in] dev                : Structure instance of bmm150_dev
 *
 * self_test_current       | Self-test current Direction
 *-------------------------|------------------------------
 *     0x00                | BMM150_DISABLE_SELF_TEST_CURRENT
 *     0x02                | BMM150_ENABLE_NEGATIVE_CURRENT
 *     0x03                | BMM150_ENABLE_POSITIVE_CURRENT
 *
 * @return Result of API execution status
 * @retval 0 -> Success
 * @retval >0 -> Warning
 * @retval <0 -> Fail
 */
static int8_t set_adv_self_test_current(uint8_t self_test_current, struct bmm150_dev *dev);

/********************** Global function definitions ************************/

/*!
 *  @brief This API is the entry point, Call this API before using other APIs.
 *  This API reads the chip-id of the sensor which is the first step to
 *  verify the sensor and updates the trim parameters of the sensor.
 */
int8_t bmm150_init(struct bmm150_dev *dev)
{
    int8_t rslt;
    uint8_t chip_id = 0;

    /* Power up the sensor from suspend to sleep mode */
    rslt = set_power_control_bit(BMM150_POWER_CNTRL_ENABLE, dev);

    if (rslt == BMM150_OK)
    {
        /* Start-up time delay of 3ms */
        dev->delay_us(BMM150_START_UP_TIME, dev->intf_ptr);

        /* Chip ID of the sensor is read */
        rslt = bmm150_get_regs(BMM150_REG_CHIP_ID, &chip_id, 1, dev);

        /* Proceed if everything is fine until now */
        if (rslt == BMM150_OK)
        {
            /* Check for chip id validity */
            if (chip_id == BMM150_CHIP_ID)
            {
                dev->chip_id = chip_id;

                /* Function to update trim values */
                rslt = read_trim_registers(dev);
            }
        }
    }

    return rslt;
}

/*!
 * @brief This API writes the given data to the register address
 * of the sensor.
 */
int8_t bmm150_set_regs(uint8_t reg_addr, const uint8_t *reg_data, uint32_t len, struct bmm150_dev *dev)
{
    int8_t rslt;

    /* Check for null pointer in the device structure */
    rslt = null_ptr_check(dev);

    /* Proceed if null check is fine */
    if ((rslt == BMM150_OK) && (reg_data != NULL) && (len != 0))
    {
        /* Write the data to the reg_addr */

        /* SPI write requires to set The MSB of reg_addr as 0
         * but in default the MSB is always 0
         */
        dev->intf_rslt = dev->write(reg_addr, reg_data, len, dev->intf_ptr);
    }
    else
    {
        rslt = BMM150_E_NULL_PTR;
    }

    return rslt;
}

/*!
 * @brief This API reads the data from the given register address of the sensor.
 */
int8_t bmm150_get_regs(uint8_t reg_addr, uint8_t *reg_data, uint32_t len, struct bmm150_dev *dev)
{
    int8_t rslt;

    /* Check for null pointer in the device structure */
    rslt = null_ptr_check(dev);

    /* Proceed if null check is fine */
    if ((rslt == BMM150_OK) && (reg_data != NULL))
    {
        if (dev->intf != BMM150_I2C_INTF)
        {
            /* If interface selected is SPI */
            reg_addr = reg_addr | 0x80;
        }

        /* Read the data from the reg_addr */
        dev->intf_rslt = dev->read(reg_addr, reg_data, len, dev->intf_ptr);
    }
    else
    {
        rslt = BMM150_E_NULL_PTR;
    }

    return rslt;
}

/*!
 * @brief This API is used to perform soft-reset of the sensor
 * where all the registers are reset to their default values except 0x4B.
 */
int8_t bmm150_soft_reset(struct bmm150_dev *dev)
{
    int8_t rslt;
    uint8_t reg_data;

    rslt = bmm150_get_regs(BMM150_REG_POWER_CONTROL, &reg_data, 1, dev);

    if (rslt == BMM150_OK)
    {
        reg_data = reg_data | BMM150_SET_SOFT_RESET;
        rslt = bmm150_set_regs(BMM150_REG_POWER_CONTROL, &reg_data, 1, dev);
        dev->delay_us(BMM150_DELAY_SOFT_RESET, dev->intf_ptr);
    }

    return rslt;
}

/*!
 * @brief This API is used to set the power mode of the sensor.
 */
int8_t bmm150_set_op_mode(const struct bmm150_settings *settings, struct bmm150_dev *dev)
{
    int8_t rslt;
    uint8_t pwr_mode;

    /* Check for null pointer in the device structure */
    rslt = null_ptr_check(dev);

    /* Proceed if null check is fine */
    if (rslt == BMM150_OK)
    {
        pwr_mode = settings->pwr_mode;

        /* Select the power mode to set */
        switch (pwr_mode)
        {
            case BMM150_POWERMODE_NORMAL:

                /* If the sensor is in suspend mode
                 * put the device to sleep mode
                 */
                rslt = suspend_to_sleep_mode(dev);
                if (rslt == BMM150_OK)
                {
                    /* write the op mode */
                    rslt = write_op_mode(pwr_mode, dev);
                }

                break;
            case BMM150_POWERMODE_FORCED:

                /* If the sensor is in suspend mode
                 * put the device to sleep mode
                 */
                rslt = suspend_to_sleep_mode(dev);
                if (rslt == BMM150_OK)
                {
                    /* write the op mode */
                    rslt = write_op_mode(pwr_mode, dev);
                }

                break;
            case BMM150_POWERMODE_SLEEP:

                /* If the sensor is in suspend mode
                 * put the device to sleep mode
                 */
                rslt = suspend_to_sleep_mode(dev);
                if (rslt == BMM150_OK)
                {
                    /* write the op mode */
                    rslt = write_op_mode(pwr_mode, dev);
                }

                break;
            case BMM150_POWERMODE_SUSPEND:

                /* Set the power control bit to zero */
                rslt = set_power_control_bit(BMM150_POWER_CNTRL_DISABLE, dev);
                break;
            default:
                rslt = BMM150_E_INVALID_CONFIG;
                break;
        }
    }

    return rslt;
}

/*!
 * @brief This API is used to get the power mode of the sensor.
 */
int8_t bmm150_get_op_mode(uint8_t *op_mode, struct bmm150_dev *dev)
{
    int8_t rslt;
    uint8_t reg_data;

    /* Check for null pointer in the device structure */
    rslt = null_ptr_check(dev);

    /* Proceed if null check is fine */
    if (rslt == BMM150_OK)
    {
        if (dev->pwr_cntrl_bit == BMM150_POWER_CNTRL_DISABLE)
        {
            /* Power mode set is suspend mode */
            *op_mode = BMM150_POWERMODE_SUSPEND;
        }
        else
        {
            /* Power mode set is stored in the op_mode */
            rslt = bmm150_get_regs(BMM150_REG_OP_MODE, &reg_data, 1, dev);
            *op_mode = BMM150_GET_BITS(reg_data, BMM150_OP_MODE);
        }
    }

    return rslt;
}

/*!
 * @brief This API is used to set the preset mode of the sensor.
 */
int8_t bmm150_set_presetmode(struct bmm150_settings *settings, struct bmm150_dev *dev)
{
    int8_t rslt;
    uint8_t preset_mode;

    /* Check for null pointer in the device structure */
    rslt = null_ptr_check(dev);

    /* Proceed if null check is fine */
    if (rslt == BMM150_OK)
    {
        preset_mode = settings->preset_mode;

        switch (preset_mode)
        {
            case BMM150_PRESETMODE_LOWPOWER:

                /* Set the data rate x,y,z repetition
                 * for Low Power mode
                 */
                settings->data_rate = BMM150_DATA_RATE_10HZ;
                settings->xy_rep = BMM150_REPXY_LOWPOWER;
                settings->z_rep = BMM150_REPZ_LOWPOWER;
                rslt = set_odr_xyz_rep(settings, dev);
                break;
            case BMM150_PRESETMODE_REGULAR:

                /* Set the data rate x,y,z repetition
                 * for Regular mode
                 */
                settings->data_rate = BMM150_DATA_RATE_10HZ;
                settings->xy_rep = BMM150_REPXY_REGULAR;
                settings->z_rep = BMM150_REPZ_REGULAR;
                rslt = set_odr_xyz_rep(settings, dev);
                break;
            case BMM150_PRESETMODE_HIGHACCURACY:

                /* Set the data rate x,y,z repetition
                 * for High Accuracy mode *
                 */
                settings->data_rate = BMM150_DATA_RATE_20HZ;
                settings->xy_rep = BMM150_REPXY_HIGHACCURACY;
                settings->z_rep = BMM150_REPZ_HIGHACCURACY;
                rslt = set_odr_xyz_rep(settings, dev);
                break;
            case BMM150_PRESETMODE_ENHANCED:

                /* Set the data rate x,y,z repetition
                 * for Enhanced Accuracy mode
                 */
                settings->data_rate = BMM150_DATA_RATE_10HZ;
                settings->xy_rep = BMM150_REPXY_ENHANCED;
                settings->z_rep = BMM150_REPZ_ENHANCED;
                rslt = set_odr_xyz_rep(settings, dev);
                break;
            default:
                rslt = BMM150_E_INVALID_CONFIG;
                break;
        }
    }

    return rslt;
}

/*!
 * @brief This API sets the sensor settings based on the desired_settings
 * and the dev structure configuration
 */
int8_t bmm150_set_sensor_settings(uint16_t desired_settings,
                                  const struct bmm150_settings *settings,
                                  struct bmm150_dev *dev)
{
    int8_t rslt;

    /* Check for null pointer in the device structure */
    rslt = null_ptr_check(dev);

    /* Proceed if null check is fine */
    if (rslt == BMM150_OK)
    {
        if (are_settings_changed(MODE_SETTING_SEL, desired_settings))
        {
            /* ODR, Control measurement, XY,Z repetition values */
            rslt = mode_settings(desired_settings, settings, dev);
        }

        if ((!rslt) && are_settings_changed(INTERRUPT_PIN_SETTING_SEL, desired_settings))
        {
            /* Interrupt pin settings */
            rslt = interrupt_pin_settings(desired_settings, settings, dev);
        }

        if ((!rslt) && are_settings_changed(INTERRUPT_CONFIG_SEL, desired_settings))
        {
            /* Interrupt configuration settings */
            rslt = interrupt_config(desired_settings, settings, dev);
        }

        if ((!rslt) && are_settings_changed(INTERRUPT_THRESHOLD_CONFIG_SEL, desired_settings))
        {
            /* Interrupt threshold settings */
            rslt = interrupt_threshold_settings(desired_settings, settings, dev);
        }
    }

    return rslt;
}

/*!
 * @brief This API gets the sensor settings and updates the dev structure
 */
int8_t bmm150_get_sensor_settings(struct bmm150_settings *settings, struct bmm150_dev *dev)
{
    int8_t rslt;
    uint8_t setting[BMM150_LEN_SETTING_DATA] = { 0 };

    /* Read the entire sensor settings */
    rslt = bmm150_get_regs(BMM150_REG_POWER_CONTROL, setting, BMM150_LEN_SETTING_DATA, dev);

    if (rslt == BMM150_OK)
    {
        /* Parse and store the settings */
        parse_setting(setting, settings);
    }

    return rslt;
}

/*!
 * @brief This API is used to read the magnetometer data from registers
 * 0x42 to 0x49 and update the dev structure with the
 * compensated mag data in micro-tesla.
 */
int8_t bmm150_read_mag_data(struct bmm150_mag_data *mag_data, struct bmm150_dev *dev)
{
    int8_t rslt;
    int16_t msb_data;
    uint8_t reg_data[BMM150_LEN_XYZR_DATA] = { 0 };
    struct bmm150_raw_mag_data raw_mag_data;

    /* Read the mag data registers */
    rslt = bmm150_get_regs(BMM150_REG_DATA_X_LSB, reg_data, BMM150_LEN_XYZR_DATA, dev);

    if (rslt == BMM150_OK)
    {
        /* Mag X axis data */
        reg_data[0] = BMM150_GET_BITS(reg_data[0], BMM150_DATA_X);

        /* Shift the MSB data to left by 5 bits */
        /* Multiply by 32 to get the shift left by 5 value */
        msb_data = ((int16_t)((int8_t)reg_data[1])) * 32;

        /* Raw mag X axis data */
        raw_mag_data.raw_datax = (int16_t)(msb_data | reg_data[0]);

        /* Mag Y axis data */
        reg_data[2] = BMM150_GET_BITS(reg_data[2], BMM150_DATA_Y);

        /* Shift the MSB data to left by 5 bits */
        /* Multiply by 32 to get the shift left by 5 value */
        msb_data = ((int16_t)((int8_t)reg_data[3])) * 32;

        /* Raw mag Y axis data */
        raw_mag_data.raw_datay = (int16_t)(msb_data | reg_data[2]);

        /* Mag Z axis data */
        reg_data[4] = BMM150_GET_BITS(reg_data[4], BMM150_DATA_Z);

        /* Shift the MSB data to left by 7 bits */
        /* Multiply by 128 to get the shift left by 7 value */
        msb_data = ((int16_t)((int8_t)reg_data[5])) * 128;

        /* Raw mag Z axis data */
        raw_mag_data.raw_dataz = (int16_t)(msb_data | reg_data[4]);

        /* Mag R-HALL data */
        reg_data[6] = BMM150_GET_BITS(reg_data[6], BMM150_DATA_RHALL);
        raw_mag_data.raw_data_r = (uint16_t)(((uint16_t)reg_data[7] << 6) | reg_data[6]);

        /* Compensated Mag X data in int16_t format */
        mag_data->x = compensate_x(raw_mag_data.raw_datax, raw_mag_data.raw_data_r, dev);

        /* Compensated Mag Y data in int16_t format */
        mag_data->y = compensate_y(raw_mag_data.raw_datay, raw_mag_data.raw_data_r, dev);

        /* Compensated Mag Z data in int16_t format */
        mag_data->z = compensate_z(raw_mag_data.raw_dataz, raw_mag_data.raw_data_r, dev);
    }

    return rslt;
}

/*!
 * @brief This API is used to perform the complete self test
 * (both normal and advanced) for the BMM150 sensor
 */
int8_t bmm150_perform_self_test(uint8_t self_test_mode, struct bmm150_dev *dev)
{
    int8_t rslt;
    int8_t self_test_rslt = 0;
    struct bmm150_settings settings;

    /* Check for null pointer in the device structure */
    rslt = null_ptr_check(dev);

    /* Proceed if null check is fine */
    if (rslt == BMM150_OK)
    {
        switch (self_test_mode)
        {
            case BMM150_SELF_TEST_NORMAL:

                /* Set the sensor in sleep mode */
                settings.pwr_mode = BMM150_POWERMODE_SLEEP;
                rslt = bmm150_set_op_mode(&settings, dev);

                if (rslt == BMM150_OK)
                {
                    /* Perform the normal self test */
                    rslt = perform_normal_self_test(dev);
                }

                break;
            case BMM150_SELF_TEST_ADVANCED:

                /* Perform the advanced self test */
                rslt = perform_adv_self_test(dev);

                /* Check to ensure bus error does not occur */
                if (rslt >= BMM150_OK)
                {
                    /* Store the status of self test result */
                    self_test_rslt = rslt;

                    /* Perform soft reset */
                    rslt = bmm150_soft_reset(dev);
                }

                /* Check to ensure bus operations are success */
                if (rslt == BMM150_OK)
                {
                    /* Restore self_test_rslt as return value */
                    rslt = self_test_rslt;
                }

                break;
            default:
                rslt = BMM150_E_INVALID_CONFIG;
                break;
        }
    }

    return rslt;
}

/*!
 * @brief This API is used to get the status flags of all interrupt
 * which is used to check for the assertion of interrupts
 */
int8_t bmm150_get_interrupt_status(struct bmm150_dev *dev)
{
    int8_t rslt;
    uint8_t interrupt_status;
    uint8_t data_ready_status;

    /* Read the data ready status from the register 0x48 */
    rslt = bmm150_get_regs(BMM150_REG_DATA_READY_STATUS, &data_ready_status, 1, dev);

    if (rslt == BMM150_OK)
    {
        /* Read the interrupt status from the register 0x50 */
        rslt = bmm150_get_regs(BMM150_REG_INTERRUPT_STATUS, &interrupt_status, 1, dev);

        if (rslt == BMM150_OK)
        {
            /* Mask and store the data ready status bit */
            data_ready_status = BMM150_GET_BITS_POS_0(data_ready_status, BMM150_DRDY_STATUS);

            /* Store the entire interrupt status in dev */
            dev->int_status = (data_ready_status << 8) | interrupt_status;
        }
    }

    return rslt;
}

/****************************************************************************/
/**\name    BMM150 as Auxiliary Mag                                     */

/*!
 * @brief This API is used to compensate the raw mag data
 */
int8_t bmm150_aux_mag_data(uint8_t *aux_data, struct bmm150_mag_data *mag_data, const struct bmm150_dev *dev)
{
    int8_t rslt;
    int16_t msb_data;
    struct bmm150_raw_mag_data raw_mag_data;

    /* Check for null pointer in the device structure */
    rslt = null_ptr_check(dev);

    /* Proceed if null check is fine */
    if ((rslt == BMM150_OK) && (aux_data != NULL))
    {
        /* Mag X axis data */
        aux_data[0] = BMM150_GET_BITS(aux_data[0], BMM150_DATA_X);

        /* Shift the MSB data to left by 5 bits */
        /* Multiply by 32 to get the shift left by 5 value */
        msb_data = ((int16_t)((int8_t)aux_data[1])) * 32;

        /* Raw mag X axis data */
        raw_mag_data.raw_datax = (int16_t)(msb_data | aux_data[0]);

        /* Mag Y axis data */
        aux_data[2] = BMM150_GET_BITS(aux_data[2], BMM150_DATA_Y);

        /* Shift the MSB data to left by 5 bits */
        /* Multiply by 32 to get the shift left by 5 value */
        msb_data = ((int16_t)((int8_t)aux_data[3])) * 32;

        /* Raw mag Y axis data */
        raw_mag_data.raw_datay = (int16_t)(msb_data | aux_data[2]);

        /* Mag Z axis data */
        aux_data[4] = BMM150_GET_BITS(aux_data[4], BMM150_DATA_Z);

        /* Shift the MSB data to left by 7 bits */
        /* Multiply by 128 to get the shift left by 7 value */
        msb_data = ((int16_t)((int8_t)aux_data[5])) * 128;

        /* Raw mag Z axis data */
        raw_mag_data.raw_dataz = (int16_t)(msb_data | aux_data[4]);

        /* Mag R-HALL data */
        aux_data[6] = BMM150_GET_BITS(aux_data[6], BMM150_DATA_RHALL);
        raw_mag_data.raw_data_r = (uint16_t)(((uint16_t)aux_data[7] << 6) | aux_data[6]);

        /* Compensated Mag X data in int16_t format */
        mag_data->x = compensate_x(raw_mag_data.raw_datax, raw_mag_data.raw_data_r, dev);

        /* Compensated Mag Y data in int16_t format */
        mag_data->y = compensate_y(raw_mag_data.raw_datay, raw_mag_data.raw_data_r, dev);

        /* Compensated Mag Z data in int16_t format */
        mag_data->z = compensate_z(raw_mag_data.raw_dataz, raw_mag_data.raw_data_r, dev);
    }

    return rslt;
}

/****************************************************************************/
/**\name    INTERNAL APIs                                               */

/*!
 * @brief This internal API is used to validate the device structure pointer for
 * null conditions.
 */
static int8_t null_ptr_check(const struct bmm150_dev *dev)
{
    int8_t rslt;

    if ((dev == NULL) || (dev->read == NULL) || (dev->write == NULL) || (dev->delay_us == NULL) ||
        (dev->intf_ptr == NULL))
    {
        /* Device structure pointer is not valid */
        rslt = BMM150_E_NULL_PTR;
    }
    else
    {
        /* Device structure is fine */
        rslt = BMM150_OK;
    }

    return rslt;
}

/*!
 * @brief This internal API sets/resets the power control bit of 0x4B register.
 */
static int8_t set_power_control_bit(uint8_t pwrcntrl_bit, struct bmm150_dev *dev)
{
    int8_t rslt;
    uint8_t reg_data = 0;

    /* Power control register 0x4B is read */
    rslt = bmm150_get_regs(BMM150_REG_POWER_CONTROL, &reg_data, 1, dev);

    /* Proceed if everything is fine until now */
    if (rslt == BMM150_OK)
    {
        /* Sets the value of power control bit */
        reg_data = BMM150_SET_BITS_POS_0(reg_data, BMM150_PWR_CNTRL, pwrcntrl_bit);
        rslt = bmm150_set_regs(BMM150_REG_POWER_CONTROL, &reg_data, 1, dev);

        if (rslt == BMM150_OK)
        {
            /* Store the power control bit
             * value in dev structure
             */
            dev->pwr_cntrl_bit = pwrcntrl_bit;
        }
    }

    return rslt;
}

/*!
 * @brief This internal API reads the trim registers of the sensor and stores
 * the trim values in the "trim_data" of device structure.
 */
static int8_t read_trim_registers(struct bmm150_dev *dev)
{
    int8_t rslt;
    uint8_t trim_x1y1[2] = { 0 };
    uint8_t trim_xyz_data[4] = { 0 };
    uint8_t trim_xy1xy2[10] = { 0 };
    uint16_t temp_msb = 0;

    /* Trim register value is read */
    rslt = bmm150_get_regs(BMM150_DIG_X1, trim_x1y1, 2, dev);

    if (rslt == BMM150_OK)
    {
        rslt = bmm150_get_regs(BMM150_DIG_Z4_LSB, trim_xyz_data, 4, dev);

        if (rslt == BMM150_OK)
        {
            rslt = bmm150_get_regs(BMM150_DIG_Z2_LSB, trim_xy1xy2, 10, dev);

            if (rslt == BMM150_OK)
            {
                /* Trim data which is read is updated
                 * in the device structure
                 */
                dev->trim_data.dig_x1 = (int8_t)trim_x1y1[0];
                dev->trim_data.dig_y1 = (int8_t)trim_x1y1[1];
                dev->trim_data.dig_x2 = (int8_t)trim_xyz_data[2];
                dev->trim_data.dig_y2 = (int8_t)trim_xyz_data[3];
                temp_msb = ((uint16_t)trim_xy1xy2[3]) << 8;
                dev->trim_data.dig_z1 = (uint16_t)(temp_msb | trim_xy1xy2[2]);
                temp_msb = ((uint16_t)trim_xy1xy2[1]) << 8;
                dev->trim_data.dig_z2 = (int16_t)(temp_msb | trim_xy1xy2[0]);
                temp_msb = ((uint16_t)trim_xy1xy2[7]) << 8;
                dev->trim_data.dig_z3 = (int16_t)(temp_msb | trim_xy1xy2[6]);
                temp_msb = ((uint16_t)trim_xyz_data[1]) << 8;
                dev->trim_data.dig_z4 = (int16_t)(temp_msb | trim_xyz_data[0]);
                dev->trim_data.dig_xy1 = trim_xy1xy2[9];
                dev->trim_data.dig_xy2 = (int8_t)trim_xy1xy2[8];
                temp_msb = ((uint16_t)(trim_xy1xy2[5] & 0x7F)) << 8;
                dev->trim_data.dig_xyz1 = (uint16_t)(temp_msb | trim_xy1xy2[4]);
            }
        }
    }

    return rslt;
}

/*!
 * @brief This internal API writes the op_mode value in the Opmode bits
 * (bits 1 and 2) of 0x4C register.
 */
static int8_t write_op_mode(uint8_t op_mode, struct bmm150_dev *dev)
{
    int8_t rslt;
    uint8_t reg_data;

    /* Read the 0x4C register */
    rslt = bmm150_get_regs(BMM150_REG_OP_MODE, &reg_data, 1, dev);

    if (rslt == BMM150_OK)
    {
        /* Set the op_mode value in Opmode bits of 0x4C */
        reg_data = BMM150_SET_BITS(reg_data, BMM150_OP_MODE, op_mode);
        rslt = bmm150_set_regs(BMM150_REG_OP_MODE, &reg_data, 1, dev);
    }

    return rslt;
}

/*!
 * @brief This internal API sets the device from suspend to sleep mode
 * by setting the power control bit to '1' of 0x4B register
 */
static int8_t suspend_to_sleep_mode(struct bmm150_dev *dev)
{
    int8_t rslt;

    /* Check for null pointer in the device structure */
    rslt = null_ptr_check(dev);

    /* Proceed if null check is fine */
    if (rslt == BMM150_OK)
    {
        if (dev->pwr_cntrl_bit == BMM150_POWER_CNTRL_DISABLE)
        {
            rslt = set_power_control_bit(BMM150_POWER_CNTRL_ENABLE, dev);

            /* Start-up time delay of 3ms */
            dev->delay_us(BMM150_START_UP_TIME, dev->intf_ptr);
        }
    }

    return rslt;
}

/*!
 * @brief This internal API sets the xy repetition value in the 0x51 register.
 */
static int8_t set_xy_rep(const struct bmm150_settings *settings, struct bmm150_dev *dev)
{
    int8_t rslt;
    uint8_t rep_xy;

    /* Set the xy repetition */
    rep_xy = settings->xy_rep;
    rslt = bmm150_set_regs(BMM150_REG_REP_XY, &rep_xy, 1, dev);

    return rslt;
}

/*!
 * @brief This internal API sets the z repetition value in the 0x52 register.
 */
static int8_t set_z_rep(const struct bmm150_settings *settings, struct bmm150_dev *dev)
{
    int8_t rslt;
    uint8_t rep_z;

    /* Set the z repetition */
    rep_z = settings->z_rep;
    rslt = bmm150_set_regs(BMM150_REG_REP_Z, &rep_z, 1, dev);

    return rslt;
}

/*!
 * @brief This internal API is used to set the output data rate of the sensor.
 */
static int8_t set_odr(const struct bmm150_settings *settings, struct bmm150_dev *dev)
{
    int8_t rslt;
    uint8_t reg_data;

    /* Read the 0x4C register */
    rslt = bmm150_get_regs(BMM150_REG_OP_MODE, &reg_data, 1, dev);

    if (rslt == BMM150_OK)
    {
        /* Set the ODR value */
        reg_data = BMM150_SET_BITS(reg_data, BMM150_ODR, settings->data_rate);
        rslt = bmm150_set_regs(BMM150_REG_OP_MODE, &reg_data, 1, dev);
    }

    return rslt;
}

/*!
 * @brief This internal API sets the preset mode ODR and repetition settings.
 */
static int8_t set_odr_xyz_rep(const struct bmm150_settings *settings, struct bmm150_dev *dev)
{
    int8_t rslt;

    /* Set the ODR */
    rslt = set_odr(settings, dev);

    if (rslt == BMM150_OK)
    {
        /* Set the XY-repetitions number */
        rslt = set_xy_rep(settings, dev);

        if (rslt == BMM150_OK)
        {
            /* Set the Z-repetitions number */
            rslt = set_z_rep(settings, dev);
        }
    }

    return rslt;
}

/*!
 * @brief This internal API is used to enable or disable the magnetic
 * measurement of x,y,z axes based on the value of xyz_axes_control.
 */
static int8_t set_control_measurement_xyz(const struct bmm150_settings *settings, struct bmm150_dev *dev)
{
    int8_t rslt;
    uint8_t reg_data;

    rslt = bmm150_get_regs(BMM150_REG_AXES_ENABLE, &reg_data, 1, dev);

    if (rslt == BMM150_OK)
    {
        /* Set the axes to be enabled/disabled */
        reg_data = BMM150_SET_BITS(reg_data, BMM150_CONTROL_MEASURE, settings->xyz_axes_control);
        rslt = bmm150_set_regs(BMM150_REG_AXES_ENABLE, &reg_data, 1, dev);
    }

    return rslt;
}

/*!
 * @brief This internal API is used to identify the settings which the user
 * wants to modify in the sensor.
 */
static uint8_t are_settings_changed(uint16_t sub_settings, uint16_t desired_settings)
{
    uint8_t settings_changed;

    if (sub_settings & desired_settings)
    {
        /* User wants to modify this particular settings */
        settings_changed = BMM150_TRUE;
    }
    else
    {
        /* User don't want to modify this particular settings */
        settings_changed = BMM150_FALSE;
    }

    return settings_changed;
}

/*!
 * @brief This API sets the ODR , measurement axes control ,
 * repetition values of xy,z.
 */
static int8_t mode_settings(uint16_t desired_settings, const struct bmm150_settings *settings, struct bmm150_dev *dev)
{
    int8_t rslt = BMM150_E_INVALID_CONFIG;

    if (desired_settings & BMM150_SEL_DATA_RATE)
    {
        /* Sets the ODR */
        rslt = set_odr(settings, dev);
    }

    if (desired_settings & BMM150_SEL_CONTROL_MEASURE)
    {
        /* Enables/Disables the control measurement axes */
        rslt = set_control_measurement_xyz(settings, dev);
    }

    if (desired_settings & BMM150_SEL_XY_REP)
    {
        /* Sets the XY repetition */
        rslt = set_xy_rep(settings, dev);
    }

    if (desired_settings & BMM150_SEL_Z_REP)
    {
        /* Sets the Z repetition */
        rslt = set_z_rep(settings, dev);
    }

    return rslt;
}

/*!
 * @brief This internal API is used to parse and store the sensor
 * settings in the device structure
 */
static void parse_setting(const uint8_t *reg_data, struct bmm150_settings *settings)
{
    /* Parse all the w/r registers and update the
     * current sensor settings in the dev structure
     */
    settings->z_rep = reg_data[7];
    settings->xy_rep = reg_data[6];
    settings->int_settings.high_threshold = reg_data[5];
    settings->int_settings.low_threshold = reg_data[4];
    settings->xyz_axes_control = BMM150_GET_BITS(reg_data[3], BMM150_CONTROL_MEASURE);
    settings->int_settings.drdy_pin_en = BMM150_GET_BITS(reg_data[3], BMM150_DRDY_EN);
    settings->int_settings.int_pin_en = BMM150_GET_BITS(reg_data[3], BMM150_INT_PIN_EN);
    settings->int_settings.drdy_polarity = BMM150_GET_BITS(reg_data[3], BMM150_DRDY_POLARITY);
    settings->int_settings.int_latch = BMM150_GET_BITS(reg_data[3], BMM150_INT_LATCH);
    settings->int_settings.int_polarity = BMM150_GET_BITS_POS_0(reg_data[3], BMM150_INT_POLARITY);
    settings->int_settings.data_overrun_en = BMM150_GET_BITS(reg_data[2], BMM150_DATA_OVERRUN_INT);
    settings->int_settings.overflow_int_en = BMM150_GET_BITS(reg_data[2], BMM150_OVERFLOW_INT);
    settings->int_settings.high_int_en = BMM150_GET_BITS(reg_data[2], BMM150_HIGH_THRESHOLD_INT);
    settings->int_settings.low_int_en = BMM150_GET_BITS_POS_0(reg_data[2], BMM150_LOW_THRESHOLD_INT);
    settings->data_rate = BMM150_GET_BITS(reg_data[1], BMM150_ODR);
}

/*!
 * @brief This API is used to enable the interrupts and map them to the
 * corresponding interrupt pins and specify the pin characteristics like the
 * polarity , latch settings for the interrupt pins.
 */
static int8_t interrupt_pin_settings(uint16_t desired_settings,
                                     const struct bmm150_settings *settings,
                                     struct bmm150_dev *dev)
{
    int8_t rslt;
    uint8_t reg_data;
    struct bmm150_int_ctrl_settings int_settings;

    rslt = bmm150_get_regs(BMM150_REG_AXES_ENABLE, &reg_data, 1, dev);

    if (rslt == BMM150_OK)
    {
        int_settings = settings->int_settings;
        if (desired_settings & BMM150_SEL_DRDY_PIN_EN)
        {
            /* Enables the Data ready interrupt and
             * maps it to the DRDY pin of the sensor
             */
            reg_data = BMM150_SET_BITS(reg_data, BMM150_DRDY_EN, int_settings.drdy_pin_en);
        }

        if (desired_settings & BMM150_SEL_INT_PIN_EN)
        {
            /* Sets interrupt pin enable */
            reg_data = BMM150_SET_BITS(reg_data, BMM150_INT_PIN_EN, int_settings.int_pin_en);
        }

        if (desired_settings & BMM150_SEL_DRDY_POLARITY)
        {
            /* Sets Data ready pin's polarity */
            reg_data = BMM150_SET_BITS(reg_data, BMM150_DRDY_POLARITY, int_settings.drdy_polarity);
        }

        if (desired_settings & BMM150_SEL_INT_LATCH)
        {
            /* Sets Interrupt in latched or non-latched mode */
            reg_data = BMM150_SET_BITS(reg_data, BMM150_INT_LATCH, int_settings.int_latch);
        }

        if (desired_settings & BMM150_SEL_INT_POLARITY)
        {
            /* Sets Interrupt pin's polarity */
            reg_data = BMM150_SET_BITS_POS_0(reg_data, BMM150_INT_POLARITY, int_settings.int_polarity);
        }

        /* Set the interrupt configurations in the 0x4E register */
        rslt = bmm150_set_regs(BMM150_REG_AXES_ENABLE, &reg_data, 1, dev);
    }

    return rslt;
}

/*!
 * @brief This API is used to enable data overrun , overflow interrupts and
 * enable/disable high/low threshold interrupts for x,y,z axis based on the
 * threshold values set by the user in the High threshold (0x50) and
 * Low threshold (0x4F) registers.
 */
static int8_t interrupt_config(uint16_t desired_settings, const struct bmm150_settings *settings,
                               struct bmm150_dev *dev)
{
    int8_t rslt;
    uint8_t reg_data;
    struct bmm150_int_ctrl_settings int_settings;

    rslt = bmm150_get_regs(BMM150_REG_INT_CONFIG, &reg_data, 1, dev);

    if (rslt == BMM150_OK)
    {
        int_settings = settings->int_settings;
        if (desired_settings & BMM150_SEL_DATA_OVERRUN_INT)
        {
            /* Sets Data overrun interrupt */
            reg_data = BMM150_SET_BITS(reg_data, BMM150_DATA_OVERRUN_INT, int_settings.data_overrun_en);
        }

        if (desired_settings & BMM150_SEL_OVERFLOW_INT)
        {
            /* Sets Data overflow interrupt */
            reg_data = BMM150_SET_BITS(reg_data, BMM150_OVERFLOW_INT, int_settings.overflow_int_en);
        }

        if (desired_settings & BMM150_SEL_HIGH_THRESHOLD_INT)
        {
            /* Sets high threshold interrupt */
            reg_data = BMM150_SET_BITS(reg_data, BMM150_HIGH_THRESHOLD_INT, int_settings.high_int_en);
        }

        if (desired_settings & BMM150_SEL_LOW_THRESHOLD_INT)
        {
            /* Sets low threshold interrupt */
            reg_data = BMM150_SET_BITS_POS_0(reg_data, BMM150_LOW_THRESHOLD_INT, int_settings.low_int_en);
        }

        /* Set the interrupt configurations in the 0x4D register */
        rslt = bmm150_set_regs(BMM150_REG_INT_CONFIG, &reg_data, 1, dev);
    }

    return rslt;
}

/*!
 * @brief This API is used to write the user specified High/Low threshold value
 * as a reference to generate the high/low threshold interrupt.
 */
static int8_t interrupt_threshold_settings(uint16_t desired_settings,
                                           const struct bmm150_settings *settings,
                                           struct bmm150_dev *dev)
{
    int8_t rslt = BMM150_E_INVALID_CONFIG;
    uint8_t reg_data;

    if (desired_settings & BMM150_SEL_LOW_THRESHOLD_SETTING)
    {
        /* Sets the Low threshold value to trigger interrupt */
        reg_data = settings->int_settings.low_threshold;
        rslt = bmm150_set_regs(BMM150_REG_LOW_THRESHOLD, &reg_data, 1, dev);
    }

    if (desired_settings & BMM150_SEL_HIGH_THRESHOLD_SETTING)
    {
        /* Sets the High threshold value to trigger interrupt */
        reg_data = settings->int_settings.high_threshold;
        rslt = bmm150_set_regs(BMM150_REG_HIGH_THRESHOLD, &reg_data, 1, dev);
    }

    return rslt;
}

#ifdef BMM150_USE_FLOATING_POINT

/*!
 * @brief This internal API is used to obtain the compensated
 * magnetometer x axis data(micro-tesla) in float.
 */
static float compensate_x(int16_t mag_data_x, uint16_t data_rhall, const struct bmm150_dev *dev)
{
    float retval = 0;
    float process_comp_x0;
    float process_comp_x1;
    float process_comp_x2;
    float process_comp_x3;
    float process_comp_x4;

    /* Overflow condition check */
    if ((mag_data_x != BMM150_OVERFLOW_ADCVAL_XYAXES_FLIP) && (data_rhall != 0) && (dev->trim_data.dig_xyz1 != 0))
    {
        /* Processing compensation equations */
        process_comp_x0 = (((float)dev->trim_data.dig_xyz1) * 16384.0f / data_rhall);
        retval = (process_comp_x0 - 16384.0f);
        process_comp_x1 = ((float)dev->trim_data.dig_xy2) * (retval * retval / 268435456.0f);
        process_comp_x2 = process_comp_x1 + retval * ((float)dev->trim_data.dig_xy1) / 16384.0f;
        process_comp_x3 = ((float)dev->trim_data.dig_x2) + 160.0f;
        process_comp_x4 = mag_data_x * ((process_comp_x2 + 256.0f) * process_comp_x3);
        retval = ((process_comp_x4 / 8192.0f) + (((float)dev->trim_data.dig_x1) * 8.0f)) / 16.0f;
    }
    else
    {
        /* Overflow, set output to 0.0f */
        retval = BMM150_OVERFLOW_OUTPUT_FLOAT;
    }

    return retval;
}

/*!
 * @brief This internal API is used to obtain the compensated
 * magnetometer y axis data(micro-tesla) in float.
 */
static float compensate_y(int16_t mag_data_y, uint16_t data_rhall, const struct bmm150_dev *dev)
{
    float retval = 0;
    float process_comp_y0;
    float process_comp_y1;
    float process_comp_y2;
    float process_comp_y3;
    float process_comp_y4;

    /* Overflow condition check */
    if ((mag_data_y != BMM150_OVERFLOW_ADCVAL_XYAXES_FLIP) && (data_rhall != 0) && (dev->trim_data.dig_xyz1 != 0))
    {
        /* Processing compensation equations */
        process_comp_y0 = ((float)dev->trim_data.dig_xyz1) * 16384.0f / data_rhall;
        retval = process_comp_y0 - 16384.0f;
        process_comp_y1 = ((float)dev->trim_data.dig_xy2) * (retval * retval / 268435456.0f);
        process_comp_y2 = process_comp_y1 + retval * ((float)dev->trim_data.dig_xy1) / 16384.0f;
        process_comp_y3 = ((float)dev->trim_data.dig_y2) + 160.0f;
        process_comp_y4 = mag_data_y * (((process_comp_y2) + 256.0f) * process_comp_y3);
        retval = ((process_comp_y4 / 8192.0f) + (((float)dev->trim_data.dig_y1) * 8.0f)) / 16.0f;
    }
    else
    {
        /* Overflow, set output to 0.0f */
        retval = BMM150_OVERFLOW_OUTPUT_FLOAT;
    }

    return retval;
}

/*!
 * @brief This internal API is used to obtain the compensated
 * magnetometer z axis data(micro-tesla) in float.
 */
static float compensate_z(int16_t mag_data_z, uint16_t data_rhall, const struct bmm150_dev *dev)
{
    float retval = 0;
    float process_comp_z0;
    float process_comp_z1;
    float process_comp_z2;
    float process_comp_z3;
    float process_comp_z4;
    float process_comp_z5;

    /* Overflow condition check */
    if ((mag_data_z != BMM150_OVERFLOW_ADCVAL_ZAXIS_HALL) && (dev->trim_data.dig_z2 != 0) &&
        (dev->trim_data.dig_z1 != 0) && (dev->trim_data.dig_xyz1 != 0) && (data_rhall != 0))
    {
        /* Processing compensation equations */
        process_comp_z0 = ((float)mag_data_z) - ((float)dev->trim_data.dig_z4);
        process_comp_z1 = ((float)data_rhall) - ((float)dev->trim_data.dig_xyz1);
        process_comp_z2 = (((float)dev->trim_data.dig_z3) * process_comp_z1);
        process_comp_z3 = ((float)dev->trim_data.dig_z1) * ((float)data_rhall) / 32768.0f;
        process_comp_z4 = ((float)dev->trim_data.dig_z2) + process_comp_z3;
        process_comp_z5 = (process_comp_z0 * 131072.0f) - process_comp_z2;
        retval = (process_comp_z5 / ((process_comp_z4) * 4.0f)) / 16.0f;
    }
    else
    {
        /* Overflow, set output to 0.0f */
        retval = BMM150_OVERFLOW_OUTPUT_FLOAT;
    }

    return retval;
}

#else

/*!
 * @brief This internal API is used to obtain the compensated
 * magnetometer X axis data(micro-tesla) in int16_t.
 */
static int16_t compensate_x(int16_t mag_data_x, uint16_t data_rhall, const struct bmm150_dev *dev)
{
    int16_t retval;
    uint16_t process_comp_x0 = 0;
    int32_t process_comp_x1;
    uint16_t process_comp_x2;
    int32_t process_comp_x3;
    int32_t process_comp_x4;
    int32_t process_comp_x5;
    int32_t process_comp_x6;
    int32_t process_comp_x7;
    int32_t process_comp_x8;
    int32_t process_comp_x9;
    int32_t process_comp_x10;

    /* Overflow condition check */
    if (mag_data_x != BMM150_OVERFLOW_ADCVAL_XYAXES_FLIP)
    {
        if (data_rhall != 0)
        {
            /* Availability of valid data */
            process_comp_x0 = data_rhall;
        }
        else if (dev->trim_data.dig_xyz1 != 0)
        {
            process_comp_x0 = dev->trim_data.dig_xyz1;
        }
        else
        {
            process_comp_x0 = 0;
        }

        if (process_comp_x0 != 0)
        {
            /* Processing compensation equations */
            process_comp_x1 = ((int32_t)dev->trim_data.dig_xyz1) * 16384;
            process_comp_x2 = ((uint16_t)(process_comp_x1 / process_comp_x0)) - ((uint16_t)0x4000);
            retval = ((int16_t)process_comp_x2);
            process_comp_x3 = (((int32_t)retval) * ((int32_t)retval));
            process_comp_x4 = (((int32_t)dev->trim_data.dig_xy2) * (process_comp_x3 / 128));
            process_comp_x5 = (int32_t)(((int16_t)dev->trim_data.dig_xy1) * 128);
            process_comp_x6 = ((int32_t)retval) * process_comp_x5;
            process_comp_x7 = (((process_comp_x4 + process_comp_x6) / 512) + ((int32_t)0x100000));
            process_comp_x8 = ((int32_t)(((int16_t)dev->trim_data.dig_x2) + ((int16_t)0xA0)));
            process_comp_x9 = ((process_comp_x7 * process_comp_x8) / 4096);
            process_comp_x10 = ((int32_t)mag_data_x) * process_comp_x9;
            retval = ((int16_t)(process_comp_x10 / 8192));
            retval = (retval + (((int16_t)dev->trim_data.dig_x1) * 8)) / 16;
        }
        else
        {
            retval = BMM150_OVERFLOW_OUTPUT;
        }
    }
    else
    {
        /* Overflow condition */
        retval = BMM150_OVERFLOW_OUTPUT;
    }

    return retval;
}

/*!
 * @brief This internal API is used to obtain the compensated
 * magnetometer Y axis data(micro-tesla) in int16_t.
 */
static int16_t compensate_y(int16_t mag_data_y, uint16_t data_rhall, const struct bmm150_dev *dev)
{
    int16_t retval;
    uint16_t process_comp_y0 = 0;
    int32_t process_comp_y1;
    uint16_t process_comp_y2;
    int32_t process_comp_y3;
    int32_t process_comp_y4;
    int32_t process_comp_y5;
    int32_t process_comp_y6;
    int32_t process_comp_y7;
    int32_t process_comp_y8;
    int32_t process_comp_y9;

    /* Overflow condition check */
    if (mag_data_y != BMM150_OVERFLOW_ADCVAL_XYAXES_FLIP)
    {
        if (data_rhall != 0)
        {
            /* Availability of valid data */
            process_comp_y0 = data_rhall;
        }
        else if (dev->trim_data.dig_xyz1 != 0)
        {
            process_comp_y0 = dev->trim_data.dig_xyz1;
        }
        else
        {
            process_comp_y0 = 0;
        }

        if (process_comp_y0 != 0)
        {
            /* Processing compensation equations */
            process_comp_y1 = (((int32_t)dev->trim_data.dig_xyz1) * 16384) / process_comp_y0;
            process_comp_y2 = ((uint16_t)process_comp_y1) - ((uint16_t)0x4000);
            retval = ((int16_t)process_comp_y2);
            process_comp_y3 = ((int32_t) retval) * ((int32_t)retval);
            process_comp_y4 = ((int32_t)dev->trim_data.dig_xy2) * (process_comp_y3 / 128);
            process_comp_y5 = ((int32_t)(((int16_t)dev->trim_data.dig_xy1) * 128));
            process_comp_y6 = ((process_comp_y4 + (((int32_t)retval) * process_comp_y5)) / 512);
            process_comp_y7 = ((int32_t)(((int16_t)dev->trim_data.dig_y2) + ((int16_t)0xA0)));
            process_comp_y8 = (((process_comp_y6 + ((int32_t)0x100000)) * process_comp_y7) / 4096);
            process_comp_y9 = (((int32_t)mag_data_y) * process_comp_y8);
            retval = (int16_t)(process_comp_y9 / 8192);
            retval = (retval + (((int16_t)dev->trim_data.dig_y1) * 8)) / 16;
        }
        else
        {
            retval = BMM150_OVERFLOW_OUTPUT;
        }
    }
    else
    {
        /* Overflow condition */
        retval = BMM150_OVERFLOW_OUTPUT;
    }

    return retval;
}

/*!
 * @brief This internal API is used to obtain the compensated
 * magnetometer Z axis data(micro-tesla) in int16_t.
 */
static int16_t compensate_z(int16_t mag_data_z, uint16_t data_rhall, const struct bmm150_dev *dev)
{
    int32_t retval;
    int16_t process_comp_z0;
    int32_t process_comp_z1;
    int32_t process_comp_z2;
    int32_t process_comp_z3;
    int16_t process_comp_z4;

    if (mag_data_z != BMM150_OVERFLOW_ADCVAL_ZAXIS_HALL)
    {
        if ((dev->trim_data.dig_z2 != 0) && (dev->trim_data.dig_z1 != 0) && (data_rhall != 0) &&
            (dev->trim_data.dig_xyz1 != 0))
        {
            /*Processing compensation equations */
            process_comp_z0 = ((int16_t)data_rhall) - ((int16_t) dev->trim_data.dig_xyz1);
            process_comp_z1 = (((int32_t)dev->trim_data.dig_z3) * ((int32_t)(process_comp_z0))) / 4;
            process_comp_z2 = (((int32_t)(mag_data_z - dev->trim_data.dig_z4)) * 32768);
            process_comp_z3 = ((int32_t)dev->trim_data.dig_z1) * (((int16_t)data_rhall) * 2);
            process_comp_z4 = (int16_t)((process_comp_z3 + (32768)) / 65536);
            retval = ((process_comp_z2 - process_comp_z1) / (dev->trim_data.dig_z2 + process_comp_z4));

            /* Saturate result to +/- 2 micro-tesla */
            if (retval > BMM150_POSITIVE_SATURATION_Z)
            {
                retval = BMM150_POSITIVE_SATURATION_Z;
            }
            else if (retval < BMM150_NEGATIVE_SATURATION_Z)
            {
                retval = BMM150_NEGATIVE_SATURATION_Z;
            }

            /* Conversion of LSB to micro-tesla */
            retval = retval / 16;
        }
        else
        {
            retval = BMM150_OVERFLOW_OUTPUT;
        }
    }
    else
    {
        /* Overflow condition */
        retval = BMM150_OVERFLOW_OUTPUT;
    }

    return (int16_t)retval;
}

#endif

/*!
 * @brief This internal API is used to perform the normal self test
 * of the sensor and return the self test result as return value
 */
static int8_t perform_normal_self_test(struct bmm150_dev *dev)
{
    int8_t rslt;
    uint8_t self_test_bit;

    /* Triggers the start of normal self test */
    rslt = enable_normal_self_test(&self_test_bit, dev);

    /* Check for self test completion status */
    if ((rslt == BMM150_OK) && (self_test_bit == 0))
    {
        /* Validates the self test results for all 3 axes */
        rslt = validate_normal_self_test(dev);
    }

    return rslt;
}

/*!
 * @brief This internal API is used to enable the normal self test by setting
 * the Self Test bit (bit0) of the 0x4C register,
 * which triggers the start of self test
 */
static int8_t enable_normal_self_test(uint8_t *self_test_enable, struct bmm150_dev *dev)
{
    int8_t rslt;
    uint8_t reg_data;
    uint8_t self_test_val;

    /* Read the data from register 0x4C */
    rslt = bmm150_get_regs(BMM150_REG_OP_MODE, &reg_data, 1, dev);

    if (rslt == BMM150_OK)
    {
        /* Set the Self Test bit(bit0) of the 0x4C register */
        self_test_val = 1;
        reg_data = BMM150_SET_BITS_POS_0(reg_data, BMM150_SELF_TEST, self_test_val);

        /* Write the data to 0x4C register to trigger self test */
        rslt = bmm150_set_regs(BMM150_REG_OP_MODE, &reg_data, 1, dev);
        dev->delay_us(BMM150_DELAY_NORMAL_SELF_TEST, dev->intf_ptr);

        if (rslt == BMM150_OK)
        {
            /* Read the data from register 0x4C */
            rslt = bmm150_get_regs(BMM150_REG_OP_MODE, &reg_data, 1, dev);

            /* Self Test bit(bit0) is stored in self_test_enable,
             * It will be reset to zero after the self test is over
             */
            *self_test_enable = BMM150_GET_BITS_POS_0(reg_data, BMM150_SELF_TEST);
        }
    }

    return rslt;
}

/*!
 * @brief This internal API is used to validate the results of normal self test
 * by using the self test status available in the bit0 of registers 0x42,0x44
 * and 0x46.
 */
static int8_t validate_normal_self_test(struct bmm150_dev *dev)
{
    int8_t rslt;
    uint8_t status;
    uint8_t self_test_rslt[5];

    /* Read the data from register 0x42 to 0x46 */
    rslt = bmm150_get_regs(BMM150_REG_DATA_X_LSB, self_test_rslt, BMM150_LEN_SELF_TEST, dev);

    if (rslt == BMM150_OK)
    {
        /* Parse and get the self test status bits */
        /* X-Self-Test (bit0) of 0x42 register is stored */
        self_test_rslt[0] = BMM150_GET_BITS_POS_0(self_test_rslt[0], BMM150_SELF_TEST);

        /* Y-Self-Test (bit0) of 0x44 register is stored */
        self_test_rslt[2] = BMM150_GET_BITS_POS_0(self_test_rslt[2], BMM150_SELF_TEST);

        /* Z-Self-Test (bit0) of 0x46 register is stored */
        self_test_rslt[4] = BMM150_GET_BITS_POS_0(self_test_rslt[4], BMM150_SELF_TEST);

        /* Combine the self test status and store it in the first
         * 3 bits of the status variable for processing
         */
        status = (uint8_t)((self_test_rslt[4] << 2) | (self_test_rslt[2] << 1) | self_test_rslt[0]);

        /* Validate status and store Self test result in "rslt" */
        if (status == BMM150_SELF_TEST_STATUS_SUCCESS)
        {
            /* Self test is success when all status bits are set */
            rslt = BMM150_OK;
        }
        else
        {
            if (status == BMM150_SELF_TEST_STATUS_XYZ_FAIL)
            {
                /* Self test - all axis fail condition */
                rslt = BMM150_W_NORMAL_SELF_TEST_XYZ_FAIL;
            }
            else
            {
                /* Self test - some axis fail condition */
                rslt = (int8_t)status;
            }
        }
    }

    return rslt;
}

/*!
 * @brief This internal API is used to perform advanced self test for Z axis
 */
static int8_t perform_adv_self_test(struct bmm150_dev *dev)
{
    int8_t rslt;
    uint8_t self_test_current;
    int16_t positive_data_z;
    int16_t negative_data_z;

    /* Set the desired power mode ,axes control and repetition settings */
    rslt = adv_self_test_settings(dev);

    if (rslt == BMM150_OK)
    {
        /* Measure the Z axes data with positive self-test current */
        self_test_current = BMM150_ENABLE_POSITIVE_CURRENT;
        rslt = adv_self_test_measurement(self_test_current, &positive_data_z, dev);

        if (rslt == BMM150_OK)
        {
            /* Measure the Z axes data with
             * negative self-test current
             */
            self_test_current = BMM150_ENABLE_NEGATIVE_CURRENT;
            rslt = adv_self_test_measurement(self_test_current, &negative_data_z, dev);

            if (rslt == BMM150_OK)
            {
                /* Disable self-test current */
                self_test_current = BMM150_DISABLE_SELF_TEST_CURRENT;
                rslt = set_adv_self_test_current(self_test_current, dev);

                if (rslt == BMM150_OK)
                {
                    /* Validate the advanced self test */
                    rslt = validate_adv_self_test(positive_data_z, negative_data_z);
                }
            }
        }
    }

    return rslt;
}

/*!
 * @brief This internal API is used to set the desired power mode ,
 * axes control and repetition settings for advanced self test
 */
static int8_t adv_self_test_settings(struct bmm150_dev *dev)
{
    int8_t rslt;
    struct bmm150_settings settings;

    /* Set the power mode as sleep mode */
    settings.pwr_mode = BMM150_POWERMODE_SLEEP;
    rslt = bmm150_set_op_mode(&settings, dev);

    if (rslt == BMM150_OK)
    {
        /* Disable XY-axis measurement */
        settings.xyz_axes_control = BMM150_DISABLE_XY_AXIS;
        rslt = set_control_measurement_xyz(&settings, dev);

        if (rslt == BMM150_OK)
        {
            /* Repetition value is set as 0x04 */
            settings.z_rep = BMM150_SELF_TEST_REP_Z;
            rslt = set_z_rep(&settings, dev);
        }
    }

    return rslt;
}

/*!
 * @brief This internal API is used to set the positive or negative value of
 * self-test current and obtain the corresponding magnetometer z axis data
 */
static int8_t adv_self_test_measurement(uint8_t self_test_current, int16_t *data_z, struct bmm150_dev *dev)
{
    int8_t rslt;
    struct bmm150_settings settings;
    struct bmm150_mag_data mag_data;

    /* Set the advanced self test current as positive or
     * negative based on the value of parameter "self_test_current"
     */
    rslt = set_adv_self_test_current(self_test_current, dev);

    if (rslt == BMM150_OK)
    {
        /* Set the device in forced mode */
        settings.pwr_mode = BMM150_POWERMODE_FORCED;
        rslt = bmm150_set_op_mode(&settings, dev);

        /* Delay to ensure measurement is complete */
        dev->delay_us(BMM150_DELAY_ADV_SELF_TEST, dev->intf_ptr);

        if (rslt == BMM150_OK)
        {
            /* Read Mag data and store the value of Z axis data */
            rslt = bmm150_read_mag_data(&mag_data, dev);

            if (rslt == BMM150_OK)
            {
                /* Mag Z axis data is stored */
                *data_z = mag_data.z;
            }
        }
    }

    return rslt;
}

/*!
 * @brief This internal API is used to get the difference between the
 * Z axis mag data obtained by positive and negative self-test current
 * and validate whether the advanced self test is done successfully or not.
 */
static int8_t validate_adv_self_test(int16_t positive_data_z, int16_t negative_data_z)
{
    int32_t adv_self_test_rslt;
    int8_t rslt;

    /* Advanced self test difference between the Z axis mag data
     * obtained by the positive and negative self-test current
     */
    adv_self_test_rslt = positive_data_z - negative_data_z;

    /* Advanced self test validation */
    /*Value of adv_self_test_rslt should be in between 180-240 micro-tesla */
    if ((adv_self_test_rslt > 180) && (adv_self_test_rslt < 240))
    {
        /* Advanced self test success */
        rslt = BMM150_OK;
    }
    else
    {
        /* Advanced self test fail */
        rslt = BMM150_W_ADV_SELF_TEST_FAIL;
    }

    return rslt;
}

/*!
 * @brief This internal API is used to set the self test current value in
 * the Adv. ST bits (bit6 and bit7) of 0x4C register
 */
static int8_t set_adv_self_test_current(uint8_t self_test_current, struct bmm150_dev *dev)
{
    int8_t rslt;
    uint8_t reg_data;

    /* Read the 0x4C register */
    rslt = bmm150_get_regs(BMM150_REG_OP_MODE, &reg_data, 1, dev);

    if (rslt == BMM150_OK)
    {
        /* Set the self test current value in the Adv. ST bits
         * (bit6 and bit7) of 0x4c register
         */
        reg_data = BMM150_SET_BITS(reg_data, BMM150_ADV_SELF_TEST, self_test_current);
        rslt = bmm150_set_regs(BMM150_REG_OP_MODE, &reg_data, 1, dev);
    }

    return rslt;
}
