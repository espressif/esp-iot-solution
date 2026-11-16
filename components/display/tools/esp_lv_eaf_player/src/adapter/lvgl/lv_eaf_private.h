/*
 * SPDX-FileCopyrightText: 2025-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
/**
 * @file lv_eaf_private.h
 * @brief Private state for the optional LVGL adapter
 *
 */

#ifndef LV_EAF_PRIVATE_H
#define LV_EAF_PRIVATE_H

#ifdef __cplusplus
extern "C" {
#endif

/*********************
 *      INCLUDES
 *********************/

#include "lvgl.h"
#include "lv_eaf.h"
#include "esp_eaf_player.h"

#if LVGL_VERSION_MAJOR < 8 || LVGL_VERSION_MAJOR >= 10
#error "EAF adapter requires LVGL 8 or 9"
#endif

#if LVGL_VERSION_MAJOR >= 9
#include "src/widgets/image/lv_image_private.h"
typedef lv_image_t lv_eaf_widget_t;
typedef lv_image_dsc_t lv_eaf_image_dsc_t;
#define LV_EAF_BASE_CLASS (&lv_image_class)
#else
/* LVGL v8 compatibility */
#include "src/widgets/lv_img.h"
typedef lv_img_t lv_eaf_widget_t;
typedef lv_img_dsc_t lv_eaf_image_dsc_t;
#define LV_EAF_BASE_CLASS (&lv_img_class)
#endif

struct _lv_eaf_t {
    lv_eaf_widget_t img;
    esp_eaf_player_t *player;              /*!< GUI-independent EAF decoder/player. */
    int32_t color_key;                     /*!< 1-bit RGB565 color key (-1 if disabled) */
    uint8_t *file_data;                    /*!< Buffer for file data (when loaded from file) */
    int current_frame;                     /*!< Current frame index */
    int total_frames;                      /*!< Total number of frames */
    uint32_t frame_delay;                  /*!< Frame delay in milliseconds */
    bool loop_enabled;                     /*!< Whether looping is enabled */
    bool frame_delay_explicit;             /*!< Preserve explicitly configured delay, including zero. */
    int loops_remaining;                  /*!< Runtime repeat count, reset on restart/load. */
    int loop_count;                        /*!< Loop count (-1 for infinite) */
    lv_timer_t * timer;                    /*!< Timer for animation */
    lv_eaf_image_dsc_t imgdsc;             /*!< Image descriptor for LVGL */
    uint32_t last_call;                    /*!< Last timer call timestamp */
};

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif /*LV_EAF_PRIVATE_H*/
