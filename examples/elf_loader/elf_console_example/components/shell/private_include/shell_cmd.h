/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "argtable3/argtable3.h"
#include "esp_console.h"

#define SHELL_CMD_CHECK(ins)                            \
    if (arg_parse(argc, argv, (void **)&ins)){          \
        arg_print_errors(stderr, ins.end, argv[0]);     \
        return -1;                                      \
    }

void shell_regitser_cmd_ls(void);
void shell_regitser_cmd_free(void);
void shell_regitser_cmd_exec();
void shell_regitser_cmd_list();
void shell_regitser_cmd_insmod();
void shell_regitser_cmd_rmmod();
