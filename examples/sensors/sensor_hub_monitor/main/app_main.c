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

#include "esp_log.h"
#include "board.h"
#include "iot_sensor_hub.h"

#define SENSOR_PERIOD CONFIG_SENSOR_EXAMPLE_PERIOD

#define TAG "Sensors Monitor"

static void sensor_event_handler(void *handler_args, esp_event_base_t base, int32_t id, void *event_data)
{
    sensor_data_t *sensor_data = (sensor_data_t *)event_data;
    sensor_type_t sensor_type = (sensor_type_t)((sensor_data->sensor_id) >> 4 & SENSOR_ID_MASK);

    if (sensor_type >= SENSOR_TYPE_MAX) {
        ESP_LOGE(TAG, "sensor_id invalid, id=%d", sensor_data->sensor_id);
        return;
    }
    switch (id) {
        case SENSOR_STARTED:
            ESP_LOGI(TAG, "Timestamp = %llu - %s SENSOR_STARTED",
                     sensor_data->timestamp,
                     SENSOR_TYPE_STRING[sensor_type]);
            break;
        case SENSOR_STOPED:
            ESP_LOGI(TAG, "Timestamp = %llu - %s SENSOR_STOPED",
                     sensor_data->timestamp,
                     SENSOR_TYPE_STRING[sensor_type]);
            break;
        case SENSOR_HUMI_DATA_READY:
            ESP_LOGI(TAG, "Timestamp = %llu - SENSOR_HUMI_DATA_READY - "
                     "humiture=%.2f",
                     sensor_data->timestamp,
                     sensor_data->humidity);
            break;
        case SENSOR_TEMP_DATA_READY:
            ESP_LOGI(TAG, "Timestamp = %llu - SENSOR_TEMP_DATA_READY - "
                     "temperature=%.2f\n",
                     sensor_data->timestamp,
                     sensor_data->temperature);
            break;
        case SENSOR_ACCE_DATA_READY:
            ESP_LOGI(TAG, "Timestamp = %llu - SENSOR_ACCE_DATA_READY - "
                     "acce_x=%.2f, acce_y=%.2f, acce_z=%.2f\n",
                     sensor_data->timestamp,
                     sensor_data->acce.x, sensor_data->acce.y, sensor_data->acce.z);
            break;
        case SENSOR_GYRO_DATA_READY:
            ESP_LOGI(TAG, "Timestamp = %llu - SENSOR_GYRO_DATA_READY - "
                     "gyro_x=%.2f, gyro_y=%.2f, gyro_z=%.2f\n",
                     sensor_data->timestamp,
                     sensor_data->gyro.x, sensor_data->gyro.y, sensor_data->gyro.z);
            break;
        case SENSOR_LIGHT_DATA_READY:
            ESP_LOGI(TAG, "Timestamp = %llu - SENSOR_LIGHT_DATA_READY - "
                     "light=%.2f",
                     sensor_data->timestamp,
                     sensor_data->light);
            break;
        case SENSOR_RGBW_DATA_READY:
            ESP_LOGI(TAG, "Timestamp = %llu - SENSOR_RGBW_DATA_READY - "
                     "r=%.2f, g=%.2f, b=%.2f, w=%.2f\n",
                     sensor_data->timestamp,
                     sensor_data->rgbw.r, sensor_data->rgbw.r, sensor_data->rgbw.b, sensor_data->rgbw.w);
            break;
        case SENSOR_UV_DATA_READY:
            ESP_LOGI(TAG, "Timestamp = %llu - SENSOR_UV_DATA_READY - "
                     "uv=%.2f, uva=%.2f, uvb=%.2f\n",
                     sensor_data->timestamp,
                     sensor_data->uv.uv, sensor_data->uv.uva, sensor_data->uv.uvb);
            break;
        default:
            ESP_LOGI(TAG, "Timestamp = %llu - event id = %d", sensor_data->timestamp, id);
            break;
    }
}

void app_main(void)
{
    /*initialize board for peripheral auto-init with default parameters,
    you can use menuconfig to choose a target board*/
    esp_err_t err = iot_board_init();
    if (err != ESP_OK) {
        goto error_loop;
    }

    /*get the i2c0 bus handle with a resource ID*/
    bus_handle_t i2c0_bus_handle = (bus_handle_t)iot_board_get_handle(BOARD_I2C0_ID);
    if (i2c0_bus_handle == NULL) {
        goto error_loop;
    }

    /*register handler with NULL specific typeID, thus all events posted to sensor_loop will be handled*/
    ESP_ERROR_CHECK(iot_sensor_handler_register_with_type(NULL_ID, NULL_ID, sensor_event_handler, NULL));

    /*create sensors based on sensor scan result*/
    sensor_info_t* sensor_infos[10];
    sensor_handle_t sensor_handle[10] = {NULL};
    sensor_config_t sensor_config = {
        .bus = i2c0_bus_handle, /*which bus sensors will connect to*/
        .mode = MODE_POLLING, /*data acquire mode*/
        .min_delay = SENSOR_PERIOD /*data acquire period*/
    };
    int num = iot_sensor_scan(i2c0_bus_handle, sensor_infos, 10); /*scan for valid sensors based on active i2c address*/
    for (size_t i = 0; i < num && i<10; i++) {

        if (ESP_OK != iot_sensor_create(sensor_infos[i]->sensor_id, &sensor_config, &sensor_handle[i])) { /*create a sensor with specific sensor_id and configurations*/
            goto error_loop;
        }

        iot_sensor_start(sensor_handle[i]); /*start a sensor, data ready events will be posted once data acquired successfully*/
        ESP_LOGI(TAG,"%s (%s) created",sensor_infos[i]->name, sensor_infos[i]->desc);
    }

    while (1) {
        vTaskDelay(1000);
    }

error_loop:
    ESP_LOGE(TAG, "ERRO");
    while (1) {
        vTaskDelay(1000);
    }
}