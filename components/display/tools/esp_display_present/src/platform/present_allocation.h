/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once
#include "platform/present_os.h"
#include <stdbool.h>
#include <stddef.h>
#include <assert.h>
/* Caller serialized. These references track why pixels cannot be written;
 * they are independent of frame receipts and content versions. In particular,
 * confirming an FB frame does not release its current scanout reference. */
typedef enum {
    PRESENT_REF_BASELINE,
    PRESENT_REF_COPY,
    PRESENT_REF_TRANSFER,
    PRESENT_REF_SCANOUT,
    PRESENT_REF_COUNT,
} present_ref_kind_t;
typedef struct {
    size_t readers[PRESENT_REF_COUNT];
    bool writer;
} present_allocation_t;
static inline bool present_allocation_write(present_allocation_t *allocation, bool retained, bool tearing)
{
    if (allocation->writer || allocation->readers[PRESENT_REF_COPY] || allocation->readers[PRESENT_REF_TRANSFER] ||
            (!retained && allocation->readers[PRESENT_REF_BASELINE]) || (!tearing && allocation->readers[PRESENT_REF_SCANOUT])) {
        return false;
    }
    allocation->writer = true;
    return true;
}
static inline void present_allocation_claim(present_allocation_t *allocation)
{
    bool claimed = present_allocation_write(allocation, false, false);
    assert(claimed);
    (void)claimed;
}
static inline void present_allocation_written(present_allocation_t *allocation)
{
    assert(allocation->writer);
    allocation->writer = false;
}
static inline void present_allocation_hold(present_allocation_t *allocation, present_ref_kind_t kind)
{
    assert(!allocation->writer && allocation->readers[kind] != (size_t) -1);
    allocation->readers[kind]++;
}
static inline void PRESENT_ISR_ATTR present_allocation_drop(present_allocation_t *allocation, present_ref_kind_t kind)
{
    assert(allocation->readers[kind]);
    allocation->readers[kind]--;
}
