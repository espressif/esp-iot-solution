/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stddef.h>
#include <stdint.h>
#include "cJSON.h"
#include "esp_xiaozhi_protocol.h"

esp_err_t esp_xiaozhi_protocol_add_capabilities(cJSON *root, bool include_font);
bool esp_xiaozhi_protocol_probe_enabled(void);
esp_err_t esp_xiaozhi_protocol_read_session_id(const cJSON *root, char *session, size_t capacity);
bool esp_xiaozhi_protocol_is_legacy_initialize(const cJSON *request);
esp_err_t esp_xiaozhi_protocol_encode_audio(int version, const uint8_t *data, size_t length,
                                            uint32_t timestamp, uint8_t *output, size_t capacity, size_t *written);
esp_err_t esp_xiaozhi_protocol_decode_frame(int version, const uint8_t *data, size_t length,
                                            const uint8_t **payload, size_t *payload_length,
                                            uint32_t *timestamp, unsigned *type);
