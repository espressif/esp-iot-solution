/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "ble_esl_ap.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ESL_PRODUCT_MAX_TAGS           8
#define ESL_PRODUCT_KEY_LEN            16
#define ESL_PRODUCT_IV_LEN             8
#define ESL_PRODUCT_ADDR_LEN           6

#define ESL_INVENTORY_MAGIC            0x45534C31u    /* "ESL1" */
#define ESL_INVENTORY_SCHEMA_VER       1

#define ESL_PRODUCT_CAP_DISPLAY        (1u << 0)
#define ESL_PRODUCT_CAP_LED            (1u << 1)
#define ESL_PRODUCT_CAP_SENSOR         (1u << 2)

/** Persistent provision transaction progress */
typedef enum {
    ESL_PRODUCT_PHASE_INVALID = 0,
    ESL_PRODUCT_PHASE_PREPARED,       /*!< ESL_ID/address reserved, pairing not confirmed */
    ESL_PRODUCT_PHASE_BONDED,         /*!< NimBLE bond confirmed */
    ESL_PRODUCT_PHASE_CONFIGURED,     /*!< ESL config (id/group/keys) written to TAG */
    ESL_PRODUCT_PHASE_ACTIVE,         /*!< First sync confirmation succeeded */
} esl_product_provision_phase_t;

/** Inventory entry desired state */
typedef enum {
    ESL_PRODUCT_DESIRED_INVALID = 0,
    ESL_PRODUCT_DESIRED_ACTIVE,
    ESL_PRODUCT_DESIRED_DISABLED,
    ESL_PRODUCT_DESIRED_RETIRED,
} esl_product_desired_state_t;

/** QUARANTINED reason; meaningful only when product_state == QUARANTINED */
typedef enum {
    ESL_PRODUCT_QUARANTINE_INVALID = 0,
    ESL_PRODUCT_QUARANTINE_NONE,
    ESL_PRODUCT_QUARANTINE_BOND_MISMATCH,
    ESL_PRODUCT_QUARANTINE_IDENTITY_UNCERTAIN,
    ESL_PRODUCT_QUARANTINE_KEY_MISSING,
    ESL_PRODUCT_QUARANTINE_RECORD_CORRUPT,
    ESL_PRODUCT_QUARANTINE_TAG_UNASSOCIATED, /*!< Known TAG advertising Unassociated (ESL UUID) */
} esl_product_quarantine_reason_t;

/** Product availability (RAM-only; coordinator-owned when phase == ACTIVE) */
typedef enum {
    ESL_PRODUCT_STATE_INVALID = 0,
    ESL_PRODUCT_STATE_RECOVERING,
    ESL_PRODUCT_STATE_ONLINE,
    ESL_PRODUCT_STATE_RETRY_WAIT,
    ESL_PRODUCT_STATE_OFFLINE,
    ESL_PRODUCT_STATE_QUARANTINED,
} esl_product_state_t;

/** Failure / health reason (Gateway failure_reason) */
typedef enum {
    ESL_AP_FAIL_INVALID = 0,
    ESL_AP_FAIL_NONE,
    ESL_AP_FAIL_UNREACHABLE,
    ESL_AP_FAIL_ACL_TIMEOUT,
    ESL_AP_FAIL_ACL_ENC_FAILED,
    ESL_AP_FAIL_WRITE_NOT_PERMITTED,
    ESL_AP_FAIL_PAST_FAILED,
    ESL_AP_FAIL_PING_TIMEOUT,
    ESL_AP_FAIL_TIME_SYNC_FAILED,
    ESL_AP_FAIL_HEALTH_MISS,
    ESL_AP_FAIL_ADVERTISING_DETECTED,
    ESL_AP_FAIL_BOND_MISMATCH,
    ESL_AP_FAIL_IDENTITY_UNCERTAIN,
    ESL_AP_FAIL_KEY_MISSING,
    ESL_AP_FAIL_RECORD_CORRUPT,
} esl_ap_failure_reason_t;

/** AP-wide lifecycle */
typedef enum {
    ESL_AP_LIFECYCLE_INVALID = 0,
    ESL_AP_LIFECYCLE_BOOTING,
    ESL_AP_LIFECYCLE_RECOVERING,
    ESL_AP_LIFECYCLE_READY,
    ESL_AP_LIFECYCLE_FAILED,
} esl_ap_lifecycle_t;

typedef enum {
    ESL_AP_ERROR_INVALID = 0,
    ESL_AP_ERROR_NONE,
    ESL_AP_ERROR_PAWR_INIT_FAILED,
    ESL_AP_ERROR_CONTROLLER_INIT_FAILED,
    ESL_AP_ERROR_INVENTORY_UNREADABLE,
    ESL_AP_ERROR_KEY_STORE_UNAVAILABLE,
} esl_ap_error_code_t;

typedef struct {
    esl_ap_lifecycle_t lifecycle;
    bool retryable;
    esl_ap_error_code_t error_code;
    uint16_t retry_count;
    int64_t next_retry_uptime_us;
} esl_ap_lifecycle_status_t;

/**
 * Fixed-width NVS wire record. Multi-byte fields are little-endian on the wire;
 * use serializer helpers — do not memcpy this struct as a host-endian blob.
 */
typedef struct __attribute__((packed))
{
    uint8_t  valid;                  /*!< 0/1 */
    uint8_t  addr[ESL_PRODUCT_ADDR_LEN];
    uint8_t  addr_type;
    uint8_t  esl_id;
    uint8_t  group_id;

    uint8_t  associated;             /*!< 0/1 */
    uint8_t  desired_state;          /*!< esl_product_desired_state_t */
    uint8_t  provision_phase;        /*!< esl_product_provision_phase_t */
    uint8_t  quarantine_reason;      /*!< esl_product_quarantine_reason_t */
    uint32_t record_generation_le;   /*!< nonzero when valid */
    uint16_t attempt_count;
    uint8_t  last_failure_reason;    /*!< esl_ap_failure_reason_t persistable subset */

    uint8_t  ap_sync_key[ESL_PRODUCT_KEY_LEN];
    uint8_t  ap_sync_iv[ESL_PRODUCT_IV_LEN];
    uint8_t  resp_key[ESL_PRODUCT_KEY_LEN];
    uint8_t  resp_iv[ESL_PRODUCT_IV_LEN];

    uint8_t  num_displays;
    uint8_t  max_image_index;
    uint8_t  capability_flags;       /*!< bit0 display, bit1 LED, bit2 sensor */

    uint8_t  image_known;
    uint8_t  current_image_index;
    uint32_t image_version_le;
    uint32_t image_length_le;
    uint32_t image_crc32_le;
} esl_product_tag_wire_t;

/** Boot-local runtime; rebuilt by app_inventory_restore_runtime(), never persisted */
typedef struct {
    ble_esl_state_t   esl_runtime_state;
    esl_product_state_t      product_state;
    bool                     time_synced;
    uint16_t                 conn_handle;
    uint32_t                 recovery_generation;
    int64_t                  retry_deadline_us;
    int64_t                  last_seen_uptime_us;
    uint16_t                 miss_count;
    uint16_t                 retry_count;
    esl_ap_failure_reason_t  failure_reason;
    uint32_t                 bound_record_generation;
} esl_ap_tag_runtime_t;

typedef struct __attribute__((packed))
{
    uint32_t magic_le;
    uint16_t schema_version_le;
    uint16_t header_length_le;
    uint16_t record_length_le;
    uint16_t max_records_le;
    uint8_t  count;
    uint8_t  reserved[3];
    uint32_t generation_le;
    uint32_t checksum_le;
} esl_inventory_header_t;

typedef struct __attribute__((packed))
{
    esl_inventory_header_t   header;
    esl_product_tag_wire_t   tags[ESL_PRODUCT_MAX_TAGS];
} esl_inventory_blob_t;

/** Orphan bond diagnostic (RAM-only, rebuilt each boot) */
typedef struct {
    uint8_t peer_id_addr[ESL_PRODUCT_ADDR_LEN];
    uint8_t addr_type;
    int64_t first_seen_uptime_us;
} esl_ap_orphan_bond_t;

/**
 * Legacy inventory layout (pre-schema). Kept only for NVS migration size/field
 * matching. New code must use wire + runtime.
 */
typedef struct {
    uint8_t addr[ESL_PRODUCT_ADDR_LEN];
    uint8_t addr_type;
    uint16_t conn_handle;
    uint16_t esl_addr;
    uint8_t esl_id;
    uint8_t group_id;
    ble_esl_state_t state;
    uint8_t current_image_index;
    uint8_t num_displays;
    uint8_t max_image_index;
    bool has_display;
    bool has_led;
    bool has_sensor;
    uint8_t ap_sync_key[ESL_PRODUCT_KEY_LEN];
    uint8_t ap_sync_iv[ESL_PRODUCT_IV_LEN];
    uint8_t resp_key[ESL_PRODUCT_KEY_LEN];
    uint8_t resp_iv[ESL_PRODUCT_IV_LEN];
    bool valid;
} esl_product_tag_record_legacy_t;

typedef struct {
    uint8_t selected_index;
    uint8_t count;
    esl_product_tag_record_legacy_t tags[ESL_PRODUCT_MAX_TAGS];
} esl_product_inventory_legacy_t;

/**
 * Transitional RAM-facing view combining wire + runtime pointers for a slot.
 * Prefer accessing wire/runtime directly via app_inventory helpers.
 */
typedef struct {
    esl_product_tag_wire_t *wire;
    esl_ap_tag_runtime_t *rt;
    uint8_t slot;
} esl_product_tag_record_t;

/** RAM inventory facade (wire blob + per-slot runtime + selection) */
typedef struct {
    esl_inventory_blob_t blob;
    esl_ap_tag_runtime_t runtime[ESL_PRODUCT_MAX_TAGS];
    uint8_t selected_index;
    uint8_t count;   /*!< mirror of blob.header.count for call-site convenience */
} esl_product_inventory_t;

_Static_assert(sizeof(esl_product_tag_wire_t) == 86, "esl_product_tag_wire_t size drift");
_Static_assert(sizeof(esl_inventory_header_t) == 24, "esl_inventory_header_t size drift");
_Static_assert(sizeof(esl_inventory_blob_t) ==
               sizeof(esl_inventory_header_t) +
               ESL_PRODUCT_MAX_TAGS * sizeof(esl_product_tag_wire_t),
               "esl_inventory_blob_t size drift");

static inline uint16_t esl_product_wire_esl_addr(const esl_product_tag_wire_t *w)
{
    if (w == NULL) {
        return 0;
    }
    return (uint16_t)(w->esl_id | ((uint16_t)w->group_id << 8));
}

static inline ble_esl_address_t esl_product_pack_esl_addr(uint8_t esl_id, uint8_t group_id)
{
    return ble_esl_addr_make(esl_id, group_id);
}

static inline ble_esl_address_t esl_product_pack_esl_addr_u16(uint16_t wire_addr)
{
    return ble_esl_addr_make((uint8_t)(wire_addr & 0xFF), (uint8_t)((wire_addr >> 8) & 0x7F));
}

static inline uint16_t esl_product_unpack_esl_addr(ble_esl_address_t addr)
{
    return (uint16_t)(addr.esl_id | ((uint16_t)BLE_ESL_ADDR_GROUP_ID(addr) << 8));
}

static inline bool esl_product_esl_addr_eq(ble_esl_address_t a, ble_esl_address_t b)
{
    return a.esl_id == b.esl_id && BLE_ESL_ADDR_GROUP_ID(a) == BLE_ESL_ADDR_GROUP_ID(b);
}

static inline uint32_t esl_product_rd_le32(uint32_t le)
{
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    return le;
#else
    return __builtin_bswap32(le);
#endif
}

static inline uint16_t esl_product_rd_le16(uint16_t le)
{
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    return le;
#else
    return (uint16_t)__builtin_bswap16(le);
#endif
}

static inline uint32_t esl_product_wr_le32(uint32_t v)
{
    return esl_product_rd_le32(v);
}

static inline uint16_t esl_product_wr_le16(uint16_t v)
{
    return esl_product_rd_le16(v);
}

#ifdef __cplusplus
}
#endif
