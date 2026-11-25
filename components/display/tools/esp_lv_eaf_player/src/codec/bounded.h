/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
/* SPDX-License-Identifier: Apache-2.0 */
#pragma once
/* Frozen Bounded block opcodes; see docs/NAMING.md. */
typedef enum {
    BOUNDED_BLOCK_SKIP = 0,
    BOUNDED_BLOCK_SOLID = 1,
    BOUNDED_BLOCK_PALETTE2 = 2,
    BOUNDED_BLOCK_PALETTE4 = 3,
    BOUNDED_BLOCK_LITERAL = 4,
} eaf_bounded_block_t;
