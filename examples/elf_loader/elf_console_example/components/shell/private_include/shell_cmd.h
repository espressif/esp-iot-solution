/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdio.h>
#include "argtable3/argtable3.h"
#include "esp_console.h"

/**
 * @brief Validates command-line arguments using argtable3 parser
 *
 * The SHELL_CMD_CHECK macro validates command-line arguments by calling arg_parse()
 * with the provided argument table instance. This macro has implicit dependencies:
 * - Requires argc and argv to be present in the caller's scope
 * - On parse failure, calls arg_print_errors() with stderr, ins.end, and argv[0]
 * - Returns -1 from the enclosing function if parsing fails
 *
 * @param ins Argument table instance (argtable3 structure) to validate against
 *
 * @note This macro must be used within a function that has argc and argv parameters
 *       in scope. On parse failure, errors are printed to stderr and the function
 *       returns -1.
 */
#define SHELL_CMD_CHECK(ins)                            \
    do {                                                \
        if (arg_parse(argc, argv, (void **)&ins)){      \
            arg_print_errors(stderr, ins.end, argv[0]); \
            return -1;                                  \
        }                                               \
    } while(0)

void shell_register_cmd_ls(void);
void shell_register_cmd_free(void);
void shell_register_cmd_exec(void);
void shell_register_cmd_list(void);
void shell_register_cmd_mod_load(void);
void shell_register_cmd_mod_unload(void);
