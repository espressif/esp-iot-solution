// Copyright 2021-2023 Espressif Systems (Shanghai) PTE LTD
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
#include "esp_err.h"
#include "usb/usb_types_stack.h"
#include "usb_stream.h"

/*********************************** Note: below is uvc_stream v0.1 api, will be abandoned **************************/
#warning "legacy uvc_stream driver is deprecated, please using usb_stream.h instead"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Start just uvc streaming with pre-configs.(uvc_stream v1.0 api, will be abandoned)
 * @note please use usb_streaming_start instead
 */
esp_err_t uvc_streaming_start(uvc_frame_callback_t *cb, void *user_ptr);

/**
 * @brief Stop uvc streaming.(uvc_stream v1.0 api, will be abandoned)
 * @note please use usb_streaming_start instead
 */
esp_err_t uvc_streaming_stop(void);

/**
 * @brief Suspend current IN streaming, only isochronous mode camera support.(uvc_stream v1.0 api, will be abandoned)
 * @note please use usb_streaming_suspend instead
 */
esp_err_t uvc_streaming_suspend(void);

/**
 * @brief Resume current IN streaming, only isochronous mode camera support.(uvc_stream v1.0 api, will be abandoned)
 * @note please use usb_streaming_resume instead
 */
esp_err_t uvc_streaming_resume(void);

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