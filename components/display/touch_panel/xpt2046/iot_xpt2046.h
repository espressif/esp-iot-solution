// Copyright 2015-2020 Espressif Systems (Shanghai) PTE LTD
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

#ifndef _XPT2046_H
#define _XPT2046_H

#ifdef __cplusplus
extern "C" {
#endif

#include "string.h"
#include "stdio.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "iot_touch.h"



/**
 * @brief Initial touch panel
 *
 * @param config Pointer to a structure with touch config arguments.
 *
 * @return
 *     - ESP_OK Success
 *     - ESP_FAIL Fail
 */
esp_err_t iot_xpt2046_init(lcd_touch_config_t *xpt_conf);

/**
 * @brief Deinitial touch panel
 * 
 * @param free_bus Is free bus
 * 
 * @return
 *     - ESP_OK Success
 *     - ESP_FAIL Fail
 */
esp_err_t iot_xpt2046_deinit(bool free_bus);

/**
 * @brief Check if there is a press
 * 
 * @return 
 *      - 0 Not press
 *      - 1 pressed
 */
int iot_xpt2046_is_pressed(void);

/**
 * @brief Set touch rotate direction
 * 
 * @param dir rotate direction
 * 
 * @return
 *     - ESP_OK Success
 *     - ESP_FAIL Fail 
 */
esp_err_t iot_xpt2046_set_direction(touch_dir_t dir);

/**
 * @brief Get raw data
 * 
 * @param x Value of X axis direction
 * @param y Value of Y axis direction
 * 
 * @return
 *     - ESP_OK Success
 *     - ESP_FAIL Fail 
 */
esp_err_t iot_xpt2046_get_rawdata(uint16_t *x, uint16_t *y);

/**
 * @brief Start run touch panel calibration
 * 
 * @param screen LCD driver for display prompts
 * @param recalibrate Is calibration mandatory
 * 
 * @return
 *     - ESP_OK Success
 *     - ESP_FAIL Fail 
 */
esp_err_t iot_xpt2046_calibration_run(const lcd_driver_fun_t *screen, bool recalibrate);

/**
 * @brief Start a sample for screen
 *
 * @param info a pointer of touch_info_t contained touch information.
 *
 * @return
 *     - ESP_OK Success
 *     - ESP_FAIL Fail
 */
esp_err_t iot_xpt2046_sample(touch_info_t* info);


#ifdef __cplusplus
}
#endif

#endif
