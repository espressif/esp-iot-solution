/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <esp_log.h>
#include <string.h>
#include <driver/gpio.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <bmi270_api.h>

#include "app_driver.h"
#include "app_imu_gesture.h"

/* BMI270 Toy shake detection bitmasks */
#define BMI270_TOY_SHAKE_STATUS_ADDR        0x1F
#define BMI270_TOY_SHAKE_SLIGHT_BIT         0x08
#define BMI270_TOY_SHAKE_HEAVY_BIT          0x80
#define BMI270_TOY_SHAKE_SLIGHT_X_BIT       0x01
#define BMI270_TOY_SHAKE_SLIGHT_Y_BIT       0x02
#define BMI270_TOY_SHAKE_SLIGHT_Z_BIT       0x04
#define BMI270_TOY_SHAKE_HEAVY_X_BIT        0x10
#define BMI270_TOY_SHAKE_HEAVY_Y_BIT        0x20
#define BMI270_TOY_SHAKE_HEAVY_Z_BIT        0x40

static const char *TAG = "app_driver";

static bmi270_handle_t bmi_handle = NULL;
static i2c_bus_handle_t i2c_bus;
static TaskHandle_t imu_gesture_task_handle = NULL;

static void IRAM_ATTR gpio_isr_edge_handler(void* arg)
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    if (imu_gesture_task_handle) {
        vTaskNotifyGiveFromISR(imu_gesture_task_handle, &xHigherPriorityTaskWoken);
        if (xHigherPriorityTaskWoken) {
            portYIELD_FROM_ISR();
        }
    }
}

static esp_err_t i2c_sensor_bmi270_init(void)
{
    const i2c_config_t i2c_bus_conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = I2C_MASTER_SDA_IO,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_io_num = I2C_MASTER_SCL_IO,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = I2C_MASTER_FREQ_HZ
    };
#if I2C_USE_SOFTWARE
    i2c_bus = i2c_bus_create(I2C_NUM_SW_1, &i2c_bus_conf);
#else
    i2c_bus = i2c_bus_create(I2C_MASTER_NUM, &i2c_bus_conf);
#endif
    if (!i2c_bus) {
        ESP_LOGE(TAG, "I2C bus create failed");
        return ESP_FAIL;
    }

    esp_err_t ret = bmi270_sensor_create(i2c_bus, &bmi_handle, bmi270_toy_config_file, 0);
    if (ret != ESP_OK || bmi_handle == NULL) {
        ESP_LOGE(TAG, "BMI270 sensor create failed");
        i2c_bus_delete(&i2c_bus);
        return ESP_FAIL;
    }

    return ESP_OK;
}

static int8_t set_accel_gyro_config(bmi270_handle_t bmi2_dev)
{
    int8_t rslt;
    struct bmi2_sens_config config[2];

    config[BMI2_ACCEL].type = BMI2_ACCEL;
    config[BMI2_GYRO].type = BMI2_GYRO;

    rslt = bmi2_get_sensor_config(config, 2, bmi2_dev);
    bmi2_error_codes_print_result(rslt);

    if (rslt == BMI2_OK) {
        /* Configure accelerometer output data rate */
        config[BMI2_ACCEL].cfg.acc.odr = BMI2_ACC_ODR_200HZ;        /* 200Hz sampling rate */
        config[BMI2_ACCEL].cfg.acc.range = BMI2_ACC_RANGE_16G;      /* ±16G range */
        config[BMI2_ACCEL].cfg.acc.bwp = BMI2_ACC_NORMAL_AVG4;      /* Standard averaging */
        config[BMI2_ACCEL].cfg.acc.filter_perf = BMI2_PERF_OPT_MODE; /* Filter performance */

        /* Configure gyroscope output data rate */
        config[BMI2_GYRO].cfg.gyr.odr = BMI2_GYR_ODR_200HZ;         /* 200Hz sampling rate */
        config[BMI2_GYRO].cfg.gyr.range = BMI2_GYR_RANGE_2000;      /* ±2000dps range */
        config[BMI2_GYRO].cfg.gyr.bwp = BMI2_GYR_NORMAL_MODE;       /* Standard filtering */
        config[BMI2_GYRO].cfg.gyr.noise_perf = BMI2_POWER_OPT_MODE;  /* Noise performance */
        config[BMI2_GYRO].cfg.gyr.filter_perf = BMI2_PERF_OPT_MODE; /* Filter performance */

        rslt = bmi2_set_sensor_config(config, 2, bmi2_dev);
        bmi2_error_codes_print_result(rslt);
    }

    return rslt;
}

static int8_t set_feature_interrupt(bmi270_handle_t bmi2_dev)
{
    int8_t rslt;
    uint8_t data = BMI270_TOY_INT_SHAKE_MASK;
    struct bmi2_int_pin_config pin_config = { 0 };

    /* Map shake interrupt to INT1 pin */
    rslt = bmi2_set_regs(BMI2_INT1_MAP_FEAT_ADDR, &data, 1, bmi2_dev);
    bmi2_error_codes_print_result(rslt);

    if (rslt == BMI2_OK) {
        /* Configure interrupt pin properties */
        pin_config.pin_type = BMI2_INT1;
        pin_config.pin_cfg[0].input_en = BMI2_INT_INPUT_DISABLE;
        pin_config.pin_cfg[0].lvl = BMI2_INT_ACTIVE_HIGH;
        pin_config.pin_cfg[0].od = BMI2_INT_PUSH_PULL;
        pin_config.pin_cfg[0].output_en = BMI2_INT_OUTPUT_ENABLE;
        pin_config.int_latch = BMI2_INT_LATCH;

        rslt = bmi2_set_int_pin_config(&pin_config, bmi2_dev);
        bmi2_error_codes_print_result(rslt);
    }

    return rslt;
}

static int8_t bmi270_enable_shake_int(bmi270_handle_t bmi2_dev)
{
    int8_t rslt;
    uint8_t sens_list[2] = { BMI2_ACCEL, BMI2_GYRO };

    rslt = bmi2_set_adv_power_save(BMI2_DISABLE, bmi2_dev);
    bmi2_error_codes_print_result(rslt);

    if (rslt == BMI2_OK) {
        rslt = set_accel_gyro_config(bmi2_dev);
        bmi2_error_codes_print_result(rslt);
    }

    if (rslt == BMI2_OK) {
        rslt = set_feature_interrupt(bmi2_dev);
        bmi2_error_codes_print_result(rslt);
    }

    if (rslt == BMI2_OK) {
        rslt = bmi2_sensor_enable(sens_list, 2, bmi2_dev);
        bmi2_error_codes_print_result(rslt);
    }

    if (rslt == BMI2_OK) {
        rslt = bmi270_enable_toy_shake(bmi2_dev, BMI2_ENABLE);
        bmi2_error_codes_print_result(rslt);
    }

    return rslt;
}

static void app_imu_gesture_detection(void *pvParameters)
{
    int8_t rslt;
    uint8_t int_status = 0;
    bmi270_handle_t bmi2_dev = (bmi270_handle_t) pvParameters;
    gesture_event_t event = {0};

    while (1) {
        if (ulTaskNotifyTake(pdTRUE, portMAX_DELAY)) {
            int_status = 0;

            rslt = bmi2_get_regs(BMI2_INT_STATUS_0_ADDR, &int_status, 1, bmi2_dev);
            if (rslt != BMI2_OK) {
                bmi2_error_codes_print_result(rslt);
                continue;
            }

            if (int_status & BMI270_TOY_INT_SHAKE_MASK) {
                uint8_t data = 0;
                memset(&event, 0, sizeof(event));
                event.has_orientation = true;
                event.type = GESTURE_EVENT_SHAKE;
                rslt = bmi2_get_regs(BMI270_TOY_SHAKE_STATUS_ADDR, &data, 1, bmi2_dev);
                if (rslt != BMI2_OK) {
                    bmi2_error_codes_print_result(rslt);
                    continue;
                }

                if (data & BMI270_TOY_SHAKE_SLIGHT_BIT) {
                    ESP_LOGI(TAG, "Slight shake on: ");
                    if (data & BMI270_TOY_SHAKE_SLIGHT_X_BIT) {
                        ESP_LOGI(TAG, "X axis");
                        event.x_angle = (float)90.0;
                    }
                    if (data & BMI270_TOY_SHAKE_SLIGHT_Y_BIT) {
                        ESP_LOGI(TAG, "Y axis");
                        event.y_angle = (float)90.0;
                    }
                    if (data & BMI270_TOY_SHAKE_SLIGHT_Z_BIT) {
                        ESP_LOGI(TAG, "Z axis");
                        event.z_angle = (float)90.0;
                    }
                }
                if (data & BMI270_TOY_SHAKE_HEAVY_BIT) {
                    ESP_LOGI(TAG, "Heavy shake on: ");
                    if (data & BMI270_TOY_SHAKE_HEAVY_X_BIT) {
                        ESP_LOGI(TAG, "X axis");
                        event.x_angle = (float)180.0;
                    }
                    if (data & BMI270_TOY_SHAKE_HEAVY_Y_BIT) {
                        ESP_LOGI(TAG, "Y axis");
                        event.y_angle = (float)180.0;
                    }
                    if (data & BMI270_TOY_SHAKE_HEAVY_Z_BIT) {
                        ESP_LOGI(TAG, "Z axis");
                        event.z_angle = (float)180.0;
                    }
                }
                if ((data & (BMI270_TOY_SHAKE_SLIGHT_BIT | BMI270_TOY_SHAKE_HEAVY_BIT)) == 0x00) {
                    ESP_LOGI(TAG, "Unknown shake:");
                }
                app_gesture_event_report(&event);
            }
        }
    }
}

void app_driver_init(void)
{
    ESP_LOGI(TAG, "=== BMI270 Shake Detection Configuration ===");
#ifdef CONFIG_BOARD_ESP_SPOT_C5
    ESP_LOGI(TAG, "Selected Board: ESP SPOT C5");
#elif defined(CONFIG_BOARD_ESP_SPOT_S3)
    ESP_LOGI(TAG, "Selected Board: ESP SPOT S3");
#elif defined(CONFIG_BOARD_ESP_ASTOM_S3)
    ESP_LOGI(TAG, "Selected Board: ESP ASTOM S3");
#elif defined(CONFIG_BOARD_ESP_ECHOEAR_S3)
    ESP_LOGI(TAG, "Selected Board: ESP ECHOEAR S3");
#elif defined(CONFIG_BOARD_CUSTOM)
    ESP_LOGI(TAG, "Selected Board: Other Boards (Custom)");
#else
    ESP_LOGI(TAG, "Selected Board: Default (ECHOEAR_S3)");
#endif
    ESP_LOGI(TAG, "GPIO Configuration:");
    ESP_LOGI(TAG, "  - Interrupt GPIO: %d", I2C_INT_IO);
    ESP_LOGI(TAG, "  - I2C SCL GPIO: %d", I2C_MASTER_SCL_IO);
    ESP_LOGI(TAG, "  - I2C SDA GPIO: %d", I2C_MASTER_SDA_IO);
#if I2C_USE_SOFTWARE
    ESP_LOGI(TAG, "  - I2C Type: Software I2C");
#else
    ESP_LOGI(TAG, "  - I2C Type: Hardware I2C");
#endif
    ESP_LOGI(TAG, "================================");

    gpio_config_t io_conf = {
        .intr_type = GPIO_INTR_POSEDGE,
        .pin_bit_mask = (1ULL << I2C_INT_IO),
        .mode = GPIO_MODE_INPUT,
        .pull_down_en = GPIO_PULLDOWN_ENABLE,
    };

    esp_err_t err = gpio_config(&io_conf);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "GPIO config failed");
        return;
    }

    err = gpio_install_isr_service(0);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "GPIO ISR service install failed");
        return;
    }

    err = gpio_isr_handler_add(I2C_INT_IO, gpio_isr_edge_handler, NULL);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "GPIO ISR handler add failed");
        gpio_uninstall_isr_service();
        return;
    }

    if (i2c_sensor_bmi270_init() != ESP_OK) {
        ESP_LOGE(TAG, "BMI270 initialization failed");
        gpio_isr_handler_remove(I2C_INT_IO);
        gpio_uninstall_isr_service();
        return;
    }

    if (bmi270_enable_shake_int(bmi_handle) != BMI2_OK) {
        ESP_LOGE(TAG, "Failed to enable shake interrupt");
        gpio_isr_handler_remove(I2C_INT_IO);
        gpio_uninstall_isr_service();
        bmi270_sensor_del(&bmi_handle);
        i2c_bus_delete(&i2c_bus);
        return;
    }

    BaseType_t ret = xTaskCreate(app_imu_gesture_detection, "imu_gesture_task", 4096, (void *)bmi_handle, 4, &imu_gesture_task_handle);
    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create IMU gesture task");
        gpio_isr_handler_remove(I2C_INT_IO);
        gpio_uninstall_isr_service();
        bmi270_sensor_del(&bmi_handle);
        i2c_bus_delete(&i2c_bus);
        return;
    }

    ESP_LOGI(TAG, "Move the sensor to get shake interrupt...");
}
