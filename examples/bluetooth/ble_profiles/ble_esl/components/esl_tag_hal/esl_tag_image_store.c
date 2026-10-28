/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "esl_tag_image_store.h"

#include <inttypes.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_partition.h"
#include "nvs.h"
#include "ble_esl.h"
#include "esl_tag_image_store_layout.h"

static const char *TAG = "img_store";

#define ESL_NVS_NAMESPACE           "esl_prod"
#define ESL_NVS_KEY_IMG_WIPE        "img_wipe"
#define ESL_NVS_KEY_IMG_META_PREFIX "img_meta"

#define IMG_STORE_QUEUE_DEPTH       8
#define IMG_STORE_TASK_STACK        4096
#define IMG_STORE_TASK_PRIO         3

#ifndef CONFIG_BLE_ESL_MAX_IMAGE_SIZE
#error CONFIG_BLE_ESL_MAX_IMAGE_SIZE required for image store
#endif

_Static_assert(sizeof(esl_img_bank_header_t) == 32, "header size");
_Static_assert(sizeof(esl_img_bank_header_t) + CONFIG_BLE_ESL_MAX_IMAGE_SIZE <=
               ESL_IMG_BANK_SIZE_BYTES,
               "header + max image exceeds bank size");
_Static_assert(ESL_IMG_PARTITION_SIZE_BYTES == 0x10000u, "partition size mismatch");

typedef enum {
    IMG_MSG_SAVE_SLOT = 1,
    IMG_MSG_ERASE_ALL = 2,
} img_msg_type_t;

typedef struct {
    img_msg_type_t type;
    uint8_t image_index;
} img_msg_t;

static const esp_partition_t *s_part;
static uint8_t s_active_slots;
static QueueHandle_t s_queue;
static TaskHandle_t s_task;
static bool s_reject_saves;
static bool s_erase_pending;
static bool s_pending_save[ESL_IMG_PHYSICAL_SLOT_COUNT];
static uint8_t s_snapshot[CONFIG_BLE_ESL_MAX_IMAGE_SIZE];

static esp_err_t wipe_flag_get(bool *out_pending)
{
    if (out_pending == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    *out_pending = false;

    nvs_handle_t handle;
    esp_err_t ret = nvs_open(ESL_NVS_NAMESPACE, NVS_READONLY, &handle);
    if (ret == ESP_ERR_NVS_NOT_FOUND) {
        return ESP_OK;
    }
    if (ret != ESP_OK) {
        return ret;
    }

    uint8_t val = 0;
    ret = nvs_get_u8(handle, ESL_NVS_KEY_IMG_WIPE, &val);
    nvs_close(handle);
    if (ret == ESP_ERR_NVS_NOT_FOUND) {
        return ESP_OK;
    }
    if (ret != ESP_OK) {
        return ret;
    }
    *out_pending = (val != 0);
    return ESP_OK;
}

static esp_err_t wipe_flag_set(bool pending)
{
    nvs_handle_t handle;
    esp_err_t ret = nvs_open(ESL_NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (ret != ESP_OK) {
        return ret;
    }

    if (pending) {
        ret = nvs_set_u8(handle, ESL_NVS_KEY_IMG_WIPE, 1);
    } else {
        ret = nvs_erase_key(handle, ESL_NVS_KEY_IMG_WIPE);
        if (ret == ESP_ERR_NVS_NOT_FOUND) {
            ret = ESP_OK;
        }
    }
    if (ret == ESP_OK) {
        ret = nvs_commit(handle);
    }
    nvs_close(handle);
    return ret;
}

static void cleanup_legacy_img_meta_keys(void)
{
    nvs_handle_t handle;
    esp_err_t ret = nvs_open(ESL_NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (ret != ESP_OK) {
        return;
    }

    bool erased_any = false;
    for (uint8_t i = 0; i < ESL_IMG_PHYSICAL_SLOT_COUNT; i++) {
        char key[16];
        snprintf(key, sizeof(key), "%s_%u", ESL_NVS_KEY_IMG_META_PREFIX, i);
        ret = nvs_erase_key(handle, key);
        if (ret == ESP_OK) {
            erased_any = true;
        }
    }
    if (erased_any) {
        (void)nvs_commit(handle);
        ESP_LOGI(TAG, "Cleared legacy img_meta_0..3 NVS keys");
    }
    nvs_close(handle);
}

static esp_err_t read_bank_header(uint8_t slot, uint8_t bank, esl_img_bank_header_t *hdr)
{
    size_t off = esl_img_bank_offset(slot, bank);
    return esp_partition_read(s_part, off, hdr, sizeof(*hdr));
}

static esp_err_t read_bank_payload(uint8_t slot, uint8_t bank, uint8_t *dst, uint32_t len)
{
    size_t off = esl_img_bank_offset(slot, bank) + sizeof(esl_img_bank_header_t);
    return esp_partition_read(s_part, off, dst, len);
}

static void probe_bank(uint8_t slot, uint8_t bank, esl_img_bank_view_t *view)
{
    memset(view, 0, sizeof(*view));
    view->bank_index = bank;

    esl_img_bank_header_t hdr;
    if (read_bank_header(slot, bank, &hdr) != ESP_OK) {
        return;
    }
    if (hdr.commit_marker != ESL_IMG_COMMIT_MARKER ||
            hdr.data_len == 0 ||
            hdr.data_len > CONFIG_BLE_ESL_MAX_IMAGE_SIZE) {
        return;
    }
    if (read_bank_payload(slot, bank, s_snapshot, hdr.data_len) != ESP_OK) {
        return;
    }
    if (!esl_img_bank_header_valid(&hdr, slot, CONFIG_BLE_ESL_MAX_IMAGE_SIZE, s_snapshot)) {
        ESP_LOGW(TAG, "Reject bank slot=%u bank=%u (crc/header)", slot, bank);
        return;
    }
    view->valid = true;
    view->generation = hdr.generation;
    view->data_len = hdr.data_len;
}

static esp_err_t erase_partition_all(void)
{
    ESP_LOGI(TAG, "Erasing images partition (%" PRIu32 " bytes)", (uint32_t)s_part->size);
    esp_err_t ret = esp_partition_erase_range(s_part, 0, s_part->size);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "partition erase failed: %s", esp_err_to_name(ret));
        return ret;
    }
    return ESP_OK;
}

static esp_err_t save_slot_now(uint8_t slot)
{
    size_t snap_len = 0;
    esp_err_t ret = ble_esl_image_snapshot(slot, s_snapshot, sizeof(s_snapshot), &snap_len);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "snapshot failed slot=%u: %s", slot, esp_err_to_name(ret));
        return ret;
    }
    if (snap_len == 0 || snap_len > CONFIG_BLE_ESL_MAX_IMAGE_SIZE) {
        return ESP_ERR_INVALID_SIZE;
    }

    esl_img_bank_view_t va = {0};
    esl_img_bank_view_t vb = {0};
    probe_bank(slot, 0, &va);
    /* probe_bank overwrites s_snapshot — re-snapshot after probe */
    probe_bank(slot, 1, &vb);

    ret = ble_esl_image_snapshot(slot, s_snapshot, sizeof(s_snapshot), &snap_len);
    if (ret != ESP_OK) {
        return ret;
    }

    int active = esl_img_pick_active_bank(&va, &vb);
    uint8_t write_bank = esl_img_next_write_bank(active);
    uint32_t active_gen = 0;
    bool have_active = false;
    if (active == 0 && va.valid) {
        active_gen = va.generation;
        have_active = true;
    } else if (active == 1 && vb.valid) {
        active_gen = vb.generation;
        have_active = true;
    }

    esl_img_bank_header_t hdr = {0};
    hdr.magic = ESL_IMG_MAGIC;
    hdr.schema = ESL_IMG_SCHEMA;
    hdr.image_index = slot;
    hdr.generation = esl_img_next_generation(active_gen, have_active);
    hdr.data_len = (uint32_t)snap_len;
    hdr.crc32 = esl_img_compute_crc(&hdr, s_snapshot, hdr.data_len);
    hdr.commit_marker = ESL_IMG_COMMIT_ERASED;

    size_t bank_off = esl_img_bank_offset(slot, write_bank);
    ret = esp_partition_erase_range(s_part, bank_off, ESL_IMG_BANK_SIZE_BYTES);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "erase bank failed slot=%u bank=%u: %s",
                 slot, write_bank, esp_err_to_name(ret));
        return ret;
    }

    size_t payload_off = bank_off + sizeof(hdr);
    ret = esp_partition_write(s_part, payload_off, s_snapshot, hdr.data_len);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "write payload failed slot=%u: %s", slot, esp_err_to_name(ret));
        return ret;
    }

    /* Write header prefix+crc+reserved with commit still erased (0xFF). */
    ret = esp_partition_write(s_part, bank_off, &hdr, sizeof(hdr) - sizeof(hdr.commit_marker));
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "write header failed slot=%u: %s", slot, esp_err_to_name(ret));
        return ret;
    }

    uint32_t commit = ESL_IMG_COMMIT_MARKER;
    size_t commit_off = bank_off + offsetof(esl_img_bank_header_t, commit_marker);
    ret = esp_partition_write(s_part, commit_off, &commit, sizeof(commit));
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "write commit failed slot=%u: %s", slot, esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGI(TAG, "Saved slot=%u bank=%u gen=%" PRIu32 " len=%" PRIu32 " crc=0x%08" PRIx32,
             slot, write_bank, hdr.generation, hdr.data_len, hdr.crc32);
    return ESP_OK;
}

static esp_err_t restore_slot(uint8_t slot)
{
    esl_img_bank_view_t va = {0};
    esl_img_bank_view_t vb = {0};
    probe_bank(slot, 0, &va);
    probe_bank(slot, 1, &vb);

    int active = esl_img_pick_active_bank(&va, &vb);
    if (active < 0) {
        ESP_LOGI(TAG, "restore slot=%u: no valid bank", slot);
        return ESP_ERR_NOT_FOUND;
    }

    const esl_img_bank_view_t *sel = (active == 0) ? &va : &vb;
    if (read_bank_payload(slot, (uint8_t)active, s_snapshot, sel->data_len) != ESP_OK) {
        return ESP_FAIL;
    }

    esl_img_bank_header_t hdr;
    if (read_bank_header(slot, (uint8_t)active, &hdr) != ESP_OK ||
            !esl_img_bank_header_valid(&hdr, slot, CONFIG_BLE_ESL_MAX_IMAGE_SIZE, s_snapshot)) {
        ESP_LOGW(TAG, "restore slot=%u bank=%d CRC reject after re-read", slot, active);
        return ESP_ERR_INVALID_CRC;
    }

    esp_err_t ret = ble_esl_image_restore(slot, s_snapshot, sel->data_len);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "ble_esl_image_restore slot=%u failed: %s",
                 slot, esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGI(TAG, "Restored slot=%u bank=%d gen=%" PRIu32 " len=%" PRIu32 " crc=0x%08" PRIx32,
             slot, active, sel->generation, sel->data_len, hdr.crc32);
    return ESP_OK;
}

static esp_err_t restore_all(void)
{
    for (uint8_t i = 0; i < s_active_slots; i++) {
        (void)restore_slot(i);
    }
    return ESP_OK;
}

static void drain_save_messages(void)
{
    img_msg_t msg;
    while (xQueueReceive(s_queue, &msg, 0) == pdTRUE) {
        if (msg.type == IMG_MSG_ERASE_ALL) {
            /* Keep a single erase coalesced: put back by setting flag only. */
            ESP_LOGW(TAG, "Coalesced extra ERASE_ALL while draining");
        }
        /* Drop SAVE_SLOT */
    }
    memset(s_pending_save, 0, sizeof(s_pending_save));
}

static void worker_task(void *arg)
{
    (void)arg;
    img_msg_t msg;

    while (1) {
        if (s_erase_pending) {
            s_erase_pending = false;
            msg.type = IMG_MSG_ERASE_ALL;
            msg.image_index = 0;
        } else if (xQueueReceive(s_queue, &msg, portMAX_DELAY) != pdTRUE) {
            continue;
        }

        if (msg.type == IMG_MSG_ERASE_ALL) {
            s_reject_saves = true;
            drain_save_messages();
            esp_err_t ret = erase_partition_all();
            if (ret == ESP_OK) {
                ret = wipe_flag_set(false);
                if (ret != ESP_OK) {
                    ESP_LOGE(TAG, "CRITICAL: clear img_wipe failed: %s",
                             esp_err_to_name(ret));
                } else {
                    ESP_LOGI(TAG, "Erase-all complete; wipe flag cleared");
                }
            } else {
                ESP_LOGE(TAG, "CRITICAL: erase-all failed; wipe flag kept");
            }
            s_reject_saves = false;
            continue;
        }

        if (msg.type == IMG_MSG_SAVE_SLOT) {
            if (s_reject_saves) {
                continue;
            }
            bool wipe = false;
            if (wipe_flag_get(&wipe) == ESP_OK && wipe) {
                continue;
            }

            if (msg.image_index < ESL_IMG_PHYSICAL_SLOT_COUNT) {
                s_pending_save[msg.image_index] = true;
            }

            /* Merge any additional SAVE messages already queued. */
            img_msg_t more;
            while (xQueueReceive(s_queue, &more, 0) == pdTRUE) {
                if (more.type == IMG_MSG_ERASE_ALL) {
                    /* Sticky bit — never drop erase via zero-wait requeue. */
                    s_reject_saves = true;
                    memset(s_pending_save, 0, sizeof(s_pending_save));
                    s_erase_pending = true;
                    break;
                }
                if (more.image_index < ESL_IMG_PHYSICAL_SLOT_COUNT) {
                    s_pending_save[more.image_index] = true;
                }
            }

            if (s_erase_pending || s_reject_saves) {
                continue;
            }

            for (uint8_t i = 0; i < s_active_slots; i++) {
                if (!s_pending_save[i]) {
                    continue;
                }
                s_pending_save[i] = false;
                if (s_reject_saves || s_erase_pending) {
                    break;
                }
                (void)save_slot_now(i);
            }
        }
    }
}

esp_err_t esl_tag_image_store_init(uint8_t active_slot_count)
{
    if (active_slot_count == 0 || active_slot_count > ESL_IMG_PHYSICAL_SLOT_COUNT) {
        return ESP_ERR_INVALID_ARG;
    }
    if (s_part != NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    s_part = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, 0x40, "images");
    if (s_part == NULL) {
        ESP_LOGE(TAG, "images partition not found");
        return ESP_ERR_NOT_FOUND;
    }
    if (s_part->size < ESL_IMG_PARTITION_SIZE_BYTES) {
        ESP_LOGE(TAG, "images partition too small: %" PRIu32, (uint32_t)s_part->size);
        return ESP_ERR_INVALID_SIZE;
    }

    s_active_slots = active_slot_count;
    cleanup_legacy_img_meta_keys();

    bool wipe = false;
    esp_err_t ret = wipe_flag_get(&wipe);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "read img_wipe failed: %s", esp_err_to_name(ret));
        return ret;
    }

    if (wipe) {
        ESP_LOGW(TAG, "img_wipe_pending set; erasing and skipping restore");
        ret = erase_partition_all();
        if (ret != ESP_OK) {
            return ret;
        }
        ret = wipe_flag_set(false);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "CRITICAL: clear img_wipe after boot erase failed: %s",
                     esp_err_to_name(ret));
            return ret;
        }
    } else {
        (void)restore_all();
    }

    s_queue = xQueueCreate(IMG_STORE_QUEUE_DEPTH, sizeof(img_msg_t));
    if (s_queue == NULL) {
        return ESP_ERR_NO_MEM;
    }

    BaseType_t ok = xTaskCreate(worker_task, "img_persist", IMG_STORE_TASK_STACK,
                                NULL, IMG_STORE_TASK_PRIO, &s_task);
    if (ok != pdPASS) {
        vQueueDelete(s_queue);
        s_queue = NULL;
        return ESP_ERR_NO_MEM;
    }

    ESP_LOGI(TAG, "Image store ready active_slots=%u part_size=%" PRIu32,
             s_active_slots, (uint32_t)s_part->size);
    return ESP_OK;
}

esp_err_t esl_tag_image_store_queue_save(uint8_t image_index)
{
    if (s_queue == NULL || s_part == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    if (image_index >= s_active_slots) {
        return ESP_ERR_INVALID_ARG;
    }
    if (s_reject_saves) {
        return ESP_ERR_INVALID_STATE;
    }

    bool wipe = false;
    if (wipe_flag_get(&wipe) == ESP_OK && wipe) {
        return ESP_ERR_INVALID_STATE;
    }

    img_msg_t msg = {
        .type = IMG_MSG_SAVE_SLOT,
        .image_index = image_index,
    };
    if (xQueueSend(s_queue, &msg, 0) != pdTRUE) {
        /* One short retry before failing — avoids silent drop under brief load. */
        if (xQueueSend(s_queue, &msg, pdMS_TO_TICKS(20)) != pdTRUE) {
            ESP_LOGE(TAG, "save queue full slot=%u", image_index);
            return ESP_ERR_TIMEOUT;
        }
    }
    return ESP_OK;
}

esp_err_t esl_tag_image_store_queue_erase_all(void)
{
    if (s_queue == NULL || s_part == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    esp_err_t ret = wipe_flag_set(true);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "CRITICAL: set img_wipe failed: %s", esp_err_to_name(ret));
        return ret;
    }
    s_reject_saves = true;

#if CONFIG_BLE_ESL_OTS_SUPPORT
    /* Same-boot Display must not see wiped images while flash erase is async. */
    ble_esl_image_invalidate_all();
#endif

    img_msg_t msg = {
        .type = IMG_MSG_ERASE_ALL,
        .image_index = 0,
    };
    if (xQueueSend(s_queue, &msg, pdMS_TO_TICKS(1000)) != pdTRUE) {
        ESP_LOGE(TAG, "CRITICAL: erase queue full; wipe flag remains set");
        return ESP_ERR_TIMEOUT;
    }
    ESP_LOGI(TAG, "Erase-all queued (wipe flag set)");
    return ESP_OK;
}
