/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once
/* Allocation policy, NOT a substitute for the platform cache-line size. */
enum {
    PRESENT_BUFFER_ALIGNMENT = 64,
    PRESENT_MAX_FRAMES = 32,
    PRESENT_MAX_DAMAGE_AREAS = 32,
    PRESENT_MAX_TILE_BUFFERS = 16,
    PRESENT_MAX_COPY_JOBS = 64,
};
