/*
 * SPDX-FileCopyrightText: 2022-2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */


#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

/**
 * @brief Initialize the underlying implementation for cache compensated timer. This might involve
 * setting up architecture-specific event counters and or allocating interrupts that handle events for those counters.
 * @return
 *  - ESP_OK: Success
 *  - ESP_ERR_INVALID_STATE: The timer has already been started previously.
 *  - Others: Fail
 */
esp_err_t ccomp_timer_impl_init(void);

/**
 * @brief Deinitialize the underlying implementation for cache compensated timer. This should restore
 * the state of the program to before ccomp_timer_impl_init.
 * @return
 *  - ESP_OK: Success
 *  - ESP_ERR_INVALID_STATE: The timer has already been started previously.
 *  - Others: Fail
 */
esp_err_t ccomp_timer_impl_deinit(void);

/**
 * @brief Make the underlying implementation start keeping time.
 *
 * @return
 *  - ESP_OK: Success
 *  - Others: Fail
 */
esp_err_t ccomp_timer_impl_start(void);

/**
 * @brief Make the underlying implementation stop keeping time.
 *
 * @return
 *  - ESP_OK: Success
 *  - Others: Fail
 */
esp_err_t ccomp_timer_impl_stop(void);

/**
 * @brief Reset the timer to its initial state.
 *
 * @return
 *  - ESP_OK: Success
 *  - Others: Fail
 */
esp_err_t ccomp_timer_impl_reset(void);

/**
 * @brief Get the elapsed time kept track of by the underlying implementation in microseconds.
 *
 * @return The elapsed time in microseconds. Set to -1 if the operation is unsuccessful.
 */
int64_t ccomp_timer_impl_get_time(void);

/**
 * @brief Obtain an internal critical section used in the implementation. Should be treated
 * as a spinlock.
 */
void ccomp_timer_impl_lock(void);

/**
 * @brief Start the performance timer on the current core.
 */
void ccomp_timer_impl_unlock(void);

/**
 * @brief Check if timer has been initialized.
 *
 * @return
 *  - true: the timer has been initialized using ccomp_timer_impl_init
 *  - false: the timer has not been initialized, or ccomp_timer_impl_deinit has been called recently
 */
bool ccomp_timer_impl_is_init(void);

/**
 * @brief Check if timer is keeping time.
 *
 * @return
 *  - true: the timer is keeping track of elapsed time from ccomp_timer_impl_start
 *  - false: the timer is not keeping track of elapsed time since ccomp_timer_impl_start has not yet been called or ccomp_timer_impl_stop has been called recently
 */
bool ccomp_timer_impl_is_active(void);
