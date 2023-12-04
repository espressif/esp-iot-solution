/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2018 Ha Thach for Adafruit Industries
 * Copyright (c) Espressif
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#ifndef BOARDS_H
#define BOARDS_H

#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include "sdkconfig.h"
#include "esp_partition.h"
#include "esp_tinyuf2.h"

//--------------------------------------------------------------------+
// Features
//--------------------------------------------------------------------+

// Flash Start Address of Application
#ifndef BOARD_FLASH_APP_START
#define BOARD_FLASH_APP_START  0
#endif

// Family ID for updating Application
#if CONFIG_IDF_TARGET_ESP32S2
#define BOARD_UF2_FAMILY_ID     0xbfdd4eee
#elif CONFIG_IDF_TARGET_ESP32S3
#define BOARD_UF2_FAMILY_ID     0xc47e5767
#else
#error unsupported MCUs
#endif

#define USB_VID           CONFIG_TUSB_VID
#define USB_PID           CONFIG_TUSB_PID
#define USB_MANUFACTURER  CONFIG_TUSB_MANUFACTURER
#define USB_PRODUCT       CONFIG_TUSB_PRODUCT

#define UF2_PRODUCT_NAME  USB_MANUFACTURER " " USB_PRODUCT
#define UF2_SERIAL_NUM    CONFIG_UF2_SERIAL_NUM
#define UF2_VOLUME_LABEL  CONFIG_UF2_VOLUME_LABEL
#define UF2_INDEX_URL     CONFIG_UF2_INDEX_URL

// Number of 512-byte blocks in the exposed filesystem
#define CFG_UF2_FLASH_SIZE           (CONFIG_UF2_FLASH_SIZE_MB*1024*1024)
// Number of 512-byte blocks in the exposed filesystem
#define CFG_UF2_BLOCK_SIZE           512
#define CFG_UF2_NUM_BLOCKS           (CONFIG_UF2_DISK_SIZE_MB*1024*1024/CFG_UF2_BLOCK_SIZE)
// Size of the flash cache in bytes, used for buffering flash writes
#define FLASH_CACHE_SIZE             (CONFIG_FLASH_CACHE_SIZE * 1024)
#define FLASH_CACHE_INVALID_ADDR     0xffffffff
#define CFG_UF2_INI_FILE_SIZE        CONFIG_UF2_INI_FILE_SIZE
extern char *_ini_file;
extern char *_ini_file_dummy;

//--------------------------------------------------------------------+
// Basic API
//--------------------------------------------------------------------+

// DFU is complete, should reset or jump to application mode and not return
void board_dfu_complete(void);

// Fill Serial Number and return its length (limit to 16 bytes)
uint8_t board_usb_get_serial(uint8_t serial_id[16]);

//--------------------------------------------------------------------+
// Flash API
//--------------------------------------------------------------------+

// Initialize flash for DFU
void board_flash_init(esp_partition_subtype_t subtype, const char *label, update_complete_cb_t complete_cb, bool if_restart);
void board_flash_deinit(void);

// Initialize flash for NVS
void board_flash_nvs_init(const char *part_name, const char *namespace_name, nvs_modified_cb_t modified_cb);
void board_flash_nvs_deinit(void);

// Update DFU RAM to NVS
void board_flash_nvs_update(const char *ini_str);

// Get size of flash
uint32_t board_flash_size(void);

// Read from flash
void board_flash_read(uint32_t addr, void* buffer, uint32_t len);

// Write to flash
void board_flash_write(uint32_t addr, void const *data, uint32_t len);

// Flush/Sync flash contents
void board_flash_flush(void);

#endif
