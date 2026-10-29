/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stddef.h>
#include "sdkconfig.h"
#include "esp_err.h"
#include "sdkconfig.h"

#define AT_RESULT_CODE_SUCCESS "OK"              /*!< Acknowledges execution of a command */
#define AT_RESULT_CODE_ERROR "ERROR"             /*!< Command not recognized, command line maximum length exceeded, parameter value invalid */
#define AT_RESULT_CODE_CONNECT "CONNECT"         /*!< A connection has been established */
#define AT_RESULT_CODE_NO_CARRIER "NO CARRIER"   /*!< Connection termincated or establish a connection failed */
#define AT_RESULT_CODE_BUSY "BUSY"               /*!< Engaged signal detected */
#define AT_RESULT_CODE_NO_ANSWER "NO ANSWER"     /*!< Wait for quiet answer */

/**
 * @brief Specific Timeout Constraint, Unit: millisecond
 *
 */
#define MODEM_COMMAND_TIMEOUT_DEFAULT     CONFIG_MODEM_COMMAND_TIMEOUT_DEFAULT
#define MODEM_COMMAND_TIMEOUT_MODE_CHANGE CONFIG_MODEM_COMMAND_TIMEOUT_MODE_CHANGE

typedef struct {
    const char *command;
    char *string;
    size_t len;
} at_common_string_t;

#ifdef __cplusplus
}
#endif
