/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "esl_nvs.h"

#include <string.h>
#include "nvs.h"
#include "nvs_flash.h"
#include "esp_log.h"
#include "esp_rom_crc.h"

static const char *TAG = "esl_nvs";

#define ESL_NVS_NAMESPACE       "esl_prod"
#define ESL_NVS_KEY_INVENTORY   "inventory"

uint32_t esl_nvs_crc32(const void *buf, size_t len)
{
    /* CRC-32/ISO-HDLC via ESP ROM LE helper. esp_rom_crc32_le(0, ...) already
     * applies the ISO-HDLC init/final XOR internally (see esp_rom_crc.h), so
     * no additional inversion is needed here -- doing so would return the
     * bitwise-inverted value, contradicting the documented algorithm below. */
    return esp_rom_crc32_le(0, (const uint8_t *)buf, (uint32_t)len);
}

static uint32_t blob_compute_checksum(const esl_inventory_blob_t *blob)
{
    esl_inventory_blob_t tmp;

    memcpy(&tmp, blob, sizeof(tmp));
    tmp.header.checksum_le = 0;
    return esl_nvs_crc32(&tmp, sizeof(tmp));
}

void esl_nvs_blob_prepare_header(esl_inventory_blob_t *blob, uint32_t generation)
{
    uint8_t count = 0;

    if (blob == NULL) {
        return;
    }

    for (uint8_t i = 0; i < ESL_PRODUCT_MAX_TAGS; i++) {
        if (blob->tags[i].valid == 1) {
            count++;
        }
    }

    blob->header.magic_le = esl_product_wr_le32(ESL_INVENTORY_MAGIC);
    blob->header.schema_version_le = esl_product_wr_le16(ESL_INVENTORY_SCHEMA_VER);
    blob->header.header_length_le = esl_product_wr_le16((uint16_t)sizeof(esl_inventory_header_t));
    blob->header.record_length_le = esl_product_wr_le16((uint16_t)sizeof(esl_product_tag_wire_t));
    blob->header.max_records_le = esl_product_wr_le16(ESL_PRODUCT_MAX_TAGS);
    blob->header.count = count;
    memset(blob->header.reserved, 0, sizeof(blob->header.reserved));
    blob->header.generation_le = esl_product_wr_le32(generation);
    blob->header.checksum_le = 0;
    blob->header.checksum_le = esl_product_wr_le32(blob_compute_checksum(blob));
}

static bool wire_enum_ok(const esl_product_tag_wire_t *w)
{
    if (w->valid != 0 && w->valid != 1) {
        return false;
    }
    if (w->valid == 0) {
        return true;
    }
    if (w->associated != 0 && w->associated != 1) {
        return false;
    }
    if (w->image_known != 0 && w->image_known != 1) {
        return false;
    }
    if (esl_product_rd_le32(w->record_generation_le) == 0) {
        return false;
    }
    if (w->desired_state > ESL_PRODUCT_DESIRED_RETIRED) {
        return false;
    }
    if (w->provision_phase > ESL_PRODUCT_PHASE_ACTIVE) {
        return false;
    }
    if (w->quarantine_reason > ESL_PRODUCT_QUARANTINE_TAG_UNASSOCIATED) {
        return false;
    }
    if (w->last_failure_reason > ESL_AP_FAIL_RECORD_CORRUPT) {
        return false;
    }
    return true;
}

esp_err_t esl_nvs_blob_validate(const esl_inventory_blob_t *blob)
{
    if (blob == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (esl_product_rd_le32(blob->header.magic_le) != ESL_INVENTORY_MAGIC) {
        return ESP_ERR_INVALID_STATE;
    }
    if (esl_product_rd_le16(blob->header.schema_version_le) != ESL_INVENTORY_SCHEMA_VER) {
        return ESP_ERR_INVALID_STATE;
    }
    if (esl_product_rd_le16(blob->header.header_length_le) != sizeof(esl_inventory_header_t) ||
            esl_product_rd_le16(blob->header.record_length_le) != sizeof(esl_product_tag_wire_t) ||
            esl_product_rd_le16(blob->header.max_records_le) != ESL_PRODUCT_MAX_TAGS) {
        return ESP_ERR_INVALID_STATE;
    }

    uint32_t expect = blob_compute_checksum(blob);
    if (esl_product_rd_le32(blob->header.checksum_le) != expect) {
        return ESP_ERR_INVALID_CRC;
    }

    uint8_t valid_count = 0;
    for (uint8_t i = 0; i < ESL_PRODUCT_MAX_TAGS; i++) {
        if (!wire_enum_ok(&blob->tags[i])) {
            return ESP_ERR_INVALID_STATE;
        }
        if (blob->tags[i].valid == 1) {
            valid_count++;
        }
    }
    if (blob->header.count != valid_count) {
        return ESP_ERR_INVALID_STATE;
    }
    return ESP_OK;
}

static void migrate_legacy_record(const esl_product_tag_record_legacy_t *legacy,
                                  esl_product_tag_wire_t *wire,
                                  uint32_t record_generation)
{
    memset(wire, 0, sizeof(*wire));
    if (!legacy->valid) {
        return;
    }

    wire->valid = 1;
    memcpy(wire->addr, legacy->addr, ESL_PRODUCT_ADDR_LEN);
    wire->addr_type = legacy->addr_type;
    wire->esl_id = legacy->esl_id;
    wire->group_id = legacy->group_id;
    wire->record_generation_le = esl_product_wr_le32(record_generation);
    wire->current_image_index = legacy->current_image_index;
    wire->num_displays = legacy->num_displays;
    wire->max_image_index = legacy->max_image_index;
    wire->capability_flags = 0;
    if (legacy->has_display) {
        wire->capability_flags |= ESL_PRODUCT_CAP_DISPLAY;
    }
    if (legacy->has_led) {
        wire->capability_flags |= ESL_PRODUCT_CAP_LED;
    }
    if (legacy->has_sensor) {
        wire->capability_flags |= ESL_PRODUCT_CAP_SENSOR;
    }
    memcpy(wire->ap_sync_key, legacy->ap_sync_key, ESL_PRODUCT_KEY_LEN);
    memcpy(wire->ap_sync_iv, legacy->ap_sync_iv, ESL_PRODUCT_IV_LEN);
    memcpy(wire->resp_key, legacy->resp_key, ESL_PRODUCT_KEY_LEN);
    memcpy(wire->resp_iv, legacy->resp_iv, ESL_PRODUCT_IV_LEN);
    wire->quarantine_reason = ESL_PRODUCT_QUARANTINE_NONE;
    wire->desired_state = ESL_PRODUCT_DESIRED_ACTIVE;
    wire->last_failure_reason = ESL_AP_FAIL_NONE;

    if (legacy->state == BLE_ESL_STATE_SYNCHRONIZED) {
        wire->associated = 1;
        wire->provision_phase = ESL_PRODUCT_PHASE_ACTIVE;
    } else if (legacy->state == BLE_ESL_STATE_CONFIGURING) {
        wire->associated = 0;
        wire->provision_phase = ESL_PRODUCT_PHASE_CONFIGURED;
    } else {
        wire->associated = 0;
        wire->provision_phase = ESL_PRODUCT_PHASE_PREPARED;
    }
}

static esp_err_t migrate_legacy_inventory(const esl_product_inventory_legacy_t *legacy,
                                          esl_inventory_blob_t *out)
{
    memset(out, 0, sizeof(*out));
    uint32_t gen = 1;

    for (uint8_t i = 0; i < ESL_PRODUCT_MAX_TAGS; i++) {
        if (i < legacy->count && legacy->tags[i].valid) {
            migrate_legacy_record(&legacy->tags[i], &out->tags[i], gen++);
        }
    }

    esl_nvs_blob_prepare_header(out, 1);
    return esl_nvs_blob_validate(out);
}

static esp_err_t commit_blob(const esl_inventory_blob_t *blob)
{
    nvs_handle_t handle;
    esp_err_t ret = nvs_open(ESL_NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (ret != ESP_OK) {
        return ret;
    }

    ret = nvs_set_blob(handle, ESL_NVS_KEY_INVENTORY, blob, sizeof(*blob));
    if (ret == ESP_OK) {
        ret = nvs_commit(handle);
    }
    nvs_close(handle);
    return ret;
}

esp_err_t esl_nvs_init(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    return ret;
}

esp_err_t esl_nvs_load_inventory(esl_inventory_blob_t *blob)
{
    if (blob == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    memset(blob, 0, sizeof(*blob));

    nvs_handle_t handle;
    esp_err_t ret = nvs_open(ESL_NVS_NAMESPACE, NVS_READONLY, &handle);
    if (ret == ESP_ERR_NVS_NOT_FOUND) {
        /* Namespace missing: treat as empty inventory. */
        esl_nvs_blob_prepare_header(blob, 0);
        return ESP_OK;
    }
    if (ret != ESP_OK) {
        return ret;
    }

    size_t required = 0;
    ret = nvs_get_blob(handle, ESL_NVS_KEY_INVENTORY, NULL, &required);
    if (ret == ESP_ERR_NVS_NOT_FOUND) {
        nvs_close(handle);
        esl_nvs_blob_prepare_header(blob, 0);
        return ESP_OK;
    }
    if (ret != ESP_OK) {
        nvs_close(handle);
        return ret;
    }

    if (required == sizeof(esl_inventory_blob_t)) {
        size_t len = sizeof(*blob);
        ret = nvs_get_blob(handle, ESL_NVS_KEY_INVENTORY, blob, &len);
        nvs_close(handle);
        if (ret != ESP_OK) {
            memset(blob, 0, sizeof(*blob));
            return ret;
        }
        ret = esl_nvs_blob_validate(blob);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Inventory blob validation failed: %s (NVS not wiped)",
                     esp_err_to_name(ret));
            memset(blob, 0, sizeof(*blob));
        }
        return ret;
    }

    if (required == sizeof(esl_product_inventory_legacy_t)) {
        esl_product_inventory_legacy_t legacy;
        size_t len = sizeof(legacy);
        ret = nvs_get_blob(handle, ESL_NVS_KEY_INVENTORY, &legacy, &len);
        nvs_close(handle);
        if (ret != ESP_OK) {
            return ret;
        }

        uint32_t maybe_magic = 0;
        memcpy(&maybe_magic, &legacy, sizeof(maybe_magic));
        if (esl_product_rd_le32(maybe_magic) == ESL_INVENTORY_MAGIC) {
            ESP_LOGE(TAG, "Legacy-sized blob has ESL1 magic; treating unreadable");
            return ESP_ERR_INVALID_STATE;
        }

        ret = migrate_legacy_inventory(&legacy, blob);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Legacy migration produced invalid blob: %s", esp_err_to_name(ret));
            memset(blob, 0, sizeof(*blob));
            return ret;
        }

        ret = commit_blob(blob);
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "Legacy migration commit failed: %s", esp_err_to_name(ret));
            return ret;
        }
        ESP_LOGI(TAG, "Migrated legacy inventory -> schema v%u (count=%u)",
                 ESL_INVENTORY_SCHEMA_VER, blob->header.count);
        return ESP_OK;
    }

    nvs_close(handle);
    ESP_LOGE(TAG, "Unexpected inventory blob size %u (NVS not wiped)", (unsigned)required);
    return ESP_ERR_INVALID_SIZE;
}

esp_err_t esl_nvs_save_inventory(esl_inventory_blob_t *blob)
{
    if (blob == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    uint32_t generation = esl_product_rd_le32(blob->header.generation_le) + 1;
    esl_nvs_blob_prepare_header(blob, generation);

    esp_err_t ret = esl_nvs_blob_validate(blob);
    if (ret != ESP_OK) {
        return ret;
    }
    return commit_blob(blob);
}
