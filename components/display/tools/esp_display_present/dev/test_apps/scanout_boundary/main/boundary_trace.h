/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#ifdef BOUNDARY_HOST
#define BOUNDARY_IRAM
#else
#include "esp_attr.h"
#define BOUNDARY_IRAM IRAM_ATTR
#endif
#define BOUNDARY_TRACE_CAPACITY 8192
/* Test-only hooks. No allocation, logging or waiting for task work in emit. */
typedef enum {
    BT_REGISTER = 1, BT_READ_BEGIN, BT_READ_END, BT_SELECT, BT_BOUNCE_EOF,
    BT_FRAME_DONE, BT_VSYNC, BT_LINK_SWITCH, BT_RESTART, BT_UNDERRUN,
    BT_SUBMIT, BT_ARM, BT_CONFIRM, BT_RELEASE, BT_WRITE_BEGIN, BT_WRITE_END,
    BT_CHAIN, BT_RAW_REQUEST, BT_STOP, BT_INJECT, BT_CALLBACK
} boundary_kind_t;
typedef struct {
    uint64_t time_us;
    uintptr_t buffer;
    uint32_t sequence, detail;
    uint16_t kind, isr;
} boundary_record_t;
void boundary_reset(void); /* Only with all previous producers stopped. */
void BOUNDARY_IRAM boundary_emit(unsigned kind, uintptr_t buffer, uint32_t detail);
void boundary_freeze(void);
const boundary_record_t *boundary_records(size_t *count, uint32_t *dropped);
void boundary_dump(const char *name, const char *scope);
