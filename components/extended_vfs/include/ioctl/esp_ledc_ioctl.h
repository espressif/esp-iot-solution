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

#define _LEDCCBASE              (0x8400) /*!< LEDC ioctl command basic value */
#define _LEDCC(nr)              (_LEDCCBASE | (nr)) /*!< LEDC ioctl command macro */

#define LEDCIOCSCFG             _LEDCC(1) /*!< Set LEDC configuration */
#define LEDCIOCSSETFREQ         _LEDCC(2) /*!< Set LEDC frequency */
#define LEDCIOCSSETDUTY         _LEDCC(3) /*!< Set LEDC duty of target channel */
#define LEDCIOCSSETPHASE        _LEDCC(4) /*!< Set LEDC phase of target channel */
#define LEDCIOCSPAUSE           _LEDCC(5) /*!< Set LEDC to pause */
#define LEDCIOCSRESUME          _LEDCC(6) /*!< Set LEDC yo resume from pausing */

/**
 * @brief LEDC channel configuration.
 */
typedef struct ledc_channel_cfg {
    uint8_t output_pin;     /*!< LEDC channel signal output pin */
    uint32_t duty;          /*!< LEDC channel signal duty */
    uint32_t phase;         /*!< LEDC channel signal phase */
} ledc_channel_cfg_t;

/**
 * @brief LEDC configuration.
 */
typedef struct ledc_cfg {
    uint32_t frequency;     /*!< LEDC Frequency */
    uint8_t channel_num;    /*!< LEDC channel number */
    const ledc_channel_cfg_t *channel_cfg; /*!< LEDC channels configuration */
} ledc_cfg_t;

typedef struct ledc_duty_cfg {
    uint8_t channel;
    uint32_t duty;
} ledc_duty_cfg_t;

typedef struct ledc_phase_cfg {
    uint8_t channel;
    uint32_t phase;
} ledc_phase_cfg_t;

#ifdef __cplusplus
}
#endif
