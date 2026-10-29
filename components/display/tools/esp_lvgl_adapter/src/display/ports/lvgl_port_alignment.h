/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * LVGL Port Alignment - Memory alignment utilities for PPA hardware acceleration
 */

#pragma once

/*********************
 *      DEFINES
 *********************/

/**
 * @brief Default PPA cache alignment size
 *
 * This value can be overridden by defining it before including this header.
 */
#ifndef LVGL_PORT_PPA_ALIGNMENT
#define LVGL_PORT_PPA_ALIGNMENT 128
#endif

/**
 * @brief Default maximum pending PPA transactions per client
 *
 * This value can be overridden by defining it before including this header.
 */
#ifndef LVGL_PORT_PPA_MAX_PENDING_TRANS
#define LVGL_PORT_PPA_MAX_PENDING_TRANS 8
#endif

/**
 * @brief Align a value up to the specified alignment
 *
 * @param val Value to align
 * @param align Alignment boundary (must be power of 2)
 * @return Aligned value
 */
#define LVGL_PORT_PPA_ALIGN_UP(val, align) (((val) + ((align) - 1)) & ~((align) - 1))
