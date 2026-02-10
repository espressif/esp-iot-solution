/*
 * SPDX-FileCopyrightText: 2024-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef ESP_LV_LOTTIE_H
#define ESP_LV_LOTTIE_H

#ifdef __cplusplus
extern "C" {
#endif

/*********************
 *      INCLUDES
 *********************/
#include "lvgl.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/*********************
 *      TYPEDEFS
 *********************/
typedef struct _lv_lottie_t lv_lottie_t;

#if LVGL_VERSION_MAJOR >= 9
LV_ATTRIBUTE_EXTERN_DATA extern const lv_obj_class_t lv_lottie_class;
typedef lv_image_dsc_t lv_lottie_src_dsc_t;
typedef lv_draw_buf_t lv_lottie_draw_buf_t;
#else
extern const lv_obj_class_t lv_lottie_class;
typedef lv_img_dsc_t lv_lottie_src_dsc_t;
typedef lv_img_dsc_t lv_lottie_draw_buf_t;
#endif

/**********************
 * GLOBAL PROTOTYPES
 **********************/

/**
 * Create a Lottie animation object.
 * @param parent    pointer to an object, it will be the parent of the new Lottie animation.
 * @return          pointer to the Lottie animation object
 */
lv_obj_t * lv_lottie_create(lv_obj_t * parent);

/**
 * Set the Lottie source.
 * @param obj       pointer to a Lottie animation object
 * @param src       1) pointer to a ::lv_lottie_src_dsc_t descriptor (raw JSON data) or
 *                  2) path to a JSON file (e.g. "S:/dir/anim.json") mounted via LVGL FS
 *
 * Note: For raw data source, the data must remain valid until a new source is set or the object is deleted.
 */
void lv_lottie_set_src(lv_obj_t * obj, const void * src);

/**
 * Set the Lottie source from raw memory data.
 * @param obj       pointer to a Lottie animation object
 * @param data      pointer to raw JSON data in writable memory (ThorVG parses in-situ)
 * @param len       size of the JSON data in bytes
 *
 * Note: The data must remain valid until a new source is set or the object is deleted.
 */
void lv_lottie_set_src_data(lv_obj_t * obj, void * data, size_t len);

/**
 * Allocate and set an internal buffer for the animation.
 * @param obj   pointer to a Lottie animation object
 * @param w     width of the animation
 * @param h     height of the animation
 */
void lv_lottie_set_size(lv_obj_t * obj, int32_t w, int32_t h);

/**
 * Set a user-provided buffer for the animation.
 * @param obj   pointer to a Lottie animation object
 * @param buf   buffer with at least width x height x 4 bytes
 * @param w     width of the animation
 * @param h     height of the animation
 */
void lv_lottie_set_buffer(lv_obj_t * obj, void * buf, int32_t w, int32_t h);

/**
 * Set a draw buffer for the animation.
 * @param obj       pointer to a Lottie animation object
 * @param draw_buf  draw buffer (ARGB8888/ARGB8888_PREMULTIPLIED)
 */
void lv_lottie_set_draw_buf(lv_obj_t * obj, lv_lottie_draw_buf_t * draw_buf);

/**
 * Check if the Lottie was loaded correctly.
 * @param obj pointer to a Lottie animation object
 * @return true if the Lottie is loaded successfully, false otherwise
 */
bool lv_lottie_is_loaded(lv_obj_t * obj);

/**
 * Get the total number of frames in the animation.
 * @param obj pointer to a Lottie animation object
 * @return number of frames, or -1 if not loaded
 */
int32_t lv_lottie_get_total_frames(lv_obj_t * obj);

/**
 * Get the current frame index.
 * @param obj pointer to a Lottie animation object
 * @return current frame index, or -1 if not loaded
 */
int32_t lv_lottie_get_current_frame(lv_obj_t * obj);

/**
 * Set the frame delay in milliseconds.
 * @param obj   pointer to a Lottie animation object
 * @param delay frame delay in milliseconds
 */
void lv_lottie_set_frame_delay(lv_obj_t * obj, uint32_t delay);

/**
 * Get the frame delay in milliseconds.
 * @param obj pointer to a Lottie animation object
 * @return frame delay in milliseconds
 */
uint32_t lv_lottie_get_frame_delay(lv_obj_t * obj);

/**
 * Set the playback frame segment.
 * @param obj   pointer to a Lottie animation object
 * @param begin first frame in the segment
 * @param end   last frame in the segment
 */
void lv_lottie_set_segment(lv_obj_t * obj, uint32_t begin, uint32_t end);

/**
 * Get the current playback frame segment.
 * @param obj   pointer to a Lottie animation object
 * @param begin pointer to store the first frame
 * @param end   pointer to store the last frame
 */
void lv_lottie_get_segment(lv_obj_t * obj, uint32_t * begin, uint32_t * end);

/**
 * Set the loop count for the animation.
 * @param obj   pointer to a Lottie animation object
 * @param count loop count (-1 for infinite loop)
 */
void lv_lottie_set_loop_count(lv_obj_t * obj, int32_t count);

/**
 * Get the loop count for the animation.
 * @param obj pointer to a Lottie animation object
 * @return loop count (-1 for infinite loop)
 */
int32_t lv_lottie_get_loop_count(lv_obj_t * obj);

/**
 * Enable or disable animation looping.
 * @param obj    pointer to a Lottie animation object
 * @param enable true to enable looping, false to disable
 */
void lv_lottie_set_loop_enabled(lv_obj_t * obj, bool enable);

/**
 * Check if animation looping is enabled.
 * @param obj pointer to a Lottie animation object
 * @return true if looping is enabled, false otherwise
 */
bool lv_lottie_get_loop_enabled(lv_obj_t * obj);

/**
 * Start or resume playback.
 * @param obj pointer to a Lottie animation object
 */
void lv_lottie_play(lv_obj_t * obj);

/**
 * Pause playback.
 * @param obj pointer to a Lottie animation object
 */
void lv_lottie_pause(lv_obj_t * obj);

/**
 * Stop playback and reset to the first frame of the segment.
 * @param obj pointer to a Lottie animation object
 */
void lv_lottie_stop(lv_obj_t * obj);

/**
 * Restart playback from the first frame of the segment.
 * @param obj pointer to a Lottie animation object
 */
void lv_lottie_restart(lv_obj_t * obj);

/**
 * Get the LVGL animation which controls the Lottie animation.
 * @param obj pointer to a Lottie animation object
 * @return the LVGL animation handle
 */
lv_anim_t * lv_lottie_get_anim(lv_obj_t * obj);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /*ESP_LV_LOTTIE_H*/
