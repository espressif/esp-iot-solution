/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <inttypes.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "unity.h"
#include "iot_sensor_hub.h"
#include "esp_log.h"

#define TEST_MEMORY_LEAK_THRESHOLD (-460)

#define I2C_MASTER_SCL_IO           2           /*!< gpio number for I2C master clock */
#define I2C_MASTER_SDA_IO           1           /*!< gpio number for I2C master data  */
#define I2C_MASTER_NUM              I2C_NUM_0   /*!< I2C port number for master dev */
#define I2C_MASTER_FREQ_HZ          100000      /*!< I2C master clock frequency */

static size_t before_free_8bit;
static size_t before_free_32bit;

static i2c_bus_handle_t i2c_bus = NULL;
static sensor_handle_t sht3x_handle1 = NULL;
static sensor_handle_t sht3x_handle2 = NULL;
static sensor_handle_t mpu6050_handle = NULL;
static sensor_handle_t bh1750_handle = NULL;
static sensor_event_handler_instance_t sht3x_handler_handle1 = NULL;
static sensor_event_handler_instance_t sht3x_handler_handle2 = NULL;
static sensor_event_handler_instance_t mpu6050_handler_handle = NULL;
static sensor_event_handler_instance_t bh1750_handler_handle = NULL;

static const char* TAG = "sensor_hub_test";

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

TEST_CASE("SENSOR_HUB test for virtual sensors", "[virtual sensors][iot]")
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

    sensor_config_t virtual_sht3x_config1 = {
        .bus = i2c_bus,
        .addr = 0x44,
        .type = HUMITURE_ID,
        .mode = MODE_POLLING,
        .min_delay = 100,
    };
    TEST_ASSERT(ESP_OK == iot_sensor_create("virtual_sht3x", &virtual_sht3x_config1, &sht3x_handle1));

    sensor_config_t virtual_sht3x_config2 = {
        .bus = i2c_bus,
        .addr = 0x5F,
        .type = HUMITURE_ID,
        .mode = MODE_POLLING,
        .min_delay = 200,
    };
    TEST_ASSERT(ESP_OK == iot_sensor_create("virtual_sht3x", &virtual_sht3x_config2, &sht3x_handle2));

    sensor_config_t virtual_mpu6050_config = {
        .bus = i2c_bus,
        .addr = 0x68,
        .type = IMU_ID,
        .mode = MODE_POLLING,
        .min_delay = 300,
    };
    TEST_ASSERT(ESP_OK == iot_sensor_create("virtual_mpu6050", &virtual_mpu6050_config, &mpu6050_handle));

    sensor_config_t virtual_bh1750_config = {
        .bus = i2c_bus,
        .addr = 0x23,
        .type = LIGHT_SENSOR_ID,
        .mode = MODE_POLLING,
        .min_delay = 400,
    };
    TEST_ASSERT(ESP_OK == iot_sensor_create("virtual_bh1750", &virtual_bh1750_config, &bh1750_handle));

    TEST_ASSERT(NULL != sht3x_handle1);
    TEST_ASSERT(NULL != sht3x_handle2);
    TEST_ASSERT(NULL != mpu6050_handle);
    TEST_ASSERT(NULL != bh1750_handle);

    TEST_ASSERT(ESP_OK == iot_sensor_handler_register(sht3x_handle1, sensor_event_handler, &sht3x_handler_handle1));
    TEST_ASSERT(ESP_OK == iot_sensor_handler_register(sht3x_handle2, sensor_event_handler, &sht3x_handler_handle2));
    TEST_ASSERT(ESP_OK == iot_sensor_handler_register(mpu6050_handle, sensor_event_handler, &mpu6050_handler_handle));
    TEST_ASSERT(ESP_OK == iot_sensor_handler_register(bh1750_handle, sensor_event_handler, &bh1750_handler_handle));
    TEST_ASSERT(ESP_OK == iot_sensor_start(sht3x_handle1));
    TEST_ASSERT(ESP_OK == iot_sensor_start(sht3x_handle2));
    TEST_ASSERT(ESP_OK == iot_sensor_start(mpu6050_handle));
    TEST_ASSERT(ESP_OK == iot_sensor_start(bh1750_handle));
    vTaskDelay(2000 / portTICK_RATE_MS);
    TEST_ASSERT(ESP_OK == iot_sensor_stop(sht3x_handle1));
    TEST_ASSERT(ESP_OK == iot_sensor_stop(sht3x_handle2));
    TEST_ASSERT(ESP_OK == iot_sensor_stop(mpu6050_handle));
    TEST_ASSERT(ESP_OK == iot_sensor_stop(bh1750_handle));
    vTaskDelay(100 / portTICK_RATE_MS);
    TEST_ASSERT(ESP_OK == iot_sensor_handler_unregister(sht3x_handle1, sht3x_handler_handle1));
    TEST_ASSERT(ESP_OK == iot_sensor_handler_unregister(sht3x_handle2, sht3x_handler_handle2));
    TEST_ASSERT(ESP_OK == iot_sensor_handler_unregister(mpu6050_handle, mpu6050_handler_handle));
    TEST_ASSERT(ESP_OK == iot_sensor_handler_unregister(bh1750_handle, bh1750_handler_handle));

    TEST_ASSERT(ESP_OK == iot_sensor_delete(sht3x_handle1));
    TEST_ASSERT(ESP_OK == iot_sensor_delete(sht3x_handle2));
    TEST_ASSERT(ESP_OK == iot_sensor_delete(mpu6050_handle));
    TEST_ASSERT(ESP_OK == iot_sensor_delete(bh1750_handle));
    TEST_ASSERT(ESP_OK == i2c_bus_delete(&i2c_bus));
    TEST_ASSERT(NULL == i2c_bus);
}

TEST_CASE("SENSOR_HUB scan driver", "[virtual sensors][drivers][iot]")
{
    iot_sensor_scan();
}

static void check_leak(size_t before_free, size_t after_free, const char *type)
{
    ssize_t delta = after_free - before_free;
    printf("MALLOC_CAP_%s: Before %u bytes free, After %u bytes free (delta %d)\n", type, before_free, after_free, delta);
    TEST_ASSERT_MESSAGE(delta >= TEST_MEMORY_LEAK_THRESHOLD, "memory leak");
}

void setUp(void)
{
    before_free_8bit = heap_caps_get_free_size(MALLOC_CAP_8BIT);
    before_free_32bit = heap_caps_get_free_size(MALLOC_CAP_32BIT);
}

void tearDown(void)
{
    size_t after_free_8bit = heap_caps_get_free_size(MALLOC_CAP_8BIT);
    size_t after_free_32bit = heap_caps_get_free_size(MALLOC_CAP_32BIT);
    check_leak(before_free_8bit, after_free_8bit, "8BIT");
    check_leak(before_free_32bit, after_free_32bit, "32BIT");
}

void app_main(void)
{
    printf("SENSOR HUB TEST \n");
    unity_run_menu();
}
