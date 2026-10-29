/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once
#include <sys/stat.h>
#include <time.h>

/* ESP-IDF newlib may omit lstat(); quickjs-libc uses lstat in js_os_stat. */
#ifndef lstat
#define lstat(path, buf) stat((path), (buf))
#endif
