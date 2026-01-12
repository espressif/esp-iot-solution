/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
/**
 * @file lv_eaf_private.h
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
#include "esp_eaf_dec.h"

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
    esp_eaf_format_handle_t eaf_handle;    /*!< EAF parser handle */
    esp_eaf_header_t eaf_header;           /*!< Current frame header */
    uint8_t *frame_buffer;                 /*!< Buffer for decoded frame data */
    uint32_t frame_buffer_size;            /*!< Size of frame buffer */
    uint32_t color_buffer_size;            /*!< Size of RGB565 buffer */
    bool use_alpha;                        /*!< Whether RGB565A8 format is used */
    uint8_t *file_data;                    /*!< Buffer for file data (when loaded from file) */
    int current_frame;                     /*!< Current frame index */
    int total_frames;                      /*!< Total number of frames */
    uint32_t frame_delay;                  /*!< Frame delay in milliseconds */
    bool loop_enabled;                     /*!< Whether looping is enabled */
    int loop_count;                        /*!< Loop count (-1 for infinite) */
    lv_timer_t * timer;                    /*!< Timer for animation */
    lv_eaf_image_dsc_t imgdsc;             /*!< Image descriptor for LVGL */
    uint32_t last_call;                    /*!< Last timer call timestamp */
};

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif /*LV_EAF_PRIVATE_H*/
