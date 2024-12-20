/*
 * SPDX-FileCopyrightText: 2023-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <inttypes.h>
#include "esp_log.h"
#include "esp_check.h"
#include "driver/gpio.h"
#include "button_matrix.h"
#include "button_interface.h"

static const char *TAG = "matrix_button";

typedef struct {
    button_driver_t base;          /**< base button driver */
    int32_t row_gpio_num;          /**< row gpio */
    int32_t col_gpio_num;          /**< col gpio */
} button_matrix_obj;

static esp_err_t button_matrix_gpio_init(int32_t gpio_num, gpio_mode_t mode)
{
    ESP_RETURN_ON_FALSE(GPIO_IS_VALID_GPIO(gpio_num), ESP_ERR_INVALID_ARG, TAG, "gpio_num error");
    gpio_config_t gpio_conf = {0};
    gpio_conf.intr_type = GPIO_INTR_DISABLE;
    gpio_conf.pull_down_en = GPIO_PULLDOWN_ENABLE;
    gpio_conf.pin_bit_mask = (1ULL << gpio_num);
    gpio_conf.mode = mode;
    gpio_config(&gpio_conf);
    return ESP_OK;
}

esp_err_t button_matrix_del(button_driver_t *button_driver)
{
    button_matrix_obj *matrix_btn = __containerof(button_driver, button_matrix_obj, base);
    //Reset an gpio to default state (select gpio function, enable pullup and disable input and output).
    gpio_reset_pin(matrix_btn->row_gpio_num);
    gpio_reset_pin(matrix_btn->col_gpio_num);
    free(matrix_btn);
    return ESP_OK;
}

uint8_t button_matrix_get_key_level(button_driver_t *button_driver)
{
    button_matrix_obj *matrix_btn = __containerof(button_driver, button_matrix_obj, base);
    gpio_set_level(matrix_btn->row_gpio_num, 1);
    uint8_t level = gpio_get_level(matrix_btn->col_gpio_num);
    gpio_set_level(matrix_btn->row_gpio_num, 0);
    return level;
}

esp_err_t iot_button_new_matrix_device(const button_config_t *button_config, const button_matrix_config_t *matrix_config, button_handle_t *ret_button, size_t *size)
{
    esp_err_t ret = ESP_OK;
    ESP_RETURN_ON_FALSE(button_config && matrix_config && ret_button, ESP_ERR_INVALID_ARG, TAG, "Invalid argument");
    ESP_RETURN_ON_FALSE(matrix_config->col_gpios && matrix_config->row_gpios, ESP_ERR_INVALID_ARG, TAG, "Invalid matrix config");
    ESP_RETURN_ON_FALSE(matrix_config->col_gpio_num > 0 && matrix_config->row_gpio_num > 0, ESP_ERR_INVALID_ARG, TAG, "Invalid matrix config");
    ESP_RETURN_ON_FALSE(*size == matrix_config->row_gpio_num * matrix_config->col_gpio_num, ESP_ERR_INVALID_ARG, TAG, "Invalid size");

    button_matrix_obj *matrix_btn = calloc(*size, sizeof(button_matrix_obj));
    for (int i = 0; i < matrix_config->row_gpio_num; i++) {
        button_matrix_gpio_init(matrix_config->row_gpios[i], GPIO_MODE_OUTPUT);
    }

    for (int i = 0; i < matrix_config->col_gpio_num; i++) {
        button_matrix_gpio_init(matrix_config->col_gpios[i], GPIO_MODE_INPUT);
    }

    for (int i = 0; i < *size; i++) {
        matrix_btn[i].base.get_key_level = button_matrix_get_key_level;
        matrix_btn[i].base.del = button_matrix_del;
        matrix_btn[i].row_gpio_num = matrix_config->row_gpios[i / matrix_config->col_gpio_num];
        matrix_btn[i].col_gpio_num = matrix_config->col_gpios[i % matrix_config->col_gpio_num];
        ESP_LOGD(TAG, "row_gpio_num: %"PRId32", col_gpio_num: %"PRId32"", matrix_btn[i].row_gpio_num, matrix_btn[i].col_gpio_num);
        ret = iot_button_create(button_config, &matrix_btn[i].base, &ret_button[i]);
        ESP_GOTO_ON_FALSE(ret == ESP_OK, ESP_FAIL, err, TAG, "Create button failed");
    }
    *size = matrix_config->row_gpio_num * matrix_config->col_gpio_num;
    return ESP_OK;

err:
    if (matrix_btn) {
        free(matrix_btn);
    }

    return ret;
}
