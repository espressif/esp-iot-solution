/*
 * SPDX-FileCopyrightText: 2023-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "bldc_helper.h"
#include "driver/gpio.h"

#define GPIO_CONFIG_DEFAULT(gpio_mode, gpio_num)  \
{                                      \
    .intr_type = GPIO_INTR_DISABLE,    \
    .mode = gpio_mode,                 \
    .pull_down_en = 0,                 \
    .pull_up_en = 0,                   \
    .pin_bit_mask = (1ULL << gpio_num),\
}

/**
 *
 * @brief gpio button configuration
 *
 */
typedef struct {
    int32_t gpio_num;              /*!< num of gpio */
    gpio_mode_t gpio_mode;         /*!< gpio mode input or output */
    uint8_t active_level;          /*!< INPUT_MODE: gpio level when press down */
} bldc_gpio_config_t;

/**
 * @brief gpio init
 *
 * @param config gpio config
 * @return
 *      ESP_ERR_INVALID_ARG if parameter is invalid
 *      ESP_OK if success
 */
esp_err_t bldc_gpio_init(const bldc_gpio_config_t *config);

/**
 * @brief gpio deinit
 *
 * @param gpio_num gpio num
 * @return always return ESP_OK
 */
esp_err_t bldc_gpio_deinit(int gpio_num);

/**
 * @brief gpio set level
 *
 * @param gpio_num gpio num
 * @param level gpio level
 * @return ESP_OK if success
 */
esp_err_t bldc_gpio_set_level(void *gpio_num, uint32_t level);

/**
 * @brief gpio get level
 *
 * @param gpio_num gpio num
 * @return int[out] gpio level
 */
int bldc_gpio_get_level(uint32_t gpio_num);

#ifdef __cplusplus
}
#endif
