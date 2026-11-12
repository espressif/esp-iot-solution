/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once
#include "esp_err.h"
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

typedef void *ppa_client_handle_t;
typedef enum { PPA_SRM_COLOR_MODE_RGB565, PPA_SRM_COLOR_MODE_RGB888 } ppa_srm_color_mode_t;
typedef enum { PPA_SRM_ROTATION_ANGLE_0, PPA_SRM_ROTATION_ANGLE_90, PPA_SRM_ROTATION_ANGLE_180, PPA_SRM_ROTATION_ANGLE_270 } ppa_srm_rotation_angle_t;
#define PPA_OPERATION_SRM 0
#define PPA_TRANS_MODE_BLOCKING 0
typedef struct {
    int oper_type;
    unsigned max_pending_trans_num;
} ppa_client_config_t;
typedef struct {
    const void *buffer;
    uint32_t pic_w, pic_h, block_w, block_h, block_offset_x, block_offset_y;
    ppa_srm_color_mode_t srm_cm;
} ppa_in_pic_blk_config_t;
typedef struct {
    void *buffer;
    size_t buffer_size;
    uint32_t pic_w, pic_h, block_offset_x, block_offset_y;
    ppa_srm_color_mode_t srm_cm;
} ppa_out_pic_blk_config_t;
typedef struct {
    ppa_in_pic_blk_config_t in;
    ppa_out_pic_blk_config_t out;
    ppa_srm_rotation_angle_t rotation_angle;
    float scale_x, scale_y;
    bool byte_swap;
    int mode;
} ppa_srm_oper_config_t;
esp_err_t ppa_register_client(const ppa_client_config_t *, ppa_client_handle_t *);
esp_err_t ppa_unregister_client(ppa_client_handle_t);
esp_err_t ppa_do_scale_rotate_mirror(ppa_client_handle_t, const ppa_srm_oper_config_t *);
