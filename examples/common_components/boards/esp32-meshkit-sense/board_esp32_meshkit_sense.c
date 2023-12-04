/*
 * SPDX-FileCopyrightText: 2022-2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include "iot_board.h"
#include "esp_log.h"

static const char *TAG = BOARD_NAME;

static bool s_board_is_init = false;
static bool s_board_gpio_isinit = false;

esp_err_t iot_board_specific_init(void);
esp_err_t iot_board_specific_deinit(void);

const board_specific_callbacks_t board_esp32_meshkit_callbacks = {
    .pre_init_cb = NULL,
    .pre_deinit_cb = NULL,
    .post_init_cb = iot_board_specific_init,
    .post_deinit_cb = iot_board_specific_deinit,
    .get_handle_cb = NULL,
};

/****General board level API ****/
esp_err_t iot_board_specific_init(void)
{
    if (s_board_is_init) {
        return ESP_OK;
    }
#ifdef CONFIG_BOARD_POWER_SENSOR
    iot_board_sensor_set_power(true);
#else
    iot_board_sensor_set_power(false);
#endif

#ifdef CONFIG_BOARD_POWER_SCREEN
    iot_board_screen_set_power(true);
#else
    iot_board_screen_set_power(false);
#endif
    s_board_is_init = true;
    return ESP_OK;
}

esp_err_t iot_board_specific_deinit(void)
{
    if (!s_board_is_init) {
        return ESP_OK;
    }
#ifdef CONFIG_BOARD_POWER_SENSOR
    iot_board_sensor_set_power(false);
#endif

#ifdef CONFIG_BOARD_POWER_SCREEN
    iot_board_screen_set_power(false);
#endif
    s_board_is_init = false;
    return ESP_OK;
}

/****Extended board level API ****/
esp_err_t iot_board_sensor_set_power(bool on_off)
{
    ESP_LOGI(TAG, "%s on_off = %d", __func__, on_off);
    if (!s_board_gpio_isinit) {
        return ESP_FAIL;
    }
    return gpio_set_level(BOARD_IO_POWER_ON_SENSOR_N, !on_off);
}

bool iot_board_sensor_get_power(void)
{
    if (!s_board_gpio_isinit) {
        return 0;
    }
    return !gpio_get_level(BOARD_IO_POWER_ON_SENSOR_N);
}

esp_err_t iot_board_screen_set_power(bool on_off)
{
    ESP_LOGI(TAG, "%s on_off = %d", __func__, on_off);
    if (!s_board_gpio_isinit) {
        return ESP_FAIL;
    }
    return gpio_set_level(BOARD_IO_POWER_ON_SCREEN_N, !on_off);
}

bool iot_board_screen_get_power(void)
{
    if (!s_board_gpio_isinit) {
        return 0;
    }
    return gpio_get_level(BOARD_IO_POWER_ON_SCREEN_N);
}
