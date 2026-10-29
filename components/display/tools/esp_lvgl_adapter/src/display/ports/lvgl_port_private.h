/*
 * SPDX-FileCopyrightText: 2025-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "lvgl.h"

#ifndef LVGL_VERSION_MAJOR
#ifdef LV_VERSION_MAJOR
#define LVGL_VERSION_MAJOR LV_VERSION_MAJOR
#endif
#endif

/* LVGL 9.6 installs private headers below include/lvgl_private. Older LVGL 9
 * releases expose the same header at the component root. Keep this adapter
 * header version-neutral so the public LVGL 8 path does not include private
 * LVGL 9 declarations. */
#if LVGL_VERSION_MAJOR >= 9
#if defined(__has_include)
#if __has_include("lvgl_private/lvgl_private.h")
#include "lvgl_private/lvgl_private.h"
#elif __has_include("lvgl_private.h")
#include "lvgl_private.h"
#else
#error "LVGL private header is not available"
#endif
#else
#include "lvgl_private.h"
#endif
#endif
