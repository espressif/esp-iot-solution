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
* @file       bmm350_oor.h
* @date       2023-05-26
* @version    v1.4.0
*
*/

#ifndef _BMM350_OOR_H
#define _BMM350_OOR_H

#include <stdbool.h>
#include <math.h>

#include "bmm350.h"

/*! CPP guard */
#ifdef __cplusplus
extern "C" {
#endif

/******************************************************************************/
/*! @name        General Macro Definitions                                    */
/******************************************************************************/

/*! Macro to define half self-test for out of range
 *  NOTE: Comment this to use both positive and negative self tests */
#define BMM350_OOR_HALF_SELF_TEST

/*! Macro to define mag data minimum and maximum range in uT */
#define BMM350_HALF_ST_THRESHOLD       (130.0f)
#define BMM350_FULL_ST_THRESHOLD       (300.0f)

/*! Macro to define threshold values of in range, out of range and self-test */
#define BMM350_IN_RANGE_THRESHOLD      (2000.0f)
#define BMM350_OUT_OF_RANGE_THRESHOLD  (2400.0f)
#define BMM350_SELF_TEST_THRESHOLD     (2600.0f)

/************************* Structure definitions *************************/

/*!
 * @brief Structure to define bmm350 out of range parameters
 */
struct bmm350_oor_params {
    /*! Counter to track what self test to trigger */
    uint8_t st_counter;

    /*! Current self-test command */
    uint8_t st_cmd;

    /*! Stores the last applied self test configuration */
    uint8_t last_st_cmd;

    /*! Store the last measurements for comparing against the self-test */
    float mag_xp, mag_xn, mag_yp, mag_yn;

    /*! Flags to track if the test failed to redo it */
    bool x_failed, y_failed;

    /*! Flags to enable self-test */
    bool enable_selftest;

    /*! Flags to trigger reset */
    bool trigger_reset;

    /*! Variable to store reset counter value */
    uint8_t reset_counter;
};

/******************* Function prototype declarations ********************/

/*!
 * @brief Function to read data and validate if the sensor is out of range
 *
 * @param[in,out] out_of_range : Flag that indicates that the sensor is out of range
 * @param[out] data            : Sensor data
 * @param[out] oor             : Structure that stores the state of the out of range detector
 * @param[in,out] dev          : Device structure of the BMM350
 *
 *  @return Result of API execution status
 *  @retval = 0 -> Success
 *  @retval < 0 -> Error
 */
int8_t bmm350_oor_read(bool *out_of_range,
                       struct bmm350_mag_temp_data *data,
                       struct bmm350_oor_params *oor,
                       struct bmm350_dev *dev);

/*!
 * @brief Function to perform reset sequence in forced mode.
 *
 * @param[in,out] oor             : Structure that stores the state of the out of range detector
 * @param[in,out]  dev             : Structure instance of bmm350_dev.
 *
 * @return Result of API execution status
 *  @retval = 0 -> Success
 *  @retval < 0 -> Error
 */
int8_t bmm350_oor_perform_reset_sequence_forced(struct bmm350_oor_params *oor, struct bmm350_dev *dev);

#ifdef __cplusplus
}
#endif /* End of CPP guard */

#endif /* _BMM350_OOR_H */
