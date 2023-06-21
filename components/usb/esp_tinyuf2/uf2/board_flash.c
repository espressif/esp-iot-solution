/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2020 Ha Thach for Adafruit Industries
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

#include <string.h>
#include "esp_log.h"
#include "esp_system.h"
#include "esp_partition.h"
#include "esp_ota_ops.h"
#include "esp_mac.h"
#include "board_flash.h"
//#include "spi_flash_chip_driver.h"

// Debug helper, remove later
#define PRINTF(...)           ESP_LOGI("uf2", __VA_ARGS__)
#define PRINT_LOCATION()      ESP_LOGI("uf2", "%s: %d", __PRETTY_FUNCTION__, __LINE__)
#define PRINT_MESS(x)         ESP_LOGI("uf2", "%s", (char*)(x))
#define PRINT_STR(x)          ESP_LOGI("uf2", #x " = %s"   , (char*)(x) )
#define PRINT_INT(x)          ESP_LOGI("uf2", #x " = %d"  , (int32_t) (x) )
#define PRINT_HEX(x)          ESP_LOGI("uf2", #x " = 0x%X", (uint32_t) (x) )

#define PRINT_BUFFER(buf, n) \
  do {\
    uint8_t const* p8 = (uint8_t const*) (buf);\
    printf(#buf ": ");\
    for(uint32_t i=0; i<(n); i++) printf("%x ", p8[i]);\
    printf("\n");\
  }while(0)

static uint32_t _fl_addr = FLASH_CACHE_INVALID_ADDR;
static uint8_t _fl_buf[FLASH_CACHE_SIZE] __attribute__((aligned(4)));
static bool _if_restart = false;
static update_complete_cb_t _complete_cb = NULL;
static esp_partition_t const* _part_ota = NULL;

uint8_t board_usb_get_serial(uint8_t serial_id[16])
{
  // use factory default MAC as serial ID
  esp_efuse_mac_get_default(serial_id);
  return 6;
}

void board_dfu_complete(void)
{
  esp_ota_set_boot_partition(_part_ota);

  if (_complete_cb) {
    PRINTF("dfu_complete: run user callback");
    _complete_cb();
  }

  if (_if_restart) {
    /* code */
    PRINTF("UF2 Flashing complete, restart...");
    esp_restart();
  }
}

void board_flash_init(esp_partition_subtype_t subtype, const char *label, update_complete_cb_t complete_cb, bool if_restart)
{
  _fl_addr = FLASH_CACHE_INVALID_ADDR;
  _if_restart = if_restart;
  _complete_cb = complete_cb;

  if (subtype == ESP_PARTITION_SUBTYPE_ANY) {
    _part_ota = esp_ota_get_next_update_partition(NULL);
  } else {
    _part_ota = esp_partition_find_first(ESP_PARTITION_TYPE_APP, subtype, label);
  }

  if (_part_ota == NULL) {
    PRINTF("Partition argument not found in partition table");
    assert(0);
  }
  esp_partition_t const* runing_ota_ptt = esp_ota_get_running_partition();
  if (runing_ota_ptt == _part_ota) {
    PRINTF("Can not write to running partition");
    assert(0);
  }
}

uint32_t board_flash_size(void)
{
  return _part_ota->size;
}

void board_flash_read(uint32_t addr, void* buffer, uint32_t len)
{
  esp_partition_read(_part_ota, addr, buffer, len);
}

void board_flash_flush(void)
{
  if ( _fl_addr == FLASH_CACHE_INVALID_ADDR ) return;

  //PRINTF("Erase and Write at 0x%08X", _fl_addr);

  // Check if contents already matched
  bool content_matches = true;
  uint32_t const verify_sz = 4096;
  uint8_t* verify_buf = malloc(verify_sz);

  for(uint32_t count = 0; count < FLASH_CACHE_SIZE; count += verify_sz)
  {
    board_flash_read(_fl_addr + count, verify_buf, verify_sz);
    if ( 0 != memcmp(_fl_buf + count, verify_buf, verify_sz)  )
    {
      content_matches = false;
      break;
    }
  }
  free(verify_buf);

  // skip erase & write if content already matches
  if ( !content_matches )
  {
    esp_partition_erase_range(_part_ota, _fl_addr, FLASH_CACHE_SIZE);
    esp_partition_write(_part_ota, _fl_addr, _fl_buf, FLASH_CACHE_SIZE);
  }

  _fl_addr = FLASH_CACHE_INVALID_ADDR;
}

void board_flash_write (uint32_t addr, void const *data, uint32_t len)
{
  // Align write address with size of cache
  uint32_t new_addr = addr & ~(FLASH_CACHE_SIZE - 1);

  // New address comes, we need flush cache data to real flash
  if ( new_addr != _fl_addr )
  {
    board_flash_flush();

    _fl_addr = new_addr;
    // cache origin data of new address, to prevent data loss
    board_flash_read(new_addr, _fl_buf, FLASH_CACHE_SIZE);
  }
  // write received date to cache
  memcpy(_fl_buf + (addr & (FLASH_CACHE_SIZE - 1)), data, len);
}
