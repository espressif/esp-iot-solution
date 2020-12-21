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

#define LIGHT_SENSOR_ID SENSOR_BH1750_ID
#define SENSOR_PERIOD CONFIG_SENSOR_EXAMPLE_PERIOD
#define LIGHT_SENSOR_THRESHOLD (CONFIG_SENSOR_EXAMPLE_THRESHOLD * 1.0)
#define LIGHT_SENSOR_DEADZONE (LIGHT_SENSOR_THRESHOLD * 0.1)
#define TAG "SENSOR_CONTROL"

static void led_control(float value)
{
    static bool led_is_on = false;
    if ((!led_is_on) && (value < (LIGHT_SENSOR_THRESHOLD - LIGHT_SENSOR_DEADZONE))) {
        ESP_LOGI(TAG, "LED turn on");
        iot_board_led_all_set_state(true);
        led_is_on = true;
    } else if ((led_is_on) && (value >= (LIGHT_SENSOR_THRESHOLD + LIGHT_SENSOR_DEADZONE))) {
        ESP_LOGI(TAG, "LED turn off");
        iot_board_led_all_set_state(false);
        led_is_on = false;
    }
}

static void sensor_event_handler(void *handler_args, esp_event_base_t base, int32_t id, void *event_data)
{
    sensor_data_t *sensor_data = (sensor_data_t *)event_data;
    sensor_type_t sensor_type = (sensor_type_t)((sensor_data->sensor_id) >> 4 & SENSOR_ID_MASK);
    if (sensor_data->sensor_id != LIGHT_SENSOR_ID) return;

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
        case SENSOR_LIGHT_DATA_READY:
            ESP_LOGI(TAG, "Timestamp = %llu - SENSOR_LIGHT_DATA_READY - "
                     "light=%.2f",
                     sensor_data->timestamp,
                     sensor_data->light);
            led_control(sensor_data->light);
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

    /*create sensor based on sensorID*/
    sensor_handle_t sensor_handle = NULL;
    sensor_config_t sensor_config = {
        .bus = i2c0_bus_handle, /*which bus sensors will connect to*/
        .mode = MODE_POLLING, /*data acquire mode*/
        .min_delay = SENSOR_PERIOD /*data acquire period*/
    };

    if (ESP_OK != iot_sensor_create(LIGHT_SENSOR_ID, &sensor_config, &sensor_handle)) { /*create a sensor with specific sensor_id and configurations*/
        goto error_loop;
    }

    /*register handler with sensor's handle*/
    iot_sensor_handler_register(sensor_handle, sensor_event_handler, NULL);

    iot_sensor_start(sensor_handle); /*start a sensor, data ready events will be posted once data acquired successfully*/

    while (1) {
        vTaskDelay(1000);
    }

error_loop:
    ESP_LOGE(TAG, "ERRO");
    while (1) {
        vTaskDelay(1000);
    }
}