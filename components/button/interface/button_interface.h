/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct button_driver_t button_driver_t; /*!< Type of button object */

struct button_driver_t {
    /*!< (optional) Need Support Power Save */
    bool enable_power_save;

    /*!< (necessary) Get key level */
    uint8_t (*get_key_level)(button_driver_t *button_driver);

    /*!< (optional) Enter Power Save cb */
    esp_err_t (*enter_power_save)(button_driver_t *button_driver);

    /*!< (optional) Del the hardware driver and cleanup */
    esp_err_t (*del)(button_driver_t *button_driver);
};

#ifdef __cplusplus
}
#endif
