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

#ifndef _IOT_SENSOR_H_
#define _IOT_SENSOR_H_

#include "esp_err.h"
#include "iot_sensor_event.h"

#ifdef CONFIG_SENSOR_INCLUDED_IMU
#include "imu_hal.h"
#endif

#ifdef CONFIG_SENSOR_INCLUDED_LIGHT
#include "light_sensor_hal.h"
#endif

#ifdef CONFIG_SENSOR_INCLUDED_HUMITURE
#include "humiture_hal.h"
#endif

/* Defines for registering/unregistering event handlers */
#define SENSOR_EVENT_ANY_ID ESP_EVENT_ANY_ID                      /**< register handler for any event id */
#define SENSOR_ID_MASK 0X0F
#define SENSOR_ID_OFFSET 4
extern const char *SENSOR_TYPE_STRING[];

/* SENSORS IMU EVENTS BASE */
ESP_EVENT_DECLARE_BASE(SENSOR_IMU_EVENTS);
ESP_EVENT_DECLARE_BASE(SENSOR_HUMITURE_EVENTS);
ESP_EVENT_DECLARE_BASE(SENSOR_LIGHTSENSOR_EVENTS);

typedef void *bus_handle_t;
typedef void *sensor_handle_t;
typedef void *sensor_event_handler_instance_t;
typedef const char *sensor_event_base_t; /**< unique pointer to a subsystem that exposes events */
/**< function called when an event is posted to the queue */
typedef void (*sensor_event_handler_t)(void *event_handler_arg, sensor_event_base_t event_base, int32_t event_id, void *event_data);

typedef enum {
    NULL_ID,
    HUMITURE_ID,
    IMU_ID,                     /*gyro or acc*/
    LIGHT_SENSOR_ID,
    SENSOR_TYPE_MAX,
} sensor_type_t;

typedef enum {
#ifdef CONFIG_SENSOR_INCLUDED_HUMITURE
    SENSOR_SHT3X_ID = (HUMITURE_ID << SENSOR_ID_OFFSET) | SHT3X_ID,
    SENSOR_DHT11_ID = (HUMITURE_ID << SENSOR_ID_OFFSET) | DHT11_ID,
    SENSOR_HTS221_ID = (HUMITURE_ID << SENSOR_ID_OFFSET) | HTS221_ID,
#endif
#ifdef CONFIG_SENSOR_INCLUDED_IMU
    SENSOR_MPU6050_ID = (IMU_ID << SENSOR_ID_OFFSET) | MPU6050_ID,
    SENSOR_LIS2DH12_ID = (IMU_ID << SENSOR_ID_OFFSET) | LIS2DH12_ID,
#endif
#ifdef CONFIG_SENSOR_INCLUDED_LIGHT
    SENSOR_BH1750_ID = (LIGHT_SENSOR_ID << SENSOR_ID_OFFSET) | BH1750_ID,
    SENSOR_VEML6040_ID = (LIGHT_SENSOR_ID << SENSOR_ID_OFFSET) | VEML6040_ID,
    SENSOR_VEML6075_ID = (LIGHT_SENSOR_ID << SENSOR_ID_OFFSET) | VEML6075_ID,
#endif
} sensor_id_t;

typedef enum {
    DEFAULT_MODE,
    POLLING_MODE,
    INTERRUPT_MODE,
    SENSOR_MODE_MAX,
} sensor_mode_t;

extern const char *SENSOR_MODE_STRING[];

#ifndef AXIS3_T
typedef union {
    struct {
        float x;
        float y;
        float z;
    };
    float axis[3];
} axis3_t;
#endif

#ifndef RGBW_T
typedef struct {
    float r;
    float g;
    float b;
    float w;
} rgbw_t;
#endif

#ifndef UV_T
typedef struct {
    float uv;
    float uva;
    float uvb;
} uv_t;
#endif

typedef struct {
    uint64_t        timestamp;              /* timestamp */
    uint8_t         sensor_id;              /* sensor id */
    uint8_t         reserved0;              /* reserved for future use*/
    uint16_t        min_delay;              /*  minimum delay between two events, 0-65535, unit: ms*/
    union {
        axis3_t              acce;          /* Accelerometer.       unit: G          */
        axis3_t              gyro;          /* Gyroscope.           unit: dps        */
        axis3_t              mag;           /* Magnetometer.        unit: Gauss      */
        float                temperature;   /* Temperature.         unit: dCelsius    */
        float                humidity;      /* Relative humidity.   unit: percentage  */
        float                baro;          /* Pressure.            unit: pascal (Pa) */
        float                light;         /* Light.               unit: lux         */
        rgbw_t               rgbw;          /* Color.               unit: lux         */
        uv_t                 uv;            /* UV.                  unit: lux         */
        float                proximity;     /* Distance.            unit: centimeters */
        float                hr;            /* Heat rate.           unit: HZ          */
        float                tvoc;          /* TVOC.                unit: permillage  */
        float                noise;         /* Noise Loudness.      unit: HZ          */
        float                step;          /* Step sensor.         unit: 1           */
        float                force;         /* Force sensor.        unit: mN          */
        float                current;       /* Current sensor       unit: mA          */
        float                voltage;        /* Voltage sensor       unit: mV          */
        float                data[4];       /*for general use*/
    } data;
    /*total size of sensor date is 8B+4B+4*4B=28B*/
} sensor_data_t;

// SENSORS EVENTS ID
typedef enum {
    SENSOR_STARTED,
    SENSOR_SUSPENDED,
    SENSOR_RESUMED,
    SENSOR_STOPPED,
    SENSOR_EVENT_COMMON_END = 9, /*max common events id*/
} sensor_event_id_t;

typedef enum {
    SENSOR_ACCE_DATA_READY = 10,    /*IMU BASE*/
    SENSOR_GYRO_DATA_READY,    /*IMU BASE*/
    SENSOR_MAG_DATA_READY,
    SENSOR_TEMP_DATA_READY,    /*HIMITURE BASE*/
    SENSOR_HUMI_DATA_READY,    /*HIMITURE BASE*/
    SENSOR_BARO_DATA_READY,
    SENSOR_LIGHT_DATA_READY,    /*LIGHTSENSOR BASE*/
    SENSOR_RGBW_DATA_READY,    /*LIGHTSENSOR BASE*/
    SENSOR_UV_DATA_READY,    /*LIGHTSENSOR BASE*/
    SENSOR_PROXI_DATA_READY,
    SENSOR_CURRENT_DATA_READY,
    SENSOR_VOLTAGE_DATA_READY,
    SENSOR_HR_DATA_READY,
    SENSOR_TVOC_DATA_READY,
    SENSOR_NOISE_DATA_READY,
    SENSOR_STEP_DATA_READY,
    SENSOR_FORCE_DATA_READY,
} sensor_data_event_id_t;

#ifdef __cplusplus
extern "C"
{
#endif

/**
 * @brief Create a sensor instance with specified sensor_id and desired parameters.
 * if create suceed, sensor will start to acquire data with desired mode and post events in min_delay(ms) intervals
 * SENSOR_STARTED event will be posted if succeed.
 *
 * @param bus i2c/spi bus handle the sensor attached to.
 * @param sensor_id sensor's id declared in sensor_id_t.
 * @param mode modes declared in sensor_mode_t.
 * @param min_delay minimum delay from the next read event.(1ms ~ 65535ms)
 * @return sensor_handle_t return sensor handle if succeed, NULL if failed.
 */
sensor_handle_t iot_sensor_create(bus_handle_t bus, sensor_id_t sensor_id, sensor_mode_t mode, uint16_t min_delay);

/**
 * @brief Suspend sensor acquire process and set it to sleep mode
 * SENSOR_SUSPENDED event will be posted if succeed.
 *
 * @param sensor_handle sensor handle for operation
 * @return esp_err_t
 *     - ESP_OK Success
 *     - ESP_FAIL Fail
 */
esp_err_t iot_sensor_suspend(sensor_handle_t sensor_handle);

/**
 * @brief Suspend sensor acquire process and set it to sleep mode
 * SENSOR_RESUMED event will be posted if succeed.
 *
 * @param sensor_handle sensor handle for operation
 * @return esp_err_t
 *     - ESP_OK Success
 *     - ESP_FAIL Fail
 */
esp_err_t iot_sensor_resume(sensor_handle_t sensor_handle);

/**
 * @brief Delete and release the sensor resource.
 * SENSOR_STOPPED event will be posted if succeed.
 *
 * @param p_sensor_handle point to sensor handle, will set to NULL if delete suceed.
 * @return esp_err_t
 *     - ESP_OK Success
 *     - ESP_FAIL Fail
 */
esp_err_t iot_sensor_delete(sensor_handle_t *p_sensor_handle);

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
