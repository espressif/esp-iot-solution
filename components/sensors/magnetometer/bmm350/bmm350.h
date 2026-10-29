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
* @file       bmm350.h
* @date       2023-05-26
* @version    v1.4.0
*
*/

/*!
 * @defgroup bmm350 BMM350
 * @brief Sensor driver for BMM350 sensor
 */

#ifndef _BMM350_H
#define _BMM350_H

/*! CPP guard */
#ifdef __cplusplus
extern "C" {
#endif

/*************************** Header files *******************************/

#include  "bmm350_defs.h"

/******************* Function prototype declarations ********************/

/**
 * \ingroup bmm350
 * \defgroup bmm350ApiInit Initialization
 * @brief Initialize the sensor and device structure
 */

/*!
* \ingroup bmm350ApiInit
* \page bmm350_api_bmm350_init bmm350_init
* \code
* int8_t bmm350_init(struct bmm350_dev *dev);
* \endcode
* @details This API is the entry point. Call this API before using other APIs.
*  This API reads the chip-id of the sensor which is the first step to
*  verify the sensor and also it configures the read mechanism of I2C and
*  I3C interface.
*
*  @param[in,out] dev : Structure instance of bmm350_dev
*
*  @return Result of API execution status
*  @retval = 0 -> Success
*  @retval < 0 -> Error
*/
int8_t bmm350_init(struct bmm350_dev *dev);

/**
 * \ingroup bmm350
 * \defgroup bmm350ApiReset Reset
 * @brief Reset APIs
 */

/*!
* \ingroup bmm350ApiReset
* \page bmm350_api_bmm350_soft_reset bmm350_soft_reset
* \code
* int8_t bmm350_soft_reset(struct bmm350_dev *dev);
* \endcode
* @details This API is used to perform soft-reset of the sensor
* where all the registers are reset to their default values.
*
* @param[in] dev       : Structure instance of bmm350_dev.
*
* @return Result of API execution status
*  @retval = 0 -> Success
*  @retval < 0 -> Error
*/

int8_t bmm350_soft_reset(struct bmm350_dev *dev);

/**
 * \ingroup bmm350
 * \defgroup bmm350ApiSetGet Set-Get
 * @brief Set and Get APIs
 */

/*!
* \ingroup bmm350ApiSetGet
* \page bmm350_api_bmm350_set_regs bmm350_set_regs
* \code
* int8_t bmm350_set_regs(uint8_t reg_addr, const uint8_t *reg_data, uint16_t len, struct bmm350_dev *dev);
* \endcode
* @details This API writes the given data to the register address
* of the sensor.
*
* @param[in] reg_addr : Register address from where the data to be written.
* @param[in] reg_data : Pointer to data buffer which is to be written
*                       in the reg_addr of sensor.
* @param[in] len      : No of bytes of data to write..
* @param[in, out] dev      : Structure instance of bmm350_dev.
*
* @return Result of API execution status
*  @retval = 0 -> Success
*  @retval < 0 -> Error
*/
int8_t bmm350_set_regs(uint8_t reg_addr, const uint8_t *reg_data, uint16_t len, struct bmm350_dev *dev);

/*!
* \ingroup bmm350ApiSetGet
* \page bmm350_api_bmm350_get_regs bmm350_get_regs
* \code
* int8_t bmm350_get_regs(uint8_t reg_addr, uint8_t *reg_data, uint16_t len, struct bmm350_dev *dev);
* \endcode
* @details This API reads the data from the given register address of sensor.
*
* @param[in] reg_addr  : Register address from where the data to be read
* @param[out] reg_data : Pointer to data buffer to store the read data.
* @param[in] len       : No of bytes of data to be read.
* @param[in] dev       : Structure instance of bmm350_dev.
*
* @return Result of API execution status
*  @retval = 0 -> Success
*  @retval < 0 -> Error
*/
int8_t bmm350_get_regs(uint8_t reg_addr, uint8_t *reg_data, uint16_t len, struct bmm350_dev *dev);

/**
 * \ingroup bmm350
 * \defgroup bmm350ApiDelay Delay
 * @brief Delay function in microseconds
 */

/*!
* \ingroup bmm350ApiDelay
* \page bmm350_api_bmm350_delay_us bmm350_delay_us
* \code
* int8_t bmm350_delay_us(uint32_t period_us, const struct bmm350_dev *dev);
* \endcode
* @details This function provides the delay for required time (Microsecond) as per the input provided in some of the
* APIs.
*
* @param[in] period_us  : The required wait time in microsecond.
* @param[in] dev         : Structure instance of bmm350_dev.
*
* @return Result of API execution status
*  @retval = 0 -> Success
*  @retval < 0 -> Error
*/
int8_t bmm350_delay_us(uint32_t period_us, const struct bmm350_dev *dev);

/*!
* \ingroup bmm350ApiSetGet
* \page bmm350_api_bmm350_get_interrupt_status bmm350_get_interrupt_status
* \code
* int8_t bmm350_get_interrupt_status(uint8_t *drdy_status, struct bmm350_dev *dev);
* \endcode
* @details This API obtains the status flags of all interrupt
* which is used to check for the assertion of interrupts
*
* @param[in,out] drdy_status   : Variable to store data ready interrupt status.
* @param[in,out] dev           : Structure instance of bmm350_dev.
*
*
* @return Result of API execution status and self test result.
*  @retval = 0 -> Success
*  @retval < 0 -> Error
*/
int8_t bmm350_get_interrupt_status(uint8_t *drdy_status, struct bmm350_dev *dev);

/*!
* \ingroup bmm350ApiSetGet
* \page bmm350_api_bmm350_set_powermode bmm350_set_powermode
* \code
* int8_t bmm350_set_powermode(enum bmm350_power_modes powermode, struct bmm350_dev *dev);
* \endcode
* @details This API is used to set the power mode of the sensor
*
* @param[in] powermode : Set power mode
* @param[in] dev       : Structure instance of bmm350_dev.
*
*@verbatim
                powermode |   Power mode
 -------------------------|-----------------------
                          |  BMM350_SUSPEND_MODE
                          |  BMM350_NORMAL_MODE
                          |  BMM350_FORCED_MODE
                          |  BMM350_FORCED_MODE_FAST
*@endverbatim
*
* @return Result of API execution status
*  @retval = 0 -> Success
*  @retval < 0 -> Error
*/
int8_t bmm350_set_powermode(enum bmm350_power_modes powermode, struct bmm350_dev *dev);

/*!
* \ingroup bmm350ApiSetGet
* \page bmm350_api_bmm350_set_odr_performance bmm350_set_odr_performance
* \code
* int8_t bmm350_set_odr_performance(enum bmm350_data_rates odr,
*                                   enum bmm350_performance_parameters avg,
*                                   struct bmm350_dev *dev);
*
* \endcode
* @details This API sets the ODR and averaging factor.
* If ODR and performance is a combination which is not allowed, then
* the combination setting is corrected to the next lower possible setting
*
* @param[in]  odr         :  enum bmm350_data_rates
*
*@verbatim
      Data rate (ODR)     |        odr
 -------------------------|-----------------------
   400Hz                  |  BMM350_DATA_RATE_400HZ
   200Hz                  |  BMM350_DATA_RATE_200HZ
   100Hz                  |  BMM350_DATA_RATE_100HZ
   50Hz                   |  BMM350_DATA_RATE_50HZ
   25Hz                   |  BMM350_DATA_RATE_25HZ
   12.5Hz                 |  BMM350_DATA_RATE_12_5HZ
   6.25Hz                 |  BMM350_DATA_RATE_6_25HZ
   3.125Hz                |  BMM350_DATA_RATE_3_125HZ
   1.5625Hz               |  BMM350_DATA_RATE_1_5625HZ
*@endverbatim
*
* @param[in]  avg       :  enum bmm350_performance_parameters
*
*@verbatim
     avg                    |   averaging factor            alias
 ---------------------------|------------------------------------------
   low power/highest noise  |  BMM350_NO_AVERAGING  BMM350_LOWPOWER
   lesser noise             |  BMM350_AVERAGING_2   BMM350_REGULARPOWER
   even lesser noise        |  BMM350_AVERAGING_4   BMM350_LOWNOISE
 lowest noise/highest power |  BMM350_AVERAGING_8   BMM350_ULTRALOWNOISE
*@endverbatim
*
* @param[in,out]  dev          : Structure instance of bmm350_dev
*
* @return Result of API execution status
*  @retval = 0 -> Success
*  @retval < 0 -> Error
*/
int8_t bmm350_set_odr_performance(enum bmm350_data_rates odr,
                                  enum bmm350_performance_parameters avg,
                                  struct bmm350_dev *dev);

/*!
* \ingroup bmm350ApiSetGet
* \page bmm350_api_bmm350_enable_axes bmm350_enable_axes
* \code
* int8_t bmm350_enable_axes(enum bmm350_x_axis_en_dis en_x, enum bmm350_y_axis_en_dis en_y, enum bmm350_z_axis_en_dis en_z, struct bmm350_dev *dev);
* \endcode
* @details This API is used to enable or disable the magnetic
* measurement of x,y,z axes
*
* @param[in]  en_x      : Enable or disable X axis
* @param[in]  en_y      : Enable or disable Y axis
* @param[in]  en_z      : Enable or disable Z axis
* @param[in,out]  dev   : Structure instance of bmm350_dev.
*
*@verbatim
       Value        |   Axis (en_x, en_y, en_z)
 -------------------|-----------------------
   BMM350_ENABLE    |    Enabled
  BMM350_DISABLE    |    Disabled
*@endverbatim
*
* @return Result of API execution status
*  @retval = 0 -> Success
*  @retval < 0 -> Error
*/
int8_t bmm350_enable_axes(enum bmm350_x_axis_en_dis en_x,
                          enum bmm350_y_axis_en_dis en_y,
                          enum bmm350_z_axis_en_dis en_z,
                          struct bmm350_dev *dev);

/**
 * \ingroup bmm350
 * \defgroup bmm350ApiRead Sensortime
 * @brief Reads sensortime
 */

/*!
* \ingroup bmm350ApiRead
* \page bmm350_api_bmm350_read_sensortime bmm350_read_sensortime
* \code
* int8_t bmm350_read_sensortime(uint32_t *seconds, uint32_t *nanoseconds, struct bmm350_dev *dev);
* \endcode
* @details This API is used to read the sensor time.
* It converts the sensor time register values to the representative time value.
* Returns the sensor time in ticks.
*
* @param[out] seconds        : Variable to get sensor time in seconds.
* @param[out] nanoseconds    : Variable to get sensor time in nanoseconds.
* @param[in] dev             : Structure instance of bmm350_dev.
*
* @return Result of API execution status
*  @retval = 0 -> Success
*  @retval < 0 -> Error
*/
int8_t bmm350_read_sensortime(uint32_t *seconds, uint32_t *nanoseconds, struct bmm350_dev *dev);

/**
 * \ingroup bmm350
 * \defgroup bmm350ApiInterrupt Enable Interrupt
 * @brief Interrupt enable APIs
 */

/*!
* \ingroup bmm350ApiInterrupt
* \page bmm350_api_bmm350_enable_interrupt bmm350_enable_interrupt
* \code
* int8_t bmm350_enable_interrupt(enum bmm350_interrupt_enable_disable enable_disable, struct bmm350_dev *dev);
* \endcode
* @details This API is used to enable or disable the data ready interrupt
*
* @param[in] enable_disable    : Enable/ Disable data ready interrupt.
* @param[in] dev               : Structure instance of bmm350_dev.
*
* @return Result of API execution status
*  @retval = 0 -> Success
*  @retval < 0 -> Error
*/
int8_t bmm350_enable_interrupt(enum bmm350_interrupt_enable_disable enable_disable, struct bmm350_dev *dev);

/*!
* \ingroup bmm350ApiInterrupt
* \page bmm350_api_bmm350_configure_interrupt bmm350_configure_interrupt
* \code
* int8_t  bmm350_configure_interrupt(enum bmm350_intr_latch latching,
*                                   enum bmm350_intr_polarity polarity,
*                                   enum bmm350_intr_drive drivertype,
*                                   enum bmm350_intr_map map_nomap,
*                                   struct bmm350_dev *dev);
* \endcode
* @details This API is used to configure the interrupt control settings.
*
* @param[in] latching          : Sets either latched or pulsed.
* @param[in] polarity          : Sets either polarity high or low.
* @param[in] drivertype        : Sets either open drain or push pull.
* @param[in] map_nomap         : Sets either map or unmap the pins.
* @param[in] dev               : Structure instance of bmm350_dev.
*
* @return Result of API execution status
*  @retval = 0 -> Success
*  @retval < 0 -> Error
*/
int8_t  bmm350_configure_interrupt(enum bmm350_intr_latch latching,
                                   enum bmm350_intr_polarity polarity,
                                   enum bmm350_intr_drive drivertype,
                                   enum bmm350_intr_map map_nomap,
                                   struct bmm350_dev *dev);

/**
 * \ingroup bmm350
 * \defgroup bmm350ApiUncompMag Uncompensated mag
 * @brief Reads uncompensated mag and temperature data
 */

/*!
* \ingroup bmm350ApiUncompMag
* \page bmm350_api_bmm350_read_uncomp_mag_temp_data bmm350_read_uncomp_mag_temp_data
* \code
* int8_t bmm350_read_uncomp_mag_temp_data(struct bmm350_raw_mag_data *raw_data, struct bmm350_dev *dev);
* \endcode
* @details This API is used to read uncompensated mag and temperature data
*
* @param[in, out] raw_data     : Structure instance of bmm350_raw_mag_data.
* @param[in, out] dev          : Structure instance of bmm350_dev.
*
* @return Result of API execution status
*  @retval = 0 -> Success
*  @retval < 0 -> Error
*/
int8_t bmm350_read_uncomp_mag_temp_data(struct bmm350_raw_mag_data *raw_data, struct bmm350_dev *dev);

/*!
* \ingroup bmm350ApiSetGet
* \page bmm350_api_bmm350_set_int_ctrl_ibi bmm350_set_int_ctrl_ibi
* \code
* int8_t bmm350_set_int_ctrl_ibi(enum bmm350_drdy_int_map_to_ibi en_dis,
* enum bmm350_clear_drdy_int_status_upon_ibi clear_on_ibi, struct bmm350_dev *dev);
* \endcode
* @details This API sets the interrupt control IBI configurations to the sensor.
* And enables conventional interrupt if IBI is enabled.
*
* @param[in]  en_dis          : Sets either enable or disable IBI.
* @param[in]  clear_on_ibi    : sets either clear or no clear on IBI.
* @param[in]  dev             : Structure instance of bmm350_dev.
*
* @return Result of API execution status
*  @retval = 0 -> Success
*  @retval < 0 -> Error
*/
int8_t bmm350_set_int_ctrl_ibi(enum bmm350_drdy_int_map_to_ibi en_dis,
                               enum bmm350_clear_drdy_int_status_upon_ibi clear_on_ibi,
                               struct bmm350_dev *dev);

/*!
* \ingroup bmm350ApiSetGet
* \page bmm350_api_bmm350_set_pad_drive bmm350_set_pad_drive
* \code
* int8_t bmm350_set_pad_drive(uint8_t drive, struct bmm350_dev *dev);
* \endcode
* @details This API is used to set the pad drive strength
*
* @param[in] drive     : Drive settings, range 0 (weak) ..7(strong)
* @param[in] dev       : Structure instance of bmm350_dev.
*
* @return Result of API execution status
*  @retval = 0 -> Success
*  @retval < 0 -> Error
*/
int8_t bmm350_set_pad_drive(uint8_t drive, struct bmm350_dev *dev);

/*!
* \ingroup bmm350ApiReset
* \page bmm350_api_bmm350_magnetic_reset_and_wait bmm350_magnetic_reset_and_wait
* \code
* int8_t bmm350_magnetic_reset_and_wait(struct bmm350_dev *dev)
* \endcode
* @details  This API is used to perform the magnetic reset of the sensor
* which is necessary after a field shock (400mT field applied to sensor).
* It sends flux guide or bit reset to the device in suspend mode.
*
* @param[in] dev       : Structure instance of bmm350_dev.
*
* @return Result of API execution status
*  @retval = 0 -> Success
*  @retval < 0 -> Error
*/
int8_t bmm350_magnetic_reset_and_wait(struct bmm350_dev *dev);

/**
 * \ingroup bmm350
 * \defgroup bmm350ApiMagComp Compensation
 * @brief Compensation for mag x,y,z axis and temperature API
 */

/*!
* \ingroup bmm350ApiMagComp
* \page bmm350_api_bmm350_get_compensated_mag_xyz_temp_data bmm350_get_compensated_mag_xyz_temp_data
* \code
* int8_t bmm350_get_compensated_mag_xyz_temp_data(struct bmm350_mag_temp_data *mag_temp_data, struct bmm350_dev *dev);
* \endcode
* @details This API is used to perform compensation for raw magnetometer and temperature data.
*
* @param[out] mag_temp_data    : Structure instance of bmm350_mag_temp_data.
* @param[in] dev               : Structure instance of bmm350_dev.
*
* @return Result of API execution status
*  @retval = 0 -> Success
*  @retval < 0 -> Error
*/
int8_t bmm350_get_compensated_mag_xyz_temp_data(struct bmm350_mag_temp_data *mag_temp_data, struct bmm350_dev *dev);

/**
 * \ingroup bmm350
 * \defgroup bmm350ApiSelftest Self-test
 * @brief Perform self-test for x and y axis
 */

/*!
* \ingroup bmm350ApiSelftest
* \page bmm350_api_bmm350_perform_self_test bmm350_perform_self_test
* \code
* int8_t bmm350_perform_self_test(struct bmm350_self_test *out_data, struct bmm350_dev *dev);
* \endcode
* @details This API executes FGR and BR sequences to initialize TMR sensor and performs self-test for x and y axis.
*
* @param[in, out] out_data    : Structure instance of bmm350_self_test.
* @param[in] dev              : Structure instance of bmm350_dev.
*
* @return Result of API execution status
*  @retval = 0 -> Success
*  @retval < 0 -> Error
*/
int8_t bmm350_perform_self_test(struct bmm350_self_test *out_data, struct bmm350_dev *dev);

/*!
* \ingroup bmm350ApiSetGet
* \page bmm350_api_bmm350_set_i2c_wdt bmm350_set_i2c_wdt
* \code
* int8_t bmm350_set_i2c_wdt(enum bmm350_i2c_wdt_en i2c_wdt_en_dis, enum bmm350_i2c_wdt_sel i2c_wdt_sel,
* struct bmm350_dev *dev);
* \endcode
* @details This API sets the I2C watchdog timer configurations to the sensor.
*
* @param[in]  i2c_wdt_en_dis  : Enable/ Disable I2C watchdog timer.
* @param[in]  i2c_wdt_sel     : I2C watchdog timer selection period.
* @param[in]  dev             : Structure instance of bmm350_dev.
*
* @return Result of API execution status
*  @retval = 0 -> Success
*  @retval < 0 -> Error
*/
int8_t bmm350_set_i2c_wdt(enum bmm350_i2c_wdt_en i2c_wdt_en_dis,
                          enum bmm350_i2c_wdt_sel i2c_wdt_sel,
                          struct bmm350_dev *dev);

/*!
* \ingroup bmm350ApiSetGet
* \page bmm350_api_bmm350_set_tmr_selftest_user bmm350_set_tmr_selftest_user
* \code
* int8_t bmm350_set_tmr_selftest_user(enum bmm350_st_igen_en st_igen_en_dis,
*                                     enum bmm350_st_n st_n_en_dis,
*                                     enum bmm350_st_p st_p_en_dis,
*                                     enum bmm350_ist_en_x ist_x_en_dis,
*                                     enum bmm350_ist_en_y ist_y_en_dis,
*                                     struct bmm350_dev *dev);
* \endcode
* @details This API sets the TMR user self-test register
*
* @param[in]  st_igen_en_dis  : Enable/ Disable self-test internal current gen.
* @param[in]  st_n_en_dis     : Enable/ Disable dc_st_n signal.
* @param[in]  st_p_en_dis     : Enable/ Disable dc_st_p signal.
* @param[in]  ist_x_en_dis    : Enable/ Disable dc_ist_en_x signal.
* @param[in]  ist_y_en_dis    : Enable/ Disable dc_ist_en_y signal.
* @param[in]  dev             : Structure instance of bmm350_dev.
*
* @return Result of API execution status
*  @retval = 0 -> Success
*  @retval < 0 -> Error
*/
int8_t bmm350_set_tmr_selftest_user(enum bmm350_st_igen_en st_igen_en_dis,
                                    enum bmm350_st_n st_n_en_dis,
                                    enum bmm350_st_p st_p_en_dis,
                                    enum bmm350_ist_en_x ist_x_en_dis,
                                    enum bmm350_ist_en_y ist_y_en_dis,
                                    struct bmm350_dev *dev);

/*!
* \ingroup bmm350ApiSetGet
* \page bmm350_api_bmm350_set_ctrl_user bmm350_set_ctrl_user
* \code
* int8_t bmm350_set_ctrl_user(enum bmm350_ctrl_user cfg_sens_tim_aon_en_dis, struct bmm350_dev *dev);
* \endcode
* @details This API sets the control user configurations to the sensor.
*
* @param[in]  cfg_sens_tim_aon_en_dis  : Enable/ Disable configuration of sensor time to run on suspend mode.
* @param[in]  dev                      : Structure instance of bmm350_dev.
*
* @return Result of API execution status
*  @retval = 0 -> Success
*  @retval < 0 -> Error
*/
int8_t bmm350_set_ctrl_user(enum bmm350_ctrl_user cfg_sens_tim_aon_en_dis, struct bmm350_dev *dev);

/*!
* \ingroup bmm350ApiSetGet
* \page bmm350_api_bmm350_get_pmu_cmd_status_0 bmm350_get_pmu_cmd_status_0
* \code
* int8_t bmm350_get_pmu_cmd_status_0(struct bmm350_pmu_cmd_status_0 *pmu_cmd_stat_0, struct bmm350_dev *dev);
* \endcode
* @details This API gets the PMU command status 0 value.
*
* @param[out]  pmu_cmd_stat_0  : Structure instance of bmm350_pmu_cmd_status_0.
* @param[in]  dev              : Structure instance of bmm350_dev.
*
* @return Result of API execution status
*  @retval = 0 -> Success
*  @retval < 0 -> Error
*/
int8_t bmm350_get_pmu_cmd_status_0(struct bmm350_pmu_cmd_status_0 *pmu_cmd_stat_0, struct bmm350_dev *dev);

#ifdef __cplusplus
}
#endif /* End of CPP guard */

#endif /* _BMM350_H */
