/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once
#include "esp_err.h"

/**
 * @brief Initializes the console for the lightbulb example.
 *
 * This function sets up the console interface for the lightbulb example,
 * preparing it for receiving commands and interacting with the user. It must be
 * called before any console commands are processed.
 *
 * @return
 *    - ESP_OK: Success in initializing the console.
 *    - other: Specific error code indicating what went wrong during initialization.
 */
esp_err_t lightbulb_example_console_init(void);

/**
 * @brief Deinitializes the console for the lightbulb example.
 *
 * This function tears down the console interface for the lightbulb example,
 * freeing any resources that were allocated during initialization. It should be
 * called to gracefully shut down the console interface when it is no longer needed.
 *
 * @return
 *    - ESP_OK: Success in deinitializing the console.
 *    - other: Specific error code indicating what went wrong during deinitialization.
 */
esp_err_t lightbulb_example_console_deinit(void);

/**
 * @brief Gets the current status of the console for the lightbulb example.
 *
 * This function checks whether the console interface for the lightbulb example is
 * currently active and ready to process commands. It can be used to determine if
 * the console needs to be initialized or if it's already up and running.
 *
 * @return
 *    - true: Console is currently active.
 *    - false: Console is not active.
 */
bool lightbulb_example_get_console_status(void);
