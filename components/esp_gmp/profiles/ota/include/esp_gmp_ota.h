/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"
#include "esp_gmp.h"
#include "esp_gmp_profile.h"

#ifdef __cplusplus
extern "C" {
#endif

/** OTA device role value for esp_gmp_profile_event_t.role */
#define ESP_GMP_OTA_ROLE_DEVICE 1
#define ESP_GMP_OTA_ROLE_HOST   2

/** Shared OTA event_id values for esp_gmp_profile_event_t */
#define ESP_GMP_OTA_EVT_STARTED    1
#define ESP_GMP_OTA_EVT_PROGRESS   2
#define ESP_GMP_OTA_EVT_VERIFYING  3
#define ESP_GMP_OTA_EVT_COMPLETED  4
#define ESP_GMP_OTA_EVT_FAILED     5
#define ESP_GMP_OTA_EVT_CANCELLED  6

/**
 * @brief Optional START accept hook.
 *
 * Return false to reject the OTA image. NULL accept_cb means accept all.
 */
typedef bool (*esp_gmp_ota_accept_cb_t)(uint32_t image_size, void *ctx);

/**
 * @brief Device-side progress / status callback (unified profile event).
 */
typedef void (*esp_gmp_ota_event_cb_t)(const esp_gmp_profile_event_t *event, void *ctx);

/**
 * @brief When to reboot after a successful OTA apply.
 */
typedef enum {
    /** Schedule restart after restart_delay_ms (default). */
    ESP_GMP_OTA_APPLY_RESTART_DELAYED = 0,
    /** Restart as soon as boot partition is set (delay 0). */
    ESP_GMP_OTA_APPLY_RESTART_IMMEDIATE,
    /** Set boot partition only; application calls esp_gmp_ota_apply() / restarts. */
    ESP_GMP_OTA_APPLY_MANUAL,
} esp_gmp_ota_apply_policy_t;

typedef struct {
    /** OTA partition label; NULL uses esp_ota_get_next_update_partition(). */
    const char *ota_partition_label;
    /**
     * Suggested OTA_UPLOAD_DATA.data bytes per chunk in start response.
     * 0 = auto from esp_gmp_max_payload_effective(link) - 8.
     */
    uint16_t chunk_hint;
    /** Delay before esp_restart() after successful apply (ms). Ignored for MANUAL. */
    uint32_t restart_delay_ms;
    esp_gmp_ota_apply_policy_t apply_policy;
    esp_gmp_ota_accept_cb_t accept_cb; /* NULL = accept all */
    void *accept_ctx;
    esp_gmp_ota_event_cb_t event_cb; /* optional; may be NULL */
    void *event_ctx;
} esp_gmp_ota_config_t;

/** Snapshot of one link's OTA device session. */
typedef struct {
    bool in_progress;
    esp_gmp_link_t link;
    uint8_t session_id;
    uint64_t transferred;
    uint64_t total;
    uint8_t percent;
} esp_gmp_ota_status_t;

/** Initialize OTA handler. @p cfg may be NULL for Kconfig defaults. */
esp_err_t esp_gmp_ota_init(const esp_gmp_ota_config_t *cfg);

void esp_gmp_ota_deinit(void);

/**
 * Dispatch GRP_OTA packets from esp_gmp_on_packet.
 * Handles OTA_UPLOAD_CONTROL / DATA / QUERY and sends GMP responses.
 *
 * Prefer profile self-registration via esp_gmp_ota_init(); applications
 * normally need not call this.
 */
/** Return true when rx->frame_buf ownership is taken (OTA DATA zero-copy path). */
bool esp_gmp_ota_on_packet(const esp_gmp_rx_t *pkt);

/**
 * Call on link disconnect or esp_gmp_link_unregister.
 * Also invoked automatically via link-event subscription.
 */
void esp_gmp_ota_on_link_down(esp_gmp_link_t link);

/**
 * @brief Query status for @p link (NULL = first active session).
 */
esp_err_t esp_gmp_ota_get_status(esp_gmp_link_t link, esp_gmp_ota_status_t *status);

/**
 * @brief For APPLY_MANUAL: reboot now after a successful finish (boot partition already set).
 * No-op / error if no pending manual apply.
 */
esp_err_t esp_gmp_ota_apply(void);

#ifdef __cplusplus
}
#endif
