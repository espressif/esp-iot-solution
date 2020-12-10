// Copyright 2015-2016 Espressif Systems (Shanghai) PTE LTD
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

#ifndef _VEML6040_H_
#define _VEML6040_H_
#include "i2c_bus.h"
#include "esp_log.h"

#define VEML6040_I2C_ADDRESS    (0x10)
#define VEML6040_I2C_ERR_RES    (-1)

#define VEML6040_INTEGRATION_TIME_DEFAULT  VEML6040_INTEGRATION_TIME_40MS
#define VEML6040_TRIGGER_DEFAULT           VEML6040_TRIGGER_DIS
#define VEML6040_MODE_DEFAULT              VEML6040_MODE_AUTO
#define VEML6040_SWITCH_DEFAULT            VEML6040_SWITCH_EN

typedef enum {
    VEML6040_INTEGRATION_TIME_40MS = 0, /*!< Command to set integration time 40ms*/
    VEML6040_INTEGRATION_TIME_80MS = 1, /*!< Command to set integration time 80ms*/
    VEML6040_INTEGRATION_TIME_160MS = 2, /*!< Command to set integration time 160ms*/
    VEML6040_INTEGRATION_TIME_320MS = 3, /*!< Command to set integration time 320ms*/
    VEML6040_INTEGRATION_TIME_640MS = 4, /*!< Command to set integration time 640ms*/
    VEML6040_INTEGRATION_TIME_1280MS = 5, /*!< Command to set integration time 1280ms*/
    VEML6040_INTEGRATION_TIME_MAX,
} veml6040_integration_time_t;

typedef enum {
    VEML6040_TRIGGER_DIS = 0, /*!< set not trigger 				*/
    VEML6040_TRIGGER_ONCE = 1, /*!< set trigger one time detect cycle*/
    VEML6040_TRIGGER_MAX,
} veml6040_trigger_t;

typedef enum {
    VEML6040_MODE_AUTO = 0, /*!< set auto mode  */
    VEML6040_MODE_FORCE = 1, /*!< set force mode */
    VEML6040_MODE_MAX,
} veml6040_mode_t;

typedef enum {
    VEML6040_SWITCH_EN = 0, /*!< set enable  */
    VEML6040_SWITCH_DIS = 1, /*!< set disable */
    VEML6040_SWITCH_MAX,
} veml6040_switch_t;

/**
 * @brief  VEML6040 Init structure definition.
 */
typedef struct {
    veml6040_integration_time_t integration_time; /*!< set integration time  */
    veml6040_trigger_t trigger; /*!< set  trigger  */
    veml6040_mode_t mode; /*!< set  mode  */
    veml6040_switch_t switch_en; /*!< set if enable  */
} veml6040_config_t;

typedef void *veml6040_handle_t;

#ifdef __cplusplus
extern "C"
{
#endif

/**
 * @brief   create veml6040 sensor device
 * @param   bus device object handle
 * @param   dev_addr device I2C address
 * @return
 *     - NULL if fail to create
 *     - Others handle of veml6040 device
 */
veml6040_handle_t veml6040_create(i2c_bus_handle_t bus, uint8_t dev_addr);

/**
 * @brief   delete veml6040 device
 * @param   sensor Point to device object handle of veml6040
 * @return
 *     - ESP_OK Success
 *     - ESP_FAIL Fail
 */
esp_err_t veml6040_delete(veml6040_handle_t *sensor);

/**
 * @brief  set veml6040 mode
 * @param  sensor device object	handle of veml6040
 * @param  device_info device config info
 * @return
 *     - ESP_OK Success
 *     - ESP_FAIL Fail
 */
esp_err_t veml6040_set_mode(veml6040_handle_t sensor, veml6040_config_t *device_info);

/**
 * @brief  set veml6040 mode
 * @param  sensor device object	handle of veml6040
 * @param  device_info device config info
 * @return
 *     - ESP_OK Success
 *     - ESP_FAIL Fail
 */
esp_err_t veml6040_get_mode(veml6040_handle_t sensor, veml6040_config_t *device_info);

/**
 * Get the current red light brightness value
 * @param sensor device object handle of veml6040
 * @return
 *    - red value
 *    - -1 if fail to read sensor
 */
int veml6040_get_red(veml6040_handle_t sensor);

/**
 * Get the current green light brightness value
 * @param  sensor device object	handle of veml6040
 * @return
 *    - green valueveml6040_info
 *    - -1 if fail to read sensor
 */
int veml6040_get_green(veml6040_handle_t sensor);

/**
 * Get the current blue light brightness value
 * @param  sensor device object	handle of veml6040
 * @return
 *    - blue value
 *    - -1 if fail to read sensor
 */
int veml6040_get_blue(veml6040_handle_t sensor);

/**
 * Get the current white light brightness value
 * @param  sensor device object	handle of veml6040
 * @return
 *    - white value
 *    - -1 if fail to read sensor
 */
int veml6040_get_white(veml6040_handle_t sensor);

/*
 * Get LUX value
 * @param  sensor device object	handle of veml6040
 * @return
 *    - LUX value
 *    - -1 if fail to read sensor
 */
float veml6040_get_lux(veml6040_handle_t sensor);

/*
 * Get color temperature
 * @param  sensor device object	handle of veml6040
 * @param  offset set offset
 * @note
 *     In open-air conditions the offset = 0.5. Depending on the optical
 *     conditions (e.g. cover glass) this offset may change.
 *
 * @return
 *    - white value
 */
float veml6040_get_cct(veml6040_handle_t sensor, float offset);

/**implements of light sensor hal interface**/
#ifdef CONFIG_SENSOR_LIGHT_INCLUDED_VEML6040
/**
 * @brief initialize veml6040 with default configurations
 * 
 * @param i2c_bus i2c bus handle the sensor will attached to
 * @return
 *     - ESP_OK Success
 *     - ESP_FAIL Fail
 */
esp_err_t light_sensor_veml6040_init(i2c_bus_handle_t handle);

/**
 * @brief de-initialize veml6040
 * 
 * @return
 *     - ESP_OK Success
 *     - ESP_FAIL Fail
 */
esp_err_t light_sensor_veml6040_deinit(void);

/**
 * @brief test if veml6040 is active
 * 
 * @return
 *     - ESP_OK Success
 *     - ESP_FAIL Fail
 */
esp_err_t light_sensor_veml6040_test(void);

/**
 * @brief Acquire light sensor color result result one time.
 * light color includes red green blue and white.
 * 
 * @param r red result (unit:lux)
 * @param g green result (unit:lux)
 * @param b blue result (unit:lux)
 * @param w white result (unit:lux)
 * @return
 *     - ESP_OK Success
 *     - ESP_FAIL Fail
 */
esp_err_t light_sensor_veml6040_acquire_rgbw(float *r, float *g, float *b, float *w);

#endif

#ifdef __cplusplus
}
#endif

#endif
