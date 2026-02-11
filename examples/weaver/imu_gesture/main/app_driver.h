/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <esp_err.h>
#include <bmi270_api.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/* Configure GPIO pins based on selected development board */
#ifdef CONFIG_BOARD_ESP_SPOT_C5
#define I2C_INT_IO              3       /*!< Interrupt pin */
#define I2C_MASTER_SCL_IO       26      /*!< I2C SCL pin */
#define I2C_MASTER_SDA_IO       25      /*!< I2C SDA pin */
#define I2C_USE_SOFTWARE        1       /*!< Use software I2C */
#elif defined(CONFIG_BOARD_ESP_SPOT_S3)
#define I2C_INT_IO              5
#define I2C_MASTER_SCL_IO       1
#define I2C_MASTER_SDA_IO       2
#define I2C_USE_SOFTWARE        1
#elif defined(CONFIG_BOARD_ESP_ASTOM_S3)
#define I2C_INT_IO              16
#define I2C_MASTER_SCL_IO       0
#define I2C_MASTER_SDA_IO       45
#define I2C_USE_SOFTWARE        1
#elif defined(CONFIG_BOARD_ESP_ECHOEAR_S3)
#define I2C_INT_IO              21
#define I2C_MASTER_SCL_IO       1
#define I2C_MASTER_SDA_IO       2
#define I2C_USE_SOFTWARE        1
#else
#define I2C_INT_IO              CONFIG_I2C_INT_IO
#define I2C_MASTER_SCL_IO       CONFIG_I2C_MASTER_SCL_IO
#define I2C_MASTER_SDA_IO       CONFIG_I2C_MASTER_SDA_IO
#define I2C_USE_SOFTWARE        CONFIG_I2C_USE_SOFTWARE
#endif

#define I2C_MASTER_NUM          I2C_NUM_0
#define I2C_MASTER_FREQ_HZ      (100 * 1000)

/**
 * @brief Initialize BMI270 sensor and start the IMU gesture detection task
 */
void app_driver_init(void);

#ifdef __cplusplus
}
#endif
