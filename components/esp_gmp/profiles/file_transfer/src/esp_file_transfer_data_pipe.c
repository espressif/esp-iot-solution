/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "esp_file_transfer_data_pipe.h"

#include <string.h>

void ft_data_pipe_reset(ft_data_pipe_t *pipe)
{
    if (pipe) {
        memset(pipe, 0, sizeof(*pipe));
    }
}

int ft_data_pipe_count(const ft_data_pipe_t *pipe)
{
    if (!pipe) {
        return 0;
    }
    int count = 0;
    for (int i = 0; i < FT_DATA_PIPE_MAX; i++) {
        if (pipe->slots[i].active) {
            count++;
        }
    }
    return count;
}

int ft_data_pipe_add(ft_data_pipe_t *pipe, uint16_t sequence, uint32_t block_index,
                     uint32_t data_len)
{
    if (!pipe || data_len == 0) {
        return -1;
    }
    for (int i = 0; i < FT_DATA_PIPE_MAX; i++) {
        if (!pipe->slots[i].active) {
            pipe->slots[i].active = true;
            pipe->slots[i].sequence = sequence;
            pipe->slots[i].block_index = block_index;
            pipe->slots[i].data_len = data_len;
            return i;
        }
    }
    return -1;
}

int ft_data_pipe_find_seq(const ft_data_pipe_t *pipe, uint16_t sequence)
{
    if (!pipe) {
        return -1;
    }
    for (int i = 0; i < FT_DATA_PIPE_MAX; i++) {
        if (pipe->slots[i].active && pipe->slots[i].sequence == sequence) {
            return i;
        }
    }
    return -1;
}

void ft_data_pipe_clear(ft_data_pipe_t *pipe, int index)
{
    if (!pipe || index < 0 || index >= FT_DATA_PIPE_MAX) {
        return;
    }
    memset(&pipe->slots[index], 0, sizeof(pipe->slots[index]));
}

bool ft_data_pipe_has_block(const ft_data_pipe_t *pipe, uint32_t block_index)
{
    if (!pipe) {
        return false;
    }
    for (int i = 0; i < FT_DATA_PIPE_MAX; i++) {
        if (pipe->slots[i].active && pipe->slots[i].block_index == block_index) {
            return true;
        }
    }
    return false;
}
