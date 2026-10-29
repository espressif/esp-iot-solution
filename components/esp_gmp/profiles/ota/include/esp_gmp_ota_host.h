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
#include "esp_gmp.h"
#include "esp_gmp_ota.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    esp_gmp_link_t link;
    uint16_t chunk_size;
    uint32_t max_gmp_payload;
} esp_gmp_ota_host_caps_t;

typedef struct {
    /** Optional progress / status callback; may be NULL. */
    esp_gmp_ota_event_cb_t event_cb;
    void *event_ctx;
} esp_gmp_ota_host_config_t;

/**
 * Read @p len bytes at @p offset into @p buf; set @p out_len to bytes read.
 */
typedef esp_err_t (*esp_gmp_ota_host_read_fn_t)(void *ctx, size_t offset, uint8_t *buf,
                                                size_t len, size_t *out_len);

esp_err_t esp_gmp_ota_host_init(const esp_gmp_ota_host_config_t *cfg);
void esp_gmp_ota_host_deinit(void);

void esp_gmp_ota_host_on_link_event(esp_gmp_link_t link, esp_gmp_link_event_type_t ev,
                                    esp_err_t err);

/**
 * Consume GRP_OTA / GRP_OS response packets for an in-flight host transfer.
 *
 * Host registers its own handlers for GRP_OTA and GRP_OS; GMP delivers each
 * packet to every co-registered handler for that group. Callers may also invoke
 * this directly when forwarding from another path.
 *
 * @return false (never takes frame_buf ownership).
 */
bool esp_gmp_ota_host_on_rsp(const esp_gmp_rx_t *pkt);

esp_err_t esp_gmp_ota_host_upload_stream(esp_gmp_link_t link, size_t image_len,
                                         esp_gmp_ota_host_read_fn_t read_fn, void *read_ctx);

esp_err_t esp_gmp_ota_host_upload(esp_gmp_link_t link, const uint8_t *image, size_t image_len);

esp_err_t esp_gmp_ota_host_query_caps(esp_gmp_link_t link, esp_gmp_ota_host_caps_t *out);

void esp_gmp_ota_host_cancel(void);

#ifdef __cplusplus
}
#endif
