/*
 * SPDX-FileCopyrightText: 2022-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _LIGHT_SENSOR_HAL_H_
#define _LIGHT_SENSOR_HAL_H_

#ifdef CONFIG_SENSOR_INCLUDED_LIGHT

#include "i2c_bus.h"
#include "esp_err.h"
#include "sensor_type.h"

typedef void *sensor_light_handle_t; /*!< light sensor handle*/

/**
 * @brief Implementation Interface for Light Sensors
 *
 */
typedef struct {
    /**
     * @brief Initialize the light sensor
     *
     * @param handle The bus handle
     * @param addr The I2C address of the sensor
     * @return esp_err_t Result of initialization
     */
    esp_err_t (*init)(bus_handle_t handle, uint8_t addr);

    /**
     * @brief Deinitialize the light sensor
     *
     * @return esp_err_t Result of deinitialization
     */
    esp_err_t (*deinit)(void);

    /**
     * @brief Test the light sensor
     *
     * @return esp_err_t Result of the test
     */
    esp_err_t (*test)(void);

    /**
     * @brief Acquire ambient light data
     *
     * @param[out] l Pointer to store the ambient light intensity
     * @return esp_err_t Result of acquiring data
     */
    esp_err_t (*acquire_light)(float *l);

    /**
     * @brief Acquire RGBW (Red, Green, Blue, White) light data
     *
     * @param[out] r Pointer to store the red light intensity
     * @param[out] g Pointer to store the green light intensity
     * @param[out] b Pointer to store the blue light intensity
     * @param[out] w Pointer to store the white light intensity
     * @return esp_err_t Result of acquiring data
     */
    esp_err_t (*acquire_rgbw)(float *r, float *g, float *b, float *w);

    /**
     * @brief Acquire UV (Ultraviolet) light data
     *
     * @param[out] uv Pointer to store the total UV intensity
     * @param[out] uva Pointer to store the UVA light intensity
     * @param[out] uvb Pointer to store the UVB light intensity
     * @return esp_err_t Result of acquiring data
     */
    esp_err_t (*acquire_uv)(float *uv, float *uva, float *uvb);

    /**
     * @brief Put the light sensor to sleep
     *
     * @return esp_err_t Result of the sleep operation
     */
    esp_err_t (*sleep)(void);

    /**
     * @brief Wake up the light sensor
     *
     * @return esp_err_t Result of the wake-up operation
     */
    esp_err_t (*wakeup)(void);

    /**
     * @brief Set sensor work mode
     *
     * @return esp_err_t Result of setting work mode
     */
    esp_err_t (*set_mode)(sensor_mode_t work_mode);

    /**
     * @brief Set sensor measurement range
     *
     * @return esp_err_t Result of setting measurement range
     */
    esp_err_t (*set_range)(sensor_range_t range);
} light_impl_t;

#ifdef __cplusplus
extern "C"
{
#endif

/**
 * @brief Create a light sensor instance.
 *
 * @param bus i2c/spi bus handle the sensor attached to
 * @param sensor_name name of sensor
 * @param addr addr of sensor
 * @return sensor_light_handle_t return humiture sensor handle if succeed, return NULL if create failed.
 */
sensor_light_handle_t light_sensor_create(bus_handle_t bus, const char *sensor_name, uint8_t addr);

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
 *     - ESP_ERR_NOT_SUPPORTED Function not supported on this sensor
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
 *     - ESP_ERR_NOT_SUPPORTED Function not supported on this sensor
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
 *     - ESP_ERR_NOT_SUPPORTED Function not supported on this sensor
*/
esp_err_t light_sensor_acquire_uv(sensor_light_handle_t sensor, uv_t *uv);

/**
 * @brief Set sensor to sleep mode.
 *
 * @param sensor light sensor handle to operate.
 * @return esp_err_t
 *     - ESP_OK Success
 *     - ESP_FAIL Fail
 *     - ESP_ERR_NOT_SUPPORTED Function not supported on this sensor
*/
esp_err_t light_sensor_sleep(sensor_light_handle_t sensor);

/**
 * @brief Wakeup sensor from sleep mode.
 *
 * @param sensor light sensor handle to operate.
 * @return esp_err_t
 *     - ESP_OK Success
 *     - ESP_FAIL Fail
 *     - ESP_ERR_NOT_SUPPORTED Function not supported on this sensor
*/
esp_err_t light_sensor_wakeup(sensor_light_handle_t sensor);

/**
 * @brief acquire a group of sensor data
 *
 * @param sensor light sensor handle to operate
 * @param data_group acquired data
 * @return esp_err_t
 *     - ESP_OK Success
 *     - ESP_FAIL Fail
 */
esp_err_t light_sensor_acquire(sensor_light_handle_t sensor, sensor_data_group_t *data_group);

/**
 * @brief control sensor mode with control commands and args
 *
 * @param sensor light sensor handle to operate
 * @param cmd control commands detailed in sensor_command_t
 * @param args control commands args
 *     - ESP_OK Success
 *     - ESP_FAIL Fail
 *     - ESP_ERR_NOT_SUPPORTED Function not supported on this sensor
 */
esp_err_t light_sensor_control(sensor_light_handle_t sensor, sensor_command_t cmd, void *args);

#ifdef __cplusplus
extern "C"
}
#endif

#endif

#endif
