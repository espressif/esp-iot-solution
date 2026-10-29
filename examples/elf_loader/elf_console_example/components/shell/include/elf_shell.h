/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize the ELF loader shell component
 *
 * This function initializes the shell interface for the ELF loader console.
 * Should be called once during system initialization.
 */
void shell_init(void);

#ifdef __cplusplus
}
#endif
