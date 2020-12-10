// Copyright 2020-2021 Espressif Systems (Shanghai) PTE LTD
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

#ifndef LIGHT_SENSOR_H_
#define LIGHT_SENSOR_H_

#include "i2c_bus.h"
#include "esp_err.h"

typedef void *bus_handle_t;
typedef void *sensor_light_handle_t;

typedef enum {
    BH1750_ID = 0x01,
    VEML6040_ID,
    VEML6075_ID,
    LIGHT_MAX_ID,
} light_sensor_id_t;

typedef struct {
    float r;
    float g;
    float b;
    float w;
} rgbw_t;
#define RGBW_T rgbw_t

typedef struct {
    float uv;
    float uva;
    float uvb;
} uv_t;
#define UV_T uv_t

#ifdef __cplusplus
extern "C"
{
#endif

/**
 * @brief Create a light sensor instance.
 * same series' sensor or sensor with same address can only be created once.
 *
 * @param bus i2c bus handle the sensor attached to
 * @param id id declared in light_sensor_id_t
 * @return sensor_light_handle_t return light sensor handle if succeed, return NULL if failed.
 */
sensor_light_handle_t light_sensor_create(bus_handle_t bus, int id);

/**
 * @brief Delete and release the sensor resource.
 *
 * @param sensor point to light sensor handle, will set to NULL if delete succeed.
 * @return esp_err_t
 *     - ESP_OK Success
 *     - ESP_FAIL Fail
*/
esp_err_t light_sensor_delete(sensor_light_handle_t *sensor);

/**
 * @brief Test if sensor is active.
 *
 * @param sensor light sensor handle to operate.
 * @return esp_err_t
 *     - ESP_OK Success
 *     - ESP_FAIL Fail
*/
esp_err_t light_sensor_test(sensor_light_handle_t sensor);

/**
 * @brief Acquire light sensor illuminance result one time.
 *
 * @param sensor light sensor handle to operate.
 * @param lux result data (unit:lux)
 * @return esp_err_t
 *     - ESP_OK Success
 *     - ESP_FAIL Fail
*/
esp_err_t light_sensor_acquire_light(sensor_light_handle_t sensor, float *lux);

/**
 * @brief Acquire light sensor color result one time.
 * light color includes red green blue and white.
 *
 * @param sensor light sensor handle to operate.
 * @param rgbw result data (unit:lux)
 * @return esp_err_t
 *     - ESP_OK Success
 *     - ESP_FAIL Fail
*/
esp_err_t light_sensor_acquire_rgbw(sensor_light_handle_t sensor, rgbw_t *rgbw);

/**
 * @brief Acquire light sensor ultra violet result one time.
 * light Ultraviolet includes UVA UVB and UV.
 *
 * @param sensor light sensor handle to operate.
 * @param uv result data (unit:lux)
 * @return esp_err_t
 *     - ESP_OK Success
 *     - ESP_FAIL Fail
*/
esp_err_t light_sensor_acquire_uv(sensor_light_handle_t sensor, uv_t *uv);

/**
 * @brief Set sensor to sleep mode.
 *
 * @param sensor light sensor handle to operate.
 * @return esp_err_t
 *     - ESP_OK Success
 *     - ESP_FAIL Fail
*/
esp_err_t light_sensor_sleep(sensor_light_handle_t sensor);

/**
 * @brief Wakeup sensor from sleep mode.
 *
 * @param sensor light sensor handle to operate.
 * @return esp_err_t
 *     - ESP_OK Success
 *     - ESP_FAIL Fail
*/
esp_err_t light_sensor_wakeup(sensor_light_handle_t sensor);

#ifdef __cplusplus
extern "C"
}
#endif


#endif
