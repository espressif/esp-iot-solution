/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
/**
 * @file lv_eaf.h
 * @brief EAF (Emote Animation Format) player for LVGL
 */

#ifndef LV_EAF_H
#define LV_EAF_H

#ifdef __cplusplus
extern "C" {
#endif

/*********************
 *      INCLUDES
 *********************/

#include "lvgl.h"
#include <stdbool.h>
#include <stdint.h>

/*********************
 *      DEFINES
 *********************/

/**********************
 *      TYPEDEFS
 **********************/

typedef struct _lv_eaf_t lv_eaf_t;

#if LVGL_VERSION_MAJOR >= 9
LV_ATTRIBUTE_EXTERN_DATA extern const lv_obj_class_t lv_eaf_class;
#else
extern const lv_obj_class_t lv_eaf_class;
#endif

/**********************
 * GLOBAL PROTOTYPES
 **********************/

/**
 * Create an EAF animation object
 * @param parent    pointer to an object, it will be the parent of the new EAF animation.
 * @return          pointer to the EAF animation object
 */
lv_obj_t * lv_eaf_create(lv_obj_t * parent);

/**
 * Set the EAF data to display on the object
 * @param obj       pointer to an EAF animation object
 * @param src       1) pointer to an ::lv_image_dsc_t descriptor (which contains EAF raw data) or
 *                  2) path to an EAF file (e.g. "S:/dir/anim.eaf") mounted via LVGL FS
 */
void lv_eaf_set_src(lv_obj_t * obj, const void * src);

/**
 * Restart an EAF animation.
 * @param obj pointer to an EAF animation object
 */
void lv_eaf_restart(lv_obj_t * obj);

/**
 * Pause an EAF animation.
 * @param obj pointer to an EAF animation object
 */
void lv_eaf_pause(lv_obj_t * obj);

/**
 * Resume an EAF animation.
 * @param obj pointer to an EAF animation object
 */
void lv_eaf_resume(lv_obj_t * obj);

/**
 * Check if the EAF was loaded correctly.
 * @param obj pointer to an EAF animation object
 * @return true if the EAF is loaded successfully, false otherwise
 */
bool lv_eaf_is_loaded(lv_obj_t * obj);

/**
 * Get the loop count for the EAF animation.
 * @param obj pointer to an EAF animation object
 * @return the loop count (-1 for infinite loop)
 */
int32_t lv_eaf_get_loop_count(lv_obj_t * obj);

/**
 * Set the loop count for the EAF animation.
 * @param obj   pointer to an EAF animation object
 * @param count the loop count to set (-1 for infinite loop)
 */
void lv_eaf_set_loop_count(lv_obj_t * obj, int32_t count);

/**
 * Get the total number of frames in the EAF animation.
 * @param obj pointer to an EAF animation object
 * @return number of frames, or -1 if not loaded
 */
int32_t lv_eaf_get_total_frames(lv_obj_t * obj);

/**
 * Get the current frame index.
 * @param obj pointer to an EAF animation object
 * @return current frame index, or -1 if not loaded
 */
int32_t lv_eaf_get_current_frame(lv_obj_t * obj);

/**
 * Set the frame delay in milliseconds.
 * @param obj   pointer to an EAF animation object
 * @param delay frame delay in milliseconds
 */
void lv_eaf_set_frame_delay(lv_obj_t * obj, uint32_t delay);

/**
 * Get the frame delay in milliseconds.
 * @param obj pointer to an EAF animation object
 * @return frame delay in milliseconds
 */
uint32_t lv_eaf_get_frame_delay(lv_obj_t * obj);

/**
 * Enable or disable animation looping.
 * @param obj    pointer to an EAF animation object
 * @param enable true to enable looping, false to disable
 */
void lv_eaf_set_loop_enabled(lv_obj_t * obj, bool enable);

/**
 * Check if animation looping is enabled.
 * @param obj pointer to an EAF animation object
 * @return true if looping is enabled, false otherwise
 */
bool lv_eaf_get_loop_enabled(lv_obj_t * obj);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /*LV_EAF_H*/
