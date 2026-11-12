/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
/* Physical coordinates. Zero alignment fields mean 1. No implicit edge exception.
 * Device-specific command restrictions must augment the adapter's transport caps. */
typedef struct {
    uint32_t x_alignment, y_alignment;
    uint32_t w_align, h_align;
    size_t base_alignment, stride_align;
    bool tight_stride;
    bool full_only;
    bool ext_bufs_required; /* Exact driver pool checked by adapter. */
    bool internal_dma;
    bool ext_mem;
    size_t max_inflight; /* 0: not specified by this descriptor. */
    size_t ext_len_align; /* External DMA byte count; 0 means 1. */
} esp_display_present_output_caps_t;
#ifdef __cplusplus
}
#endif
