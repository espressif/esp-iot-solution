/*
 * SPDX-FileCopyrightText: 2022-2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "lightbulb_utils.h"
#include "lightbulb.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Map color channels to IDs
 * @note Mapping rules
 *
 * | Rule 1 | Rule 2     | Rule 3
 * |        |            |
 * | RED    | RED        | RED
 * | GREEN  | GREEN      | GREEN
 * | BLUE   | BLUE       | BLUE
 * | COLD   | CCT        | WHITE
 * | WARM   | BRIGHTNESS | YELLOW
 *
 */
#define CHANNEL_ID_RED                              0
#define CHANNEL_ID_GREEN                            1
#define CHANNEL_ID_BLUE                             2
#define CHANNEL_ID_COLD_CCT_WHITE                   3
#define CHANNEL_ID_WARM_BRIGHTNESS_YELLOW           4
#define SELECT_COLOR_CHANNEL                        ((1 << CHANNEL_ID_RED) | (1 << CHANNEL_ID_GREEN) | (1 << CHANNEL_ID_BLUE))
#define SELECT_WHITE_CHANNEL                        ((1 << CHANNEL_ID_COLD_CCT_WHITE) | (1 << CHANNEL_ID_WARM_BRIGHTNESS_YELLOW))

typedef struct {
    lightbulb_driver_t type;
    void *driver_data;
} hal_config_t;

typedef enum {
    QUERY_IS_ALLOW_ALL_OUTPUT,
    QUERY_MAX_INPUT_VALUE,
    QUERY_GRAYSCALE_LEVEL,
    QUERY_DRIVER_NAME,
} hal_feature_query_list_t;

esp_err_t hal_output_init(hal_config_t *config, lightbulb_gamma_data_t *gamma, void *priv_data);
esp_err_t hal_output_deinit(void);
esp_err_t hal_regist_channel(int channel, gpio_num_t gpio_num);
esp_err_t hal_get_driver_feature(hal_feature_query_list_t type, void *out_data);
esp_err_t hal_get_gamma_value(uint8_t r, uint8_t g, uint8_t b, uint16_t *out_r, uint16_t *out_g, uint16_t *out_b);
esp_err_t hal_get_linear_function_value(uint8_t input, uint16_t *output);
esp_err_t hal_set_channel(int channel, uint16_t value, uint16_t fade_ms);
esp_err_t hal_set_channel_group(uint16_t value[], uint8_t channel_mask, uint16_t fade_ms);
esp_err_t hal_start_channel_action(int channel, uint16_t value_min, uint16_t value_max, uint16_t period_ms, bool fade_flag);
esp_err_t hal_start_channel_group_action(uint16_t value_min[], uint16_t value_max[], uint8_t channel_mask, uint16_t period_ms, bool fade_flag);
esp_err_t hal_stop_channel_action(uint8_t channel_mask);
esp_err_t hal_register_monitor_cb(hardware_monitor_user_cb_t cb);
esp_err_t hal_sleep_control(bool enable_sleep);

/**
 * @brief To resolve some compilation warning issues
 * @note cast between incompatible function types
 *
 */
#ifdef CONFIG_ENABLE_SM2135E_DRIVER
static inline esp_err_t _sm2135e_set_channel(sm2135e_channel_t channel, uint16_t value)
{
    return sm2135e_set_channel(channel, value);
}
#endif
#ifdef CONFIG_ENABLE_SM2135EH_DRIVER
static inline esp_err_t _sm2135eh_set_channel(sm2135eh_channel_t channel, uint16_t value)
{
    return sm2135eh_set_channel(channel, value);
}
#endif
#ifdef CONFIG_ENABLE_WS2812_DRIVER
static inline esp_err_t _ws2812_set_rgb_channel(uint16_t value_r, uint16_t value_g, uint16_t value_b)
{
    return ws2812_set_rgb_channel(value_r, value_g, value_b);
}
#endif

#ifdef __cplusplus
}
#endif
