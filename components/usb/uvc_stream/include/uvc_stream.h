// Copyright 2020-2022 Espressif Systems (Shanghai) PTE LTD
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

typedef enum {
    UVC_XFER_ISOC = 0, /*!< Isochronous Transfer Mode */
    UVC_XFER_BULK      /*!< Bulk Transfer Mode */
} uvc_xfer_t;

/**
 * @brief users need to get params from camera descriptors,
 * run the example first to printf the descriptor
 */
typedef struct uvc_config{
    usb_speed_t dev_speed;      /*!< USB device speed, only support USB_SPEED_FULL now */
    uvc_xfer_t xfer_type;       /*!< UVC stream transfer type, UVC_XFER_ISOC or UVC_XFER_BULK */
    uint16_t configuration;     /*!< Configuration index value, 1 for most devices */
    uint8_t format_index;       /*!< Format index of MJPEG */
    uint8_t frame_index;        /*!< Frame index, to choose resolution */
    uint16_t frame_width;       /*!< Picture width of selected frame_index */
    uint16_t frame_height;      /*!< Picture height of selected frame_index */
    uint32_t frame_interval;    /*!< Frame interval in 100-ns units, 666666 ~ 15 Fps*/
    uint16_t interface;         /*!< UVC stream interface number */
    uint16_t interface_alt;     /*!< UVC stream alternate interface, to choose MPS (Max Packet Size), bulk fix to 0*/
    union {                     /*!< Using union for backward compatibility */
        uint8_t isoc_ep_addr;   /*!< Isochronous endpoint address of selected alternate interface*/
        uint8_t bulk_ep_addr;   /*!< Bulk endpoint address of UVC stream interface */
    };
    union {
        uint32_t isoc_ep_mps;   /*!< Isochronous MPS of selected interface_alt */
        uint32_t bulk_ep_mps;   /*!< Bulk MPS, fix to 64 for full speed */
    };
    uint32_t xfer_buffer_size;  /*!< Transfer buffer size, using double buffer here, must larger than one frame size */
    uint8_t *xfer_buffer_a;     /*!< Buffer a for usb payload */
    uint8_t *xfer_buffer_b;     /*!< Buffer b for usb payload */
    uint32_t frame_buffer_size; /*!< Frame buffer size, must larger than one frame size */
    uint8_t *frame_buffer;      /*!< Buffer for one frame */
} uvc_config_t;

/**
 * @brief Pre-config UVC driver with params from known USB Camera Descriptor
 * 
 * @param config config struct described in uvc_config_t
 * @return esp_err_t 
 *         ESP_ERR_INVALID_ARG Args not supported
 *         ESP_OK Config driver succeed
 */
esp_err_t uvc_streaming_config(const uvc_config_t *config);

/**
 * @brief Start camera IN streaming with pre-configs, uvc driver will create 2 internal task
 * to handle usb data from stream pipe, and run user's callback after new frame ready.
 * only one streaming supported now.
 * 
 * @param cb callback function to handle incoming picture frame
 * @param user_ptr user pointer used in callback
 * @return
 *         ESP_ERR_INVALID_STATE streaming not configured, or streaming running 
 *         ESP_ERR_INVALID_ARG args not supported
 *         ESP_FAIL start failed
 *         ESP_OK start succeed
 */
esp_err_t uvc_streaming_start(uvc_frame_callback_t *cb, void *user_ptr);

/**
 * @brief Suspend current IN streaming, only isochronous mode camera support
 * 
 * @return
 *         ESP_ERR_INVALID_STATE not inited
 *         ESP_FAIL suspend failed
 *         ESP_OK suspend succeed
 *         ESP_ERR_TIMEOUT suspend wait timeout
 */
esp_err_t uvc_streaming_suspend(void);

/**
 * @brief Resume current IN streaming, only isochronous mode camera support
 * 
 * @return
 *         ESP_ERR_INVALID_STATE not inited
 *         ESP_FAIL resume failed
 *         ESP_OK resume succeed
 *         ESP_ERR_TIMEOUT resume wait timeout
 */
esp_err_t uvc_streaming_resume(void);

/**
 * @brief Stop current IN streaming, internal tasks will be delete, related resourse will be free
 * 
 * @return 
 *         ESP_ERR_INVALID_STATE not inited
 *         ESP_OK stop succeed
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
 *         ESP_OK succeed
 */
esp_err_t uvc_streaming_simulate_start(uvc_frame_callback_t *cb, void *user_ptr);

#ifdef __cplusplus
}
#endif