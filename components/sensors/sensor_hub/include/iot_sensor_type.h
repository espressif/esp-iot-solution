#ifndef _IOT_SENSOR_TYPE_H_
#define _IOT_SENSOR_TYPE_H_

#include "esp_err.h"

#define SENSOR_EVENT_ANY_ID ESP_EVENT_ANY_ID                      /**< register handler for any event id */
#define SENSOR_ID_MASK 0X0F
#define SENSOR_ID_OFFSET 4

typedef void *sensor_driver_handle_t;
typedef void *bus_handle_t;
#define SENSOR_DATA_GROUP_MAX_NUM 6

#ifndef AXIS3_T
typedef union {
    struct {
        float x;
        float y;
        float z;
    };
    float axis[3];
} axis3_t;
#define AXIS3_T axis3_t
#endif

#ifndef RGBW_T
typedef struct {
    float r;
    float g;
    float b;
    float w;
} rgbw_t;
#define RGBW_T rgbw_t
#endif

#ifndef UV_T
typedef struct {
    float uv;
    float uva;
    float uvb;
} uv_t;
#define UV_T uv_t
#endif

typedef enum {
    NULL_ID,
    HUMITURE_ID,
    IMU_ID,                     /*gyro or acc*/
    LIGHT_SENSOR_ID,
    SENSOR_TYPE_MAX,
} sensor_type_t;

typedef enum {
    COMMAND_SET_MODE, /*!set measure mdoe*/
    COMMAND_SET_RANGE, /*!set measure range*/
    COMMAND_SET_ODR, /*!set output rate*/
    COMMAND_SET_POWER, /*!set power mode*/
    COMMAND_SELF_TEST, /*!sensor self test*/
    COMMAND_MAX
} sensor_command_t;

typedef enum {
    POWER_MODE_WAKEUP, /*!wakeup from sleep*/
    POWER_MODE_SLEEP, /*!set to sleep*/
    POWER_MAX
} sensor_power_mode_t;

typedef enum {
    MODE_DEFAULT,
    MODE_POLLING,
    MODE_INTERRUPT,
    MODE_MAX,
} sensor_mode_t;

// SENSORS EVENTS ID
typedef enum {
    SENSOR_STARTED,
    SENSOR_STOPED,
    SENSOR_DELETED,
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
    SENSOR_EVENT_ID_END, /*max common events id*/
} sensor_data_event_id_t;

typedef struct {
    int64_t                  timestamp;     /* timestamp */
    uint8_t                  sensor_id;     /* sensor id */
    int32_t                  event_id;      /* reserved for future use*/
    uint16_t                 min_delay;     /*  minimum delay between two events, 0-65535, unit: ms*/
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
        float                voltage;       /* Voltage sensor       unit: mV          */
        float                data[4];       /*for general use*/
    };
    /*total size of sensor date is 8B+4B+4*4B=28B*/
} sensor_data_t;

typedef struct {
    uint8_t number; /*effective data number*/
    sensor_data_t sensor_data[SENSOR_DATA_GROUP_MAX_NUM]; /*data buffer number*/
} sensor_data_group_t;

typedef struct {
    sensor_type_t type;
    sensor_driver_handle_t (*create)(bus_handle_t, int sensor_id);
    esp_err_t (*delete)(sensor_driver_handle_t *);
    esp_err_t (*acquire)(sensor_driver_handle_t, sensor_data_group_t *);
    esp_err_t (*control)(sensor_driver_handle_t, sensor_command_t cmd, void *args);
} iot_sensor_impl_t;

#endif