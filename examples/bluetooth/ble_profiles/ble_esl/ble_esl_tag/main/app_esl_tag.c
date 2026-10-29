/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "app_esl_tag.h"

#include <string.h>
#include "app_tag_events.h"
#include "esl_tag_hal.h"
#include "esl_tag_storage.h"
#include "esl_tag_image_store.h"
#include "esl_tag_image_store_layout.h"
#include "esl_tag_assoc.h"
#include "ble_esl.h"
#include "esp_esl.h"
#include "esp_eslp_tag.h"
#include "esp_ble_conn_mgr.h"
#include "esp_log.h"
#include "sdkconfig.h"
#include "host/ble_hs.h"
#include "host/ble_store.h"

static const char *TAG = "app_esl_tag";

#ifndef BLE_STORE_MAX_BONDS_LOCAL
#define BLE_STORE_MAX_BONDS_LOCAL 8
#endif

#ifndef CONFIG_ESL_TAG_MAX_IMAGES
#define CONFIG_ESL_TAG_MAX_IMAGES CONFIG_BLE_ESL_MAX_IMAGES
#endif

_Static_assert(CONFIG_BLE_ESL_MAX_IMAGES >= 4, "OTS needs 4 image slots for the SSD1680 demo");
_Static_assert(CONFIG_BLE_ESL_MAX_IMAGE_SIZE >= 4736, "OTS object must accept a 4736-byte SSD1680 frame");
_Static_assert(CONFIG_ESL_TAG_MAX_IMAGES == CONFIG_BLE_ESL_MAX_IMAGES,
               "reported image slots must match OTS objects");

/* Same flags-only AD as ble_eslp Unsynchronized. Must be in place before
 * conn_mgr starts advertising, otherwise the first packets still carry
 * ESL UUID and the AP takes the unassociated / verify-Ping path. */
static const uint8_t s_unsync_adv_data[] = {
    0x02, 0x01, 0x06,
};

static void tag_adv_unsynchronized(void)
{
    (void)esp_ble_conn_adv_data_set(s_unsync_adv_data, sizeof(s_unsync_adv_data));
}

static void tag_adv_unassociated(void)
{
    (void)esp_ble_conn_adv_data_set(NULL, 0);
}

static int count_matching_bonds(uint8_t addr_type, const uint8_t addr[6], int *out_total)
{
    ble_addr_t peers[BLE_STORE_MAX_BONDS_LOCAL];
    int num = BLE_STORE_MAX_BONDS_LOCAL;
    int rc = ble_store_util_bonded_peers(peers, &num, BLE_STORE_MAX_BONDS_LOCAL);
    if (rc != 0) {
        ESP_LOGW(TAG, "bonded_peers query failed: %d", rc);
        if (out_total) {
            *out_total = -1;
        }
        return -1;
    }
    if (out_total) {
        *out_total = num;
    }

    int matches = 0;
    for (int i = 0; i < num; i++) {
        if (peers[i].type == addr_type && memcmp(peers[i].val, addr, 6) == 0) {
            matches++;
        }
    }
    return matches;
}

static void erase_assoc_and_peer(const esl_tag_assoc_blob_t *blob, bool peer_valid)
{
    esp_err_t er = esl_tag_storage_erase_assoc();
    if (er != ESP_OK) {
        ESP_LOGE(TAG, "Failed to erase tag_assoc: %s", esp_err_to_name(er));
    }
    if (peer_valid) {
        ble_addr_t peer = {
            .type = blob->peer_addr_type,
        };
        memcpy(peer.val, blob->peer_addr, 6);
        int rc = ble_store_util_delete_peer(&peer);
        if (rc != 0) {
            ESP_LOGW(TAG, "delete_peer for stale assoc failed: %d", rc);
        }
    }
    tag_adv_unassociated();
}

static void try_restore_association(void)
{
    esl_tag_assoc_blob_t blob;
    esp_err_t ret = esl_tag_storage_load_assoc(&blob);
    if (ret == ESP_ERR_NOT_FOUND) {
        ESP_LOGI(TAG, "No tag_assoc blob — starting Unassociated");
        tag_adv_unassociated();
        return;
    }
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "tag_assoc unreadable (%s) — erase and start Unassociated",
                 esp_err_to_name(ret));
        (void)esl_tag_storage_erase_assoc();
        tag_adv_unassociated();
        return;
    }

    int total = 0;
    int matches = count_matching_bonds(blob.peer_addr_type, blob.peer_addr, &total);
    if (matches != 1) {
        ESP_LOGW(TAG, "Bond cross-check failed: matches=%d total_peers=%d — clear assoc",
                 matches, total);
        erase_assoc_and_peer(&blob, true);
        return;
    }

    ble_esl_persisted_tag_t info = {
        .esl_address = blob.esl_address,
        .ap_sync_key = blob.ap_sync_key,
        .resp_key = blob.resp_key,
        .peer_addr_type = blob.peer_addr_type,
    };
    memcpy(info.peer_addr, blob.peer_addr, 6);

    ret = ble_esl_restore_persisted_tag(&info);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "restore association failed: %s", esp_err_to_name(ret));
        erase_assoc_and_peer(&blob, true);
        return;
    }

    ESP_LOGI(TAG, "Restored association, state=UNSYNCHRONIZED");
}

static void app_esl_set_capabilities(void)
{
    esp_ble_esl_display_info_t display = {
        .width = CONFIG_ESL_TAG_DISPLAY_WIDTH,
        .height = CONFIG_ESL_TAG_DISPLAY_HEIGHT,
        .display_type = BLE_ESL_DISPLAY_TYPE_BLACK_WHITE,
    };
    esp_err_t ret = esp_ble_esl_set_display_info(&display, 1);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "set_display_info: %s", esp_err_to_name(ret));
    }

    esp_ble_esl_image_info_t image = {
        .max_image_index = (uint8_t)(CONFIG_ESL_TAG_MAX_IMAGES - 1),
    };
    ret = esp_ble_esl_set_image_info(&image);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "set_image_info: %s", esp_err_to_name(ret));
    }

#if defined(CONFIG_BLE_ESL_LED_INFO)
    uint8_t led_info = 0x3F;
    ret = esp_ble_esl_set_led_info(&led_info, 1);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "set_led_info: %s", esp_err_to_name(ret));
    }
#endif

#if defined(CONFIG_BLE_ESL_SENSOR_INFO)
    uint8_t sensor_info[3] = {
        0x00,
        0x4F,
        0x00,
    };
    ret = esp_ble_esl_set_sensor_info(sensor_info, sizeof(sensor_info));
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "set_sensor_info: %s", esp_err_to_name(ret));
    }
#endif
}

esp_err_t app_esl_tag_init(void)
{
    ESP_ERROR_CHECK(esl_tag_hal_init());
    ESP_ERROR_CHECK(esl_tag_storage_init());

    app_esl_set_capabilities();

    esp_err_t ret = esp_ble_eslp_init(NULL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "esp_ble_eslp_init failed: %s", esp_err_to_name(ret));
        return ret;
    }

    /* Association restore uses NimBLE bond store (ble_hs_lock). That is only
     * valid after ble_conn_mgr has called nimble_port_init() in
     * esp_ble_conn_start(); product firmware could restore from app_esl_tag_init
     * because it inits NimBLE first. */

    uint8_t ots_slots = CONFIG_ESL_TAG_MAX_IMAGES;
    if (ots_slots > ESL_IMG_PHYSICAL_SLOT_COUNT) {
        ots_slots = ESL_IMG_PHYSICAL_SLOT_COUNT;
    }
    ret = esl_tag_image_store_init(ots_slots);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "esl_tag_image_store_init failed: %s", esp_err_to_name(ret));
        return ret;
    }

    /* conn_mgr starts advertising on host sync *before* STARTED restore.
     * If NVS has an association, put flags-only AD in place so the first
     * packets are not ESL-UUID (the AP would treat those as Unassociated). */
    esl_tag_assoc_blob_t blob;
    if (esl_tag_storage_load_assoc(&blob) == ESP_OK) {
        tag_adv_unsynchronized();
        ESP_LOGI(TAG, "Prefetch Unsynchronized advertising (no ESL UUID)");
    }

    ESP_LOGI(TAG, "ESL Tag initialized");
    return ESP_OK;
}

esp_err_t app_esl_tag_start(void)
{
    try_restore_association();
    ESP_LOGI(TAG, "ESL Tag advertising is started by ble_conn_mgr");
    return ESP_OK;
}
