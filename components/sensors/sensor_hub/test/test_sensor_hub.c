// Copyright 2015-2016 Espressif Systems (Shanghai) PTE LTD
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
#include <stdio.h>
#include "unity.h"
#include "driver/i2c.h"
#include "i2c_bus.h"
#include "iot_sensor_hub.h"
#include "esp_log.h"

#define I2C_MASTER_SCL_IO           22          /*!< gpio number for I2C master clock */
#define I2C_MASTER_SDA_IO           21          /*!< gpio number for I2C master data  */
#define I2C_MASTER_NUM              I2C_NUM_1   /*!< I2C port number for master dev */
#define I2C_MASTER_FREQ_HZ          100000      /*!< I2C master clock frequency */

static i2c_bus_handle_t i2c_bus = NULL;
static sensor_handle_t imu_handle = NULL;
static sensor_handle_t humiture_handle = NULL;
static sensor_handle_t light_handle = NULL;
static sensor_event_handler_instance_t imu_handler_handle= NULL;
static sensor_event_handler_instance_t humiture_handler_handle= NULL;
static sensor_event_handler_instance_t light_handler_handle= NULL;
static sensor_id_t SENSOR_ID_1 = SENSOR_MPU6050_ID;
static sensor_id_t SENSOR_ID_2 = SENSOR_SHT3X_ID;
static sensor_id_t SENSOR_ID_3 = SENSOR_BH1750_ID;

static const char* TAG = "sensor_hub_test";

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

static void imu_test_get_data()
{
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = I2C_MASTER_SDA_IO,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_io_num = I2C_MASTER_SCL_IO,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = I2C_MASTER_FREQ_HZ,
    };
    i2c_bus = i2c_bus_create(I2C_MASTER_NUM, &conf);
    TEST_ASSERT(i2c_bus != NULL);

    sensor_config_t sensor_conf = {
        .bus = i2c_bus,
        .mode = MODE_POLLING,
        .min_delay = 200,                   /**< name of the event loop task; if NULL,a dedicated task is not created for sensor*/
    };

    TEST_ASSERT(ESP_OK == iot_sensor_create(SENSOR_ID_1, &sensor_conf, &imu_handle));
    TEST_ASSERT(ESP_OK == iot_sensor_create(SENSOR_ID_2, &sensor_conf, &humiture_handle));
    TEST_ASSERT(ESP_OK == iot_sensor_create(SENSOR_ID_3, &sensor_conf, &light_handle));
    TEST_ASSERT(NULL != imu_handle);
    TEST_ASSERT(NULL != humiture_handle);
    TEST_ASSERT(NULL != light_handle);
    TEST_ASSERT(ESP_OK == iot_sensor_handler_register(imu_handle, sensor_event_handler, &imu_handler_handle));
    TEST_ASSERT(ESP_OK == iot_sensor_handler_register(humiture_handle, sensor_event_handler, &humiture_handler_handle));
    TEST_ASSERT(ESP_OK == iot_sensor_handler_register(light_handle, sensor_event_handler, &light_handler_handle));
    TEST_ASSERT(ESP_OK == iot_sensor_start(imu_handle));
    TEST_ASSERT(ESP_OK == iot_sensor_start(humiture_handle));
    TEST_ASSERT(ESP_OK == iot_sensor_start(light_handle));
    vTaskDelay(10000 / portTICK_RATE_MS);
    TEST_ASSERT(ESP_OK == iot_sensor_stop(imu_handle));
    TEST_ASSERT(ESP_OK == iot_sensor_stop(humiture_handle));
    TEST_ASSERT(ESP_OK == iot_sensor_stop(light_handle));
    TEST_ASSERT(ESP_OK == iot_sensor_handler_unregister(imu_handle, imu_handler_handle));
    TEST_ASSERT(ESP_OK == iot_sensor_handler_unregister(humiture_handle, humiture_handler_handle));
    TEST_ASSERT(ESP_OK == iot_sensor_handler_unregister(light_handle, light_handler_handle));
    TEST_ASSERT(ESP_OK == iot_sensor_delete(&imu_handle));
    TEST_ASSERT(ESP_OK == iot_sensor_delete(&humiture_handle));
    TEST_ASSERT(ESP_OK == iot_sensor_delete(&light_handle));
    TEST_ASSERT(NULL == imu_handle);
    TEST_ASSERT(NULL == humiture_handle);
    TEST_ASSERT(NULL == light_handle);
    TEST_ASSERT(ESP_OK == i2c_bus_delete(&i2c_bus));
    TEST_ASSERT(NULL == i2c_bus);
}

TEST_CASE("SENSOR_HUB test get data [200ms]", "[imu_handle][iot][sensor]")
{
    imu_test_get_data();
}
