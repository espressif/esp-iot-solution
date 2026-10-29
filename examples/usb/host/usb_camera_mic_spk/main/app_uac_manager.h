/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    APP_UAC_STREAM_MIC = 0,
    APP_UAC_STREAM_SPK,
} app_uac_stream_id_t;

esp_err_t app_uac_manager_start(void);
esp_err_t app_uac_get_state_json(char *buf, size_t len);
esp_err_t app_uac_set_enabled(app_uac_stream_id_t stream, bool enabled);
esp_err_t app_uac_set_mute(app_uac_stream_id_t stream, bool mute);
esp_err_t app_uac_set_volume(app_uac_stream_id_t stream, uint8_t volume);
esp_err_t app_uac_set_format(app_uac_stream_id_t stream, uint8_t alt, uint32_t sample_freq);
esp_err_t app_uac_queue_spk_pcm(const uint8_t *data, size_t len);
esp_err_t app_uac_request_spk_refill_budget(uint32_t *bytes);
void app_uac_reset_spk_refill_budget(void);
esp_err_t app_uac_clear_spk_queue(void);

#ifdef __cplusplus
}
#endif
