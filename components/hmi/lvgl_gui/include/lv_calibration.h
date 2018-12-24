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
 * @brief Calibration Mouse
 * 
 * @param indev_drv pointer to a lv_indev_drv_t
 * @param recalibrate Whether to recalibrate， true: recalibrate，false not
 * 
 * @return Whether the calibration is successful
 */
bool lvgl_calibrate_mouse(lv_indev_drv_t indev_drv, bool recalibrate);

/**
 * @brief Transform Mouse data
 * 
 * @param data pointer to a lv_point_t
 * 
 * @return Whether the transform is successful
 */
bool lvgl_calibration_transform(lv_point_t *data);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* LV_CALIBRATION_H */