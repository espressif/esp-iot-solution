/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Max in-flight DATA requests tracked by the sender pipeline. */
#ifndef FT_DATA_PIPE_MAX
#define FT_DATA_PIPE_MAX 8
#endif

typedef struct {
    bool active;
    uint16_t sequence;
    uint32_t block_index;
    uint32_t data_len;
} ft_data_pipe_slot_t;

typedef struct {
    ft_data_pipe_slot_t slots[FT_DATA_PIPE_MAX];
} ft_data_pipe_t;

void ft_data_pipe_reset(ft_data_pipe_t *pipe);
int ft_data_pipe_count(const ft_data_pipe_t *pipe);
int ft_data_pipe_add(ft_data_pipe_t *pipe, uint16_t sequence, uint32_t block_index,
                     uint32_t data_len);
int ft_data_pipe_find_seq(const ft_data_pipe_t *pipe, uint16_t sequence);
void ft_data_pipe_clear(ft_data_pipe_t *pipe, int index);
bool ft_data_pipe_has_block(const ft_data_pipe_t *pipe, uint32_t block_index);

#ifdef __cplusplus
}
#endif
