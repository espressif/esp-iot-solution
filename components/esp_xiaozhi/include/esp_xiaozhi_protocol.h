/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
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

/**
 * @brief  Protocol capabilities shared between HTTP bootstrap and transport hello
 */
typedef struct {
    bool mcp;                  /*!< Advertise legacy MCP support */
    bool mcp_20260728;         /*!< Advertise MCP 2026-07-28 discovery/tools support (requires @c mcp) */
    bool udp_probe;            /*!< Advertise authenticated MQTT+UDP probe support */
    bool aec;                  /*!< Advertise server/device AEC feature flag */
    bool glyph_push;           /*!< Advertise text_font / glyph push support */
    const char *font_bundle;   /*!< Font bundle identifier when @c glyph_push is true */
    const char *font_charset;  /*!< Font charset identifier when @c glyph_push is true */
    unsigned font_size;        /*!< Font pixel size when @c glyph_push is true */
    unsigned font_bpp;         /*!< Font bits per pixel when @c glyph_push is true */
} esp_xiaozhi_protocol_capabilities_t;

/**
 * @brief  MQTT+UDP authenticated probe health state
 */
typedef enum {
    ESP_XIAOZHI_UDP_HEALTH_CLOSED,            /*!< UDP channel is closed */
    ESP_XIAOZHI_UDP_HEALTH_LEGACY_UNVERIFIED, /*!< Open without authenticated probe verification */
    ESP_XIAOZHI_UDP_HEALTH_PROBING,           /*!< Open and awaiting first authenticated PONG */
    ESP_XIAOZHI_UDP_HEALTH_HEALTHY,           /*!< Open with authenticated PONG verified */
    ESP_XIAOZHI_UDP_HEALTH_FAILED,            /*!< Authenticated probe timed out or failed */
} esp_xiaozhi_udp_health_t;

/**
 * @brief  Independent UDP health snapshot (separate from business/audio channel readiness)
 */
typedef struct {
    esp_xiaozhi_udp_health_t state;  /*!< Current UDP probe health state */
    int64_t last_pong_time_us;       /*!< Monotonic timestamp in us, zero until first authenticated PONG */
    uint32_t probe_sequence;         /*!< Current probe sequence number */
    unsigned attempts;               /*!< Attempts sent for the current probe round */
} esp_xiaozhi_udp_health_info_t;

/**
 * @brief  Configure protocol capabilities before bootstrap and chat start
 *
 *         Copies all strings. Glyph rendering and AEC are supplied by the
 *         application; advertise them only when the corresponding external
 *         component is ready. Passing NULL restores defaults: MCP (both
 *         revisions), UDP probe enabled, AEC and glyph push disabled.
 *         Configuration must not race an active bootstrap or chat operation.
 *
 * @param[in] capabilities  Capabilities configuration to apply, or NULL to restore defaults
 *
 * @return
 *       - ESP_OK               On success
 *       - ESP_ERR_INVALID_ARG  Invalid capability combination (e.g. mcp_20260728 without mcp, or invalid font fields)
 */
esp_err_t esp_xiaozhi_protocol_set_capabilities(const esp_xiaozhi_protocol_capabilities_t *capabilities);

#ifdef __cplusplus
}
#endif
