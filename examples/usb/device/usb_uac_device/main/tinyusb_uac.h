/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "tusb.h"
#include "esp_err.h"
#include "usb_descriptors.h"

#define SAMPLE_RATE      CFG_TUD_AUDIO_FUNC_1_MAX_SAMPLE_RATE
#define CHANNEL_MIC      CFG_TUD_AUDIO_FUNC_1_N_CHANNELS_TX
#define CHANNEL_SPK      CFG_TUD_AUDIO_FUNC_1_N_CHANNELS_RX
#define DEFAULT_VOLUME  (80)
//#define WIDTH           (16)

/**
 * @brief Initialize the tinyusb headset function
 *
 * @return esp_err_t
 *         ESP_OK   Success
 *         ESP_FAIL Failed
 */
esp_err_t tinyusb_uac_init(void);

#ifdef __cplusplus
}
#endif
