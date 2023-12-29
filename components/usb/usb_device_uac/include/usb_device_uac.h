/*
 * SPDX-FileCopyrightText: 2023-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef USB_DEVICE_UAC_H
#define USB_DEVICE_UAC_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stdint.h"

typedef esp_err_t (*uac_output_cb_t)(uint8_t *buf, uint32_t len, void *cb_ctx);
typedef esp_err_t (*uac_input_cb_t)(uint8_t *buf, uint32_t len, uint32_t *bytes_read, void *cb_ctx);
typedef void (*uac_set_mute_cb_t)(uint32_t mute, void *cb_ctx);
typedef void (*uac_set_volume_cb_t)(uint32_t volume, void *cb_ctx);

typedef struct  {
    uac_output_cb_t output_cb;                   /*!< callback function for output endpoint, if NULL, the output endpoint will be disabled */
    uac_input_cb_t input_cb;                     /*!< callback function for input endpoint, if NULL, the input endpoint will be disabled */
    uac_set_mute_cb_t set_mute_cb;               /*!< callback function for set mute, if NULL, the set mute request will be ignored */
    uac_set_volume_cb_t set_volume_cb;           /*!< callback function for set volume, if NULL, the set volume request will be ignored */
    void *cb_ctx;                                /*!< callback context, for user specific usage */
} uac_device_config_t;

/**
 * @brief
 *
 * @param config
 * @return esp_err_t
 */
esp_err_t uac_device_init(uac_device_config_t *config);

#ifdef __cplusplus
}
#endif

#endif
