/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
/*
 *
 */

#pragma once

#include <stdio.h>
#include "esp_heap_caps.h"

// ---- Logging macros ---- //

// ---- Performance logging macros ---- //

// Begin timing. _name must be unique for each measurement within a function
#define LOG_ROUTER_DEBUG_TIMING_BEGIN(_name) \
    int64_t __timing_start_##_name = esp_timer_get_time();

// Finish timing
#define LOG_ROUTER_DEBUG_TIMING_END(_name) \
    uint32_t __timing_duration_us##_name = esp_timer_get_time() - __timing_start_##_name; \

// Finish timing, print function/lineno/result and a user-defined message
#define LOG_ROUTER_DEBUG_TIMING_END_MSG(_name, format, ...) \
    uint32_t __timing_duration_us##_name = esp_timer_get_time() - __timing_start_##_name; \
    log_router_debug_printf("%s(%d): %ums: "format, __FUNCTION__, __LINE__, __timing_duration_us##_name / 1000, ##__VA_ARGS__);

// Resolves to the measurement with the corresponding _name
#define LOG_ROUTER_DEBUG_TIMING_RESULT(_name) __timing_duration_us##_name

// ---- Heap allocation macros ---- //

// capability flag combinations
#define HEAP_CAP_DEFAULT_ANY          (MALLOC_CAP_DEFAULT)
#define HEAP_CAP_DEFAULT_SPIRAM       (MALLOC_CAP_DEFAULT | MALLOC_CAP_SPIRAM)
#define HEAP_CAP_DEFAULT_INTERNAL     (MALLOC_CAP_DEFAULT | MALLOC_CAP_INTERNAL)

// Prefer external PSRAM, to reduce fragmentation of precious SRAM
#define MALLOC_PREFER_EXTERNAL(size) heap_caps_malloc_prefer((size), 2,( HEAP_CAP_DEFAULT_SPIRAM), HEAP_CAP_DEFAULT_ANY)
#define CALLOC_PREFER_EXTERNAL(n, size) heap_caps_calloc_prefer((n), (size), 2,( HEAP_CAP_DEFAULT_SPIRAM), HEAP_CAP_DEFAULT_ANY)
#define REALLOC_PREFER_EXTERNAL(ptr, size) heap_caps_realloc_prefer((ptr), (size), 2, HEAP_CAP_DEFAULT_SPIRAM, HEAP_CAP_DEFAULT_ANY)

// Use internal memory when performance is desirable and fragmentation can be minimised
#define MALLOC_PREFER_PERFORMANCE(size) heap_caps_malloc_prefer((size), 2, HEAP_CAP_DEFAULT_INTERNAL, HEAP_CAP_DEFAULT_ANY)
#define CALLOC_PREFER_PERFORMANCE(n, size) heap_caps_calloc_prefer((n), (size), 2, HEAP_CAP_DEFAULT_INTERNAL, HEAP_CAP_DEFAULT_ANY)
#define REALLOC_PREFER_PERFORMANCE(ptr, size) heap_caps_realloc_prefer((ptr), (size), 2, HEAP_CAP_DEFAULT_INTERNAL, HEAP_CAP_DEFAULT_ANY)

// Fails when internal memory not available, allowing the caller to try again with a smaller buffer
#define MALLOC_PERFORMANCE(size) heap_caps_malloc((size), HEAP_CAP_DEFAULT_INTERNAL)
#define CALLOC_PERFORMANCE(n, size) heap_caps_calloc((n), (size), HEAP_CAP_DEFAULT_INTERNAL)
#define REALLOC_PERFORMANCE(ptr, size) heap_caps_realloc((ptr), (size), 2, HEAP_CAP_DEFAULT_INTERNAL)
