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
* @file       bmm350_oor.c
* @date       2023-05-26
* @version    v1.4.0
*
*/

#include "bmm350_oor.h"

#ifdef BMM350_OOR_HALF_SELF_TEST

/*!
 * @brief This internal API is used to trigger half self-test
 */
static int8_t trigger_half_selftest(struct bmm350_oor_params *oor, struct bmm350_dev *dev)
{
    int8_t rslt = BMM350_OK;

    oor->st_cmd = BMM350_SELF_TEST_DISABLE;

    /* Trigger a self-test on every alternate measurement if needed */
    if (oor->enable_selftest) {
        oor->st_counter++;

        switch (oor->st_counter) {
        case 1:
            oor->st_cmd = BMM350_SELF_TEST_POS_X;
            break;
        case 2:
            oor->st_cmd = BMM350_SELF_TEST_DISABLE;
            break;
        case 3:
            oor->st_cmd = BMM350_SELF_TEST_POS_Y;
            break;
        case 4:
            oor->st_cmd = BMM350_SELF_TEST_DISABLE;
            break;

        default:
            oor->st_counter = 0;
            oor->st_cmd = BMM350_SELF_TEST_DISABLE;
            break;
        }

        rslt = bmm350_set_regs(BMM350_REG_TMR_SELFTEST_USER, &(oor->st_cmd), 1, dev);
    } else {
        if (oor->last_st_cmd != BMM350_SELF_TEST_DISABLE) {
            rslt = bmm350_set_regs(BMM350_REG_TMR_SELFTEST_USER, &(oor->st_cmd), 1, dev);
            oor->st_counter = 0;
        }
    }

    return rslt;
}

/*!
 * @brief This internal API is used to validate half self-test
 */
static void validate_half_selftest(const struct bmm350_mag_temp_data *data, struct bmm350_oor_params *oor)
{
    switch (oor->last_st_cmd) {
    case BMM350_SELF_TEST_DISABLE:
        oor->mag_xn = data->x;
        oor->mag_yn = data->y;
        break;

    case BMM350_SELF_TEST_POS_X:
        oor->mag_xp = data->x;
        oor->x_failed = (oor->mag_xp - oor->mag_xn) < BMM350_HALF_ST_THRESHOLD ? true : false;
        break;

    case BMM350_SELF_TEST_POS_Y:
        oor->mag_yp = data->y;
        oor->y_failed = (oor->mag_yp - oor->mag_yn) < BMM350_HALF_ST_THRESHOLD ? true : false;
        break;

    default:
        break;
    }
}
#else

/*!
 * @brief This internal API is used to trigger full self-test
 */
static int8_t trigger_full_selftest(struct bmm350_oor_params *oor, struct bmm350_dev *dev)
{
    int8_t rslt = BMM350_OK;

    oor->st_cmd = BMM350_SELF_TEST_DISABLE;

    /* Trigger a self-test on every alternate measurement if needed */
    if (oor->enable_selftest) {
        oor->st_counter++;

        switch (oor->st_counter) {
        case 1:
            oor->st_cmd = BMM350_SELF_TEST_POS_X;
            break;
        case 2:
            oor->st_cmd = BMM350_SELF_TEST_NEG_X;
            break;
        case 3:
            oor->st_cmd = BMM350_SELF_TEST_POS_Y;
            break;
        case 4:
            oor->st_cmd = BMM350_SELF_TEST_NEG_Y;
            break;

        default:
            oor->st_counter = 0;
            oor->st_cmd = BMM350_SELF_TEST_DISABLE;
            break;
        }

        rslt = bmm350_set_regs(BMM350_REG_TMR_SELFTEST_USER, &(oor->st_cmd), 1, dev);
    } else {
        if (oor->last_st_cmd != BMM350_SELF_TEST_DISABLE) {
            rslt = bmm350_set_regs(BMM350_REG_TMR_SELFTEST_USER, &(oor->st_cmd), 1, dev);
            oor->st_counter = 0;
        }
    }

    return rslt;
}

/*!
 * @brief This internal API is used to validate full self-test
 */
static void validate_full_selftest(const struct bmm350_mag_temp_data *data, struct bmm350_oor_params *oor)
{
    switch (oor->last_st_cmd) {
    case BMM350_SELF_TEST_POS_X:
        oor->mag_xp = data->x;
        break;

    case BMM350_SELF_TEST_NEG_X:
        oor->mag_xn = data->x;
        oor->x_failed = (oor->mag_xp - oor->mag_xn) < BMM350_FULL_ST_THRESHOLD ? true : false;
        break;

    case BMM350_SELF_TEST_POS_Y:
        oor->mag_yp = data->y;
        break;

    case BMM350_SELF_TEST_NEG_Y:
        oor->mag_yn = data->y;
        oor->y_failed = (oor->mag_yp - oor->mag_yn) < BMM350_FULL_ST_THRESHOLD ? true : false;
        break;

    default:
        break;
    }
}
#endif

/*!
 * @brief This internal API is used to validate out of range.
 */
static void validate_out_of_range(bool *out_of_range,
                                  const struct bmm350_mag_temp_data *data,
                                  struct bmm350_oor_params *oor)
{
    float field_str = 0.0f;

    /* Threshold to start out of range detection */
    float threshold = BMM350_OUT_OF_RANGE_THRESHOLD;

    /* Threshold to start self-tests */
    float st_threshold = BMM350_SELF_TEST_THRESHOLD;

    /* If either self-test failed, alert that the sensor is out of range and continue self-tests */
    if (oor->x_failed || oor->y_failed) {
        *out_of_range = true;
        oor->enable_selftest = true;
    } else {
        field_str = sqrtf((data->x * data->x) + (data->y * data->y) + (data->z * data->z));

        /* Check for the self-test threshold and perform self-tests to catch if the sensor is out of range */
        if ((fabsf(data->x) >= st_threshold) || (fabsf(data->y) >= st_threshold) || (fabsf(data->z) >= st_threshold) ||
                (field_str >= st_threshold)) {
            oor->enable_selftest = true;
        } else if (oor->st_counter == 0) { /* If a self-test procedure has started, wait for it to complete */
            oor->enable_selftest = false;
        }

        /* If out of range was previously detected, reduce the threshold to get back in range,
         * effectively preventing hysteresis. Selecting 400uT */
        if (*out_of_range) {
            threshold = BMM350_IN_RANGE_THRESHOLD;
        }

        /* Check if X or Y or Z > the threshold or the magnitude of all 3 is greater */
        if ((fabsf(data->x) >= threshold) || (fabsf(data->y) >= threshold) || (fabsf(data->z) >= threshold) ||
                (field_str >= threshold)) {
            *out_of_range = true;
        } else if (oor->st_counter == 0) { /* If a self-test procedure has started, wait for it to complete */
            *out_of_range = false;
        }
    }
}

/*!
 * @brief This API is used to perform reset sequence in forced mode.
 */
int8_t bmm350_oor_perform_reset_sequence_forced(struct bmm350_oor_params *oor, struct bmm350_dev *dev)
{
    int8_t rslt = 0;
    uint8_t pmu_cmd = 0;

    oor->reset_counter++;

    switch (oor->reset_counter) {
    case 1: /* Trigger the Bit reset fast */
        pmu_cmd = BMM350_PMU_CMD_BR_FAST;
        rslt = bmm350_set_regs(BMM350_REG_PMU_CMD, &pmu_cmd, 1, dev);
        break;

    case 2: /* Trigger Flux Guide reset */
        pmu_cmd = BMM350_PMU_CMD_FGR;
        rslt = bmm350_set_regs(BMM350_REG_PMU_CMD, &pmu_cmd, 1, dev);
        break;

    case 3: /* Flux Guide dummy */
        break;

    default: /* Default acts like the Flux guide reset dummy */
        oor->reset_counter = 0;
        oor->trigger_reset = false;
        break;
    }

    return rslt;
}

/*!
 * @brief This API is used to read out of range in half or full self-test.
 */
int8_t bmm350_oor_read(bool *out_of_range,
                       struct bmm350_mag_temp_data *data,
                       struct bmm350_oor_params *oor,
                       struct bmm350_dev *dev)
{
    int8_t rslt = 0;
    uint8_t pmu_cmd = BMM350_PMU_CMD_SUS;

#ifdef BMM350_OOR_HALF_SELF_TEST
    rslt = trigger_half_selftest(oor, dev);
#else
    rslt = trigger_full_selftest(oor, dev);
#endif

    if (rslt == BMM350_OK) {
        pmu_cmd = BMM350_PMU_CMD_FM_FAST;
        rslt = bmm350_set_regs(BMM350_REG_PMU_CMD, &pmu_cmd, 1, dev);

        if (rslt == BMM350_OK) {
            rslt = bmm350_get_compensated_mag_xyz_temp_data(data, dev);
        }
    }

#ifdef BMM350_OOR_HALF_SELF_TEST
    validate_half_selftest(data, oor);
#else
    validate_full_selftest(data, oor);
#endif

    validate_out_of_range(out_of_range, data, oor);

    oor->last_st_cmd = oor->st_cmd;

    return rslt;
}
