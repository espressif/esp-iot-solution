/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define _GPIOCBASE              (0x8100) /*!< GPIO ioctl command basic value */
#define _GPIOC(nr)              (_GPIOCBASE | (nr)) /*!< GPIO ioctl command macro */

/**
 * @brief GPIO ioctl commands.
 */
#define GPIOCSCFG               _GPIOC(0x0001) /*!< Set GPIO configuration */

/**
 * @brief GPIO configuration flag
 */
#define GPIOC_PULLDOWN_EN       (1 << 0)  /*!< Enable GPIO pin pull-up */
#define GPIOC_PULLUP_EN         (1 << 1)  /*!< Enable GPIO pin pull-down */
#define GPIOC_OPENDRAIN_EN      (1 << 2)  /*!< Enable GPIO pin open-drain */

/**
 * @brief GPIO configuration.
 */
typedef struct gpioc_cfg {
    union {
        struct {
            uint32_t pulldown_en   : 1;  /*!< Enable GPIO pin pull-up */
            uint32_t pullup_en     : 1;  /*!< Enable GPIO pin pull-down */
            uint32_t opendrain_en  : 1;  /*!< Enable GPIO pin open-drain */
        } flags_data;
        uint32_t flags; /*!< GPIO configuration flags */
    };
} gpioc_cfg_t;

#ifdef __cplusplus
}
#endif
