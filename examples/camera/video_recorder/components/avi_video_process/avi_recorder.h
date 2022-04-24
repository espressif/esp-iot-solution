/*
 * SPDX-FileCopyrightText: 2021-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef _AVI_RECORDER_H_
#define _AVI_RECORDER_H_

#include "esp_camera.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief  Start avi video recorder
 *
 * @param[in] fname video file name
 * @param[in] get_frame function to get frames
 * @param[in] return_frame function to free the frame
 * @param[in] image_width the width of the image width
 * @param[in] image_high the width of the image high
 * @param[in] rec_time the time the video was recorded
 * @param[in] block if true, block here to wait video recoredr done
 * 
 * @return
 *     - ESP_OK   Success
 *     - ESP_ERR_INVALID_ARG Parameter error
 *     - ESP_FAIL Driver installation error
*/
esp_err_t avi_recorder_start(const char *fname,
                        int (*get_frame)(void **buf, size_t *len),
                        int (*return_frame)(void *buf),
                        uint16_t image_width,
                        uint16_t image_high,
                        uint32_t rec_time,
                        bool block);

/**
  * @brief  Stop avi recorder
  *
  */
void avi_recorder_stop(void);

#ifdef __cplusplus
}
#endif

#endif