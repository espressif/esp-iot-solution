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
#include "esp_xiaozhi_protocol.h"

typedef struct esp_xiaozhi_udp_s *esp_xiaozhi_udp_handle_t;
typedef void (*esp_xiaozhi_udp_audio_cb_t)(void *ctx, const uint8_t *data, size_t length, uint32_t timestamp);
typedef void (*esp_xiaozhi_udp_error_cb_t)(void *ctx, esp_err_t error, const char *source);

esp_err_t esp_xiaozhi_udp_init(esp_xiaozhi_udp_handle_t *handle, esp_xiaozhi_udp_audio_cb_t audio,
                               esp_xiaozhi_udp_error_cb_t error, void *ctx);
esp_err_t esp_xiaozhi_udp_configure(esp_xiaozhi_udp_handle_t handle, const char *server, int port,
                                    const char *key_hex, const char *nonce_hex, bool probe);
esp_err_t esp_xiaozhi_udp_open(esp_xiaozhi_udp_handle_t handle);
esp_err_t esp_xiaozhi_udp_close(esp_xiaozhi_udp_handle_t handle);
esp_err_t esp_xiaozhi_udp_deinit(esp_xiaozhi_udp_handle_t handle);
esp_err_t esp_xiaozhi_udp_send(esp_xiaozhi_udp_handle_t handle, const uint8_t *data, size_t length, uint32_t timestamp);
bool esp_xiaozhi_udp_is_open(esp_xiaozhi_udp_handle_t handle);
void esp_xiaozhi_udp_note_activity(esp_xiaozhi_udp_handle_t handle);

esp_err_t esp_xiaozhi_udp_get_health(esp_xiaozhi_udp_handle_t udp, esp_xiaozhi_udp_health_info_t *info);
bool esp_xiaozhi_udp_probe_mac(const uint8_t key[16], const uint8_t header[16], uint8_t tag[16]);
