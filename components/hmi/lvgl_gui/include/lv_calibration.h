/**
 * @file lv_calibration.h
 *
 */

#ifndef LV_CALIBRATION_H
#define LV_CALIBRATION_H

#ifdef __cplusplus
extern "C" {
#endif

/*********************
 *      INCLUDES
 *********************/
#include "iot_lvgl.h"

/*********************
 *      DEFINES
 *********************/

/**********************
 *      TYPEDEFS
 **********************/

/**********************
 * GLOBAL PROTOTYPES
 **********************/

/**
 * Calibration Mouse
 * @param th pointer to a lv_indev_drv_t
 */
bool lvgl_calibrate_mouse(lv_indev_drv_t indev_drv);

/**
 * Transform Mouse data
 * @param th pointer to a lv_point_t
 */
bool lvgl_calibration_transform(lv_point_t *data);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* LV_CALIBRATION_H */