/*
 * SPDX-FileCopyrightText: 2023-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "esp_log.h"
#include "driver/gpio.h"
#include "button_matrix.h"

static const char *TAG = "matrix button";

#define MATRIX_BTN_CHECK(a, str, ret_val)                          \
    if (!(a))                                                     \
    {                                                             \
        ESP_LOGE(TAG, "%s(%d): %s", __FUNCTION__, __LINE__, str); \
        return (ret_val);                                         \
    }

esp_err_t button_matrix_init(const button_matrix_config_t *config)
{
    MATRIX_BTN_CHECK(NULL != config, "Pointer of config is invalid", ESP_ERR_INVALID_ARG);
    MATRIX_BTN_CHECK(GPIO_IS_VALID_GPIO(config->row_gpio_num), "row GPIO number error", ESP_ERR_INVALID_ARG);
    MATRIX_BTN_CHECK(GPIO_IS_VALID_GPIO(config->col_gpio_num), "col GPIO number error", ESP_ERR_INVALID_ARG);

    // set row gpio as output
    gpio_config_t gpio_conf = {0};
    gpio_conf.intr_type = GPIO_INTR_DISABLE;
    gpio_conf.mode = GPIO_MODE_OUTPUT;
    gpio_conf.pull_down_en = GPIO_PULLDOWN_ENABLE;
    gpio_conf.pin_bit_mask = (1ULL << config->row_gpio_num);
    gpio_config(&gpio_conf);

    // set col gpio as input
    gpio_conf.mode = GPIO_MODE_INPUT;
    gpio_conf.pin_bit_mask = (1ULL << config->col_gpio_num);
    gpio_config(&gpio_conf);

    return ESP_OK;
}

esp_err_t button_matrix_deinit(int row_gpio_num, int col_gpio_num)
{
    //Reset an gpio to default state (select gpio function, enable pullup and disable input and output).
    gpio_reset_pin(row_gpio_num);
    gpio_reset_pin(col_gpio_num);
    return ESP_OK;
}

uint8_t button_matrix_get_key_level(void *hardware_data)
{
    uint32_t row = MATRIX_BUTTON_SPLIT_ROW(hardware_data);
    uint32_t col = MATRIX_BUTTON_SPLIT_COL(hardware_data);
    gpio_set_level(row, 1);
    uint8_t level = gpio_get_level(col);
    gpio_set_level(row, 0);

    return level;
}
