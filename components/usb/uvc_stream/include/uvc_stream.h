// Copyright 2016-2021 Espressif Systems (Shanghai) PTE LTD
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at

//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#pragma once

#include <stdio.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include "usb/usb_types_stack.h"
#include "libuvc_def.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief users need to get params from camera descriptors,
 * eg. run `lsusb -v` in linux 
 */
typedef struct uvc_config{
    usb_speed_t dev_speed; /*!< USB Device speed, Fix to USB_SPEED_FULL now */
    uint16_t configuration; /*!< bConfigurationValue */
    uint8_t format_index; /*!< bFormatIndex */
    uint16_t frame_width;  /*!< wWidth */
    uint16_t frame_height;  /*!< wHeight */
    uint8_t frame_index;  /*!< bFrameIndex */
    uint32_t frame_interval;  /*!< dwFrameInterval */
    uint16_t interface;  /*!< bInterfaceNumber */
    uint16_t interface_alt;  /*!< bAlternateSetting, ep MPS must =< 512 */
    uint8_t isoc_ep_addr;  /*!< bEndpointAddress */
    uint32_t isoc_ep_mps;  /*!< MPS size of bAlternateSetting */
    uint32_t xfer_buffer_size;  /*!< transfer buffer size */
    uint8_t *xfer_buffer_a;  /*!< buffer for usb payload */
    uint8_t *xfer_buffer_b;  /*!< buffer for usb payload */
    uint32_t frame_buffer_size;  /*!< frame buffer size */
    uint8_t *frame_buffer;  /*!< buffer for one frame */
} uvc_config_t;

/**
 * @brief Pre-config UVC driver with params from known USB Camera Descriptor
 * 
 * @param config config struct described in uvc_config_t
 * @return esp_err_t 
 *         ESP_ERR_INVALID_ARG Args not supported
 *         ESP_OK Config driver suceed
 */
esp_err_t uvc_streaming_config(const uvc_config_t *config);

/**
 * @brief Start camera IN streaming with pre-configs, uvc driver will create multi-tasks internal
 * to handle usb data from different pipes, and run user's callback after new frame ready.
 * only one streaming supported now.
 * 
 * @param cb callback function to handle incoming assembled UVC frame
 * @param user_ptr user pointer used in callback 
 * @return
 *         ESP_ERR_INVALID_STATE streaming not configured, or streaming running 
 *         ESP_ERR_INVALID_ARG args not supported
 *         ESP_FAIL start failed
 *         ESP_OK start suceed
 */
esp_err_t uvc_streaming_start(uvc_frame_callback_t *cb, void *user_ptr);

/**
 * @brief Suspend current IN streaming
 * 
 * @return
 *         ESP_ERR_INVALID_STATE not inited
 *         ESP_FAIL suspend failed
 *         ESP_OK suspend suceed
 *         ESP_ERR_TIMEOUT suspend wait timeout
 */
esp_err_t uvc_streaming_suspend(void);

/**
 * @brief Resume current IN streaming
 * 
 * @return
 *         ESP_ERR_INVALID_STATE not inited
 *         ESP_FAIL resume failed
 *         ESP_OK resume suceed
 *         ESP_ERR_TIMEOUT resume wait timeout
 */
esp_err_t uvc_streaming_resume(void);

/**
 * @brief Stop current IN streaming, internal tasks will be delete, related resourses will be free
 * 
 * @return 
 *         ESP_ERR_INVALID_STATE not inited
 *         ESP_OK stop suceed
 *         ESP_ERR_TIMEOUT stop wait timeout
 */
esp_err_t uvc_streaming_stop(void);

/**
 * @brief start a simulate streaming from internal flash,
 * this api can be used to test display system without camera input,
 * this api is disabled by default to save flash size.
 *
 * @param cb function to handle incoming assembled simulate frame
 * @param user_ptr user pointer used in callback 
 * @return 
 *         ESP_ERR_NOT_SUPPORTED simulate not enable
 *         ESP_ERR_INVALID_ARG invalid input args
 *         ESP_ERR_NO_MEM no enough memory
 *         ESP_OK suceed
 */
esp_err_t uvc_streaming_simulate_start(uvc_frame_callback_t *cb, void *user_ptr);

#ifdef __cplusplus
}
#endif