/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"
#include "lvgl.h"
#include "hw_init.h"
typedef enum {B_NONE, B_DOUBLE_FULL, B_TRIPLE_FULL, B_DOUBLE_DIRECT, B_DOUBLE_PARTIAL, B_TRIPLE_PARTIAL, B_AUTO, B_TE} bench_mode_t;
typedef struct {
    const char *name;
    bench_mode_t mode;
    unsigned rotation, tiles, compose;
    bool psram, adapter_supported;
    const char *skip_reason;
    unsigned adapter_te_pipeline; /* 0=sync, 1=async full, 2=async bounce; not a buffer count. */
} bench_case_t;
esp_err_t bench_init(void);
esp_err_t bench_open(const bench_case_t *, lv_display_t **);
esp_err_t bench_lock(void);
void bench_unlock(void);
void bench_pump(void);
esp_err_t bench_close(void);
uint32_t bench_frame_count(void);
void bench_meter_reset(void);
void bench_meter_report(const char *);

#ifdef BENCH_PROFILE
void bench_profile_reset(void);
void bench_profile_report(const char *label);
#else
static inline void bench_profile_reset(void) {}
static inline void bench_profile_report(const char *label)
{
    (void)label;
}
#endif

void bench_buffer_audit_start(lv_display_t *, const bench_case_t *);
void bench_buffer_audit_stop(lv_display_t *);
void bench_buffer_audit_report(void);
