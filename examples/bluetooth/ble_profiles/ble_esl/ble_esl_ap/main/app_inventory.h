/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "esp_err.h"
#include "esl_product_types.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef ESL_AP_MAX_ORPHAN_BONDS
#define ESL_AP_MAX_ORPHAN_BONDS  8
#endif

esp_err_t app_inventory_init(void);

/**
 * Load wire blob from NVS and clear boot-local runtime.
 * On ESP_ERR_INVALID_CRC / ESP_ERR_INVALID_STATE the blob is unreadable;
 * caller should drive AP lifecycle FAILED(INVENTORY_UNREADABLE).
 */
esp_err_t app_inventory_load(void);
esp_err_t app_inventory_save(void);

esl_product_inventory_t *app_inventory_get(void);
esl_inventory_blob_t *app_inventory_get_blob(void);
esl_ap_tag_runtime_t *app_inventory_get_runtime(uint8_t slot);

/** Fill a transitional view for slot; returns false if slot invalid/out of range. */
bool app_inventory_tag_view(uint8_t slot, esl_product_tag_record_t *out);

esl_product_tag_wire_t *app_inventory_selected_wire(void);
esl_product_tag_record_t *app_inventory_selected_tag(void);

/**
 * Persist a PREPARED provisioning transaction before opening the ACL.
 * Re-entering with the same peer and ESL address resumes an incomplete
 * PREPARED/BONDED/CONFIGURED transaction.
 */
esp_err_t app_inventory_prepare_provision(const uint8_t addr[6], uint8_t addr_type,
                                          uint8_t esl_id, uint8_t group_id);

esp_err_t app_inventory_upsert_from_connection(uint16_t conn_handle, const uint8_t addr[6],
                                               uint8_t addr_type, bool has_display,
                                               bool has_led, bool has_sensor);
esp_err_t app_inventory_release_conn(uint16_t conn_handle);
esl_product_tag_wire_t *app_inventory_find_wire_for_conn(uint16_t conn_handle, const uint8_t addr[6]);
esl_product_tag_record_t *app_inventory_find_for_conn(uint16_t conn_handle, const uint8_t addr[6]);

esp_err_t app_inventory_set_configured(uint16_t conn_handle, const uint8_t addr[6],
                                       uint8_t esl_id, uint8_t group_id,
                                       const uint8_t ap_sync_key[16], const uint8_t ap_sync_iv[8],
                                       const uint8_t resp_key[16], const uint8_t resp_iv[8]);

/**
 * Mark first sync confirmation: provision_phase ACTIVE, associated=1.
 * Does NOT claim product ONLINE from NVS alone — runtime stays for coordinator.
 */
esp_err_t app_inventory_set_synchronized(uint16_t esl_addr, uint8_t current_image_index);

esp_err_t app_inventory_select_next(void);

/** True if any ACTIVE desired-ACTIVE tag is product ONLINE with time_synced. */
bool app_inventory_has_synchronized_tag(void);

esl_product_tag_wire_t *app_inventory_find_wire_by_addr(const uint8_t addr[6]);
esl_product_tag_record_t *app_inventory_find_by_addr(const uint8_t addr[6]);

uint16_t app_inventory_esl_addr_wire(const esl_product_tag_wire_t *wire);
uint16_t app_inventory_esl_addr(const esl_product_tag_record_t *rec);

/**
 * Seed ble_esl runtime UNSYNCHRONIZED + product RECOVERING for
 * ACTIVE + desired ACTIVE + no quarantine. Quarantine → QUARANTINED.
 * PREPARED/BONDED/CONFIGURED skip recovery eligibility (placeholder RECOVERING).
 */
esp_err_t app_inventory_restore_runtime(void);

bool app_inventory_has_addr(const uint8_t addr[6]);
bool app_inventory_esl_id_in_use(uint8_t esl_id, uint8_t group_id);
/**
 * Allocate next free esl_id within the preferred group.
 * @param[out] esl_id Allocated ESL ID
 * @param[in,out] group_id Preferred group on input; unchanged on success
 */
esp_err_t app_inventory_allocate_esl_id(uint8_t *esl_id, uint8_t *group_id);

/** Invalidate slot (valid=0); does not compact. */
esp_err_t app_inventory_remove(uint8_t esl_id, uint8_t group_id);

/** Cancel PREPARED/BONDED/CONFIGURED and remove its peer bond and fixed slot. */
esp_err_t app_inventory_cancel_provision(uint8_t esl_id, uint8_t group_id);

/** Reconcile NimBLE bonds vs inventory; may quarantine BOND_MISMATCH. */
esp_err_t app_inventory_reconcile_bonds(void);

const esl_ap_orphan_bond_t *app_inventory_orphan_bonds(uint8_t *out_count);

/** True if runtime.bound_record_generation matches current wire slot generation. */
bool app_inventory_runtime_generation_valid(uint8_t slot);

void app_inventory_dump(const char *reason);

#ifdef __cplusplus
}
#endif
