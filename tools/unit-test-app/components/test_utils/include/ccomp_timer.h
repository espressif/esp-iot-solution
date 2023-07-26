/*
 * SPDX-FileCopyrightText: 2022-2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */


#pragma once

#include <stdint.h>

#include "esp_err.h"

/**
 * @brief Start the timer on the current core.
 *
 * @return
 *  - ESP_OK: Success
 *  - ESP_ERR_INVALID_STATE: The timer has already been started previously.
 *  - Others: Fail
 */
esp_err_t ccomp_timer_start(void);

/**
 * @brief Stop the timer on the current core.
 * 
 * @note Returns -1 if an error has occured and stopping the timer failed.
 *
 * @return The time elapsed from the last ccomp_timer_start call on the current
 *         core.
 */
int64_t ccomp_timer_stop(void);

/**
 * Return the current timer value on the current core without stopping the timer.
 *
 * @note Returns -1 if an error has occured and stopping the timer failed.
 * 
 * @note If called while timer is active i.e. between ccomp_timer_start and ccomp_timer_stop,
 * this function returns the elapsed time from ccomp_timer_start. Once ccomp_timer_stop 
 * has been called, the timer becomes inactive and stops keeping time. As a result, if this function gets
 * called after esp_cccomp_timer_stop, this function will return the same value as when the timer was stopped.
 *
 * @return The elapsed time from the last ccomp_timer_start call on the current 
 *          core.
 */
int64_t ccomp_timer_get_time(void);
