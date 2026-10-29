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
* @file       bmm350.c
* @date       2023-05-26
* @version    v1.4.0
*
*/

/*************************** Header files *******************************/
#include "bmm350.h"

#ifdef __KERNEL__
#include <linux/types.h>
#include <linux/kernel.h>
#else
#include <stdio.h>
#include <stdlib.h>
#endif

/********************** Static function declarations ************************/

/*!
 * @brief This internal API is used to validate the device pointer for
 * null conditions.
 *
 * @param[in] dev : Structure instance of bmm350_dev.
 *
 * @return Result of API execution status
 * @retval = 0 -> Success
 * @retval < 0 -> Error
 */
static int8_t null_ptr_check(const struct bmm350_dev *dev);

/*!
 * @brief This internal API is used to update magnetometer offset and sensitivity data.
 *
 * @param[in] dev : Structure instance of bmm350_dev.
 *
 *  @return void
 */
static void update_mag_off_sens(struct bmm350_dev *dev);

/*!
 * @brief This internal API converts the raw data from the IC data registers to signed integer
 *
 * @param[in] inval         : Unsigned data from data registers
 * @param[in number_of_bits : Width of data register
 *
 * @return Conversion to signed integer
 */
static int32_t fix_sign(uint32_t inval, int8_t number_of_bits);

/*!
 * @brief This internal API is used to read OTP word
 *
 * @param[in]  addr        : Stores OTP address
 * @param[in, out] lsb_msb : Pointer to store OTP word
 * @param[in, out] dev     : Structure instance of bmm350_dev.
 *
 * @return Result of API execution status
 * @retval = 0 -> Success
 * @retval < 0 -> Error
 */
static int8_t read_otp_word(uint8_t addr, uint16_t *lsb_msb, struct bmm350_dev *dev);

/*!
 * @brief This internal API is used to read raw magnetic x,y and z axis data along with temperature.
 *
 * @param[out]  out_data     : Pointer variable to store mag and temperature data.
 * @param[in, out] dev       : Structure instance of bmm350_dev.
 *
 * @return Result of API execution status
 * @retval = 0 -> Success
 * @retval < 0 -> Error
 */
static int8_t read_out_raw_data(float *out_data, struct bmm350_dev *dev);

/*!
 * @brief This internal API is used to convert raw mag lsb data to uT and raw temperature data to degC.
 *
 *  @param[in,out] lsb_to_ut_degc : Float variable to store converted value of mag lsb in micro tesla(uT) and
 *  temperature data in degC.
 *
 *  @return void
 */
static void update_default_coefiecents(float *lsb_to_ut_degc);

/*!
 * @brief This internal API is used to read OTP data after boot in user mode.
 *
 * @param[in, out] dev  : Structure instance of bmm350_dev.
 *
 * @return Result of API execution status
 * @retval = 0 -> Success
 * @retval < 0 -> Error
 */
static int8_t otp_dump_after_boot(struct bmm350_dev *dev);

/*!
 * @brief This internal API is used for self-test entry configuration
 *
 * @param[in, out] dev  : Structure instance of bmm350_dev.
 *
 * @return Result of API execution status
 * @retval = 0 -> Success
 * @retval < 0 -> Error
 */
static int8_t self_test_entry_config(struct bmm350_dev *dev);

/*!
 * @brief This internal API is used to test self-test for X and Y axis
 *
 * @param[in, out] out_data  : Structure instance of bmm350_self_test.
 * @param[in, out] dev       : Structure instance of bmm350_dev.
 *
 * @return Result of API execution status
 * @retval = 0 -> Success
 * @retval < 0 -> Error
 */
static int8_t self_test_xy_axis(struct bmm350_self_test *out_data, struct bmm350_dev *dev);

/*!
 * @brief This internal API is used to set self-test configurations.
 *
 * @param[in] st_cmd         : Variable to store self-test command.
 * @param[in] pmu_cmd        : Variable to store PMU command.
 * @param[in, out] out_data  : Structure instance of bmm350_self_test.
 * @param[in, out] dev       : Structure instance of bmm350_dev.
 *
 * @return Result of API execution status
 * @retval = 0 -> Success
 * @retval < 0 -> Error
 */
static int8_t self_test_config(uint8_t st_cmd,
                               uint8_t pmu_cmd,
                               struct bmm350_self_test *out_data,
                               struct bmm350_dev *dev);

/*!
 * @brief This internal API is used to set powermode.
 *
 * @param[in] powermode          : Variable to set new powermode.
 * @param[in, out] dev           : Structure instance of bmm350_dev.
 *
 * @return Result of API execution status
 * @retval = 0 -> Success
 * @retval < 0 -> Error
 */
static int8_t set_powermode(enum bmm350_power_modes powermode, struct bmm350_dev *dev);

/********************** Global function definitions ************************/

/*!
 * @brief This API is the entry point. Call this API before using other APIs.
 */
int8_t bmm350_init(struct bmm350_dev *dev)
{
    /* Variable to store the function result */
    int8_t rslt;

    /* Variable to get chip id */
    uint8_t chip_id = BMM350_DISABLE;

    /* Variable to store the command to power-off the OTP */
    uint8_t otp_cmd = BMM350_OTP_CMD_PWR_OFF_OTP;

    /* Variable to store soft-reset command */
    uint8_t soft_reset;

    /* Check for null pointer in the device structure */
    rslt = null_ptr_check(dev);

    /* Proceed if null check is fine */
    if (rslt == BMM350_OK) {
        dev->chip_id = 0;

        /* Assign axis_en with all axis enabled (BMM350_EN_XYZ_MSK) */
        dev->axis_en = BMM350_EN_XYZ_MSK;

        rslt = bmm350_delay_us(BMM350_START_UP_TIME_FROM_POR, dev);

        if (rslt == BMM350_OK) {
            /* Soft-reset */
            soft_reset = BMM350_CMD_SOFTRESET;

            /* Set the command in the command register */
            rslt = bmm350_set_regs(BMM350_REG_CMD, &soft_reset, 1, dev);

            if (rslt == BMM350_OK) {
                rslt = bmm350_delay_us(BMM350_SOFT_RESET_DELAY, dev);
            }
        }

        if (rslt == BMM350_OK) {
            /* Chip ID of the sensor is read */
            rslt = bmm350_get_regs(BMM350_REG_CHIP_ID, &chip_id, 1, dev);

            if (rslt == BMM350_OK) {
                /* Assign chip_id to dev->chip_id */
                dev->chip_id = chip_id;
            }
        }

        /* Check for chip id validity */
        if ((rslt == BMM350_OK) && (dev->chip_id == BMM350_CHIP_ID)) {
            /* Download OTP memory */
            rslt = otp_dump_after_boot(dev);

            if (rslt == BMM350_OK) {
                /* Power off OTP */
                rslt = bmm350_set_regs(BMM350_REG_OTP_CMD_REG, &otp_cmd, 1, dev);

                if (rslt == BMM350_OK) {
                    rslt = bmm350_magnetic_reset_and_wait(dev);
                }
            }
        } else {
            rslt = BMM350_E_DEV_NOT_FOUND;
        }
    }

    return rslt;
}

/*!
 * @brief This API writes the given data to the register address
 * of the sensor.
 */
int8_t bmm350_set_regs(uint8_t reg_addr, const uint8_t *reg_data, uint16_t len, struct bmm350_dev *dev)
{
    /* Variable to store the function result */
    int8_t rslt;

    /* Check for null pointer in the device structure */
    rslt = null_ptr_check(dev);

    /* Proceed if null check is fine */
    if ((rslt == BMM350_OK) && (reg_data != NULL) && (len != 0)) {
        /* Write the data to the reg_addr */
        dev->intf_rslt = dev->write(reg_addr, reg_data, len, dev->intf_ptr);

        if (dev->intf_rslt != BMM350_INTF_RET_SUCCESS) {
            rslt = BMM350_E_COM_FAIL;
        }
    } else {
        rslt = BMM350_E_NULL_PTR;
    }

    return rslt;
}

/*!
 * @brief This API reads the data from the given register address of sensor.
 */
int8_t bmm350_get_regs(uint8_t reg_addr, uint8_t *reg_data, uint16_t len, struct bmm350_dev *dev)
{
    /* Variable to store the function result */
    int8_t rslt;

    /* Variable to define temporary length */
    uint16_t temp_len = len + BMM350_DUMMY_BYTES;

    /* Variable to define temporary buffer */
    uint8_t temp_buf[BMM350_READ_BUFFER_LENGTH];

    /* Variable to define loop */
    uint16_t index = 0;

    /* Check for null pointer in the device structure */
    rslt = null_ptr_check(dev);

    /* Proceed if null check is fine */
    if ((rslt == BMM350_OK) && (reg_data != NULL)) {
        /* Read the data from the reg_addr */
        dev->intf_rslt = dev->read(reg_addr, temp_buf, temp_len, dev->intf_ptr);

        if (dev->intf_rslt == BMM350_INTF_RET_SUCCESS) {
            /* Copy data after dummy byte indices */
            while (index < len) {
                reg_data[index] = temp_buf[index + BMM350_DUMMY_BYTES];
                index++;
            }
        } else {
            rslt = BMM350_E_COM_FAIL;
        }
    } else {
        rslt = BMM350_E_NULL_PTR;
    }

    return rslt;
}

/*!
 * @brief This function provides the delay for required time (Microsecond) as per the input provided in some of the
 * APIs.
 */
int8_t bmm350_delay_us(uint32_t period_us, const struct bmm350_dev *dev)
{
    /* Variable to store the function result */
    int8_t rslt;

    /* Check for null pointer in the device structure */
    rslt = null_ptr_check(dev);

    if (rslt == BMM350_OK) {
        dev->delay_us(period_us, dev->intf_ptr);
    }

    return rslt;
}

/*!
 * @brief This API is used to perform soft-reset of the sensor
 * where all the registers are reset to their default values
 */
int8_t bmm350_soft_reset(struct bmm350_dev *dev)
{
    /* Variable to store the function result */
    int8_t rslt;

    uint8_t reg_data;

    /* Variable to store the command to power-off the OTP */
    uint8_t otp_cmd = BMM350_OTP_CMD_PWR_OFF_OTP;

    /* Check for null pointer in the device structure */
    rslt = null_ptr_check(dev);

    if (rslt == BMM350_OK) {
        reg_data = BMM350_CMD_SOFTRESET;

        /* Set the command in the command register */
        rslt = bmm350_set_regs(BMM350_REG_CMD, &reg_data, 1, dev);

        if (rslt == BMM350_OK) {
            rslt = bmm350_delay_us(BMM350_SOFT_RESET_DELAY, dev);

            if (rslt == BMM350_OK) {
                /* Power off OTP */
                rslt = bmm350_set_regs(BMM350_REG_OTP_CMD_REG, &otp_cmd, 1, dev);

                if (rslt == BMM350_OK) {
                    rslt = bmm350_magnetic_reset_and_wait(dev);
                }
            }
        }
    }

    return rslt;
}

/*!
 * @brief This API is used to read the sensor time.
 * It converts the sensor time register values to the representative time value.
 * Returns the sensor time in ticks.
 */
int8_t bmm350_read_sensortime(uint32_t *seconds, uint32_t *nanoseconds, struct bmm350_dev *dev)
{
    /* Variable to store the function result */
    int8_t rslt;
    uint64_t time;

    uint8_t reg_data[3];

    if ((seconds != NULL) && (nanoseconds != NULL)) {
        /* Get sensor time raw data */
        rslt = bmm350_get_regs(BMM350_REG_SENSORTIME_XLSB, reg_data, 3, dev);

        if (rslt == BMM350_OK) {
            time = (uint32_t)(reg_data[0] + ((uint32_t)reg_data[1] << 8) + ((uint32_t)reg_data[2] << 16));

            /* 1 LSB is 39.0625us. Converting to nanoseconds */
            time *= UINT64_C(390625);
            time /= UINT64_C(10);
            *seconds = (uint32_t)(time / UINT64_C(1000000000));
            *nanoseconds = (uint32_t)(time - ((*seconds) * UINT64_C(1000000000)));
        }
    } else {
        rslt = BMM350_E_NULL_PTR;
    }

    return rslt;
}

/*!
 * @brief This API is used to get the status flags of all interrupt
 * which is used to check for the assertion of interrupts
 */
int8_t bmm350_get_interrupt_status(uint8_t *drdy_status, struct bmm350_dev *dev)
{
    /* Variable to store the function result */
    int8_t rslt;

    uint8_t int_status_reg;

    if (drdy_status != NULL) {
        /* Get the status of interrupt */
        rslt = bmm350_get_regs(BMM350_REG_INT_STATUS, &int_status_reg, 1, dev);

        if (rslt == BMM350_OK) {
            /* Read the interrupt status */
            (*drdy_status) = BMM350_GET_BITS(int_status_reg, BMM350_DRDY_DATA_REG);
        }
    } else {
        rslt = BMM350_E_NULL_PTR;
    }

    return rslt;
}

/*!
 * @brief This API is used to set the power mode of the sensor
 */
int8_t bmm350_set_powermode(enum bmm350_power_modes powermode, struct bmm350_dev *dev)
{
    /* Variable to store the function result */
    int8_t rslt;

    uint8_t last_pwr_mode;
    uint8_t reg_data;

    /* Check for null pointer in the device structure */
    rslt = null_ptr_check(dev);

    if (rslt == BMM350_OK) {
        rslt = bmm350_get_regs(BMM350_REG_PMU_CMD, &last_pwr_mode, 1, dev);

        if (rslt == BMM350_OK) {
            if (last_pwr_mode > BMM350_PMU_CMD_NM_TC) {
                rslt = BMM350_E_INVALID_CONFIG;
            }

            if ((rslt == BMM350_OK) &&
                    ((last_pwr_mode == BMM350_PMU_CMD_NM) || (last_pwr_mode == BMM350_PMU_CMD_UPD_OAE))) {
                reg_data = BMM350_PMU_CMD_SUS;

                /* Set PMU command configuration */
                rslt = bmm350_set_regs(BMM350_REG_PMU_CMD, &reg_data, 1, dev);

                if (rslt == BMM350_OK) {
                    rslt = bmm350_delay_us(BMM350_GOTO_SUSPEND_DELAY, dev);
                }
            }

            if (rslt == BMM350_OK) {
                rslt = set_powermode(powermode, dev);
            }
        }
    }

    return rslt;
}

/*!
 * @brief This API sets the ODR and averaging factor.
 */
int8_t bmm350_set_odr_performance(enum bmm350_data_rates odr,
                                  enum bmm350_performance_parameters performance,
                                  struct bmm350_dev *dev)
{
    /* Variable to store the function result */
    int8_t rslt;

    /* Variable to get PMU command */
    uint8_t reg_data = 0;

    enum bmm350_performance_parameters performance_fix = performance;

    /* Check for null pointer in the device structure */
    rslt = null_ptr_check(dev);

    if (rslt == BMM350_OK) {
        /* Reduce the performance setting when too high for the chosen ODR */
        if ((odr == BMM350_DATA_RATE_400HZ) && (performance >= BMM350_AVERAGING_2)) {
            performance_fix = BMM350_NO_AVERAGING;
        } else if ((odr == BMM350_DATA_RATE_200HZ) && (performance >= BMM350_AVERAGING_4)) {
            performance_fix = BMM350_AVERAGING_2;
        } else if ((odr == BMM350_DATA_RATE_100HZ) && (performance >= BMM350_AVERAGING_8)) {
            performance_fix = BMM350_AVERAGING_4;
        }

        /* ODR is an enum taking the generated constants from the register map */
        reg_data = ((uint8_t)odr & BMM350_ODR_MSK);

        /* AVG / performance is an enum taking the generated constants from the register map */
        reg_data = BMM350_SET_BITS(reg_data, BMM350_AVG, (uint8_t)performance_fix);

        /* Set PMU command configurations for ODR and performance */
        rslt = bmm350_set_regs(BMM350_REG_PMU_CMD_AGGR_SET, &reg_data, 1, dev);

        if (rslt == BMM350_OK) {
            /* Set PMU command configurations to update odr and average */
            reg_data = BMM350_PMU_CMD_UPD_OAE;

            /* Set PMU command configuration */
            rslt = bmm350_set_regs(BMM350_REG_PMU_CMD, &reg_data, 1, dev);

            if (rslt == BMM350_OK) {
                rslt = bmm350_delay_us(BMM350_UPD_OAE_DELAY, dev);
            }
        }
    }

    return rslt;
}

/*!
 * @brief This API is used to enable or disable the magnetic
 * measurement of x,y,z axes
 */
int8_t bmm350_enable_axes(enum bmm350_x_axis_en_dis en_x,
                          enum bmm350_y_axis_en_dis en_y,
                          enum bmm350_z_axis_en_dis en_z,
                          struct bmm350_dev *dev)
{
    /* Variable to store the function result */
    int8_t rslt;

    /* Variable to store axis data */
    uint8_t data;

    /* Check for null pointer in the device structure */
    rslt = null_ptr_check(dev);

    if (rslt == BMM350_OK) {
        if ((en_x == BMM350_X_DIS) && (en_y == BMM350_Y_DIS) && (en_z == BMM350_Z_DIS)) {
            rslt = BMM350_E_ALL_AXIS_DISABLED;

            /* Assign axis_en with all axis disabled status */
            dev->axis_en = BMM350_DISABLE;
        } else {
            data = (en_x & BMM350_EN_X_MSK);
            data = BMM350_SET_BITS(data, BMM350_EN_Y, en_y);
            data = BMM350_SET_BITS(data, BMM350_EN_Z, en_z);

            rslt = bmm350_set_regs(BMM350_REG_PMU_CMD_AXIS_EN, &data, 1, dev);

            if (rslt == BMM350_OK) {
                /* Assign axis_en with the axis selection done */
                dev->axis_en = data;
            }
        }
    }

    return rslt;
}

/*!
 * @brief This API is used to enable or disable the data ready interrupt
 */
int8_t bmm350_enable_interrupt(enum bmm350_interrupt_enable_disable enable_disable, struct bmm350_dev *dev)
{
    /* Variable to get interrupt control configuration */
    uint8_t reg_data = 0;

    /* Variable to store the function result */
    int8_t rslt;

    /* Get interrupt control configuration */
    rslt = bmm350_get_regs(BMM350_REG_INT_CTRL, &reg_data, 1, dev);

    if (rslt == BMM350_OK) {
        reg_data = BMM350_SET_BITS(reg_data, BMM350_DRDY_DATA_REG_EN, (uint8_t)enable_disable);

        /* Finally transfer the interrupt configurations */
        rslt = bmm350_set_regs(BMM350_REG_INT_CTRL, &reg_data, 1, dev);
    }

    return rslt;
}

/*!
 * @brief This API is used to configure the interrupt control settings
 */
int8_t bmm350_configure_interrupt(enum bmm350_intr_latch latching,
                                  enum bmm350_intr_polarity polarity,
                                  enum bmm350_intr_drive drivertype,
                                  enum bmm350_intr_map map_nomap,
                                  struct bmm350_dev *dev)
{
    /* Variable to get interrupt control configuration */
    uint8_t reg_data = 0;

    /* Variable to store the function result */
    int8_t rslt;

    /* Get interrupt control configuration */
    rslt = bmm350_get_regs(BMM350_REG_INT_CTRL, &reg_data, 1, dev);

    if (rslt == BMM350_OK) {
        reg_data = BMM350_SET_BITS_POS_0(reg_data, BMM350_INT_MODE, latching);
        reg_data = BMM350_SET_BITS(reg_data, BMM350_INT_POL, polarity);
        reg_data = BMM350_SET_BITS(reg_data, BMM350_INT_OD, drivertype);
        reg_data = BMM350_SET_BITS(reg_data, BMM350_INT_OUTPUT_EN, map_nomap);

        /* Finally transfer the interrupt configurations */
        rslt = bmm350_set_regs(BMM350_REG_INT_CTRL, &reg_data, 1, dev);
    }

    return rslt;
}

/*!
 * @brief This API is used to read uncompensated mag and temperature data.
 */
int8_t bmm350_read_uncomp_mag_temp_data(struct bmm350_raw_mag_data *raw_data, struct bmm350_dev *dev)
{
    /* Variable to store the function result */
    int8_t rslt;

    uint8_t mag_data[12] = { 0 };

    uint32_t raw_mag_x, raw_mag_y, raw_mag_z, raw_temp;

    if (raw_data != NULL) {
        /* Get uncompensated mag data */
        rslt = bmm350_get_regs(BMM350_REG_MAG_X_XLSB, mag_data, BMM350_MAG_TEMP_DATA_LEN, dev);

        if (rslt == BMM350_OK) {
            raw_mag_x = mag_data[0] + ((uint32_t)mag_data[1] << 8) + ((uint32_t)mag_data[2] << 16);
            raw_mag_y = mag_data[3] + ((uint32_t)mag_data[4] << 8) + ((uint32_t)mag_data[5] << 16);
            raw_mag_z = mag_data[6] + ((uint32_t)mag_data[7] << 8) + ((uint32_t)mag_data[8] << 16);
            raw_temp = mag_data[9] + ((uint32_t)mag_data[10] << 8) + ((uint32_t)mag_data[11] << 16);

            if ((dev->axis_en & BMM350_EN_X_MSK) == BMM350_DISABLE) {
                raw_data->raw_xdata = BMM350_DISABLE;
            } else {
                raw_data->raw_xdata = fix_sign(raw_mag_x, BMM350_SIGNED_24_BIT);
            }

            if ((dev->axis_en & BMM350_EN_Y_MSK) == BMM350_DISABLE) {
                raw_data->raw_ydata = BMM350_DISABLE;
            } else {
                raw_data->raw_ydata = fix_sign(raw_mag_y, BMM350_SIGNED_24_BIT);
            }

            if ((dev->axis_en & BMM350_EN_Z_MSK) == BMM350_DISABLE) {
                raw_data->raw_zdata = BMM350_DISABLE;
            } else {
                raw_data->raw_zdata = fix_sign(raw_mag_z, BMM350_SIGNED_24_BIT);
            }

            raw_data->raw_data_t = fix_sign(raw_temp, BMM350_SIGNED_24_BIT);
        }
    } else {
        rslt = BMM350_E_NULL_PTR;
    }

    return rslt;
}

/*!
 * @brief This API sets the interrupt control IBI configurations to the sensor.
 */
int8_t bmm350_set_int_ctrl_ibi(enum bmm350_drdy_int_map_to_ibi en_dis,
                               enum bmm350_clear_drdy_int_status_upon_ibi clear_on_ibi,
                               struct bmm350_dev *dev)
{
    /* Variable to store the function result */
    int8_t rslt;

    /* Variable to get interrupt control configuration */
    uint8_t reg_data = 0;

    /* Get interrupt control configuration */
    rslt = bmm350_get_regs(BMM350_REG_INT_CTRL_IBI, &reg_data, 1, dev);

    if (rslt == BMM350_OK) {
        reg_data = BMM350_SET_BITS_POS_0(reg_data, BMM350_DRDY_INT_MAP_TO_IBI, en_dis);
        reg_data = BMM350_SET_BITS(reg_data, BMM350_CLEAR_DRDY_INT_STATUS_UPON_IBI, clear_on_ibi);

        /* Set the IBI control configuration */
        rslt = bmm350_set_regs(BMM350_REG_INT_CTRL_IBI, &reg_data, 1, dev);

        if (en_dis == BMM350_IBI_ENABLE) {
            /* Enable data ready interrupt if IBI is enabled */
            rslt = bmm350_enable_interrupt(BMM350_ENABLE_INTERRUPT, dev);
        }
    }

    return rslt;
}

/*!
 * @brief This API is used to set the pad drive strength
 */
int8_t bmm350_set_pad_drive(uint8_t drive, struct bmm350_dev *dev)
{
    uint8_t reg_data;

    /* Variable to store the function result */
    int8_t rslt = BMM350_E_BAD_PAD_DRIVE;

    if (drive <= BMM350_PAD_DRIVE_STRONGEST) {
        reg_data = drive & BMM350_DRV_MSK;

        /* Set drive */
        rslt = bmm350_set_regs(BMM350_REG_PAD_CTRL, &reg_data, 1, dev);
    }

    return rslt;
}

/*!
 * @brief This API is used to perform the magnetic reset of the sensor
 * which is necessary after a field shock (400mT field applied to sensor).
 * It sends flux guide or bit reset to the device in suspend mode.
 */
int8_t bmm350_magnetic_reset_and_wait(struct bmm350_dev *dev)
{
    /* Variable to store the function result */
    int8_t rslt;

    uint8_t pmu_cmd = 0;
    struct bmm350_pmu_cmd_status_0 pmu_cmd_stat_0 = { 0 };
    uint8_t restore_normal = BMM350_DISABLE;

    rslt = null_ptr_check(dev);

    if ((rslt == BMM350_OK) && (dev->mraw_override) && (dev->var_id >= BMM350_MIN_VAR)) {
        rslt = dev->mraw_override(dev);
    } else {
        /* Read PMU CMD status */
        rslt = bmm350_get_pmu_cmd_status_0(&pmu_cmd_stat_0, dev);

        /* Check the powermode is normal before performing magnetic reset */
        if ((rslt == BMM350_OK) && (pmu_cmd_stat_0.pwr_mode_is_normal == BMM350_ENABLE)) {
            restore_normal = BMM350_ENABLE;

            /* Reset can only be triggered in suspend */
            rslt = bmm350_set_powermode(BMM350_SUSPEND_MODE, dev);
        }

        if (rslt == BMM350_OK) {
            /* Set BR to PMU_CMD register */
            pmu_cmd = BMM350_PMU_CMD_BR;

            rslt = bmm350_set_regs(BMM350_REG_PMU_CMD, &pmu_cmd, 1, dev);

            if (rslt == BMM350_OK) {
                rslt = bmm350_delay_us(BMM350_BR_DELAY, dev);
            }
        }

        if (rslt == BMM350_OK) {
            /* Verify if PMU_CMD_STATUS_0 register has BR set */
            rslt = bmm350_get_pmu_cmd_status_0(&pmu_cmd_stat_0, dev);

            if ((rslt == BMM350_OK) && (pmu_cmd_stat_0.pmu_cmd_value != BMM350_PMU_CMD_STATUS_0_BR)) {
                rslt = BMM350_E_PMU_CMD_VALUE;
            }
        }

        if (rslt == BMM350_OK) {
            /* Set FGR to PMU_CMD register */
            pmu_cmd = BMM350_PMU_CMD_FGR;

            rslt = bmm350_set_regs(BMM350_REG_PMU_CMD, &pmu_cmd, 1, dev);

            if (rslt == BMM350_OK) {
                rslt = bmm350_delay_us(BMM350_FGR_DELAY, dev);
            }
        }

        if (rslt == BMM350_OK) {
            /* Verify if PMU_CMD_STATUS_0 register has FGR set */
            rslt = bmm350_get_pmu_cmd_status_0(&pmu_cmd_stat_0, dev);

            if ((rslt == BMM350_OK) && (pmu_cmd_stat_0.pmu_cmd_value != BMM350_PMU_CMD_STATUS_0_FGR)) {
                rslt = BMM350_E_PMU_CMD_VALUE;
            }
        }

        if ((rslt == BMM350_OK) && (restore_normal == BMM350_ENABLE)) {
            rslt = bmm350_set_powermode(BMM350_NORMAL_MODE, dev);
        }
    }

    return rslt;
}

/*!
 * @brief This API is used to perform compensation for raw magnetometer and temperature data.
 */
int8_t bmm350_get_compensated_mag_xyz_temp_data(struct bmm350_mag_temp_data *mag_temp_data, struct bmm350_dev *dev)
{
    /* Variable to store the function result */
    int8_t rslt;

    uint8_t index;
    float out_data[4] = { 0.0f };
    float dut_offset_coef[3], dut_sensit_coef[3], dut_tco[3], dut_tcs[3];
    float cr_ax_comp_x, cr_ax_comp_y, cr_ax_comp_z;

    if (mag_temp_data != NULL) {
        /* Reads raw magnetic x,y and z axis along with temperature */
        rslt = read_out_raw_data(out_data, dev);

        if (rslt == BMM350_OK) {
            /* Apply compensation to temperature reading */
            out_data[3] = (1 + dev->mag_comp.dut_sensit_coef.t_sens) * out_data[3] +
                          dev->mag_comp.dut_offset_coef.t_offs;

            /* Store magnetic compensation structure to an array */
            dut_offset_coef[0] = dev->mag_comp.dut_offset_coef.offset_x;
            dut_offset_coef[1] = dev->mag_comp.dut_offset_coef.offset_y;
            dut_offset_coef[2] = dev->mag_comp.dut_offset_coef.offset_z;

            dut_sensit_coef[0] = dev->mag_comp.dut_sensit_coef.sens_x;
            dut_sensit_coef[1] = dev->mag_comp.dut_sensit_coef.sens_y;
            dut_sensit_coef[2] = dev->mag_comp.dut_sensit_coef.sens_z;

            dut_tco[0] = dev->mag_comp.dut_tco.tco_x;
            dut_tco[1] = dev->mag_comp.dut_tco.tco_y;
            dut_tco[2] = dev->mag_comp.dut_tco.tco_z;

            dut_tcs[0] = dev->mag_comp.dut_tcs.tcs_x;
            dut_tcs[1] = dev->mag_comp.dut_tcs.tcs_y;
            dut_tcs[2] = dev->mag_comp.dut_tcs.tcs_z;

            /* Compensate raw magnetic data */
            for (index = 0; index < 3; index++) {
                out_data[index] *= 1 + dut_sensit_coef[index];
                out_data[index] += dut_offset_coef[index];
                out_data[index] += dut_tco[index] * (out_data[3] - dev->mag_comp.dut_t0);
                out_data[index] /= 1 + dut_tcs[index] * (out_data[3] - dev->mag_comp.dut_t0);
            }

            cr_ax_comp_x = (out_data[0] - dev->mag_comp.cross_axis.cross_x_y * out_data[1]) /
                           (1 - dev->mag_comp.cross_axis.cross_y_x * dev->mag_comp.cross_axis.cross_x_y);
            cr_ax_comp_y = (out_data[1] - dev->mag_comp.cross_axis.cross_y_x * out_data[0]) /
                           (1 - dev->mag_comp.cross_axis.cross_y_x * dev->mag_comp.cross_axis.cross_x_y);
            cr_ax_comp_z =
                (out_data[2] +
                 (out_data[0] *
                  (dev->mag_comp.cross_axis.cross_y_x * dev->mag_comp.cross_axis.cross_z_y -
                   dev->mag_comp.cross_axis.cross_z_x) - out_data[1] *
                  (dev->mag_comp.cross_axis.cross_z_y - dev->mag_comp.cross_axis.cross_x_y *
                   dev->mag_comp.cross_axis.cross_z_x)) /
                 (1 - dev->mag_comp.cross_axis.cross_y_x * dev->mag_comp.cross_axis.cross_x_y));

            out_data[0] = cr_ax_comp_x;
            out_data[1] = cr_ax_comp_y;
            out_data[2] = cr_ax_comp_z;
        }

        if (rslt == BMM350_OK) {
            if ((dev->axis_en & BMM350_EN_X_MSK) == BMM350_DISABLE) {
                mag_temp_data->x = BMM350_DISABLE;
            } else {
                mag_temp_data->x = out_data[0];
            }

            if ((dev->axis_en & BMM350_EN_Y_MSK) == BMM350_DISABLE) {
                mag_temp_data->y = BMM350_DISABLE;
            } else {
                mag_temp_data->y = out_data[1];
            }

            if ((dev->axis_en & BMM350_EN_Z_MSK) == BMM350_DISABLE) {
                mag_temp_data->z = BMM350_DISABLE;
            } else {
                mag_temp_data->z = out_data[2];
            }

            mag_temp_data->temperature = out_data[3];
        }
    } else {
        rslt = BMM350_E_NULL_PTR;
    }

    return rslt;
}

/*!
 * @brief This function executes FGR and BR sequences to initialize TMR sensor and performs the user self-test.
 */
int8_t bmm350_perform_self_test(struct bmm350_self_test *out_data, struct bmm350_dev *dev)
{
    /* Variable to store the function result */
    int8_t rslt;

    /* Variable to store last powermode */
    uint8_t last_pwr_mode;

    if (out_data != NULL) {
        rslt = bmm350_get_regs(BMM350_REG_PMU_CMD, &last_pwr_mode, 1, dev);

        if (rslt == BMM350_OK) {
            /* Self-test entry configuration */
            rslt = self_test_entry_config(dev);

            if (rslt == BMM350_OK) {
                /* Updates self-test values to structure */
                rslt = self_test_xy_axis(out_data, dev);
            }
        }

        if (rslt == BMM350_OK) {
            /* Setup DUT: disable user self-test */
            rslt = bmm350_set_tmr_selftest_user(BMM350_ST_IGEN_DIS,
                                                BMM350_ST_N_DIS,
                                                BMM350_ST_P_DIS,
                                                BMM350_IST_X_DIS,
                                                BMM350_IST_Y_DIS,
                                                dev);

            if (rslt == BMM350_OK) {
                rslt = bmm350_delay_us(1000, dev);
            }

            if (last_pwr_mode == BMM350_PMU_CMD_NM) {
                rslt = bmm350_set_powermode(BMM350_NORMAL_MODE, dev);
            }
        }
    } else {
        rslt = BMM350_E_NULL_PTR;
    }

    return rslt;
}

/*!
 * @brief This API sets the I2C watchdog timer configurations to the sensor.
 */
int8_t bmm350_set_i2c_wdt(enum bmm350_i2c_wdt_en i2c_wdt_en_dis,
                          enum bmm350_i2c_wdt_sel i2c_wdt_sel,
                          struct bmm350_dev *dev)
{
    /* Variable to store the function result */
    int8_t rslt;

    uint8_t reg_data;

    /* Get I2C WDT configuration */
    rslt = bmm350_get_regs(BMM350_REG_I2C_WDT_SET, &reg_data, 1, dev);

    if (rslt == BMM350_OK) {
        reg_data = BMM350_SET_BITS_POS_0(reg_data, BMM350_I2C_WDT_EN, i2c_wdt_en_dis);
        reg_data = BMM350_SET_BITS(reg_data, BMM350_I2C_WDT_SEL, i2c_wdt_sel);

        /* Set I2C WDT configuration */
        rslt = bmm350_set_regs(BMM350_REG_I2C_WDT_SET, &reg_data, 1, dev);
    }

    return rslt;
}

/*!
 * @brief This API sets the TMR user self-test register
 */
int8_t bmm350_set_tmr_selftest_user(enum bmm350_st_igen_en st_igen_en_dis,
                                    enum bmm350_st_n st_n_en_dis,
                                    enum bmm350_st_p st_p_en_dis,
                                    enum bmm350_ist_en_x ist_x_en_dis,
                                    enum bmm350_ist_en_y ist_y_en_dis,
                                    struct bmm350_dev *dev)
{
    /* Variable to store the function result */
    int8_t rslt;

    uint8_t reg_data;

    /* Get TMR self-test user configuration */
    rslt = bmm350_get_regs(BMM350_REG_TMR_SELFTEST_USER, &reg_data, 1, dev);

    if (rslt == BMM350_OK) {
        reg_data = BMM350_SET_BITS_POS_0(reg_data, BMM350_ST_IGEN_EN, st_igen_en_dis);
        reg_data = BMM350_SET_BITS(reg_data, BMM350_ST_N, st_n_en_dis);
        reg_data = BMM350_SET_BITS(reg_data, BMM350_ST_P, st_p_en_dis);
        reg_data = BMM350_SET_BITS(reg_data, BMM350_IST_EN_X, ist_x_en_dis);
        reg_data = BMM350_SET_BITS(reg_data, BMM350_IST_EN_Y, ist_y_en_dis);

        /* Set TMR self-test user configuration */
        rslt = bmm350_set_regs(BMM350_REG_TMR_SELFTEST_USER, &reg_data, 1, dev);
    }

    return rslt;
}

/*!
 * @brief This API sets the control user configurations to the sensor which forces the sensor timer to be always
 * running, even in suspend mode.
 */
int8_t bmm350_set_ctrl_user(enum bmm350_ctrl_user cfg_sens_tim_aon_en_dis, struct bmm350_dev *dev)
{
    /* Variable to store the function result */
    int8_t rslt;

    uint8_t reg_data;

    /* Get control user configuration */
    rslt = bmm350_get_regs(BMM350_REG_CTRL_USER, &reg_data, 1, dev);

    if (rslt == BMM350_OK) {
        reg_data = BMM350_SET_BITS_POS_0(reg_data, BMM350_CFG_SENS_TIM_AON, cfg_sens_tim_aon_en_dis);

        /* Set control user configuration */
        rslt = bmm350_set_regs(BMM350_REG_CTRL_USER, &reg_data, 1, dev);
    }

    return rslt;
}

/*!
 * @brief This API gets the PMU command status 0 value
 */
int8_t bmm350_get_pmu_cmd_status_0(struct bmm350_pmu_cmd_status_0 *pmu_cmd_stat_0, struct bmm350_dev *dev)
{
    /* Variable to store the function result */
    int8_t rslt;

    uint8_t reg_data;

    if (pmu_cmd_stat_0 != NULL) {
        /* Get PMU command status 0 data */
        rslt = bmm350_get_regs(BMM350_REG_PMU_CMD_STATUS_0, &reg_data, 1, dev);

        if (rslt == BMM350_OK) {
            pmu_cmd_stat_0->pmu_cmd_busy = BMM350_GET_BITS_POS_0(reg_data, BMM350_PMU_CMD_BUSY);

            pmu_cmd_stat_0->odr_ovwr = BMM350_GET_BITS(reg_data, BMM350_ODR_OVWR);

            pmu_cmd_stat_0->avr_ovwr = BMM350_GET_BITS(reg_data, BMM350_AVG_OVWR);

            pmu_cmd_stat_0->pwr_mode_is_normal = BMM350_GET_BITS(reg_data, BMM350_PWR_MODE_IS_NORMAL);

            pmu_cmd_stat_0->cmd_is_illegal = BMM350_GET_BITS(reg_data, BMM350_CMD_IS_ILLEGAL);

            pmu_cmd_stat_0->pmu_cmd_value = BMM350_GET_BITS(reg_data, BMM350_PMU_CMD_VALUE);
        }
    } else {
        rslt = BMM350_E_NULL_PTR;
    }

    return rslt;
}

/****************************************************************************/
/**\name     INTERNAL APIs                                                  */

/*!
 * @brief This internal API is used to validate the device structure pointer for
 * null conditions.
 */
static int8_t null_ptr_check(const struct bmm350_dev *dev)
{
    /* Variable to store the function result */
    int8_t rslt;

    if ((dev == NULL) || (dev->read == NULL) || (dev->write == NULL) || (dev->delay_us == NULL)) {
        /* Device structure pointer is not valid */
        rslt = BMM350_E_NULL_PTR;
    } else {
        /* Device structure is fine */
        rslt = BMM350_OK;
    }

    return rslt;
}

/*!
 *  @brief This internal API converts the raw data from the IC data registers to signed integer
 */
static int32_t fix_sign(uint32_t inval, int8_t number_of_bits)
{
    int32_t power = 0;
    int32_t retval;

    switch (number_of_bits) {
    case BMM350_SIGNED_8_BIT:
        power = 128; /* 2^7 */
        break;

    case BMM350_SIGNED_12_BIT:
        power = 2048; /* 2^11 */
        break;

    case BMM350_SIGNED_16_BIT:
        power = 32768; /* 2^15 */
        break;

    case BMM350_SIGNED_21_BIT:
        power = 1048576; /* 2^20 */
        break;

    case BMM350_SIGNED_24_BIT:
        power = 8388608; /* 2^23 */
        break;

    default:
        power = 0;
        break;
    }

    retval = (int32_t)inval;

    if (retval >= power) {
        retval = retval - (power * 2);
    }

    return retval;
}

/*!
 * @brief This internal API is used to read OTP word
 */
static int8_t read_otp_word(uint8_t addr, uint16_t *lsb_msb, struct bmm350_dev *dev)
{
    /* Variable to store the function result */
    int8_t rslt;

    uint8_t otp_cmd, otp_status = 0, otp_err = BMM350_OTP_STATUS_NO_ERROR, lsb = 0, msb = 0;

    if (lsb_msb != NULL) {
        /* Set OTP command at specified address */
        otp_cmd = BMM350_OTP_CMD_DIR_READ | (addr & BMM350_OTP_WORD_ADDR_MSK);
        rslt = bmm350_set_regs(BMM350_REG_OTP_CMD_REG, &otp_cmd, 1, dev);
        if (rslt == BMM350_OK) {
            do {
                rslt = bmm350_delay_us(300, dev);

                if (rslt == BMM350_OK) {
                    /* Get OTP status */
                    rslt = bmm350_get_regs(BMM350_REG_OTP_STATUS_REG, &otp_status, 1, dev);

                    otp_err = BMM350_OTP_STATUS_ERROR(otp_status);
                    if (otp_err != BMM350_OTP_STATUS_NO_ERROR) {
                        break;
                    }
                }
            } while ((!(otp_status & BMM350_OTP_STATUS_CMD_DONE)) && (rslt == BMM350_OK));

            if (otp_err != BMM350_OTP_STATUS_NO_ERROR) {
                switch (otp_err) {
                case BMM350_OTP_STATUS_BOOT_ERR:
                    rslt = BMM350_E_OTP_BOOT;
                    break;
                case BMM350_OTP_STATUS_PAGE_RD_ERR:
                    rslt = BMM350_E_OTP_PAGE_RD;
                    break;
                case BMM350_OTP_STATUS_PAGE_PRG_ERR:
                    rslt = BMM350_E_OTP_PAGE_PRG;
                    break;
                case BMM350_OTP_STATUS_SIGN_ERR:
                    rslt = BMM350_E_OTP_SIGN;
                    break;
                case BMM350_OTP_STATUS_INV_CMD_ERR:
                    rslt = BMM350_E_OTP_INV_CMD;
                    break;
                default:
                    rslt = BMM350_E_OTP_UNDEFINED;
                    break;
                }
            }
        }

        if (rslt == BMM350_OK) {
            /* Get OTP MSB data */
            rslt = bmm350_get_regs(BMM350_REG_OTP_DATA_MSB_REG, &msb, 1, dev);
            if (rslt == BMM350_OK) {
                /* Get OTP LSB data */
                rslt = bmm350_get_regs(BMM350_REG_OTP_DATA_LSB_REG, &lsb, 1, dev);
                *lsb_msb = ((uint16_t)(msb << 8) | lsb) & 0xFFFF;
            }
        }
    } else {
        rslt = BMM350_E_NULL_PTR;
    }

    return rslt;
}

/*!
 * @brief This internal API is used to update magnetometer offset and sensitivity data.
 */
static void update_mag_off_sens(struct bmm350_dev *dev)
{
    uint16_t off_x_lsb_msb, off_y_lsb_msb, off_z_lsb_msb, t_off;
    uint8_t sens_x, sens_y, sens_z, t_sens;
    uint8_t tco_x, tco_y, tco_z;
    uint8_t tcs_x, tcs_y, tcs_z;
    uint8_t cross_x_y, cross_y_x, cross_z_x, cross_z_y;

    off_x_lsb_msb = dev->otp_data[BMM350_MAG_OFFSET_X] & 0x0FFF;
    off_y_lsb_msb = ((dev->otp_data[BMM350_MAG_OFFSET_X] & 0xF000) >> 4) +
                    (dev->otp_data[BMM350_MAG_OFFSET_Y] & BMM350_LSB_MASK);
    off_z_lsb_msb = (dev->otp_data[BMM350_MAG_OFFSET_Y] & 0x0F00) +
                    (dev->otp_data[BMM350_MAG_OFFSET_Z] & BMM350_LSB_MASK);
    t_off = dev->otp_data[BMM350_TEMP_OFF_SENS] & BMM350_LSB_MASK;

    dev->mag_comp.dut_offset_coef.offset_x = fix_sign(off_x_lsb_msb, BMM350_SIGNED_12_BIT);
    dev->mag_comp.dut_offset_coef.offset_y = fix_sign(off_y_lsb_msb, BMM350_SIGNED_12_BIT);
    dev->mag_comp.dut_offset_coef.offset_z = fix_sign(off_z_lsb_msb, BMM350_SIGNED_12_BIT);
    dev->mag_comp.dut_offset_coef.t_offs = fix_sign(t_off, BMM350_SIGNED_8_BIT) / 5.0f;

    sens_x = (dev->otp_data[BMM350_MAG_SENS_X] & BMM350_MSB_MASK) >> 8;
    sens_y = (dev->otp_data[BMM350_MAG_SENS_Y] & BMM350_LSB_MASK);
    sens_z = (dev->otp_data[BMM350_MAG_SENS_Z] & BMM350_MSB_MASK) >> 8;
    t_sens = (dev->otp_data[BMM350_TEMP_OFF_SENS] & BMM350_MSB_MASK) >> 8;

    dev->mag_comp.dut_sensit_coef.sens_x = fix_sign(sens_x, BMM350_SIGNED_8_BIT) / 256.0f;
    dev->mag_comp.dut_sensit_coef.sens_y = (fix_sign(sens_y, BMM350_SIGNED_8_BIT) / 256.0f) + BMM350_SENS_CORR_Y;
    dev->mag_comp.dut_sensit_coef.sens_z = fix_sign(sens_z, BMM350_SIGNED_8_BIT) / 256.0f;
    dev->mag_comp.dut_sensit_coef.t_sens = fix_sign(t_sens, BMM350_SIGNED_8_BIT) / 512.0f;

    tco_x = (dev->otp_data[BMM350_MAG_TCO_X] & BMM350_LSB_MASK);
    tco_y = (dev->otp_data[BMM350_MAG_TCO_Y] & BMM350_LSB_MASK);
    tco_z = (dev->otp_data[BMM350_MAG_TCO_Z] & BMM350_LSB_MASK);

    dev->mag_comp.dut_tco.tco_x = fix_sign(tco_x, BMM350_SIGNED_8_BIT) / 32.0f;
    dev->mag_comp.dut_tco.tco_y = fix_sign(tco_y, BMM350_SIGNED_8_BIT) / 32.0f;
    dev->mag_comp.dut_tco.tco_z = fix_sign(tco_z, BMM350_SIGNED_8_BIT) / 32.0f;

    tcs_x = (dev->otp_data[BMM350_MAG_TCS_X] & BMM350_MSB_MASK) >> 8;
    tcs_y = (dev->otp_data[BMM350_MAG_TCS_Y] & BMM350_MSB_MASK) >> 8;
    tcs_z = (dev->otp_data[BMM350_MAG_TCS_Z] & BMM350_MSB_MASK) >> 8;

    dev->mag_comp.dut_tcs.tcs_x = fix_sign(tcs_x, BMM350_SIGNED_8_BIT) / 16384.0f;
    dev->mag_comp.dut_tcs.tcs_y = fix_sign(tcs_y, BMM350_SIGNED_8_BIT) / 16384.0f;
    dev->mag_comp.dut_tcs.tcs_z = (fix_sign(tcs_z, BMM350_SIGNED_8_BIT) / 16384.0f) - BMM350_TCS_CORR_Z;

    dev->mag_comp.dut_t0 = (fix_sign(dev->otp_data[BMM350_MAG_DUT_T_0], BMM350_SIGNED_16_BIT) / 512.0f) + 23.0f;

    cross_x_y = (dev->otp_data[BMM350_CROSS_X_Y] & BMM350_LSB_MASK);
    cross_y_x = (dev->otp_data[BMM350_CROSS_Y_X] & BMM350_MSB_MASK) >> 8;
    cross_z_x = (dev->otp_data[BMM350_CROSS_Z_X] & BMM350_LSB_MASK);
    cross_z_y = (dev->otp_data[BMM350_CROSS_Z_Y] & BMM350_MSB_MASK) >> 8;

    dev->mag_comp.cross_axis.cross_x_y = fix_sign(cross_x_y, BMM350_SIGNED_8_BIT) / 800.0f;
    dev->mag_comp.cross_axis.cross_y_x = fix_sign(cross_y_x, BMM350_SIGNED_8_BIT) / 800.0f;
    dev->mag_comp.cross_axis.cross_z_x = fix_sign(cross_z_x, BMM350_SIGNED_8_BIT) / 800.0f;
    dev->mag_comp.cross_axis.cross_z_y = fix_sign(cross_z_y, BMM350_SIGNED_8_BIT) / 800.0f;
}

/*!
 * @brief This internal API is used to read raw magnetic x,y and z axis along with temperature
 */
static int8_t read_out_raw_data(float *out_data, struct bmm350_dev *dev)
{
    /* Variable to store the function result */
    int8_t rslt;

    float temp = 0.0;
    struct bmm350_raw_mag_data raw_data = { 0 };

    /* Float variable to convert mag lsb to uT and temp lsb to degC */
    float lsb_to_ut_degc[4];

    if (out_data != NULL) {
        rslt = bmm350_read_uncomp_mag_temp_data(&raw_data, dev);

        if (rslt == BMM350_OK) {
            /* Convert mag lsb to uT and temp lsb to degC */
            update_default_coefiecents(lsb_to_ut_degc);

            out_data[0] = (float)raw_data.raw_xdata * lsb_to_ut_degc[0];
            out_data[1] = (float)raw_data.raw_ydata * lsb_to_ut_degc[1];
            out_data[2] = (float)raw_data.raw_zdata * lsb_to_ut_degc[2];
            out_data[3] = (float)raw_data.raw_data_t *lsb_to_ut_degc[3];

            if (out_data[3] > 0.0) {
                temp = (float)(out_data[3] - (1 * 25.49));
            } else if (out_data[3] < 0.0) {
                temp = (float)(out_data[3] - (-1 * 25.49));
            } else {
                temp = (float)(out_data[3]);
            }

            out_data[3] = temp;
        }
    } else {
        rslt = BMM350_E_NULL_PTR;
    }

    return rslt;
}

/*!
 * @brief This internal API is used to convert lsb to uT and degC.
 */
static void update_default_coefiecents(float *lsb_to_ut_degc)
{
    float bxy_sens, bz_sens, temp_sens, ina_xy_gain_trgt, ina_z_gain_trgt, adc_gain, lut_gain;
    float power;

    bxy_sens = 14.55f;
    bz_sens = 9.0f;
    temp_sens = 0.00204f;

    ina_xy_gain_trgt = 19.46f;

    ina_z_gain_trgt = 31.0;

    adc_gain = 1 / 1.5f;
    lut_gain = 0.714607238769531f;

    power = (float)(1000000.0 / 1048576.0);

    lsb_to_ut_degc[0] = (power / (bxy_sens * ina_xy_gain_trgt * adc_gain * lut_gain));
    lsb_to_ut_degc[1] = (power / (bxy_sens * ina_xy_gain_trgt * adc_gain * lut_gain));
    lsb_to_ut_degc[2] = (power / (bz_sens * ina_z_gain_trgt * adc_gain * lut_gain));
    lsb_to_ut_degc[3] = 1 / (temp_sens * adc_gain * lut_gain * 1048576);
}

/*!
 * @brief This internal API is used to read OTP data after boot in user mode.
 */
static int8_t otp_dump_after_boot(struct bmm350_dev *dev)
{
    /* Variable to store the function result */
    int8_t rslt;

    uint16_t otp_word = 0;
    uint8_t index;

    for (index = 0; index < BMM350_OTP_DATA_LENGTH; index++) {
        rslt = read_otp_word(index, &otp_word, dev);
        dev->otp_data[index] = otp_word;
    }

    dev->var_id = (dev->otp_data[30] & 0x7f00) >> 9;

    /* Update magnetometer offset and sensitivity data. */
    update_mag_off_sens(dev);

    return rslt;
}

/*!
 * @brief This internal API is used for self-test entry configuration
 */
static int8_t self_test_entry_config(struct bmm350_dev *dev)
{
    /* Variable to store the function result */
    int8_t rslt;

    /* Variable to store PMU command */
    uint8_t cmd;

    /* Structure instance of PMU command status 0 */
    struct bmm350_pmu_cmd_status_0 pmu_cmd_stat_0 = { 0 };

    /* Set suspend mode */
    cmd = BMM350_PMU_CMD_SUS;

    rslt = bmm350_set_regs(BMM350_REG_PMU_CMD, &cmd, 1, dev);

    if (rslt == BMM350_OK) {
        rslt = bmm350_delay_us(30000, dev);
    }

    /* Read DUT outputs in FORCED mode */
    if (rslt == BMM350_OK) {
        rslt = bmm350_set_odr_performance(BMM350_DATA_RATE_100HZ, BMM350_AVERAGING_2, dev);

        if (rslt == BMM350_OK) {
            /* Enable all axis */
            rslt = bmm350_enable_axes(BMM350_X_EN, BMM350_Y_EN, BMM350_Z_EN, dev);
        }
    }

    /* Execute FGR with full CRST recharge */
    cmd = BMM350_PMU_CMD_FGR;

    if (rslt == BMM350_OK) {
        rslt = bmm350_set_regs(BMM350_REG_PMU_CMD, &cmd, 1, dev);

        if (rslt == BMM350_OK) {
            rslt = bmm350_delay_us(30000, dev);
        }
    }

    if (rslt == BMM350_OK) {
        rslt = bmm350_get_pmu_cmd_status_0(&pmu_cmd_stat_0, dev);

        if ((rslt == BMM350_OK) && (pmu_cmd_stat_0.pmu_cmd_value == BMM350_PMU_CMD_STATUS_0_FGR)) {
            /* Execute BR with full CRST recharge */
            cmd = BMM350_PMU_CMD_BR_FAST;

            rslt = bmm350_set_regs(BMM350_REG_PMU_CMD, &cmd, 1, dev);

            if (rslt == BMM350_OK) {
                rslt = bmm350_delay_us(4000, dev);
            }
        }
    }

    if (rslt == BMM350_OK) {
        rslt = bmm350_get_pmu_cmd_status_0(&pmu_cmd_stat_0, dev);
    }

    if ((rslt == BMM350_OK) && (pmu_cmd_stat_0.pmu_cmd_value == BMM350_PMU_CMD_STATUS_0_BR_FAST)) {
        cmd = BMM350_PMU_CMD_FM_FAST;

        rslt = bmm350_set_regs(BMM350_REG_PMU_CMD, &cmd, 1, dev);

        if (rslt == BMM350_OK) {
            rslt = bmm350_delay_us(16000, dev);
        }

        if (rslt == BMM350_OK) {
            rslt = bmm350_get_pmu_cmd_status_0(&pmu_cmd_stat_0, dev);

            if ((rslt == BMM350_OK) && (pmu_cmd_stat_0.pmu_cmd_value == BMM350_PMU_CMD_STATUS_0_FM_FAST)) {
                rslt = bmm350_delay_us(10, dev);
            }
        }
    }

    return rslt;
}

/*!
 * @brief This internal API is used to test self-test for X and Y axis
 */
static int8_t self_test_xy_axis(struct bmm350_self_test *out_data, struct bmm350_dev *dev)
{
    /* Variable to store the function result */
    int8_t rslt;

    /* Set pmu command */
    uint8_t cmd = BMM350_PMU_CMD_FM_FAST;

    /* Setup DUT: enable positive user self-test on x-axis */
    rslt = self_test_config(BMM350_SELF_TEST_POS_X, cmd, out_data, dev);

    if (rslt == BMM350_OK) {
        /* Setup DUT: enable negative user self-test on x-axis */
        rslt = self_test_config(BMM350_SELF_TEST_NEG_X, cmd, out_data, dev);

        if (rslt == BMM350_OK) {
            /* Setup DUT: enable positive user self-test on y-axis */
            rslt = self_test_config(BMM350_SELF_TEST_POS_Y, cmd, out_data, dev);

            if (rslt == BMM350_OK) {
                /* Setup DUT: enable negative user self-test on y-axis */
                rslt = self_test_config(BMM350_SELF_TEST_NEG_Y, cmd, out_data, dev);
            }
        }
    }

    return rslt;
}

/*!
 * @brief This internal API is used to set self-test configurations.
 */
static int8_t self_test_config(uint8_t st_cmd,
                               uint8_t pmu_cmd,
                               struct bmm350_self_test *out_data,
                               struct bmm350_dev *dev)
{
    /* Variable to store the function result */
    int8_t rslt;

    float out_ust[4];

    struct bmm350_pmu_cmd_status_0 pmu_cmd_stat_0 = { 0 };

    static float out_ustxh = 0.0, out_ustxl = 0.0, out_ustyh = 0.0, out_ustyl = 0.0;

    rslt = bmm350_set_regs(BMM350_REG_TMR_SELFTEST_USER, &st_cmd, 1, dev);

    if (rslt == BMM350_OK) {
        rslt = bmm350_delay_us(1000, dev);
    }

    if (rslt == BMM350_OK) {
        rslt = bmm350_set_regs(BMM350_REG_PMU_CMD, &pmu_cmd, 1, dev);

        if (rslt == BMM350_OK) {
            rslt = bmm350_delay_us(6000, dev);

            if (rslt == BMM350_OK) {
                rslt = bmm350_get_pmu_cmd_status_0(&pmu_cmd_stat_0, dev);
            }
        }
    }

    if ((rslt == BMM350_OK) && (pmu_cmd_stat_0.pmu_cmd_value == BMM350_PMU_CMD_STATUS_0_FM_FAST)) {
        /* Reads raw magnetic x and y axis */
        rslt = read_out_raw_data(out_ust, dev);

        if (rslt == BMM350_OK) {
            /* Read DUT outputs in FORCED mode (XP_UST) */
            if (st_cmd == BMM350_SELF_TEST_POS_X) {
                out_ustxh = out_ust[0];
            }
            /* Read DUT outputs in FORCED mode (XN_UST) */
            else if (st_cmd == BMM350_SELF_TEST_NEG_X) {
                out_ustxl = out_ust[0];
            }
            /* Read DUT outputs in FORCED mode (YP_UST) */
            else if (st_cmd == BMM350_SELF_TEST_POS_Y) {
                out_ustyh = out_ust[1];
            }
            /* Read DUT outputs in FORCED mode (YN_UST) */
            else if (st_cmd == BMM350_SELF_TEST_NEG_Y) {
                out_ustyl = out_ust[1];
            } else {
                /* Returns error if self-test axis is wrong */
                rslt = BMM350_E_SELF_TEST_INVALID_AXIS;
            }

            if (rslt == BMM350_OK) {
                out_data->out_ust_x = out_ustxh - out_ustxl;
                out_data->out_ust_y = out_ustyh - out_ustyl;
            }
        }
    }

    return rslt;
}

/*!
 * @brief This internal API is used to switch from suspend mode to normal mode or forced mode.
 */
static int8_t set_powermode(enum bmm350_power_modes powermode, struct bmm350_dev *dev)
{
    /* Variable to store the function result */
    int8_t rslt;

    uint8_t reg_data = powermode;
    uint8_t get_avg;

    /* Array to store suspend to forced mode delay */
    uint32_t sus_to_forced_mode[4] = {
        BMM350_SUS_TO_FORCEDMODE_NO_AVG_DELAY, BMM350_SUS_TO_FORCEDMODE_AVG_2_DELAY, BMM350_SUS_TO_FORCEDMODE_AVG_4_DELAY,
        BMM350_SUS_TO_FORCEDMODE_AVG_8_DELAY
    };

    /* Array to store suspend to forced mode fast delay */
    uint32_t sus_to_forced_mode_fast[4] = {
        BMM350_SUS_TO_FORCEDMODE_FAST_NO_AVG_DELAY, BMM350_SUS_TO_FORCEDMODE_FAST_AVG_2_DELAY,
        BMM350_SUS_TO_FORCEDMODE_FAST_AVG_4_DELAY, BMM350_SUS_TO_FORCEDMODE_FAST_AVG_8_DELAY
    };

    uint8_t avg = 0;
    uint32_t delay_us = 0;

    rslt = null_ptr_check(dev);

    if (rslt == BMM350_OK) {
        /* Set PMU command configuration to desired power mode */
        rslt = bmm350_set_regs(BMM350_REG_PMU_CMD, &reg_data, 1, dev);

        if (rslt == BMM350_OK) {
            /* Get average configuration */
            rslt = bmm350_get_regs(BMM350_REG_PMU_CMD_AGGR_SET, &get_avg, 1, dev);

            if (rslt == BMM350_OK) {
                /* Mask the average value */
                avg = ((get_avg & BMM350_AVG_MSK) >> BMM350_AVG_POS);
            }
        }
    }

    if (rslt == BMM350_OK) {
        /* Check if desired power mode is normal mode */
        if (powermode == BMM350_NORMAL_MODE) {
            delay_us = BMM350_SUSPEND_TO_NORMAL_DELAY;
        }

        /* Check if desired power mode is forced mode */
        if (powermode == BMM350_FORCED_MODE) {
            /* Store delay based on averaging mode */
            delay_us = sus_to_forced_mode[avg];
        }

        /* Check if desired power mode is forced mode fast */
        if (powermode == BMM350_FORCED_MODE_FAST) {
            /* Store delay based on averaging mode */
            delay_us = sus_to_forced_mode_fast[avg];
        }

        /* Perform delay based on power mode */
        rslt = bmm350_delay_us(delay_us, dev);
    }

    return rslt;
}
