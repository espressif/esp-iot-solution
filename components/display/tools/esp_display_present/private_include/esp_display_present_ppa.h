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
#include "esp_display_present_rotate.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t esp_display_present_ppa_register_srm_client(int max_pending_trans_num, void **out_handle);

esp_err_t esp_display_present_ppa_unregister_client(void *handle);

/**
 * Best-effort PPA SRM client for tile rotate blits (FB partition / TE).
 *
 * Opens a client only for 90/270 when the platform supports PPA. On success
 * *@p inout_handle is non-NULL. Otherwise leaves it NULL so callers fall
 * back to software rotate; always returns ESP_OK for that soft-fail case.
 */
esp_err_t esp_display_present_ppa_try_open_tile_client(
    esp_display_present_rotation_t rotation,
    void **inout_handle);

void esp_display_present_ppa_close_tile_client(void **handle);

esp_err_t esp_display_present_ppa_rotate_copy(
    void *ppa_handle,
    const esp_display_present_rotate_copy_request_t *request);

/**
 * A source-local block placed at an already-resolved physical origin.
 *
 * PPA block offsets are physical coordinates, unlike a rotate-copy request's
 * logical area. Keeping that distinction in the type prevents mixing spaces.
 */
typedef struct {
    esp_display_present_blit_plane_t source;
    esp_display_present_area_t source_area;
    esp_display_present_blit_plane_t destination;
    esp_display_present_point_t destination_origin;
    esp_display_present_rotation_t rotation;
} esp_display_present_ppa_rotate_block_request_t;

esp_err_t esp_display_present_ppa_rotate_copy_block(
    void *ppa_handle,
    const esp_display_present_ppa_rotate_block_request_t *request);

#ifdef __cplusplus
}
#endif
