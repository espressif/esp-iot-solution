/*
 * SPDX-FileCopyrightText: 2022-2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>

#include "esp_log.h"
#include "esp_flash_encrypt.h"
#include "esp_idf_version.h"
#include "bootloader_flash_priv.h"
#include "bootloader_storage_flash.h"
#include "bootloader_custom_malloc.h" // Note, this header is just used to provide malloc() and free() support.

#ifdef CONFIG_BOOTLOADER_WDT_ENABLE
static wdt_hal_context_t rtc_wdt_ctx = RWDT_HAL_CONTEXT_DEFAULT();
static bool rtc_wdt_ctx_enabled = false;
#endif

#ifndef MIN
#define MIN(x, y) (((x) < (y)) ? (x) : (y))
#endif

#define BYTE_STREAM_BUFFER_SIZE (4 * 1024) // Note: The length must not small than DELTA_BLOCK_SIZE/XZ_IOBUF_SIZE

static const char *TAG = "boot_flash";

static bootloader_custom_ota_config_t *custom_ota_config;

#define BOOTLOADER_STORAGE_FLASH_STREAM_SUPPORT

#ifdef BOOTLOADER_STORAGE_FLASH_STREAM_SUPPORT
typedef struct {
    size_t left_addr;
    size_t left_size;
    size_t mid_off;
    size_t mid_size;
    size_t right_off;
    size_t right_size;
    uint8_t *stream_buffer;
} esp_byte_stream_obj_t;

static esp_byte_stream_obj_t *byte_stream;

/**
 * When reading flash in bootloader, src_addr, dest_addr and read_size have to be 4-byte aligned.
 * When writing flash in bootloader, src_addr and write_size have to be 4-byte aligned. If write_encrypted is set, dest_addr and size must be 32-byte aligned.
 * In order to remove the above restrictions on reading and writing flash, we use the help function to realize the streaming reading and writing of flash.
 * Large operations are split into (up to) 3 parts:
 * - Left padding: 4 bytes up to the first 4-byte aligned destination offset.
 * - Middle part
 * - Right padding: 4 bytes from the last 4-byte aligned offset covered.
 * Note: This is not a thread safe function.
 */
static size_t bootloader_storage_flash_stream_read(size_t src_addr, size_t size, bool allow_decrypt)
{
    size_t actual_read_size = 0;

    byte_stream->left_addr = src_addr & ~3U;
    byte_stream->left_size = MIN(((src_addr + 3) & ~3U) - src_addr, size);
    byte_stream->mid_off = byte_stream->left_size;
    byte_stream->mid_size = (size - byte_stream->left_size) & ~3U;
    byte_stream->right_off = byte_stream->left_size + byte_stream->mid_size;
    byte_stream->right_size = size - byte_stream->mid_size - byte_stream->left_size;

    if (byte_stream->left_size > 0) {
        actual_read_size += 4;
    }

    actual_read_size += byte_stream->mid_size;

    if (byte_stream->right_size > 0) {
        actual_read_size += 4;
    }

    ESP_LOGD("test", "src_addr: %u, size: %u, left_addr: %u, actual_read_size: %u", src_addr, size, byte_stream->left_addr, actual_read_size);
    if (bootloader_flash_read(byte_stream->left_addr, byte_stream->stream_buffer, actual_read_size, true) != ESP_OK) {
        ESP_LOGE(TAG, "read origin old err");
        return 0;
    }
    return actual_read_size;
}

esp_err_t bootloader_storage_flash_write(size_t dest_addr, void *src, size_t size, bool allow_decrypt)
{
    size_t actual_read_size = 0;
    if (size == 0) {
        return ESP_OK;
    }

    if (!src) {
        return ESP_ERR_INVALID_ARG;
    }

    actual_read_size = bootloader_storage_flash_stream_read(dest_addr, size, allow_decrypt);
    if (actual_read_size == ESP_OK) {
        ESP_LOGE(TAG, "read to stream buf err");
        return ESP_FAIL;
    }

    memcpy(byte_stream->stream_buffer + (dest_addr - byte_stream->left_addr), src, size);

    return bootloader_flash_write(byte_stream->left_addr, byte_stream->stream_buffer, actual_read_size, allow_decrypt);
}

int bootloader_storage_flash_init(bootloader_custom_ota_params_t *params)
{
    custom_ota_config = params->config;

    /*
    *In bootloader, If write_encrypted is set, dest_addr and size must be 32-byte aligned.
    *The stream buffer used to avoid the restriction on byte alignment and realize byte streaming reading/writing.
    */
    byte_stream = (esp_byte_stream_obj_t *)bootloader_custom_malloc(sizeof(esp_byte_stream_obj_t));
    byte_stream->stream_buffer = (uint8_t *)bootloader_custom_malloc(BYTE_STREAM_BUFFER_SIZE + 64);

    return 0;
}

int bootloader_storage_flash_input(void *buf, int size)
{
    bool encryption = false;
    static uint32_t write_count = 0;

    if (custom_ota_config->dst_offset + size < custom_ota_config->dst_size) {
        if (esp_flash_encryption_enabled()) {
            encryption = true;
        }
        if (bootloader_storage_flash_write(custom_ota_config->dst_addr + custom_ota_config->dst_offset, buf, size, encryption) != ESP_OK) {
            ESP_LOGE(TAG, "error");
            return 0;
        }
        custom_ota_config->dst_offset += size;
    }

#ifdef CONFIG_BOOTLOADER_WDT_ENABLE
    if (!(write_count % 25)) { // The current buffer for writing data is 4KB, so a maximum of 4KB can be written at one time. We feed the watchdog here every 25 times
        rtc_wdt_ctx_enabled = wdt_hal_is_enabled(&rtc_wdt_ctx);
        if (true == rtc_wdt_ctx_enabled) {
            wdt_hal_write_protect_disable(&rtc_wdt_ctx);
            wdt_hal_feed(&rtc_wdt_ctx);
            wdt_hal_write_protect_enable(&rtc_wdt_ctx);
        }
    }
    write_count++;

#endif

    return size;
}
#else // Not BOOTLOADER_STORAGE_FLASH_STREAM_SUPPORT

int bootloader_storage_flash_init(bootloader_custom_ota_params_t *params)
{
    custom_ota_config = params->config;

    return 0;
}

int bootloader_storage_flash_input(void *buf, int size)
{
    bool encryption = false;
    static uint32_t write_count = 0;

    if (custom_ota_config->dst_offset + size < custom_ota_config->dst_size) {
        uint32_t dst_addr = custom_ota_config->dst_addr + custom_ota_config->dst_offset;
        ESP_LOGD(TAG, "write buffer %x to %x, len %d", buf, dst_addr, size);
        if (esp_flash_encryption_enabled()) {
            encryption = true;
        }
        bootloader_flash_write(custom_ota_config->dst_addr + custom_ota_config->dst_offset, buf, size, encryption);
        custom_ota_config->dst_offset += size;
    }

#ifdef CONFIG_BOOTLOADER_WDT_ENABLE
    if (!(write_count % 25)) { // The current buffer for writing data is 4KB, so a maximum of 4KB can be written at one time. We feed the watchdog here every 25 times
        rtc_wdt_ctx_enabled = wdt_hal_is_enabled(&rtc_wdt_ctx);
        if (true == rtc_wdt_ctx_enabled) {
            wdt_hal_write_protect_disable(&rtc_wdt_ctx);
            wdt_hal_feed(&rtc_wdt_ctx);
            wdt_hal_write_protect_enable(&rtc_wdt_ctx);
        }
    }
    write_count++;
#endif

    return size;
}

#endif // end BOOTLOADER_STORAGE_FLASH_STREAM_SUPPORT
