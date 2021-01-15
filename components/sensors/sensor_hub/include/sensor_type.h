#ifndef _IOT_SENSOR_TYPE_H_
#define _IOT_SENSOR_TYPE_H_

#include "esp_err.h"

#define SENSOR_EVENT_ANY_ID ESP_EVENT_ANY_ID /*!< register handler for any event id */
typedef void *sensor_driver_handle_t; /*!< hal level sensor driver handle */
typedef void *bus_handle_t; /*!< i2c/spi bus handle */

/** @cond **/
#define SENSOR_ID_MASK 0X0F
#define SENSOR_ID_OFFSET 4
#define SENSOR_DATA_GROUP_MAX_NUM 6 /*default number of sensor_data_t in a group */

/**
 * @brief imu sensor type
 * 
 */
#ifndef AXIS3_T
typedef union {
    struct {
        float x; /*!< x axis */
        float y; /*!< y axis */
        float z; /*!< z axis */
    };
    float axis[3];
} axis3_t;
#define AXIS3_T axis3_t
#endif

/**
 * @brief color sensor type
 * 
 */
#ifndef RGBW_T
typedef struct {
    float r; /*!< red */
    float g; /*!< green */
    float b; /*!< blue */
    float w; /*!< white */
} rgbw_t;
#define RGBW_T rgbw_t
#endif

/**
 * @brief uv sensor data type
 * 
 */
#ifndef UV_T
typedef struct {
    float uv; /*!< ultroviolet */
    float uva; /*!< ultroviolet A */
    float uvb; /*!< ultroviolet B */
} uv_t;
#define UV_T uv_t
#endif
/** @endcond **/

/**
 * @brief sensor type
 * 
 */
typedef enum {
    NULL_ID, /*!< NULL */
    HUMITURE_ID, /*!< humidity or temperature sensor */
    IMU_ID, /*!< gyro or acc sensor */
    LIGHT_SENSOR_ID, /*!< light illumination or uv or color sensor */
    SENSOR_TYPE_MAX, /*!< max sensor type */
} sensor_type_t;

/**
 * @brief sensor operate command
 * 
 */
typedef enum {
    COMMAND_SET_MODE, /*!< set measure mdoe */
    COMMAND_SET_RANGE, /*!< set measure range */
    COMMAND_SET_ODR, /*!< set output rate */
    COMMAND_SET_POWER, /*!< set power mode */
    COMMAND_SELF_TEST, /*!< sensor self test */
    COMMAND_MAX /*!< max sensor command */
} sensor_command_t;

/**
 * @brief sensor power mode
 * 
 */
typedef enum {
    POWER_MODE_WAKEUP, /*!< wakeup from sleep */
    POWER_MODE_SLEEP, /*!< set to sleep */
    POWER_MAX /*!< max sensor power mode */
} sensor_power_mode_t;

/**
 * @brief sensor acquire mode
 * 
 */
typedef enum {
    MODE_DEFAULT, /*!< default work mode */
    MODE_POLLING, /*!< polling acquire with a interval time */
    MODE_INTERRUPT, /*!< interrupt mode, acquire data when interrupt comes */
    MODE_MAX, /*!< max sensor mode */
} sensor_mode_t;

/**
 * @brief sensor acquire range
 * 
 */
typedef enum {
    RANGE_DEFAULT, /*!< default range */
    RANGE_MIN, /*!< minimum range for high-speed or high-precision*/
    RANGE_MEDIUM, /*!< medium range for general use*/
    RANGE_MAX, /*!< maximum range for full scale*/
} sensor_range_t;

/**
 * @brief sensor general events
 * 
 */
typedef enum {
    SENSOR_STARTED,                 /*!< sensor started, data acquire will be started */
    SENSOR_STOPED,                  /*!< sensor stoped, data acquire will be stoped */
    SENSOR_EVENT_COMMON_END = 9,    /*!< max common events id */
} sensor_event_id_t;

/**
 * @brief sensor data ready events
 * 
 */
typedef enum {
    SENSOR_ACCE_DATA_READY = 10,    /*!< Accelerometer data ready */
    SENSOR_GYRO_DATA_READY,         /*!< Gyroscope data ready */
    SENSOR_MAG_DATA_READY,          /*!< Magnetometer data ready */
    SENSOR_TEMP_DATA_READY,         /*!< Temperature data ready */
    SENSOR_HUMI_DATA_READY,         /*!< Relative humidity data ready */
    SENSOR_BARO_DATA_READY,         /*!< Pressure data ready */
    SENSOR_LIGHT_DATA_READY,        /*!< Light data ready */
    SENSOR_RGBW_DATA_READY,         /*!< Color data ready */
    SENSOR_UV_DATA_READY,           /*!< ultraviolet data ready */
    SENSOR_PROXI_DATA_READY,        /*!< Distance data ready */
    SENSOR_HR_DATA_READY,           /*!< Heat rate data ready */
    SENSOR_TVOC_DATA_READY,         /*!< TVOC data ready */
    SENSOR_NOISE_DATA_READY,        /*!< Noise Loudness data ready */
    SENSOR_STEP_DATA_READY,         /*!< Step data ready */
    SENSOR_FORCE_DATA_READY,        /*!< Force data ready */
    SENSOR_CURRENT_DATA_READY,      /*!< Current data ready */
    SENSOR_VOLTAGE_DATA_READY,      /*!< Voltage data ready */
    SENSOR_EVENT_ID_END,            /*!< max common events id */
} sensor_data_event_id_t;

/**
 * @brief sensor data type
 * 
 */
typedef struct {
    int64_t                  timestamp;     /*!< timestamp  */
    uint8_t                  sensor_id;     /*!< sensor id  */
    int32_t                  event_id;      /*!< reserved for future use */
    uint32_t                 min_delay;     /*!<  minimum delay between two events, unit: ms */
    union {
        axis3_t              acce;          /*!< Accelerometer.       unit: G           */
        axis3_t              gyro;          /*!< Gyroscope.           unit: dps         */
        axis3_t              mag;           /*!< Magnetometer.        unit: Gauss       */
        float                temperature;   /*!< Temperature.         unit: dCelsius     */
        float                humidity;      /*!< Relative humidity.   unit: percentage   */
        float                baro;          /*!< Pressure.            unit: pascal (Pa)  */
        float                light;         /*!< Light.               unit: lux          */
        rgbw_t               rgbw;          /*!< Color.               unit: lux          */
        uv_t                 uv;            /*!< ultraviole           unit: lux          */
        float                proximity;     /*!< Distance.            unit: centimeters  */
        float                hr;            /*!< Heat rate.           unit: HZ           */
        float                tvoc;          /*!< TVOC.                unit: permillage   */
        float                noise;         /*!< Noise Loudness.      unit: HZ           */
        float                step;          /*!< Step sensor.         unit: 1            */
        float                force;         /*!< Force sensor.        unit: mN           */
        float                current;       /*!< Current sensor       unit: mA           */
        float                voltage;       /*!< Voltage sensor       unit: mV           */
        float                data[4];       /*!< for general use */
    };
} sensor_data_t;

/**
 * @brief sensor data group type
 * 
 */
typedef struct {
    uint8_t number; /*!< effective data number */
    sensor_data_t sensor_data[SENSOR_DATA_GROUP_MAX_NUM]; /*!< data buffer */
} sensor_data_group_t;

/** @cond **/
/**
 * @brief sensor hal level interface
 * 
 */
typedef struct {
    sensor_type_t type; /*!< sensor type  */
    sensor_driver_handle_t (*create)(bus_handle_t, int sensor_id); /*!< create a sensor  */
    esp_err_t (*delete)(sensor_driver_handle_t *); /*!< delete a sensor  */
    esp_err_t (*acquire)(sensor_driver_handle_t, sensor_data_group_t *); /*!< acquire a group of sensor data  */
    esp_err_t (*control)(sensor_driver_handle_t, sensor_command_t cmd, void *args); /*!< modify the sensor configuration  */
} iot_sensor_impl_t;
/** @endcond **/

#endif