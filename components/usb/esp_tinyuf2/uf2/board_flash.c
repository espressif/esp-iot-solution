/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2020 Ha Thach for Adafruit Industries
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

#include <string.h>
#include "esp_log.h"
#include "esp_system.h"
#include "esp_partition.h"
#include "esp_ota_ops.h"
#include "esp_mac.h"
#include "esp_heap_caps.h"
#include "board_flash.h"
#include "nvs.h"
#include "esp_inih.h"
#include "esp_tinyuf2.h"
#include "esp_check.h"

#define TAG                   "uf2"
#define PRINTF(...)           ESP_LOGI(TAG, __VA_ARGS__)
#define PRINTFE(...)          ESP_LOGE(TAG, __VA_ARGS__)
#define PRINTFD(...)          ESP_LOGD(TAG, __VA_ARGS__)
#define PRINT_LOCATION()      ESP_LOGI(TAG, "%s: %d", __PRETTY_FUNCTION__, __LINE__)
#define PRINT_MESS(x)         ESP_LOGI(TAG, "%s", (char*)(x))
#define PRINT_STR(x)          ESP_LOGI(TAG, #x " = %s"   , (char*)(x) )
#define PRINT_INT(x)          ESP_LOGI(TAG, #x " = %d"  , (int32_t) (x) )
#define PRINT_HEX(x)          ESP_LOGI(TAG, #x " = 0x%X", (uint32_t) (x) )

#define PRINT_BUFFER(buf, n) \
  do {\
    uint8_t const* p8 = (uint8_t const*) (buf);\
    printf(#buf ": ");\
    for(uint32_t i=0; i<(n); i++) printf("%x ", p8[i]);\
    printf("\n");\
  }while(0)

static uint32_t _fl_addr = FLASH_CACHE_INVALID_ADDR;
static uint8_t *_fl_buf = NULL;
static bool _if_restart = false;
static update_complete_cb_t _complete_cb = NULL;
static esp_partition_t const* _part_ota = NULL;
char *_ini_file = NULL;
char *_ini_file_dummy = NULL;
static nvs_modified_cb_t _modified_cb = NULL;
static char _part_name[16] = "";
static char _namespace_name[16] = "";
#ifdef CONFIG_UF2_INI_NVS_VALUE_HIDDEN
char *HIDDEN_STR[CONFIG_UF2_INI_NVS_HIDDEN_MAX_NUM];
size_t hidden_str_num = 0;
bool if_all_hidden = false;
#endif

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
        esp_tinyuf2_uninstall();
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
        PRINTFE("Partition argument not found in partition table");
        assert(0);
    }
#ifndef CONFIG_UF2_OTA_FACTORY_ONLY
    esp_partition_t const* runing_ota_ptt = esp_ota_get_running_partition();
    if (runing_ota_ptt == _part_ota) {
        PRINTFE("Can not write to running partition");
        assert(0);
    }
#endif
    _fl_buf = heap_caps_malloc(FLASH_CACHE_SIZE, MALLOC_CAP_32BIT);
    if (_fl_buf == NULL || (uint32_t)_fl_buf % 4 != 0) {
        PRINTFE("Can not allocate memory for flash cache");
        assert(0);
    }
}

void board_flash_deinit(void)
{
    if (_fl_buf) {
        free(_fl_buf);
        _fl_buf = NULL;
    }
}

static void ini_insert_section(const char *section)
{
    if (section == NULL || _ini_file == NULL) {
        return;
    }
    char *p = _ini_file;
    while (*p != '\0') {
        p++;
    }
    sprintf(p, "[%s]\r\n", section);
}

static void ini_insert_pair(const char *key, const char *value)
{
    if (key == NULL || value == NULL || _ini_file == NULL) {
        return;
    }
    char *p = _ini_file;
    while (*p != '\0') {
        p++;
    }
    sprintf(p, "%s = %s\r\n", key, value);
}

#ifdef CONFIG_UF2_INI_NVS_VALUE_HIDDEN
esp_err_t esp_tinyuf2_set_all_key_hidden(bool if_hidden)
{
    if_all_hidden = if_hidden;
    return ESP_OK;
}

esp_err_t esp_tinyuf2_add_key_hidden(const char *key)
{
    ESP_RETURN_ON_FALSE(key != NULL, ESP_ERR_INVALID_ARG, TAG, "key is NULL");
    ESP_RETURN_ON_FALSE(hidden_str_num < CONFIG_UF2_INI_NVS_HIDDEN_MAX_NUM, ESP_ERR_NO_MEM, TAG, "hidden_str_num >= CONFIG_UF2_INI_NVS_HIDDEN_MAX_NUM");
    hidden_str[hidden_str_num] = (char *)malloc(strlen(key) + 1);
    ESP_RETURN_ON_FALSE(hidden_str[hidden_str_num ] != NULL, ESP_ERR_NO_MEM, TAG, "malloc failed");
    strcpy(hidden_str[hidden_str_num], key);
    hidden_str_num++;
    return ESP_OK;
}

static bool check_value_if_hidden(const char *key)
{
    ESP_RETURN_ON_FALSE(key != NULL, false, TAG, "key is NULL");
    if (if_all_hidden) {
        return true;
    }
    for (int i = 0; i < hidden_str_num; i++) {
        if (strcmp(key, hidden_str[i]) == 0) {
            return true;
        }
    }
    return false;
}
#endif

static void ini_gen_from_nvs(const char *part, const char *name)
{
    if (part == NULL || name == NULL || _ini_file == NULL) {
        return;
    }
    nvs_iterator_t it = NULL;
    nvs_handle_t nvs = 0;
    esp_err_t result;
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
    result = nvs_entry_find(part, name, NVS_TYPE_STR, &it);
    if (result == ESP_ERR_NVS_NOT_FOUND) {
        PRINTFE("No such namespace entry was found");
        return;
    }

    if (result != ESP_OK) {
        PRINTFE("NVS find error: %s", esp_err_to_name(result));
        return;
    }
#else
    it = nvs_entry_find(part, name, NVS_TYPE_STR);
    if (it == NULL) {
        PRINTFE("No such namespace entry was found");
        return;
    }
#endif

    ini_insert_section(name);

    result = nvs_open_from_partition(part, name, NVS_READONLY, &nvs);
    if (result != ESP_OK) {
        PRINTFE("NVS open error: %s", esp_err_to_name(result));
        assert(0); //should not happen
    }

    do {
        nvs_entry_info_t info;
        size_t len = 0;
        nvs_entry_info(it, &info);
        if ((result = nvs_get_str(nvs, info.key, NULL, &len)) == ESP_OK) {
            char *str = (char *)malloc(len);
            if ((result = nvs_get_str(nvs, info.key, str, &len)) == ESP_OK) {
#ifdef CONFIG_UF2_INI_NVS_VALUE_HIDDEN
                if (!check_value_if_hidden(info.key)) {
                    ini_insert_pair(info.key, "****");
                } else
#endif
                {
                    ini_insert_pair(info.key, str);
                }
                PRINTFD("Add namespace '%s', key '%s', value '%s' \n",
                        name, info.key, str);
            }
            free(str);
        }
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
        result = nvs_entry_next(&it);
    } while (result == ESP_OK);
#else
        it = nvs_entry_next(it);
    } while (it != NULL);
#endif
    strcpy(_ini_file_dummy, _ini_file);
    nvs_close(nvs);
    nvs_release_iterator(it);
}

static int nvs_write_back(void* user, const char* section, const char* name,
                          const char* value)
{
    nvs_handle_t nvs = (nvs_handle_t)user;
    PRINTFD("... [%s]", section);
    PRINTFD("... (%s=%s)", name, value);
    ESP_ERROR_CHECK(nvs_set_str(nvs, name, value));
    ESP_ERROR_CHECK(nvs_commit(nvs));
    return 1;
}

static void ini_parse_to_nvs(const char *ini_str)
{
    //we currently only support string type
    PRINTFD("ini_parse_to_nvs: \n%s", ini_str);
    nvs_handle_t nvs = 0;
    esp_err_t result = nvs_open_from_partition(_part_name, _namespace_name, NVS_READWRITE, &nvs);
    if (result != ESP_OK) {
        PRINTFE("NVS open error: %s", esp_err_to_name(result));
        assert(0); //should not happen
    }
    ini_parse_string(ini_str, nvs_write_back, (void*)nvs);
    nvs_close(nvs);
}

void board_flash_nvs_init(const char *part_name, const char *namespace_name, nvs_modified_cb_t modified_cb)
{
    _modified_cb = modified_cb;
    strcpy(_namespace_name, namespace_name);
    strcpy(_part_name, part_name);
    if (_ini_file == NULL) {
        //TODO: free when support deinit
        _ini_file = (char *)calloc(1, CFG_UF2_INI_FILE_SIZE);
        _ini_file_dummy = (char *)calloc(1, CFG_UF2_INI_FILE_SIZE);
    }
    ini_gen_from_nvs(part_name, namespace_name);
}

void board_flash_nvs_deinit(void)
{
    if (_ini_file) {
        free(_ini_file);
        _ini_file = NULL;
    }
    if (_ini_file_dummy) {
        free(_ini_file_dummy);
        _ini_file_dummy = NULL;
    }
}

void board_flash_nvs_update(const char *ini_str)
{
    if (ini_str == NULL) {
        PRINTFE("ini_str is NULL");
        return;
    }
    ini_parse_to_nvs(ini_str);
    if (_modified_cb) {
        _modified_cb();
    }
}

uint32_t board_flash_size(void)
{
    if (_part_ota == NULL) {
        return 0;
    }
    return _part_ota->size;
}

void board_flash_read(uint32_t addr, void* buffer, uint32_t len)
{
    if (_part_ota == NULL || buffer == NULL || len == 0) {
        return;
    }
    esp_partition_read(_part_ota, addr, buffer, len);
}

void board_flash_flush(void)
{
    if (_fl_addr == FLASH_CACHE_INVALID_ADDR) {
        return;
    }

    // Check if contents already matched
    bool content_matches = true;
    uint32_t const verify_sz = 4096;
    uint8_t* verify_buf = malloc(verify_sz);

    for (uint32_t count = 0; count < FLASH_CACHE_SIZE; count += verify_sz) {
        board_flash_read(_fl_addr + count, verify_buf, verify_sz);
        if (0 != memcmp(_fl_buf + count, verify_buf, verify_sz)) {
            content_matches = false;
            break;
        }
    }
    free(verify_buf);

    // skip erase & write if content already matches
    if (!content_matches) {
        esp_partition_erase_range(_part_ota, _fl_addr, FLASH_CACHE_SIZE);
        esp_partition_write(_part_ota, _fl_addr, _fl_buf, FLASH_CACHE_SIZE);
    }

    _fl_addr = FLASH_CACHE_INVALID_ADDR;
}

void board_flash_write(uint32_t addr, void const *data, uint32_t len)
{
    if (_part_ota == NULL || data == NULL || len == 0) {
        return;
    }
    // Align write address with size of cache
    uint32_t new_addr = addr & ~(FLASH_CACHE_SIZE - 1);

    // New address comes, we need flush cache data to real flash
    if (new_addr != _fl_addr) {
        board_flash_flush();

        _fl_addr = new_addr;
        // cache origin data of new address, to prevent data loss
        board_flash_read(new_addr, _fl_buf, FLASH_CACHE_SIZE);
    }
    // write received date to cache
    memcpy(_fl_buf + (addr & (FLASH_CACHE_SIZE - 1)), data, len);
}
