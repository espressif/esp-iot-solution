/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <esp_log.h>
#include <esp_random.h>
#include <esp_flash.h>
#include <esp_system.h>
#include <esp_check.h>
#include <esp_mac.h>
#include <esp_ota_ops.h>
#include <esp_chip_info.h>
#include <esp_partition.h>
#include <esp_app_desc.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <cJSON.h>

#include "esp_xiaozhi_board.h"
#include "esp_xiaozhi_keystore.h"

static const char *TAG = "ESP_XIAOZHI_BOARD";

#define LANG_CODE "zh-CN"

esp_err_t esp_xiaozhi_chat_get_board_info(esp_xiaozhi_chat_board_info_t *board_info)
{
    ESP_RETURN_ON_FALSE(board_info, ESP_ERR_INVALID_ARG, TAG, "Invalid handle");

    esp_err_t ret = ESP_OK;

    // Initialize keystore and get UUID
    esp_xiaozhi_chat_keystore_t xiaozhi_chat_keystore;
    ret = esp_xiaozhi_chat_keystore_init(&xiaozhi_chat_keystore, "board", true);
    ESP_RETURN_ON_ERROR(ret, TAG, "Failed to initialize keystore: %s", esp_err_to_name(ret));

    ret = esp_xiaozhi_chat_keystore_get_string(&xiaozhi_chat_keystore, "uuid", "", board_info->uuid, sizeof(board_info->uuid));
    if (ret) {
        uint8_t uuid[16] = {0};

        esp_fill_random(uuid, sizeof(uuid));
        uuid[6] = (uuid[6] & 0x0F) | 0x40;
        uuid[8] = (uuid[8] & 0x3F) | 0x80;

        snprintf(board_info->uuid, 37, "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
                 uuid[0], uuid[1], uuid[2], uuid[3],
                 uuid[4], uuid[5], uuid[6], uuid[7],
                 uuid[8], uuid[9], uuid[10], uuid[11],
                 uuid[12], uuid[13], uuid[14], uuid[15]);

        esp_xiaozhi_chat_keystore_set_string(&xiaozhi_chat_keystore, "uuid", board_info->uuid);
    }

    esp_xiaozhi_chat_keystore_commit(&xiaozhi_chat_keystore);
    esp_xiaozhi_chat_keystore_deinit(&xiaozhi_chat_keystore);

    // Get MAC address string
    uint8_t mac_addr[6] = {0};
    ret = esp_read_mac(mac_addr, ESP_MAC_BASE);
    if (!ret) {
        snprintf(board_info->mac_address, 18, "%02x:%02x:%02x:%02x:%02x:%02x",
                 mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);
    } else {
        strncpy(board_info->mac_address, "Unknown", 18);
        board_info->mac_address[17] = '\0';
        ESP_LOGE(TAG, "Failed to get MAC address: %s", esp_err_to_name(ret));
    }

    board_info->initialized = true;

    return ESP_OK;
}

esp_err_t esp_xiaozhi_chat_get_board_json(esp_xiaozhi_chat_board_info_t *board_info, char *json_buffer, uint16_t buffer_size)
{
    ESP_RETURN_ON_FALSE(board_info, ESP_ERR_INVALID_ARG, TAG, "Invalid board info");
    ESP_RETURN_ON_FALSE(json_buffer, ESP_ERR_INVALID_ARG, TAG, "Invalid json buffer");
    ESP_RETURN_ON_FALSE(buffer_size > 0, ESP_ERR_INVALID_ARG, TAG, "Invalid buffer size");
    ESP_RETURN_ON_FALSE(board_info->initialized, ESP_ERR_INVALID_STATE, TAG, "Board not initialized");

    cJSON *root = cJSON_CreateObject();
    ESP_RETURN_ON_FALSE(root, ESP_ERR_NO_MEM, TAG, "Failed to create root JSON object");

    uint32_t flash_size = 0;
    esp_flash_get_size(NULL, &flash_size);

    cJSON_AddNumberToObject(root, "version", 2);
    cJSON_AddStringToObject(root, "language", LANG_CODE);
    cJSON_AddNumberToObject(root, "flash_size", flash_size);
    cJSON_AddNumberToObject(root, "minimum_free_heap_size", esp_get_minimum_free_heap_size());
    cJSON_AddStringToObject(root, "mac_address", board_info->mac_address);
    cJSON_AddStringToObject(root, "uuid", board_info->uuid);
    cJSON_AddStringToObject(root, "chip_model_name", CONFIG_IDF_TARGET);

    cJSON *chip_info_obj = cJSON_CreateObject();
    if (chip_info_obj) {
        esp_chip_info_t chip_info = {0};
        esp_chip_info(&chip_info);
        cJSON_AddNumberToObject(chip_info_obj, "model", chip_info.model);
        cJSON_AddNumberToObject(chip_info_obj, "cores", chip_info.cores);
        cJSON_AddNumberToObject(chip_info_obj, "revision", chip_info.revision);
        cJSON_AddNumberToObject(chip_info_obj, "features", chip_info.features);
        cJSON_AddItemToObject(root, "chip_info", chip_info_obj);
    }

    cJSON *app_obj = cJSON_CreateObject();
    if (app_obj) {
        char compile_time[64] = {0};
        char sha256_str[65] = {0};
        const esp_app_desc_t *app_desc = esp_app_get_description();
        if (app_desc) {
            cJSON_AddStringToObject(app_obj, "name", app_desc->project_name);
            cJSON_AddStringToObject(app_obj, "version", app_desc->version);
            snprintf(compile_time, sizeof(compile_time), "%sT%sZ", app_desc->date, app_desc->time);
            cJSON_AddStringToObject(app_obj, "compile_time", compile_time);
            cJSON_AddStringToObject(app_obj, "idf_version", app_desc->idf_ver);

            for (int i = 0; i < 32; i++) {
                snprintf(sha256_str + i * 2, 3, "%02x", app_desc->app_elf_sha256[i]);
            }
            cJSON_AddStringToObject(app_obj, "elf_sha256", sha256_str);
        } else {
            cJSON_AddStringToObject(app_obj, "name", "Unknown");
            cJSON_AddStringToObject(app_obj, "version", "Unknown");
            cJSON_AddStringToObject(app_obj, "compile_time", "Unknown");
            cJSON_AddStringToObject(app_obj, "idf_version", "Unknown");
            cJSON_AddStringToObject(app_obj, "elf_sha256", "Unknown");
        }
        cJSON_AddItemToObject(root, "application", app_obj);
    }

    cJSON *partition_array = cJSON_CreateArray();
    if (partition_array != NULL) {
        esp_partition_iterator_t it = esp_partition_find(ESP_PARTITION_TYPE_ANY, ESP_PARTITION_SUBTYPE_ANY, NULL);
        while (it) {
            const esp_partition_t *partition = esp_partition_get(it);
            cJSON *partition_obj = cJSON_CreateObject();
            if (partition_obj) {
                cJSON_AddStringToObject(partition_obj, "label", partition->label);
                cJSON_AddNumberToObject(partition_obj, "type", partition->type);
                cJSON_AddNumberToObject(partition_obj, "subtype", partition->subtype);
                cJSON_AddNumberToObject(partition_obj, "address", partition->address);
                cJSON_AddNumberToObject(partition_obj, "size", partition->size);
                cJSON_AddItemToArray(partition_array, partition_obj);
            }
            it = esp_partition_next(it);
        }
        esp_partition_iterator_release(it);
        cJSON_AddItemToObject(root, "partition_table", partition_array);
    }

    cJSON *ota_obj = cJSON_CreateObject();
    if (ota_obj) {
        const esp_partition_t *ota_partition = esp_ota_get_running_partition();
        if (ota_partition != NULL) {
            cJSON_AddStringToObject(ota_obj, "label", ota_partition->label);
        } else {
            cJSON_AddStringToObject(ota_obj, "label", "Unknown");
        }
        cJSON_AddItemToObject(root, "ota", ota_obj);
    }

    cJSON *board_obj = cJSON_CreateObject();
    if (board_obj) {
        cJSON_AddStringToObject(board_obj, "type", "generic");
        cJSON_AddStringToObject(board_obj, "name", CONFIG_IDF_TARGET);
        cJSON_AddStringToObject(board_obj, "version", "1.0");
        cJSON_AddItemToObject(root, "board", board_obj);
    }

    char *json_string = cJSON_PrintUnformatted(root);
    esp_err_t ret = json_string ? ESP_OK : ESP_ERR_NO_MEM;
    if (json_string) {
        size_t json_len = strlen(json_string);
        if (json_len >= buffer_size) {
            ret = ESP_ERR_INVALID_SIZE;
        } else {
            strncpy(json_buffer, json_string, buffer_size);
            json_buffer[buffer_size - 1] = '\0';
        }
        cJSON_free(json_string);
    }

    cJSON_Delete(root);

    return ret;
}
