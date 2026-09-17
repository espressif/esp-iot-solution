/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "esl_product_types.h"
#include "ble_esl_ap.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t app_recovery_init(void);

/**
 * Start hard-reboot recovery after inventory restore_runtime.
 * Generates boot_id, enters RECOVERING (or READY if no eligible tags).
 */
esp_err_t app_recovery_start(void);

/** Inventory blob unreadable: FAILED(retryable=false, INVENTORY_UNREADABLE). */
void app_recovery_set_inventory_unreadable(void);

uint64_t app_recovery_boot_id(void);
void app_recovery_get_lifecycle_status(esl_ap_lifecycle_status_t *out);
bool app_recovery_is_healthy(void);
bool app_recovery_is_degraded(void);

/** True while a recovery TX attempt is in flight (ACL, PAST, Ping settle/retry). */
bool app_recovery_tx_busy(void);

/** True while NUS must stay stopped to avoid racing recovery PAwR/AUX_CONNECT. */
bool app_recovery_blocks_gateway(void);

/** Pause proactive recovery (provision / unassociate). */
void app_recovery_set_paused(bool paused);

/**
 * Scan hit for inventory BLE identity while product RECOVERING/RETRY_WAIT.
 * @return true if recovery consumed the scan (matched inventory identity)
 */
bool app_recovery_on_scan(const ble_esl_ap_scan_result_t *scan);

/**
 * @return true if this connection belongs to the recovery ACL transaction
 */
bool app_recovery_on_connected(const ble_esl_ap_conn_info_t *info);

/**
 * @return true if disconnect was for the recovery transaction
 */
bool app_recovery_on_disconnected(const ble_esl_ap_disconn_info_t *info);

/**
 * Absolute Time write result for the active recovery tag.
 * @return true if recovery owned this write completion
 */
bool app_recovery_on_abs_time_written(const ble_esl_ap_abs_time_written_t *info);

/**
 * PAST / Update Complete result for the active recovery tag.
 * @return true if recovery owned this synchronize completion
 */
bool app_recovery_on_synchronized(const ble_esl_ap_synchronized_t *info);

/** Feed PAwR responses into the arbiter (call from RESPONSE event path). */
void app_recovery_on_pawr_response(const esl_ap_response_t *resp);

/** Run connect/sync timeout body on app_esl_ap_task (posted from esp_timer). */
void app_recovery_handle_connect_timeout(void);

/** Run periodic recovery tick body on app_esl_ap_task (posted from esp_timer). */
void app_recovery_handle_tick(void);

/** Submit a health Ping through the arbiter (single-flight). */
esp_err_t app_recovery_submit_health_ping(uint8_t esl_id, uint8_t group_id,
                                          uint8_t record_slot, uint32_t record_generation);

#ifdef __cplusplus
}
#endif
