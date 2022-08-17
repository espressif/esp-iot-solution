/* OTA example, http download compressed image

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <string.h>
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_http_client.h"
#include "esp_flash_partitions.h"
#include "esp_partition.h"
#include "errno.h"

#if CONFIG_EXAMPLE_HTTP_DOWNLOAD
#include "compressed_ota_download_common.h"
#include "bootloader_custom_ota.h"

#define BUFFSIZE                            1024
#define UPDATE_PARTITION_RESERVED_SPACE     (1024 * 8) // Reserve some space for signature information when secure boot used

#ifdef CONFIG_EXAMPLE_USE_AES128
#include "mbedtls/aes.h"
#include "esp_efuse.h"
#define AES128_BLOCK_SZIE (16)
#define AES128_KEY_SZIE   (16)
#endif

/*an ota data write buffer ready to write to the flash*/
extern const uint8_t server_cert_pem_start[] asm("_binary_ca_cert_pem_start");
extern const uint8_t server_cert_pem_end[] asm("_binary_ca_cert_pem_end");

static char ota_write_data[BUFFSIZE + 1] = {0};
static const char *TAG = "example_http_d";

#ifdef CONFIG_EXAMPLE_USE_AES128
static esp_err_t get_aes128_key(uint8_t aes_key[16])
{
    if (aes_key == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
#ifdef CONFIG_AES_KEY_STORE_IN_FLASH
    uint8_t origin_key[] = {0xEF, 0xBF, 0xBD, 0x36, 0xEF, 0xBF, 0xBD, 0xEF, 0xBF, 0xBD, 0xEF, 0xBF, 0xBD, 0x26, 0x6B, 0x61};
#elif CONFIG_AES_KEY_STORE_IN_EFUSE
    uint8_t origin_key[16] = {0};
    esp_efuse_read_block(EFUSE_BLK3, origin_key, 16 * 8, sizeof(uint8_t) * AES128_KEY_SZIE * 8);
#endif

    memcpy(aes_key, origin_key, AES128_KEY_SZIE);
    return ESP_OK;
}

static void disp_buf(const char *TAG, uint8_t *buf, uint32_t len)
{
    int i;
    assert(buf != NULL);
    for (i = 0; i < len; i++) {
        printf("0x%02x, ", buf[i]);
        if ((i + 1) % 32 == 0) {
            printf("\n");
        }
    }
    printf("\n");
}
#endif

static void http_cleanup(esp_http_client_handle_t client)
{
    esp_http_client_close(client);
    esp_http_client_cleanup(client);
}

esp_err_t example_storage_compressed_image(const esp_partition_t *update_partition)
{
    esp_http_client_config_t config = {
        .url = CONFIG_EXAMPLE_FIRMWARE_UPG_URL,
        .cert_pem = (char *)server_cert_pem_start,
        .timeout_ms = CONFIG_EXAMPLE_OTA_RECV_TIMEOUT,
        .keep_alive_enable = true,
    };

#ifdef CONFIG_EXAMPLE_SKIP_COMMON_NAME_CHECK
    config.skip_cert_common_name_check = true;
#endif

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (client == NULL) {
        ESP_LOGE(TAG, "Failed to initialise HTTP connection");
        return ESP_FAIL;
    }
    esp_err_t err = esp_http_client_open(client, 0);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open HTTP connection: %s", esp_err_to_name(err));
        esp_http_client_cleanup(client);
        return ESP_FAIL;
    }

    int total_content_length = esp_http_client_fetch_headers(client);
    if (total_content_length < 0) {
        ESP_LOGE(TAG, "HTTP client fetch headers failed");
    } else {
        ESP_LOGW(TAG, "%d bytes to be downloaded", total_content_length);
    }

#ifdef CONFIG_EXAMPLE_USE_AES128
    mbedtls_aes_context aes;
    uint8_t aes128_key[16] = {0};
    get_aes128_key(aes128_key);
    disp_buf(TAG, aes128_key, 16); // This log is only for testing, please remove this sentence when releasing the product
    mbedtls_aes_init(&aes);
    mbedtls_aes_setkey_enc(&aes, (const unsigned char *)aes128_key, 128);
    unsigned char encry_buf[AES128_BLOCK_SZIE] = {0};
    unsigned char decry_buf[AES128_BLOCK_SZIE] = {0};
    unsigned char *forward1 = NULL;
    unsigned char *forward2 = NULL;
    int decry_count = 0;
    int total_decry_count = 0;
    char *decryed_buf = calloc(1, BUFFSIZE);
    if (decryed_buf == NULL) {
        ESP_LOGE(TAG, "Failed to allocate memory for decryed data");
        goto err;
    }
#endif
    bootloader_custom_ota_header_t custom_ota_header = {0};
    bool image_header_was_checked = false;
    uint32_t wrote_size = 0;
    /*write compressed ota image to the partiton*/
    while (1) {
        int data_read = esp_http_client_read(client, ota_write_data, BUFFSIZE);
        if (data_read < 0) {
            ESP_LOGE(TAG, "Error: SSL data read error");
            goto err;
        } else if (data_read > 0) {
#ifdef CONFIG_EXAMPLE_USE_AES128
            /*if aes encryption is enabled, decrypt the ota_write_data buffer and rewrite the decrypted data to the buffer*/
            forward1 = (unsigned char *)ota_write_data;
            forward2 = (unsigned char *)decryed_buf;
            for (decry_count = 0; decry_count < data_read; decry_count += AES128_BLOCK_SZIE) {
                memcpy(encry_buf, forward1, AES128_BLOCK_SZIE);
                mbedtls_aes_crypt_ecb(&aes, MBEDTLS_AES_DECRYPT, (const unsigned char *)encry_buf, (unsigned char *)decry_buf);
                memcpy(forward2, decry_buf, AES128_BLOCK_SZIE);
                forward1 += AES128_BLOCK_SZIE;
                forward2 += AES128_BLOCK_SZIE;
            }

            total_decry_count += data_read;
            memcpy(ota_write_data, decryed_buf, data_read);
#endif
            if (image_header_was_checked == false) {
                if (data_read > sizeof(bootloader_custom_ota_header_t)) {
                    memcpy(&custom_ota_header, ota_write_data, sizeof(bootloader_custom_ota_header_t));
                    // check whether it is a compressed app.bin(include diff compressed ota and compressed only ota).
                    if (!strcmp((char *)custom_ota_header.header.magic, BOOTLOADER_CUSTOM_OTA_HEADER_MAGIC)) {
                        ESP_LOGW(TAG, "compressed ota detected");
                    }

                    // Check compressed image bin size
                    if (custom_ota_header.header.length > update_partition->size - UPDATE_PARTITION_RESERVED_SPACE) {
                        ESP_LOGE(TAG, "Compressed image size is too large");
                        goto err;
                    }
                    image_header_was_checked = true;
                } else {
                    ESP_LOGE(TAG, "received package is not fit len");
                    goto err;
                }
            }
            err = esp_partition_write(update_partition, wrote_size, ota_write_data, data_read);
            if (err != ESP_OK) {
                ESP_LOGE(TAG, "write ota data error is %s", esp_err_to_name(err));
                goto err;
            }
            wrote_size += data_read;
        } else if (data_read == 0) {
            /*
             * As esp_http_client_read never returns negative error code, we rely on
             * `errno` to check for underlying transport connectivity closure if any
             */
#ifdef CONFIG_EXAMPLE_USE_AES128
            mbedtls_aes_free(&aes);
            free(decryed_buf);
            decryed_buf = NULL;
#endif
            if (errno == ECONNRESET || errno == ENOTCONN) {
                ESP_LOGE(TAG, "Connection closed, errno = %d", errno);
                break;
            }
            if (esp_http_client_is_complete_data_received(client) == true) {
                ESP_LOGI(TAG, "Connection closed");
                break;
            }
        }
    }
    ESP_LOGI(TAG, "Total Write binary data length: %d", wrote_size);
    if (esp_http_client_is_complete_data_received(client) != true) {
        ESP_LOGE(TAG, "Error in receiving complete file");
        goto err;
    }

#if CONFIG_SECURE_BOOT_V2_ENABLED || CONFIG_SECURE_SIGNED_ON_UPDATE_NO_SECURE_BOOT
#ifdef CONFIG_EXAMPLE_USE_AES128
    int actually_encry_size = sizeof(custom_ota_header) + custom_ota_header.header.length;
    actually_encry_size = ALIGN_UP(actually_encry_size, 4096) + 4096; // secure boot is 4096-bytes alignment, and the signature block is 4096bytes.
    ESP_LOGI(TAG, "address: %0xu, signed: %0xu", update_partition->address, actually_encry_size - sizeof(ets_secure_boot_signature_t));
    esp_err_t verify_err = esp_secure_boot_verify_signature(update_partition->address, actually_encry_size - sizeof(ets_secure_boot_signature_t));
#else
    ESP_LOGI(TAG, "address: %0xu, signed: %0xu", update_partition->address, wrote_size - sizeof(ets_secure_boot_signature_t));
    esp_err_t verify_err = esp_secure_boot_verify_signature(update_partition->address, wrote_size - sizeof(ets_secure_boot_signature_t));
#endif
    if (verify_err != ESP_OK) {
        ESP_LOGE(TAG, "verify signature err=0x%x", verify_err);
        goto err;
    } else {
        ESP_LOGW(TAG, "verify signature success");
    }
#endif
    http_cleanup(client);
    return ESP_OK;
err:
#ifdef CONFIG_EXAMPLE_USE_AES128
    if (decryed_buf) {
        free(decryed_buf);
    }
#endif
    http_cleanup(client);
    return ESP_FAIL;
}

#endif // end CONFIG_EXAMPLE_HTTP_DOWNLOAD