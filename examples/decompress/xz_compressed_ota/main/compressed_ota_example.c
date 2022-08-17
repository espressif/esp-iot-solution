/* OTA example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_flash_partitions.h"
#include "esp_partition.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "esp_secure_boot.h"
#include "esp_crc.h"
#include "mbedtls/md5.h"

#include "bootloader_custom_ota.h"
#include "compressed_ota_download_common.h"

#if CONFIG_EXAMPLE_HTTP_DOWNLOAD
#include "protocol_examples_common.h"
#if CONFIG_EXAMPLE_CONNECT_WIFI
#include "esp_wifi.h"
#endif
#endif

#define HASH_LEN                            32 /* SHA-256 digest length */
#define BUFFER_SIZE 					    (1024 * 4)
#define MD5_MAX_LEN                         16
#define ALIGN_UP(num, align)                (((num) + ((align) - 1)) & ~((align) - 1))
#define OTA_URL_SIZE                        256

static const char *TAG = "xz_compressed_ota_example";

static void print_md5_str(unsigned char digest[16])
{
    // Create a string of the digest
    char digest_str[16 * 2];

    for (int i = 0; i < 16; i++) {
        sprintf(&digest_str[i * 2], "%02x", (unsigned int)digest[i]);
    }

    // For reference, MD5 should be deeb71f585cbb3ae5f7976d5127faf2a
    ESP_LOGI(TAG, "Computed MD5 hash of the upload: %s", digest_str);
}

static esp_err_t get_custom_ota_head(const esp_partition_t *partition, bootloader_custom_ota_header_t *custom_ota_header)
{
    return esp_partition_read(partition, 0, custom_ota_header, sizeof(bootloader_custom_ota_header_t));
}

// Get the custom ota header from partition where compressed firmware is stored
static bool process_custom_ota_head(const esp_partition_t *partition)
{
    bootloader_custom_ota_header_t c_header = {0};
    uint32_t bootloader_custom_ota_header_length;
    uint32_t crc_num = 0;
    esp_err_t err = ESP_FAIL;

    err = get_custom_ota_head(partition, &c_header);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "compressed image get fail, err: %0xd", err);
        return false;
    }

    // check magic num
    if (strcmp((char *)c_header.header.magic, BOOTLOADER_CUSTOM_OTA_HEADER_MAGIC)) {
        ESP_LOGE(TAG, "magic num err");
        return false;
    }

    if (c_header.header.version > BOOTLOADER_CUSTOM_OTA_HEADER_VERSION) {
        ESP_LOGE(TAG, "compress version err");
        return false;
    }
    ESP_LOGI(TAG, "compress type is %u, delta type is %u.", c_header.header.compress_type, c_header.header.delta_type);
    ESP_LOGI(TAG, "encryption type is %u, header version is %u.", c_header.header.encryption_type, c_header.header.version);

    if (c_header.header.version == 1) {
        bootloader_custom_ota_header_length = sizeof(bootloader_custom_ota_header_v1_t);
        crc_num = c_header.header_v1.crc32;
    } else {
        bootloader_custom_ota_header_length = sizeof(bootloader_custom_ota_header_v2_t);
        crc_num = c_header.header_v2.crc32;
    }

    // check crc checksum
    if (esp_crc32_le(0, (const uint8_t *)&c_header, bootloader_custom_ota_header_length - sizeof(c_header.header_v2.crc32)) != crc_num) {
        ESP_LOGE(TAG, "crc verify failed");
        return false;
    }

    // check md5 checksum
    // Read comresssed data and compute the digest chunk by chunk
    uint8_t md5_digest[16];
    mbedtls_md5_context ctx;
    uint8_t *data = (uint8_t *)malloc(BUFFER_SIZE);
    if (data == NULL) {
        ESP_LOGE(TAG, "malloc fail");
    }
    uint32_t len = 0;
    uint32_t total_read_len = 0;
    mbedtls_md5_init(&ctx);
    mbedtls_md5_starts_ret(&ctx);

    do {
        len = (c_header.header.length - total_read_len > BUFFER_SIZE) ? BUFFER_SIZE : (c_header.header.length - total_read_len);
        err = esp_partition_read(partition, bootloader_custom_ota_header_length + total_read_len, data, len);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "partition read errno is %0xd", err);
        }
        mbedtls_md5_update_ret(&ctx, (unsigned const char *)data, len);
        total_read_len += len;
    } while (total_read_len < c_header.header.length);

    mbedtls_md5_finish_ret(&ctx, md5_digest);

    if (memcmp(c_header.header.md5, md5_digest, sizeof(md5_digest))) {
        ESP_LOGE(TAG, "md5 verify failed");
        return false;
    }
    print_md5_str(md5_digest);
    mbedtls_md5_free(&ctx);

    if (c_header.header.delta_type) {
        const esp_partition_t *running = esp_ota_get_running_partition();
        len = 0;
        crc_num = 0;

        for (uint32_t loop = 0; loop < c_header.header_v2.base_len_for_crc32;) {
            len = (c_header.header_v2.base_len_for_crc32 - loop > BUFFER_SIZE) ? BUFFER_SIZE : (c_header.header_v2.base_len_for_crc32 - loop);
            err = esp_partition_read(running, loop, data, len);
            if (err != ESP_OK) {
                ESP_LOGE(TAG, "partition read errno is %0xd", err);
            }
            crc_num = esp_crc32_le(crc_num, (const uint8_t *)data, len);
            loop += len;
        }

        // check crc checksum
        if (crc_num != c_header.header_v2.base_crc32) {
            ESP_LOGE(TAG, "base crc verify failed");
            return false;
        }
    }

    free(data);
    data = NULL;
    return true;
}

#if CONFIG_EXAMPLE_CLEAR_STORAGE_IF_CHECK_FAILED
// Verify the stored compressed data. If the verification fails, call this function to clear some data of the compressed partition
static esp_err_t custom_ota_clear_storage_header(const esp_partition_t *storage_partition)
{
#define SEC_SIZE    (1024 * 4)
    return esp_partition_erase_range(storage_partition, 0, SEC_SIZE);
}
#endif

static void __attribute__((noreturn)) task_fatal_error(void)
{
    ESP_LOGE(TAG, "Exiting task due to fatal error...");
    (void)vTaskDelete(NULL);

    while (1) {
        ;
    }
}

static void print_sha256(const uint8_t *image_hash, const char *label)
{
    char hash_print[HASH_LEN * 2 + 1];
    hash_print[HASH_LEN * 2] = 0;
    for (int i = 0; i < HASH_LEN; ++i) {
        sprintf(&hash_print[i * 2], "%02x", image_hash[i]);
    }
    ESP_LOGI(TAG, "%s: %s", label, hash_print);
}

static void ota_example_task(void *pvParameter)
{
    esp_err_t err;
    const esp_partition_t *update_partition = NULL; // This update partition is used to store compressed ota image data.

    ESP_LOGI(TAG, "Starting OTA example");

    const esp_partition_t *configured = esp_ota_get_boot_partition();
    const esp_partition_t *running = esp_ota_get_running_partition();

    if (configured != running) {
        ESP_LOGW(TAG, "Configured OTA boot partition at offset 0x%08x, but running from offset 0x%08x",
            configured->address, running->address);
        ESP_LOGW(TAG, "(This can happen if either the OTA boot data or preferred boot image become corrupted somehow.)");
    }
    ESP_LOGI(TAG, "Running partition type %d subtype %d (offset 0x%08x)",
        running->type, running->subtype, running->address);


    // In this example, we always store compressed app.bin to ota_1 partition.
    update_partition = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, BOOTLOADER_CUSTOM_OTA_PARTITION_SUBTYPE, NULL);
    assert(update_partition != NULL);
    ESP_LOGI(TAG, "Writing to partition subtype %d at offset 0x%x",
        update_partition->subtype, update_partition->address);

    err = esp_partition_erase_range(update_partition, 0, update_partition->size);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to erase partition: %s", esp_err_to_name(err));
        task_fatal_error();
    }

    if (example_storage_compressed_image(update_partition) != ESP_OK) {
        ESP_LOGE(TAG, "Storage failed");
        task_fatal_error();
    }

    // check downloaded compressed image 
    if (process_custom_ota_head(update_partition) != true) {
        ESP_LOGE(TAG, "verify compressed image header failed");
#if CONFIG_EXAMPLE_CLEAR_STORAGE_IF_CHECK_FAILED
        custom_ota_clear_storage_header(update_partition);
#endif
        task_fatal_error();
    }

    ESP_LOGI(TAG, "running->subtype: %d, update->subtype: %d.", running->subtype, update_partition->subtype);
    ESP_LOGI(TAG, "Prepare to restart system!");
    esp_restart();
    return;
}

void app_main(void)
{
    uint8_t sha_256[HASH_LEN] = {0};
    esp_partition_t partition;

    // get sha256 digest for the partition table
    partition.address = ESP_PARTITION_TABLE_OFFSET;
    partition.size = ESP_PARTITION_TABLE_MAX_LEN;
    partition.type = ESP_PARTITION_TYPE_DATA;
    esp_partition_get_sha256(&partition, sha_256);
    print_sha256(sha_256, "SHA-256 for the partition table: ");

    // get sha256 digest for bootloader
    partition.address = ESP_BOOTLOADER_OFFSET;
    partition.size = ESP_PARTITION_TABLE_OFFSET;
    partition.type = ESP_PARTITION_TYPE_APP;
    esp_partition_get_sha256(&partition, sha_256);
    print_sha256(sha_256, "SHA-256 for bootloader: ");

    // get sha256 digest for running partition
    esp_partition_get_sha256(esp_ota_get_running_partition(), sha_256);
    print_sha256(sha_256, "SHA-256 for current firmware: ");

    // Initialize NVS.
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        // OTA app partition table has a smaller NVS partition size than the non-OTA
        // partition table. This size mismatch may cause NVS initialization to fail.
        // If this happens, we erase NVS partition and initialize NVS again.
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);
#if CONFIG_EXAMPLE_HTTP_DOWNLOAD
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    /* This helper function configures Wi-Fi or Ethernet, as selected in menuconfig.
     * Read "Establishing Wi-Fi or Ethernet Connection" section in
     * examples/protocols/README.md for more information about this function.
     */
    ESP_ERROR_CHECK(example_connect());

#if CONFIG_EXAMPLE_CONNECT_WIFI
    /* Ensure to disable any WiFi power save mode, this allows best throughput
     * and hence timings for overall OTA operation.
     */
    esp_wifi_set_ps(WIFI_PS_NONE);
#endif // CONFIG_EXAMPLE_CONNECT_WIFI
#endif // CONFIG_EXAMPLE_HTTP_DOWNLOAD
#if CONFIG_EXAMPLE_BLE_DOWNLOAD
    ble_init();
#endif // CONFIG_EXAMPLE_BLE_DOWNLOAD
    xTaskCreate(&ota_example_task, "ota_example_task", 8192, NULL, 5, NULL);
}
