/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief ESP LVGL port private
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Rotation configuration
 */
typedef enum {
    LVGL_PORT_DISP_TYPE_OTHER,
    LVGL_PORT_DISP_TYPE_DSI,
    LVGL_PORT_DISP_TYPE_RGB,
} lvgl_port_disp_type_t;

/**
 * @brief Rotation configuration
 */
typedef struct {
    unsigned int avoid_tearing: 1;    /*!< Use internal RGB buffers as a LVGL draw buffers to avoid tearing effect */
} lvgl_port_disp_priv_cfg_t;

/**
 * @brief Notify LVGL task
 *
 * @note It is called from RGB vsync ready
 *
 * @param value     notification value
 * @return
 *      - true, whether a high priority task has been waken up by this function
 */
bool lvgl_port_task_notify(uint32_t value);

#ifdef __cplusplus
}
#endif
