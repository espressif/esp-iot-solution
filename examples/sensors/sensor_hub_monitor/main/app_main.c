/*
 * SPDX-FileCopyrightText: 2022-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "esp_log.h"
#include "iot_board.h"
#include "iot_sensor_hub.h"

#define SENSOR_PERIOD CONFIG_SENSOR_EXAMPLE_PERIOD

#if CONFIG_SENSOR_INCLUDED_HUMITURE
static sensor_handle_t sht3x_handle = NULL;
static sensor_event_handler_instance_t sht3x_handler_handle = NULL;
#endif

#if CONFIG_SENSOR_INCLUDED_IMU
static sensor_handle_t lis2dh12_handle = NULL;
static sensor_event_handler_instance_t lis2dh12_handler_handle = NULL;
#endif

#if CONFIG_SENSOR_INCLUDED_LIGHT
static sensor_handle_t veml6040_handle = NULL;
static sensor_event_handler_instance_t veml6040_handler_handle = NULL;
#endif

#define TAG "Sensors Monitor"

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
    case SENSOR_HUMI_DATA_READY:
        ESP_LOGI(TAG, "Timestamp = %llu - %s_0x%x HUMI_DATA_READY - "
                 "humiture=%.2f",
                 sensor_data->timestamp,
                 sensor_data->sensor_name,
                 sensor_data->sensor_addr,
                 sensor_data->humidity);
        break;
    case SENSOR_TEMP_DATA_READY:
        ESP_LOGI(TAG, "Timestamp = %llu - %s_0x%x TEMP_DATA_READY - "
                 "temperature=%.2f\n",
                 sensor_data->timestamp,
                 sensor_data->sensor_name,
                 sensor_data->sensor_addr,
                 sensor_data->temperature);
        break;
    case SENSOR_ACCE_DATA_READY:
        ESP_LOGI(TAG, "Timestamp = %llu - %s_0x%x ACCE_DATA_READY - "
                 "acce_x=%.2f, acce_y=%.2f, acce_z=%.2f\n",
                 sensor_data->timestamp,
                 sensor_data->sensor_name,
                 sensor_data->sensor_addr,
                 sensor_data->acce.x, sensor_data->acce.y, sensor_data->acce.z);
        break;
    case SENSOR_GYRO_DATA_READY:
        ESP_LOGI(TAG, "Timestamp = %llu - %s_0x%x GYRO_DATA_READY - "
                 "gyro_x=%.2f, gyro_y=%.2f, gyro_z=%.2f\n",
                 sensor_data->timestamp,
                 sensor_data->sensor_name,
                 sensor_data->sensor_addr,
                 sensor_data->gyro.x, sensor_data->gyro.y, sensor_data->gyro.z);
        break;
    case SENSOR_LIGHT_DATA_READY:
        ESP_LOGI(TAG, "Timestamp = %llu - %s_0x%x LIGHT_DATA_READY - "
                 "light=%.2f",
                 sensor_data->timestamp,
                 sensor_data->sensor_name,
                 sensor_data->sensor_addr,
                 sensor_data->light);
        break;
    case SENSOR_RGBW_DATA_READY:
        ESP_LOGI(TAG, "Timestamp = %llu - %s_0x%x RGBW_DATA_READY - "
                 "r=%.2f, g=%.2f, b=%.2f, w=%.2f\n",
                 sensor_data->timestamp,
                 sensor_data->sensor_name,
                 sensor_data->sensor_addr,
                 sensor_data->rgbw.r, sensor_data->rgbw.r, sensor_data->rgbw.b, sensor_data->rgbw.w);
        break;
    case SENSOR_UV_DATA_READY:
        ESP_LOGI(TAG, "Timestamp = %llu - %s_0x%x UV_DATA_READY - "
                 "uv=%.2f, uva=%.2f, uvb=%.2f\n",
                 sensor_data->timestamp,
                 sensor_data->sensor_name,
                 sensor_data->sensor_addr,
                 sensor_data->uv.uv, sensor_data->uv.uva, sensor_data->uv.uvb);
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

    /*scan the sensor drivers already loaded in the current project.*/
    iot_sensor_scan();

    /*create sensors*/
#if CONFIG_SENSOR_INCLUDED_HUMITURE
    sensor_config_t sht3x_config = {
        .bus = i2c0_bus_handle,
        .addr = 0x44,
        .type = HUMITURE_ID,
        .mode = MODE_POLLING,
        .min_delay = SENSOR_PERIOD,
    };
    if (ESP_OK != iot_sensor_create("sht3x", &sht3x_config, &sht3x_handle)) {
        goto error_loop;
    }
    if (ESP_OK != iot_sensor_handler_register(sht3x_handle, sensor_event_handler, &sht3x_handler_handle)) {
        goto error_loop;
    }
    if (ESP_OK != iot_sensor_start(sht3x_handle)) {
        goto error_loop;
    }
#endif

#if CONFIG_SENSOR_INCLUDED_IMU
    sensor_config_t lis2dh12_config = {
        .bus = i2c0_bus_handle,
        .addr = 0x19,
        .type = IMU_ID,
        .mode = MODE_POLLING,
        .min_delay = SENSOR_PERIOD,
    };
    if (ESP_OK != iot_sensor_create("lis2dh12", &lis2dh12_config, &lis2dh12_handle)) {
        goto error_loop;
    }

    if (ESP_OK != iot_sensor_handler_register(lis2dh12_handle, sensor_event_handler, &lis2dh12_handler_handle)) {
        goto error_loop;
    }

    if (ESP_OK != iot_sensor_start(lis2dh12_handle)) {
        goto error_loop;
    }
#endif

#if CONFIG_SENSOR_INCLUDED_LIGHT
    sensor_config_t veml6040_config = {
        .bus = i2c0_bus_handle,
        .addr = 0x10,
        .type = LIGHT_SENSOR_ID,
        .mode = MODE_POLLING,
        .min_delay = SENSOR_PERIOD,
    };
    if (ESP_OK != iot_sensor_create("veml6040", &veml6040_config, &veml6040_handle)) {
        goto error_loop;
    }

    if (ESP_OK != iot_sensor_handler_register(veml6040_handle, sensor_event_handler, &veml6040_handler_handle)) {
        goto error_loop;
    }

    if (ESP_OK != iot_sensor_start(veml6040_handle)) {
        goto error_loop;
    }
#endif

    while (1) {
        vTaskDelay(1000);
    }

error_loop:
    ESP_LOGE(TAG, "ERROR");
    while (1) {
        vTaskDelay(1000);
    }
}
