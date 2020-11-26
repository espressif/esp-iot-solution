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

#include <string.h>
#include "esp_log.h"
#include "iot_sensor.h"
#include "i2c_bus.h"

const char *TAG = "SENSOR_HUB";

#define SENSOR_CHECK(a, str, ret) if(!(a)) { \
        ESP_LOGE(TAG,"%s:%d (%s):%s", __FILE__, __LINE__, __FUNCTION__, str); \
        return (ret); \
    }

ESP_EVENT_DEFINE_BASE(SENSOR_IMU_EVENTS);
ESP_EVENT_DEFINE_BASE(SENSOR_HUMITURE_EVENTS);
ESP_EVENT_DEFINE_BASE(SENSOR_LIGHTSENSOR_EVENTS);

const char *SENSOR_TYPE_STRING[] = {"NULL", "HUMITURE", "IMU", "LIGHTSENSOR"};
const char *SENSOR_MODE_STRING[] = {"DEFAULT_MODE", "POLLING_MODE", "INTERRUPT_MODE"};

typedef void *_sensor_handle_t;

typedef struct {
    sensor_type_t type;
    sensor_id_t sensor_id;
    _sensor_handle_t _sensor_handle;
    const char *event_base;
    sensor_mode_t mode;
    uint16_t min_delay;
    TaskHandle_t task_handle;
    const char *task_name;
    uint32_t task_size;
    UBaseType_t task_priority;
    void (*task_code)(void *);
} iot_sensor_config_t;

typedef struct {
    sensor_type_t type;
    _sensor_handle_t (*create)(bus_handle_t, int sensor_id);
    esp_err_t (*delete)(_sensor_handle_t *);
    esp_err_t (*test)(_sensor_handle_t);
    esp_err_t (*sleep)(_sensor_handle_t);
    esp_err_t (*wakeup)(_sensor_handle_t);
} iot_sensor_impl_t;

typedef struct {
    bus_handle_t bus;
    sensor_type_t type;
    sensor_id_t sensor_id;
    iot_sensor_impl_t *impl;
    iot_sensor_config_t *config;
} iot_sensor_t;

static iot_sensor_impl_t sensor_implementations[] = {
#ifdef CONFIG_SENSOR_INCLUDED_HUMITURE
    {
        .type = HUMITURE_ID,
        .create = humiture_create,
        .delete = humiture_delete,
        .test = humiture_test,
        .sleep = humiture_sleep,
        .wakeup = humiture_wakeup,
    },
#endif
#ifdef CONFIG_SENSOR_INCLUDED_IMU
    {
        .type = IMU_ID,
        .create = imu_create,
        .delete = imu_delete,
        .test = imu_test,
        .sleep = imu_sleep,
        .wakeup = imu_wakeup,
    },
#endif
#ifdef CONFIG_SENSOR_INCLUDED_LIGHT
    {
        .type = LIGHT_SENSOR_ID,
        .create = lightsensor_create,
        .delete = lightsensor_delete,
        .test = lightsensor_test,
        .sleep = lightsensor_sleep,
        .wakeup = lightsensor_wakeup,
    },
#endif
};

static iot_sensor_config_t sensor_config_default = {
    .type = NULL_ID,
    .sensor_id = 0,
    ._sensor_handle = NULL,
    .event_base = NULL,
    .mode = DEFAULT_MODE,
    .min_delay = 200,
    .task_handle = NULL,
    .task_name = NULL,
    .task_size = CONFIG_SENSOR_TASK_STACK_SIZE,
#ifdef CONFIG_SENSOR_TASK_PRIORITY
    .task_priority = CONFIG_SENSOR_TASK_PRIORITY,
#endif
    .task_code = NULL,
};

sensor_info_t s_sensor_info[] = {
#ifdef CONFIG_SENSOR_INCLUDED_HUMITURE
    { "SHT31", "Humi/Temp sensor", SENSOR_SHT3X_ID, (const uint8_t *)"\x44\x45" },
    { "HTS221", "Humi/Temp sensor", SENSOR_DHT11_ID, (const uint8_t *)"\x5f" },
#endif
#ifdef CONFIG_SENSOR_INCLUDED_IMU
    { "MPU6050", "Gyro/Acce sensor", SENSOR_MPU6050_ID, (const uint8_t *)"\x69\x68" },
    { "LIS2DH", "Acce sensor", SENSOR_LIS2DH12_ID, (const uint8_t *)"\x19\x18" },
#endif
#ifdef CONFIG_SENSOR_INCLUDED_LIGHT
    { "BH1750", "Light Intensity sensor", SENSOR_BH1750_ID, (const uint8_t *)"\x23" },
    { "VEML6040", "Light Color sensor", SENSOR_VEML6040_ID, (const uint8_t *)"\x10" },
    { "VEML6075", "Light UV sensor", SENSOR_VEML6075_ID, (const uint8_t *)"\x10" },
#endif
};

static int s_sensor_info_length = sizeof(s_sensor_info)/sizeof(sensor_info_t);

/******************************************Private Functions*********************************************/
static sensor_info_t* find_sensor_info(uint8_t i2c_address) {
    sensor_info_t* last_matched_info = NULL;
    int counter = 0; 
    for(int i = 0; i < s_sensor_info_length; i++) {
        for(int j = 0; s_sensor_info[i].addrs[j] != '\0'; j++) {
            if(s_sensor_info[i].addrs[j] == i2c_address) {
                counter++;
                last_matched_info = &s_sensor_info[i];
                ESP_LOGI(TAG,"address 0x%0x might be %s (%s)\n", i2c_address, s_sensor_info[i].name, s_sensor_info[i].desc);
                break;
            }
        }
    }
    ESP_LOGI(TAG,"address 0x%0x found %d matched info\n", i2c_address, counter);
    return last_matched_info;
}

static iot_sensor_impl_t *find_sensor_implementations(int sensor_type)
{
    int count = sizeof(sensor_implementations) / sizeof(iot_sensor_impl_t);
    for (int i = 0; i < count; i++) {
        if (sensor_implementations[i].type == sensor_type) {
            return &sensor_implementations[i];
        }
    }
    return NULL;
}

static const char *find_sensor_event_base(int sensor_type)
{
    const char *event_base = NULL;
    switch (sensor_type) {
        case IMU_ID:
            event_base = SENSOR_IMU_EVENTS;
            break;
        case HUMITURE_ID:
            event_base = SENSOR_HUMITURE_EVENTS;
            break;
        case LIGHT_SENSOR_ID:
            event_base = SENSOR_LIGHTSENSOR_EVENTS;
            break;
        default :
            break;
    }
    return event_base;
}

static uint64_t get_timestamp_us()
{
    int64_t time = esp_timer_get_time();
    return (uint64_t)time;
}

#ifdef CONFIG_SENSOR_INCLUDED_IMU
static void sensor_imu_task(void *arg)
{
    iot_sensor_config_t *sensor_config = (iot_sensor_config_t *)arg;
    sensor_mode_t mode = sensor_config->mode;
    uint16_t min_delay = sensor_config->min_delay;
    if (min_delay == 0 && mode != INTERRUPT_MODE) {
        ESP_LOGE(TAG, "min_delay = 0 not supported");
        return ;
    }
    sensor_data_t sensor_data;
    memset(&sensor_data, 0, sizeof(sensor_data_t));
    sensor_data.min_delay = min_delay;
    sensor_data.sensor_id = sensor_config->sensor_id;
    sensor_data.timestamp = get_timestamp_us();
    sensors_event_post(sensor_config->event_base, SENSOR_STARTED, &sensor_data, sizeof(sensor_data_t), portMAX_DELAY);
    while (1) {
        if (mode == INTERRUPT_MODE) {
            /* code: wait for sensor ready interrupt*/
        } else {
            vTaskDelay(min_delay / portTICK_PERIOD_MS);
        }
        if (ESP_OK == imu_acquire_acce(sensor_config->_sensor_handle, &sensor_data.data.acce)) {
            sensor_data.timestamp = get_timestamp_us();
            sensors_event_post(sensor_config->event_base, SENSOR_ACCE_DATA_READY, &sensor_data, sizeof(sensor_data_t), 0);
        }
        if (ESP_OK == imu_acquire_gyro(sensor_config->_sensor_handle, &sensor_data.data.gyro)) {
            sensor_data.timestamp = get_timestamp_us();
            sensors_event_post(sensor_config->event_base, SENSOR_GYRO_DATA_READY, &sensor_data, sizeof(sensor_data_t), 0);
        }
    }
}
#endif

#ifdef CONFIG_SENSOR_INCLUDED_LIGHT
static void sensor_lightsensor_task(void *arg)
{
    iot_sensor_config_t *sensor_config = (iot_sensor_config_t *)arg;
    sensor_mode_t mode = sensor_config->mode;
    uint16_t min_delay = sensor_config->min_delay;
    if (min_delay == 0 && mode != INTERRUPT_MODE) {
        ESP_LOGE(TAG, "min_delay = 0 not supported");
        return ;
    }
    sensor_data_t sensor_data;
    memset(&sensor_data, 0, sizeof(sensor_data_t));
    sensor_data.min_delay = min_delay;
    sensor_data.sensor_id = sensor_config->sensor_id;
    sensor_data.timestamp = get_timestamp_us();
    sensors_event_post(sensor_config->event_base, SENSOR_STARTED, &sensor_data, sizeof(sensor_data_t), portMAX_DELAY);
    while (1) {
        if (mode == INTERRUPT_MODE) {
            /* code: wait for sensor ready interrupt*/
        } else {
            vTaskDelay(min_delay / portTICK_PERIOD_MS);
        }
        if (ESP_OK == lightsensor_acquire_light(sensor_config->_sensor_handle, &sensor_data.data.light)) {
            sensor_data.timestamp = get_timestamp_us();
            sensors_event_post(sensor_config->event_base, SENSOR_LIGHT_DATA_READY, &sensor_data, sizeof(sensor_data_t), 0);
        }
        if (ESP_OK == lightsensor_acquire_rgbw(sensor_config->_sensor_handle, &sensor_data.data.rgbw)) {
            sensor_data.timestamp = get_timestamp_us();
            sensors_event_post(sensor_config->event_base, SENSOR_RGBW_DATA_READY, &sensor_data, sizeof(sensor_data_t), 0);
        }
        if (ESP_OK == lightsensor_acquire_uv(sensor_config->_sensor_handle, &sensor_data.data.uv)) {
            sensor_data.timestamp = get_timestamp_us();
            sensors_event_post(sensor_config->event_base, SENSOR_UV_DATA_READY, &sensor_data, sizeof(sensor_data_t), 0);
        }
    }
}
#endif

#ifdef CONFIG_SENSOR_INCLUDED_HUMITURE
static void sensor_humiture_task(void *arg)
{
    iot_sensor_config_t *sensor_config = (iot_sensor_config_t *)arg;
    sensor_mode_t mode = sensor_config->mode;
    uint16_t min_delay = sensor_config->min_delay;
    if (min_delay == 0 && mode != INTERRUPT_MODE) {
        ESP_LOGE(TAG, "min_delay = 0 not supported");
        return ;
    }
    if (min_delay < 250) {
        min_delay = 250;
        ESP_LOGW(TAG, "min_delay < 250, not supported with humiture default configs");
        ESP_LOGW(TAG, "please modify humiture default configs to work in this situation");
        ESP_LOGW(TAG, "force set min_delay to 250ms");
    }
    sensor_data_t sensor_data;
    memset(&sensor_data, 0, sizeof(sensor_data_t));
    sensor_data.min_delay = min_delay;
    sensor_data.sensor_id = sensor_config->sensor_id;
    sensor_data.timestamp = get_timestamp_us();
    sensors_event_post(sensor_config->event_base, SENSOR_STARTED, &sensor_data, sizeof(sensor_data_t), portMAX_DELAY);
    while (1) {
        if (mode == INTERRUPT_MODE) {
            /* code: wait for sensor ready interrupt*/
        } else {
            vTaskDelay(min_delay / portTICK_PERIOD_MS);
        }
        if (ESP_OK == humiture_acquire_humidity(sensor_config->_sensor_handle, &sensor_data.data.humidity)) {
            sensor_data.timestamp = get_timestamp_us();
            sensors_event_post(sensor_config->event_base, SENSOR_HUMI_DATA_READY, &sensor_data, sizeof(sensor_data_t), 0);
        }
        if (ESP_OK == humiture_acquire_temperature(sensor_config->_sensor_handle, &sensor_data.data.temperature)) {
            sensor_data.timestamp = get_timestamp_us();
            sensors_event_post(sensor_config->event_base, SENSOR_TEMP_DATA_READY, &sensor_data, sizeof(sensor_data_t), 0);
        }
    }
}
#endif

static TaskFunction_t find_sensor_task_code(int sensor_type)
{
    TaskFunction_t task_code = NULL;

    switch (sensor_type) {
        case IMU_ID:
#ifdef CONFIG_SENSOR_INCLUDED_IMU
            task_code = sensor_imu_task;
#endif
            break;
        case HUMITURE_ID:
#ifdef CONFIG_SENSOR_INCLUDED_HUMITURE
            task_code = sensor_humiture_task;
#endif
            break;
        case LIGHT_SENSOR_ID:
#ifdef CONFIG_SENSOR_INCLUDED_LIGHT
            task_code = sensor_lightsensor_task;
#endif
            break;
        default :
            break;
    }
    return task_code;
}

#ifdef CONFIG_SENSOR_DEFAULT_HANDLER
static void sensor_default_event_handler(void *handler_args, esp_event_base_t base, int32_t id, void *event_data)
{
    sensor_data_t *sensor_data = (sensor_data_t *)event_data;
    sensor_type_t sensor_type = (sensor_type_t)((sensor_data->sensor_id) >> 4 & SENSOR_ID_MASK);
    if (sensor_type >= SENSOR_TYPE_MAX) {
        return;
    }
    if (handler_args == false && id >= SENSOR_EVENT_COMMON_END) {
        return;
    }
    switch (id) {
        case SENSOR_STARTED:
            ESP_LOGI(TAG, "Timestamp = %llu - %s SENSOR_STARTED",
                     sensor_data->timestamp,
                     SENSOR_TYPE_STRING[sensor_type]);
            break;
        case SENSOR_RESUMED:
            ESP_LOGI(TAG, "Timestamp = %llu - %s SENSOR_RESUMED",
                     sensor_data->timestamp,
                     SENSOR_TYPE_STRING[sensor_type]);
            break;
        case SENSOR_SUSPENDED:
            ESP_LOGI(TAG, "Timestamp = %llu - %s SENSOR_SUSPENDED",
                     sensor_data->timestamp,
                     SENSOR_TYPE_STRING[sensor_type]);
            break;
        case SENSOR_STOPPED:
            ESP_LOGI(TAG, "Timestamp = %llu - %s SENSOR_STOPPED",
                     sensor_data->timestamp,
                     SENSOR_TYPE_STRING[sensor_type]);
            break;
        case SENSOR_HUMI_DATA_READY:
            ESP_LOGI(TAG, "Timestamp = %llu - SENSOR_HUMI_DATA_READY - "
                     "humiture=%.2f",
                     sensor_data->timestamp,
                     sensor_data->data.humidity);
            break;
        case SENSOR_TEMP_DATA_READY:
            ESP_LOGI(TAG, "Timestamp = %llu - SENSOR_TEMP_DATA_READY - "
                     "temperature=%.2f\n",
                     sensor_data->timestamp,
                     sensor_data->data.temperature);
            break;
        case SENSOR_ACCE_DATA_READY:
            ESP_LOGI(TAG, "Timestamp = %llu - SENSOR_ACCE_DATA_READY - "
                     "acce_x=%.2f, acce_y=%.2f, acce_z=%.2f\n",
                     sensor_data->timestamp,
                     sensor_data->data.acce.x, sensor_data->data.acce.y, sensor_data->data.acce.z);
            break;
        case SENSOR_GYRO_DATA_READY:
            ESP_LOGI(TAG, "Timestamp = %llu - SENSOR_GYRO_DATA_READY - "
                     "gyro_x=%.2f, gyro_y=%.2f, gyro_z=%.2f\n",
                     sensor_data->timestamp,
                     sensor_data->data.gyro.x, sensor_data->data.gyro.y, sensor_data->data.gyro.z);
            break;
        case SENSOR_LIGHT_DATA_READY:
            ESP_LOGI(TAG, "Timestamp = %llu - SENSOR_LIGHT_DATA_READY - "
                     "light=%.2f",
                     sensor_data->timestamp,
                     sensor_data->data.light);
            break;
        case SENSOR_RGBW_DATA_READY:
            ESP_LOGI(TAG, "Timestamp = %llu - SENSOR_RGBW_DATA_READY - "
                     "r=%.2f, g=%.2f, b=%.2f, w=%.2f\n",
                     sensor_data->timestamp,
                     sensor_data->data.rgbw.r, sensor_data->data.rgbw.r, sensor_data->data.rgbw.b, sensor_data->data.rgbw.w);
            break;
        case SENSOR_UV_DATA_READY:
            ESP_LOGI(TAG, "Timestamp = %llu - SENSOR_UV_DATA_READY - "
                     "uv=%.2f, uva=%.2f, uvb=%.2f\n",
                     sensor_data->timestamp,
                     sensor_data->data.uv.uv, sensor_data->data.uv.uva, sensor_data->data.uv.uvb);
            break;
        default:
            break;
    }
}
#endif

/******************************************Public Functions*********************************************/

sensor_handle_t iot_sensor_create(bus_handle_t bus, sensor_id_t sensor_id, sensor_mode_t mode, uint16_t min_delay)
{
    /*search for driver based on sensor_id*/
    sensor_type_t sensor_type = (sensor_type_t)(sensor_id >> 4 & SENSOR_ID_MASK);
    iot_sensor_impl_t *sensor_impl = find_sensor_implementations(sensor_type);
    const char *sensor_event_base = find_sensor_event_base(sensor_type);
    if (sensor_impl == NULL || sensor_event_base == NULL) {
        ESP_LOGE(TAG, "sensor_type = %d, no driver founded", sensor_type);
        return NULL;
    }
    /*max task name length here is 16*/
    char sensor_task_name[16] = {'\0'};
    iot_sensor_t *sensor = (iot_sensor_t *)calloc(1, sizeof(iot_sensor_t));
    sensor->bus = bus;
    sensor->type = sensor_type;
    sensor->sensor_id = sensor_id;
    sensor->impl = sensor_impl;
    sensor->config = (iot_sensor_config_t *)malloc(sizeof(iot_sensor_config_t));
    memcpy(sensor->config, &sensor_config_default, sizeof(iot_sensor_config_t));
    sensor->config->type = sensor_type;
    sensor->config->sensor_id = sensor_id;
    sensor->config->mode = mode;
    sensor->config->min_delay = min_delay;
    sensor->config->event_base = sensor_event_base;
#ifdef CONFIG_SENSOR_TASK_PRIORITY_INHERIT
    sensor->config->task_priority = uxTaskPriorityGet(NULL);
#endif
    snprintf(sensor_task_name, sizeof(sensor_task_name) - 1, "%s%02x", SENSOR_TYPE_STRING[sensor_type], sensor_id);
    sensor->config->task_name = sensor_task_name;
    sensor->config->_sensor_handle = sensor->impl->create(bus, (sensor_id & SENSOR_ID_MASK));
    esp_err_t ret = sensor->impl->test(sensor->config->_sensor_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "sensor_id = %d test error", sensor_id);
        free(sensor->config);
        free(sensor);
        return NULL;
    } else {
        ESP_LOGI(TAG, "sensor_id = %d test ok", sensor_id);
    }
#ifdef CONFIG_SENSOR_DEFAULT_HANDLER_DATA
    static sensor_event_handler_instance_t s_sensor_default_handler_instance = NULL;
    if (s_sensor_default_handler_instance == NULL) {
        sensors_event_handler_instance_register(ESP_EVENT_ANY_BASE, ESP_EVENT_ANY_ID, sensor_default_event_handler, (void *)true, &s_sensor_default_handler_instance);
    }
#elif CONFIG_SENSOR_DEFAULT_HANDLER
    static sensor_event_handler_instance_t s_sensor_default_handler_instance = NULL;
    if (s_sensor_default_handler_instance == NULL) {
        sensors_event_handler_instance_register(ESP_EVENT_ANY_BASE, ESP_EVENT_ANY_ID, sensor_default_event_handler, (void *)false, &s_sensor_default_handler_instance);
    }
#endif
    sensor->config->task_code = find_sensor_task_code(sensor_type);
    if (sensor->config->task_code != NULL) {
        xTaskCreate(sensor->config->task_code, sensor->config->task_name, sensor->config->task_size,
                    sensor->config, sensor->config->task_priority, &sensor->config->task_handle);
        ESP_LOGI(TAG, "Sensor task created,Type = %s, Sensor ID = %d, Mode = %s, Min Delay = %d ms",
                 SENSOR_TYPE_STRING[sensor->type],
                 sensor->sensor_id,
                 SENSOR_MODE_STRING[sensor->config->mode],
                 sensor->config->min_delay);
    } else {
        //TODO:attached to existed task
    }
    return (sensor_handle_t)sensor;
}

esp_err_t iot_sensor_suspend(sensor_handle_t sensor_handle)
{
    if (sensor_handle == NULL) {
        return ESP_FAIL;
    }
    iot_sensor_t *sensor = (iot_sensor_t *)sensor_handle;
    if (sensor->config == NULL || sensor->config->task_handle == NULL) {
        return ESP_FAIL;
    }
    vTaskSuspend(sensor->config->task_handle);
    sensor->impl->sleep(sensor->config->_sensor_handle);
    sensor_data_t sensor_data;
    memset(&sensor_data, 0, sizeof(sensor_data_t));
    sensor_data.sensor_id = sensor->config->sensor_id;
    sensor_data.timestamp = get_timestamp_us();
    sensors_event_post(sensor->config->event_base, SENSOR_SUSPENDED, &sensor_data, sizeof(sensor_data_t), portMAX_DELAY);
    return ESP_OK;
}

esp_err_t iot_sensor_resume(sensor_handle_t sensor_handle)
{
    if (sensor_handle == NULL) {
        return ESP_FAIL;
    }
    iot_sensor_t *sensor = (iot_sensor_t *)sensor_handle;
    if (sensor->config == NULL || sensor->config->task_handle == NULL) {
        return ESP_FAIL;
    }
    vTaskResume(sensor->config->task_handle);
    sensor->impl->wakeup(sensor->config->_sensor_handle);
    sensor_data_t sensor_data;
    memset(&sensor_data, 0, sizeof(sensor_data_t));
    sensor_data.sensor_id = sensor->config->sensor_id;
    sensor_data.timestamp = get_timestamp_us();
    sensors_event_post(sensor->config->event_base, SENSOR_RESUMED, &sensor_data, sizeof(sensor_data_t), portMAX_DELAY);
    return ESP_OK;
}

esp_err_t iot_sensor_delete(sensor_handle_t *p_sensor_handle)
{
    if (*p_sensor_handle == NULL) {
        return ESP_FAIL;
    }
    iot_sensor_t *sensor = (iot_sensor_t *)(*p_sensor_handle);
    if (sensor->config == NULL || sensor->config->task_handle == NULL) {
        return ESP_FAIL;
    }
    vTaskDelete(sensor->config->task_handle);
    sensor->config->task_handle = NULL;
    sensor->impl->delete (&sensor->config->_sensor_handle);
    sensor_data_t sensor_data;
    memset(&sensor_data, 0, sizeof(sensor_data_t));
    sensor_data.sensor_id = sensor->config->sensor_id;
    sensor_data.timestamp = get_timestamp_us();
    sensors_event_post(sensor->config->event_base, SENSOR_STOPPED, &sensor_data, sizeof(sensor_data_t), portMAX_DELAY);
    free(sensor->config);
    free(sensor);
    *p_sensor_handle = NULL;
    return ESP_OK;
}

uint8_t iot_sensor_scan(bus_handle_t bus, sensor_info_t* buf[], uint8_t num)
{
    /* first call to get the num for space allocation*/
    uint8_t num_attached = i2c_bus_scan(bus, NULL, 0);
    uint8_t num_valid = 0;
    uint8_t* addrs = malloc(num_attached);
    /* second call to get the addresses*/
    i2c_bus_scan(bus, addrs, num);
    for (size_t i = 0; i < num; i++) {
        sensor_info_t* matched_info = find_sensor_info(*(addrs+i));
        if (matched_info == NULL) continue;
        if (buf != NULL && num_valid < num) *(buf+num_valid) = matched_info;
        num_valid++;
    }
    free(addrs);
    return num_valid;
}

esp_err_t iot_sensor_handler_register(sensor_handle_t sensor_handle, sensor_event_handler_t handler, sensor_event_handler_instance_t *context)
{
    if (sensor_handle == NULL) {
        return ESP_FAIL;
    }
    iot_sensor_t *sensor = (iot_sensor_t *)sensor_handle;
    if (sensor->config == NULL || sensor->config->event_base == NULL) {
        return ESP_FAIL;
    }
    ESP_ERROR_CHECK(sensors_event_handler_instance_register(sensor->config->event_base, ESP_EVENT_ANY_ID, handler, NULL, context));
    return ESP_OK;
}

esp_err_t iot_sensor_handler_unregister(sensor_handle_t sensor_handle, sensor_event_handler_instance_t context)
{
    if (context == NULL) {
        ESP_LOGE(TAG, "invalid sensor handler instance");
        return ESP_FAIL;
    }
    iot_sensor_t *sensor = (iot_sensor_t *)sensor_handle;
    if (sensor->config == NULL || sensor->config->event_base == NULL) {
        return ESP_FAIL;
    }
    ESP_ERROR_CHECK(sensors_event_handler_instance_unregister(sensor->config->event_base, ESP_EVENT_ANY_ID, context));
    return ESP_OK;
}

esp_err_t iot_sensor_handler_register_with_type(sensor_type_t sensor_type, int32_t event_id, sensor_event_handler_t handler, sensor_event_handler_instance_t *context)
{
    switch (sensor_type) {
        case NULL_ID:
            ESP_ERROR_CHECK(sensors_event_handler_instance_register(ESP_EVENT_ANY_BASE, ESP_EVENT_ANY_ID, handler, NULL, context));
            break;
        case HUMITURE_ID:
            ESP_ERROR_CHECK(sensors_event_handler_instance_register(SENSOR_HUMITURE_EVENTS, event_id, handler, NULL, context));
            break;
        case IMU_ID:
            ESP_ERROR_CHECK(sensors_event_handler_instance_register(SENSOR_IMU_EVENTS, event_id, handler, NULL, context));
            break;
        case LIGHT_SENSOR_ID:
            ESP_ERROR_CHECK(sensors_event_handler_instance_register(SENSOR_LIGHTSENSOR_EVENTS, event_id, handler, NULL, context));
            break;
        default :
            ESP_LOGE(TAG, "no driver founded, sensor base = %d", sensor_type);
            break;
    }
    return ESP_OK;
}

esp_err_t iot_sensor_handler_unregister_with_type(sensor_type_t sensor_type, int32_t event_id, sensor_event_handler_instance_t context)
{
    if (context == NULL) {
        ESP_LOGE(TAG, "invalid sensor handler instance");
        return ESP_FAIL;
    }
    switch (sensor_type) {
        case NULL_ID:
            ESP_ERROR_CHECK(sensors_event_handler_instance_unregister(SENSOR_HUMITURE_EVENTS, ESP_EVENT_ANY_ID, context));
            break;
        case HUMITURE_ID:
            ESP_ERROR_CHECK(sensors_event_handler_instance_unregister(SENSOR_HUMITURE_EVENTS, event_id, context));
            break;
        case IMU_ID:
            ESP_ERROR_CHECK(sensors_event_handler_instance_unregister(SENSOR_IMU_EVENTS, event_id, context));
            break;
        case LIGHT_SENSOR_ID:
            ESP_ERROR_CHECK(sensors_event_handler_instance_unregister(SENSOR_LIGHTSENSOR_EVENTS, event_id, context));
            break;
        default :
            ESP_LOGE(TAG, "no driver founded, sensor base = %d", sensor_type);
            break;
    }
    return ESP_OK;
}
