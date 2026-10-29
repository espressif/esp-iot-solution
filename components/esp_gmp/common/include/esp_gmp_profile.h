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

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Link lifecycle events use `esp_gmp_link_event_type_t` from esp_gmp.h
 * (via esp_gmp_link_event_register / esp_gmp_notify_link_event).
 * Do not define a parallel link-event enum in this header.
 */

/**
 * @brief Unified event structure for all profiles.
 *
 * Profiles use this as a base structure and may extend via profile_extra.
 */
typedef struct {
    uint8_t   event_id;       /* Profile-specific event ID (STARTED/PROGRESS/COMPLETED/etc) */
    uint8_t   role;           /* Profile-specific role (e.g., SENDER/RECEIVER, HOST/DEVICE) */
    uint64_t  transferred;    /* Bytes transferred so far */
    uint64_t  total;          /* Total bytes in transfer */
    uint8_t   percent;        /* Progress percentage (0-100) */
    uint16_t  reason;         /* Profile-specific reason code */
    esp_err_t detail;         /* ESP error code with additional detail */
    void     *profile_extra;  /* Profile-specific extension (valid only during callback) */
} esp_gmp_profile_event_t;

/**
 * @brief Profile lifecycle contract.
 *
 * All device-side profiles (FT both roles, OTA device) must implement:
 *   - init(config) / deinit()
 *   - on_packet(pkt) - may take ownership of frame_buf by returning true
 *   - on_link_event(link, event, err)
 *
 * Host-side profiles (OTA host) have different entry points:
 *   - init(config) / deinit()
 *   - on_link_event(link, event, err)
 *   - Active APIs like upload_stream() instead of passive on_packet()
 */

/**
 * @brief Profile instance model.
 *
 * All profiles use "global singleton entry point + internal per-link session array".
 * - init/deinit take no handle and do not return an instance pointer
 * - Internally, profiles maintain an array indexed by esp_gmp_link_t
 * - This allows one device to serve multiple links concurrently
 *
 * Configuration passed to init() specifies global resources (worker task, queues)
 * but does not carry a specific link - runtime state is split by link internally.
 */

#ifdef __cplusplus
}
#endif
