/*
 * SPDX-FileCopyrightText: 2022-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "esp_log.h"
#include "iot_board.h"
#include "sht3x.h"
#include "iot_sensor_hub.h"

#define SENSOR_PERIOD CONFIG_SENSOR_EXAMPLE_PERIOD
#define TEMPERATURE_SENSOR_THRESHOLD (CONFIG_SENSOR_EXAMPLE_THRESHOLD * 1.0)
#define TEMPERATURE_SENSOR_DEADZONE (TEMPERATURE_SENSOR_THRESHOLD * 0.1)
#define TAG "SENSOR_CONTROL"

static void led_control(float value)
{
    static bool led_is_on = false;
    if ((!led_is_on) && (value < (TEMPERATURE_SENSOR_THRESHOLD - TEMPERATURE_SENSOR_DEADZONE))) {
        ESP_LOGI(TAG, "LED turn on");
        iot_board_led_all_set_state(true);
        led_is_on = true;
    } else if ((led_is_on) && (value >= (TEMPERATURE_SENSOR_THRESHOLD + TEMPERATURE_SENSOR_DEADZONE))) {
        ESP_LOGI(TAG, "LED turn off");
        iot_board_led_all_set_state(false);
        led_is_on = false;
    }
}

static void sensor_event_handler(void *handler_args, esp_event_base_t base, int32_t id, void *event_data)
{
    sensor_data_t *sensor_data = (sensor_data_t *)event_data;

    switch (id) {
    case SENSOR_STARTED:
        ESP_LOGI(TAG, "Timestamp = %llu - %s_0x%x STARTED",
                 sensor_data->timestamp,
                 sensor_data->sensor_name,
                 sensor_data->sensor_addr);
        break;
    case SENSOR_STOPED:
        ESP_LOGI(TAG, "Timestamp = %llu - %s_0x%x STOPPED",
                 sensor_data->timestamp,
                 sensor_data->sensor_name,
                 sensor_data->sensor_addr);
        break;
    case SENSOR_TEMP_DATA_READY:
        ESP_LOGI(TAG, "Timestamp = %llu - %s_0x%x TEMP_DATA_READY - "
                 "temperature=%.2f\n",
                 sensor_data->timestamp,
                 sensor_data->sensor_name,
                 sensor_data->sensor_addr,
                 sensor_data->temperature);
        led_control(sensor_data->temperature);
        break;
    default:
        ESP_LOGI(TAG, "Timestamp = %" PRIi64 " - event id = %" PRIi32, sensor_data->timestamp, id);
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
        .bus = i2c0_bus_handle,            /*which bus sensors will connect to*/
        .type = HUMITURE_ID,               /*sensor type*/
        .addr = SHT3x_ADDR_PIN_SELECT_VSS, /*sensor addr*/
        .mode = MODE_POLLING,              /*data acquire mode*/
        .min_delay = SENSOR_PERIOD         /*data acquire period*/
    };

    if (ESP_OK != iot_sensor_create("sht3x", &sensor_config, &sensor_handle)) { /*create a sensor with specific sensor_id and configurations*/
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
