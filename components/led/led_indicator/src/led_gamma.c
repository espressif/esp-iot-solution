/* SPDX-FileCopyrightText: 2022-2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "math.h"
#include "led_gamma.h"
#include "freertos/FreeRTOS.h"

#ifdef CONFIG_USE_GAMMA_CORRECTION // gamma calibration supported
#define MAX_PROGRESS 256

/* step brightness table: gamma = 2.3 */
uint8_t gamma_table[MAX_PROGRESS] = {
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  1,  1,  1,  1,  1,  1,  1,  1,  2,
    2,  2,  2,  2,  2,  3,  3,  3,  3,  3,  4,  4,  4,  4,  4,  5,
    5,  5,  6,  6,  6,  6,  7,  7,  7,  8,  8,  8,  9,  9,  9, 10,
    10, 10, 11, 11, 12, 12, 13, 13, 13, 14, 14, 15, 15, 16, 16, 17,
    17, 18, 18, 19, 19, 20, 20, 21, 22, 22, 23, 23, 24, 25, 25, 26,
    26, 27, 28, 28, 29, 30, 30, 31, 32, 33, 33, 34, 35, 36, 36, 37,
    38, 39, 40, 40, 41, 42, 43, 44, 45, 45, 46, 47, 48, 49, 50, 51,
    52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63, 64, 65, 66, 67,
    68, 69, 70, 71, 72, 74, 75, 76, 77, 78, 79, 81, 82, 83, 84, 86,
    87, 88, 89, 91, 92, 93, 95, 96, 97, 99, 100, 101, 103, 104, 105, 107,
    108, 110, 111, 112, 114, 115, 117, 118, 120, 121, 123, 124, 126, 128, 129, 131,
    132, 134, 135, 137, 139, 140, 142, 144, 145, 147, 149, 150, 152, 154, 156, 157,
    159, 161, 163, 164, 166, 168, 170, 172, 174, 175, 177, 179, 181, 183, 185, 187,
    189, 191, 193, 195, 197, 199, 201, 203, 205, 207, 209, 211, 213, 215, 217, 219,
    221, 223, 226, 228, 230, 232, 234, 236, 239, 241, 243, 245, 248, 250, 252, 255,
};

static portMUX_TYPE s_table_lock = portMUX_INITIALIZER_UNLOCKED;
#define ENTER_CRITICAL()           portENTER_CRITICAL(&s_table_lock)
#define EXIT_CRITICAL()            portEXIT_CRITICAL(&s_table_lock)

#endif

uint8_t led_indicator_get_gamma_value(uint8_t input)
{
#ifdef CONFIG_USE_GAMMA_CORRECTION
    ENTER_CRITICAL();
    uint8_t output = gamma_table[input];
    EXIT_CRITICAL();
    return output;
#else
    return input;
#endif
}

esp_err_t led_indicator_new_gamma_table(float gamma)
{
#ifdef CONFIG_USE_GAMMA_CORRECTION
    if (gamma <= 0) {
        ESP_LOGI("led_indicator", "gamma value should be greater than 0");
        return ESP_ERR_INVALID_ARG;
    }
    ENTER_CRITICAL();
    for (int i = 0; i < MAX_PROGRESS; i++) {
        gamma_table[i] = pow(i / 255.0, gamma) * 255;
    }
    EXIT_CRITICAL();
#endif
    return ESP_OK;
}
