/*
 * SPDX-FileCopyrightText: 2022-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

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

typedef void *sensor_handle_t;                                                                                                        /*!< sensor handle*/
typedef void *sensor_event_handler_instance_t;                                                                                        /*!< sensor event handler handle*/
typedef const char *sensor_event_base_t;                                                                                              /*!< unique pointer to a subsystem that exposes events */

typedef void (*sensor_event_handler_t)(void *event_handler_arg, sensor_event_base_t event_base, int32_t event_id, void *event_data);  /*!< function called when an event is posted to the queue */

extern const char *SENSOR_TYPE_STRING[];
extern const char *SENSOR_MODE_STRING[];

/**
 * @brief sensor information, written by the driver developer.
 *
 */
typedef struct {
    const char *name;          /*!< sensor name */
    sensor_type_t sensor_type; /*!< sensor type */
} sensor_info_t;

/**
 * @brief sensor initialization parameter
 *
 */
typedef struct {
    bus_handle_t bus;     /*!< i2c/spi bus handle*/
    uint8_t addr;         /*!< set addr of sensor*/
    sensor_type_t type;   /*!< sensor type */
    sensor_mode_t mode;   /*!< set acquire mode detiled in sensor_mode_t*/
    sensor_range_t range; /*!< set measuring range*/
    uint32_t min_delay;   /*!< set minimum acquisition interval*/
    int intr_pin;         /*!< set interrupt pin */
    int intr_type;        /*!< set interrupt type*/
} sensor_config_t;

/**
 * @brief Detection function interface for sensors
 */
typedef struct {
    /**
     * @brief Function pointer for sensor detection
     *
     * This function detects a sensor based on the provided sensor information.
     *
     * @param sensor_info Pointer to the sensor information structure
     * @return void* Pointer to the detected sensor instance, or NULL if detection failed
     */
    void* (*fn)(sensor_info_t *sensor_info);
} sensor_hub_detect_fn_t;

/**
 * @brief Defines a sensor detection function to be executed automatically in the application layer.
 *
 * @param type_id  The sensor type identifier.
 * @param name_id  The unique name identifier for the sensor.
 * @param impl     The implementation returned on successful detection.
 *
 * This macro defines a function and its corresponding metadata to facilitate automatic sensor detection
 * and initialization. The function must return the implementation (`impl`) on success; otherwise,
 * an error is logged, and the automatic detection process in the application layer will be aborted.
 *
 * To prevent the linker from optimizing out the sensor driver, the driver must include at least one
 * undefined symbol that is explicitly referenced during the linking phase. This ensures that the linker
 * retains the driver, even if no other files directly depend on its symbols.
 *
 * For example, in the sensor driver's `CMakeLists.txt`, include the following to force the linker
 * to keep the required symbol:
 *
 *  target_link_libraries(${COMPONENT_LIB} INTERFACE "-u <symbol_name>")
 *
 * Replace `<symbol_name>` with an appropriate symbol, such as `sht3x_init`, defined in the driver.
 */
#define SENSOR_HUB_DETECT_FN(type_id, name_id, impl) \
    static void*  __sensor_hub_detect_fn_##name_id(sensor_info_t *sensor_info); \
    __attribute__((used)) _SECTION_ATTR_IMPL(".sensor_hub_detect_fn", __COUNTER__) \
        sensor_hub_detect_fn_t sensor_hub_detect_fn_##name_id = { \
            .fn = __sensor_hub_detect_fn_##name_id, \
        }; \
    static void* __sensor_hub_detect_fn_##name_id(sensor_info_t *sensor_info) { \
        sensor_info->name = #name_id; \
        sensor_info->sensor_type = type_id; \
        return impl; \
    }

extern sensor_hub_detect_fn_t __sensor_hub_detect_fn_array_start;
extern sensor_hub_detect_fn_t __sensor_hub_detect_fn_array_end;

#ifdef __cplusplus
extern "C"
{
#endif

/**
 * @brief Create a sensor instance with specified name and desired configurations.
 *
 * @param sensor_name sensor's name. Sensor information will be registered by ESP_SENSOR_DETECT_FN
 * @param config sensor's configurations detailed in sensor_config_t
 * @param p_sensor_handle return sensor handle if succeed, NULL if failed.
 * @return esp_err_t
 *     - ESP_OK Success
 *     - ESP_FAIL Fail
 */
esp_err_t iot_sensor_create(const char *sensor_name, const sensor_config_t *config, sensor_handle_t *p_sensor_handle);

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
 * @param p_sensor_handle sensor handle, will set to NULL if delete succeed.
 * @return esp_err_t
 *     - ESP_OK Success
 *     - ESP_FAIL Fail
 */
esp_err_t iot_sensor_delete(sensor_handle_t p_sensor_handle);

/**
 * @brief Scan for valid sensors attached on bus
 *
 * @param bus bus handle
 * @param buf Pointer to a buffer to save sensors' information, if NULL no information will be saved.
 * @param num Maximum number of sensor information to save, invalid if buf set to NULL,
 * latter sensors will be discarded if num less-than the total number found on the bus.
 * @return uint8_t total number of valid sensors found on the bus
 */

/**
 * @brief Scan for valid sensors registered in the system
 *
 * @return int number of valid sensors
 */
int iot_sensor_scan();

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
 * @param sensor_type sensor type declared in sensor_type_t.
 * @param event_id sensor event declared in sensor_event_id_t and sensor_data_event_id_t
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
 * @param sensor_type sensor type declared in sensor_type_t.
 * @param event_id sensor event declared in sensor_event_id_t and sensor_data_event_id_t
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
