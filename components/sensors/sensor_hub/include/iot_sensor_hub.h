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

#ifndef _IOT_SENSOR_HUB_H_
#define _IOT_SENSOR_HUB_H_

#include "esp_err.h"

#ifdef CONFIG_SENSOR_INCLUDED_IMU
#include "hal/imu_hal.h"
#endif

#ifdef CONFIG_SENSOR_INCLUDED_LIGHT
#include "hal/light_sensor_hal.h"
#endif

#ifdef CONFIG_SENSOR_INCLUDED_HUMITURE
#include "hal/humiture_hal.h"
#endif

#include "sensor_event.h"
#include "sensor_type.h"

/** @cond **/
/* SENSORS IMU EVENTS BASE */
ESP_EVENT_DECLARE_BASE(SENSOR_IMU_EVENTS);
ESP_EVENT_DECLARE_BASE(SENSOR_HUMITURE_EVENTS);
ESP_EVENT_DECLARE_BASE(SENSOR_LIGHTSENSOR_EVENTS);
/** @endcond **/

typedef void *sensor_handle_t;  /*!< sensor handle*/
typedef void *sensor_event_handler_instance_t;  /*!< sensor event handler handle*/
typedef const char *sensor_event_base_t; /*!< unique pointer to a subsystem that exposes events */

typedef void (*sensor_event_handler_t)(void *event_handler_arg, sensor_event_base_t event_base, int32_t event_id, void *event_data); /*!< function called when an event is posted to the queue */

extern const char *SENSOR_TYPE_STRING[];
extern const char *SENSOR_MODE_STRING[];

/**
 * @brief sensor id, used for iot_sensor_create
 * 
 */
typedef enum {
#ifdef CONFIG_SENSOR_INCLUDED_HUMITURE
    SENSOR_SHT3X_ID = (HUMITURE_ID << SENSOR_ID_OFFSET) | SHT3X_ID, /*!< sht3x sensor id*/
    SENSOR_HTS221_ID = (HUMITURE_ID << SENSOR_ID_OFFSET) | HTS221_ID, /*!< hts221 sensor id*/
#endif
#ifdef CONFIG_SENSOR_INCLUDED_IMU
    SENSOR_MPU6050_ID = ((IMU_ID << SENSOR_ID_OFFSET) | MPU6050_ID), /*!< mpu6050 sensor id*/
    SENSOR_LIS2DH12_ID = ((IMU_ID << SENSOR_ID_OFFSET) | LIS2DH12_ID), /*!< lis2dh12 sensor id*/
#endif
#ifdef CONFIG_SENSOR_INCLUDED_LIGHT
    SENSOR_BH1750_ID = (LIGHT_SENSOR_ID << SENSOR_ID_OFFSET) | BH1750_ID, /*!< bh1750 sensor id*/
    SENSOR_VEML6040_ID = (LIGHT_SENSOR_ID << SENSOR_ID_OFFSET) | VEML6040_ID, /*!< veml6040 sensor id*/
    SENSOR_VEML6075_ID = (LIGHT_SENSOR_ID << SENSOR_ID_OFFSET) | VEML6075_ID, /*!< veml6075 sensor id*/
#endif
} sensor_id_t;

/**
 * @brief sensor information type
 * 
 */
typedef struct {
    const char* name;  /*!< sensor name*/
    const char* desc;  /*!< sensor descriptive message*/
    sensor_id_t sensor_id;  /*!< sensor id*/
    const uint8_t *addrs;  /*!< sensor address list*/
} sensor_info_t;

/**
 * @brief sensor initialization parameter
 * 
 */
typedef struct {
    bus_handle_t bus;                           /*!< i2c/spi bus handle*/
    sensor_mode_t mode;                         /*!< set acquire mode detiled in sensor_mode_t*/
    sensor_range_t range;                       /*!< set measuring range*/
    uint32_t min_delay;                         /*!< set minimum acquisition interval*/
    int intr_pin;                               /*!< set interrupt pin */
    int intr_type;                              /*!< set interrupt type*/
} sensor_config_t;

#ifdef __cplusplus
extern "C"
{
#endif

/**
 * @brief Create a sensor instance with specified sensor_id and desired configurations.
 * 
 * @param sensor_id sensor's id detailed in sensor_id_t.
 * @param config sensor's configurations detailed in sensor_config_t
 * @param p_sensor_handle return sensor handle if succeed, NULL if failed.
 * @return esp_err_t
 *     - ESP_OK Success
 *     - ESP_FAIL Fail
 */
esp_err_t iot_sensor_create(sensor_id_t sensor_id, const sensor_config_t *config, sensor_handle_t *p_sensor_handle);

/**
 * @brief start sensor acquisition, post data ready events when data acquired.
 * if start succeed, sensor will start to acquire data with desired mode and post events in min_delay(ms) intervals
 * SENSOR_STARTED event will be posted.
 *
 * @param sensor_handle sensor handle for operation
 * @return esp_err_t
 *     - ESP_OK Success
 *     - ESP_FAIL Fail
 */
esp_err_t iot_sensor_start(sensor_handle_t sensor_handle);

/**
 * @brief stop sensor acquisition, and stop post data events.
 * if stop succeed, SENSOR_STOPED event will be posted.
 *
 * @param sensor_handle sensor handle for operation
 * @return esp_err_t
 *     - ESP_OK Success
 *     - ESP_FAIL Fail
 */
esp_err_t iot_sensor_stop(sensor_handle_t sensor_handle);

/**
 * @brief delete and release the sensor resource.
 *
 * @param p_sensor_handle point to sensor handle, will set to NULL if delete suceed.
 * @return esp_err_t
 *     - ESP_OK Success
 *     - ESP_FAIL Fail
 */
esp_err_t iot_sensor_delete(sensor_handle_t *p_sensor_handle);

/**
 * @brief Scan for valid sensors attached on bus
 * 
 * @param bus bus handle
 * @param buf Pointer to a buffer to save sensors' information, if NULL no information will be saved.
 * @param num Maximum number of sensor information to save, invalid if buf set to NULL, 
 * latter sensors will be discarded if num less-than the total number found on the bus.
 * @return uint8_t total number of valid sensors found on the bus 
 */
uint8_t iot_sensor_scan(bus_handle_t bus, sensor_info_t* buf[], uint8_t num);

/**
 * @brief Register a event handler to a sensor's event with sensor_handle.
 *
 * @param sensor_handle sensor handle for operation
 * @param handler the handler function which gets called when the sensor's any event is dispatched
 * @param context An event handler instance object related to the registered event handler and data, can be NULL.
 *                This needs to be kept if the specific callback instance should be unregistered before deleting the whole
 *                event loop. Registering the same event handler multiple times is possible and yields distinct instance
 *                objects. The data can be the same for all registrations.
 *                If no unregistration is needed but the handler should be deleted when the event loop is deleted,
 *                instance can be NULL.
 * @return esp_err_t
 *     - ESP_OK Success
 *     - ESP_FAIL Fail
 */
esp_err_t iot_sensor_handler_register(sensor_handle_t sensor_handle, sensor_event_handler_t handler, sensor_event_handler_instance_t *context);

/**
 * @brief Unregister a event handler from a sensor's event.
 *
 * @param sensor_handle sensor handle for operation
 * @param context the instance object of the registration to be unregistered
 * @return esp_err_t
 *     - ESP_OK Success
 *     - ESP_FAIL Fail
 */
esp_err_t iot_sensor_handler_unregister(sensor_handle_t sensor_handle, sensor_event_handler_instance_t context);

/**
 * @brief Register a event handler with sensor_type instead of sensor_handle.
 * the api only care about the event type, don't care who post it.
 *
 * @param sensor_type sensor type decleared in sensor_type_t.
 * @param event_id sensor event decleared in sensor_event_id_t and sensor_data_event_id_t
 * @param handler the handler function which gets called when the event is dispatched
 * @param context An event handler instance object related to the registered event handler and data, can be NULL.
 *                This needs to be kept if the specific callback instance should be unregistered before deleting the whole
 *                event loop. Registering the same event handler multiple times is possible and yields distinct instance
 *                objects. The data can be the same for all registrations.
 *                If no unregistration is needed but the handler should be deleted when the event loop is deleted,
 *                instance can be NULL.
 * @return esp_err_t
 *     - ESP_OK Success
 *     - ESP_FAIL Fail
 */
esp_err_t iot_sensor_handler_register_with_type(sensor_type_t sensor_type, int32_t event_id, sensor_event_handler_t handler, sensor_event_handler_instance_t *context);

/**
 * @brief Unregister a event handler from a event.
 * the api only care about the event type, don't care who post it.
 *
 * @param sensor_type sensor type decleared in sensor_type_t.
 * @param event_id sensor event decleared in sensor_event_id_t and sensor_data_event_id_t
 * @param context the instance object of the registration to be unregistered
 * @return esp_err_t
 *     - ESP_OK Success
 *     - ESP_FAIL Fail
 */
esp_err_t iot_sensor_handler_unregister_with_type(sensor_type_t sensor_type, int32_t event_id, sensor_event_handler_instance_t context);

#ifdef __cplusplus
extern "C"
}
#endif
#endif
