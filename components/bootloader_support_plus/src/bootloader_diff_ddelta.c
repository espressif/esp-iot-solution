/*
 * SPDX-FileCopyrightText: 2020-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include <stdlib.h>

#include "esp_log.h"

#include "bootloader_diff_ddelta.h"
#include "bootloader_flash_priv.h"
#include "bootloader_custom_malloc.h"

#ifdef CONFIG_BOOTLOADER_DIFF_DDELTA

#ifndef MIN
#define MIN(x, y) (((x) < (y)) ? (x) : (y))
#endif

typedef enum {
    ESP_DDELTA_HAVE_NOT_GET_HEADER = 1,
    ESP_DDELTA_HAVE_NOT_GET_ENTRY,
    ESP_DDELTA_IN_DIFF_PROCESS,
    ESP_DDELTA_IN_COPY_PROCESS,
    ESP_DDELTA_IN_SKIP_PROCESS,
} esp_ddelta_status;

typedef struct ddelta_handler {
    esp_ddelta_status current_stage;
    struct ddelta_header d_header;
    struct ddelta_entry_header entry;
    uint32_t bytes_written;
    uint32_t current_processed_data_count;
    uint8_t *patch_buf;
    uint8_t *old_buf;
    int (*read_old)(void *buf, size_t size);
    int (*seek_old)(int64_t skip_value);
    int (*write_new)(void *buf, size_t size);
    uint32_t (*get_patch_valid_len)(void);
    int (*read_patch)(uint8_t *buffer, uint32_t len);
} esp_ddelta_handler;

typedef struct {
    uint8_t *current_buf_addr;
    unsigned int current_valid_len;
    unsigned int dec_read_pos;
} esp_decompress_flush_buf;

static bootloader_custom_ota_engine_t **custom_ota_engines;
static esp_ddelta_handler *s_ddelta_handler = NULL;
static esp_decompress_flush_buf flush_buf;
static bootloader_custom_ota_config_t *custom_ota_config;
static bool s_ddelta_complete = false;
static const char *TAG = "ddelta";

static uint64_t ddelta_be64toh(uint64_t be64)
{
#if defined(__GNUC__) && defined(__BYTE_ORDER__) && defined(__ORDER_LITTLE_ENDIAN__) && __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    return __builtin_bswap64(be64);
#elif defined(__GNUC__) && defined(__BYTE_ORDER__) && defined(__ORDER_BIG_ENDIAN__) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
    return be64;
#else
    unsigned char *buf = (unsigned char *)&be64;
    return (uint64_t)buf[0] << 56 | (uint64_t)buf[1] << 48 | (uint64_t)buf[2] << 40 |
           (uint64_t)buf[3] << 32 | (uint64_t)buf[4] << 24 | (uint64_t)buf[5] << 16 |
           (uint64_t)buf[6] << 8 | (uint64_t)buf[7];
#endif
}

static int64_t ddelta_from_unsigned(uint64_t u)
{
    return u & 0x80000000 ? -(int64_t)~(u - 1) : (int64_t)u;
}

static int esp_ddelta_init(int (*read_old)(void *buf, size_t size),
                           int (*seek_old)(int64_t skip_value),
                           int (*write_new)(void *buf, size_t size),
                           uint32_t (*get_patch_valid_len)(void),
                           int (*read_patch)(uint8_t *buffer, uint32_t len))
{
    if (s_ddelta_handler) {
        return -1;
    }

    s_ddelta_handler = (esp_ddelta_handler *)bootloader_custom_malloc(sizeof(esp_ddelta_handler));
    if (!s_ddelta_handler) {
        return -1;
    }
    memset(s_ddelta_handler, 0, sizeof(esp_ddelta_handler));
    s_ddelta_handler->patch_buf = (uint8_t *)bootloader_custom_malloc(DDELTA_BLOCK_SIZE);
    s_ddelta_handler->old_buf = (uint8_t *)bootloader_custom_malloc(DDELTA_BLOCK_SIZE);
    if (s_ddelta_handler->patch_buf == NULL || s_ddelta_handler->old_buf == NULL) {
        bootloader_custom_free(s_ddelta_handler);
        s_ddelta_handler = NULL;
        return -1;
    }

    s_ddelta_handler->read_old = read_old;
    s_ddelta_handler->seek_old = seek_old;
    s_ddelta_handler->write_new = write_new;
    s_ddelta_handler->get_patch_valid_len = get_patch_valid_len;
    s_ddelta_handler->read_patch = read_patch;
    s_ddelta_handler->current_stage = ESP_DDELTA_HAVE_NOT_GET_HEADER;
    return 0;
}

static void esp_ddelta_deinit(void)
{
    if (s_ddelta_handler) {
        bootloader_custom_free(s_ddelta_handler->patch_buf);
        bootloader_custom_free(s_ddelta_handler->old_buf);
        bootloader_custom_free(s_ddelta_handler);
        s_ddelta_handler = NULL;
    }
}

static int get_entry_header(struct ddelta_entry_header *entry, uint32_t current_used_data_len, uint32_t current_processed_data_count)
{
    if (s_ddelta_handler->read_patch((uint8_t *)entry + current_processed_data_count, current_used_data_len) != (int)current_used_data_len) {
        return -DDELTA_EPATCHIO;
    }
    return 0;
}

static int esp_apply_diff(uint32_t current_used_data_len)
{
    while (current_used_data_len > 0) {
        uint32_t toread = MIN((uint32_t)DDELTA_BLOCK_SIZE, current_used_data_len);
        uint8_t *patch = s_ddelta_handler->patch_buf;
        uint8_t *old = s_ddelta_handler->old_buf;

        if (s_ddelta_handler->read_patch(patch, toread) < (int)toread) {
            return -DDELTA_EPATCHIO;
        }
        if (s_ddelta_handler->read_old(old, toread) < (int)toread) {
            return -DDELTA_EOLDIO;
        }
        for (uint32_t i = 0; i < toread; i++) {
            old[i] = (uint8_t)(old[i] + patch[i]);
        }
        if (s_ddelta_handler->write_new(old, toread) < (int)toread) {
            return -DDELTA_ENEWIO;
        }
        current_used_data_len -= toread;
    }
    return 0;
}

static int esp_apply_copy(uint32_t current_used_data_len)
{
    while (current_used_data_len > 0) {
        uint32_t toread = MIN((uint32_t)DDELTA_BLOCK_SIZE, current_used_data_len);
        if (s_ddelta_handler->read_patch(s_ddelta_handler->patch_buf, toread) < (int)toread) {
            return -DDELTA_EPATCHIO;
        }
        if (s_ddelta_handler->write_new(s_ddelta_handler->patch_buf, toread) < (int)toread) {
            return -DDELTA_ENEWIO;
        }
        current_used_data_len -= toread;
    }
    return 0;
}

static int esp_ddelta_apply(void)
{
    while (1) {
        uint32_t current_valid_data_len;
        uint32_t current_used_data_len;
        switch (s_ddelta_handler->current_stage) {
        case ESP_DDELTA_HAVE_NOT_GET_HEADER:
            current_valid_data_len = s_ddelta_handler->get_patch_valid_len();
            if (current_valid_data_len == 0) {
                return -2;
            }
            current_used_data_len = MIN(current_valid_data_len,
                                        (uint32_t)sizeof(struct ddelta_header) - s_ddelta_handler->current_processed_data_count);
            if (s_ddelta_handler->read_patch((uint8_t *)&s_ddelta_handler->d_header + s_ddelta_handler->current_processed_data_count,
                                             current_used_data_len) != (int)current_used_data_len) {
                goto err_handler;
            }
            s_ddelta_handler->current_processed_data_count += current_used_data_len;
            if (s_ddelta_handler->current_processed_data_count != sizeof(struct ddelta_header)) {
                return -2;
            }
            if (memcmp(DDELTA_MAGIC, s_ddelta_handler->d_header.magic, sizeof(s_ddelta_handler->d_header.magic)) != 0) {
                goto err_handler;
            }
            s_ddelta_handler->d_header.new_file_size = ddelta_be64toh(s_ddelta_handler->d_header.new_file_size);
            if (s_ddelta_handler->d_header.new_file_size > (uint64_t)custom_ota_config->dst_size) {
                ESP_LOGE(TAG, "new image %llu exceeds dst %u",
                         (unsigned long long)s_ddelta_handler->d_header.new_file_size,
                         (unsigned)custom_ota_config->dst_size);
                goto err_handler;
            }
            s_ddelta_handler->current_stage = ESP_DDELTA_HAVE_NOT_GET_ENTRY;
            s_ddelta_handler->current_processed_data_count = 0;
            break;
        case ESP_DDELTA_HAVE_NOT_GET_ENTRY:
            current_valid_data_len = s_ddelta_handler->get_patch_valid_len();
            if (current_valid_data_len == 0) {
                return -2;
            }
            current_used_data_len = MIN(current_valid_data_len,
                                        (uint32_t)sizeof(struct ddelta_entry_header) - s_ddelta_handler->current_processed_data_count);
            if (get_entry_header(&s_ddelta_handler->entry, current_used_data_len, s_ddelta_handler->current_processed_data_count) != 0) {
                goto err_handler;
            }
            s_ddelta_handler->current_processed_data_count += current_used_data_len;
            if (s_ddelta_handler->current_processed_data_count != sizeof(struct ddelta_entry_header)) {
                return -2;
            }
            s_ddelta_handler->entry.diff = ddelta_be64toh(s_ddelta_handler->entry.diff);
            s_ddelta_handler->entry.extra = ddelta_be64toh(s_ddelta_handler->entry.extra);
            s_ddelta_handler->entry.seek.value = ddelta_from_unsigned(ddelta_be64toh(s_ddelta_handler->entry.seek.value));
            if (s_ddelta_handler->entry.diff == 0 && s_ddelta_handler->entry.extra == 0 && s_ddelta_handler->entry.seek.value == 0) {
                if (s_ddelta_handler->bytes_written == s_ddelta_handler->d_header.new_file_size) {
                    ESP_LOGI(TAG, "ddelta OK");
                    s_ddelta_complete = true;
                    return 0;
                }
                ESP_LOGE(TAG, "ddelta patch short");
                goto err_handler;
            }
            s_ddelta_handler->current_stage = ESP_DDELTA_IN_DIFF_PROCESS;
            s_ddelta_handler->current_processed_data_count = 0;
            break;
        case ESP_DDELTA_IN_DIFF_PROCESS:
            current_valid_data_len = s_ddelta_handler->get_patch_valid_len();
            if (current_valid_data_len == 0) {
                return -2;
            }
            current_used_data_len = MIN(current_valid_data_len,
                                        s_ddelta_handler->entry.diff - s_ddelta_handler->current_processed_data_count);
            if (esp_apply_diff(current_used_data_len) != 0) {
                goto err_handler;
            }
            s_ddelta_handler->current_processed_data_count += current_used_data_len;
            if (s_ddelta_handler->current_processed_data_count != s_ddelta_handler->entry.diff) {
                return -2;
            }
            s_ddelta_handler->current_stage = ESP_DDELTA_IN_COPY_PROCESS;
            s_ddelta_handler->current_processed_data_count = 0;
            break;
        case ESP_DDELTA_IN_COPY_PROCESS:
            current_valid_data_len = s_ddelta_handler->get_patch_valid_len();
            if (current_valid_data_len == 0 && s_ddelta_handler->entry.extra != s_ddelta_handler->current_processed_data_count) {
                return -2;
            }
            current_used_data_len = MIN(current_valid_data_len,
                                        s_ddelta_handler->entry.extra - s_ddelta_handler->current_processed_data_count);
            if (esp_apply_copy(current_used_data_len) != 0) {
                goto err_handler;
            }
            s_ddelta_handler->current_processed_data_count += current_used_data_len;
            if (s_ddelta_handler->current_processed_data_count != s_ddelta_handler->entry.extra) {
                return -2;
            }
            s_ddelta_handler->current_stage = ESP_DDELTA_IN_SKIP_PROCESS;
            s_ddelta_handler->current_processed_data_count = 0;
            break;
        case ESP_DDELTA_IN_SKIP_PROCESS:
            if (s_ddelta_handler->seek_old(s_ddelta_handler->entry.seek.value) < 0) {
                goto err_handler;
            }
            s_ddelta_handler->bytes_written += s_ddelta_handler->entry.diff + s_ddelta_handler->entry.extra;
            s_ddelta_handler->current_stage = ESP_DDELTA_HAVE_NOT_GET_ENTRY;
            s_ddelta_handler->current_processed_data_count = 0;
            break;
        default:
            goto err_handler;
        }
    }

err_handler:
    esp_ddelta_deinit();
    return -3;
}

static int esp_ddelta_read_patch(uint8_t *buffer, uint32_t len)
{
    if (flush_buf.current_buf_addr == NULL || buffer == NULL) {
        return -1;
    }
    if (flush_buf.current_valid_len == 0) {
        return 0;
    }
    if (len > flush_buf.current_valid_len) {
        len = flush_buf.current_valid_len;
    }
    memcpy(buffer, flush_buf.current_buf_addr + flush_buf.dec_read_pos, len);
    flush_buf.dec_read_pos += len;
    flush_buf.current_valid_len -= len;
    return (int)len;
}

static void update_patch_flush_buf(uint8_t *buffer, uint32_t len)
{
    flush_buf.current_buf_addr = buffer;
    flush_buf.dec_read_pos = 0;
    flush_buf.current_valid_len = len;
}

static uint32_t get_patch_flush_buf_valid_len(void)
{
    return flush_buf.current_valid_len;
}

static int flash_read_bytes(uint32_t addr, void *buf, size_t size)
{
    uint8_t *out = (uint8_t *)buf;
    bool allow_decrypt = bootloader_custom_flash_encryption_enabled();
    uint8_t tmp[4];

    while (size > 0) {
        uint32_t aligned = addr & ~3U;
        uint32_t skip = addr - aligned;
        size_t chunk = 4U - skip;
        if (chunk > size) {
            chunk = size;
        }
        bootloader_custom_sync_int_cache();
        if (bootloader_flash_read(aligned, tmp, sizeof(tmp), allow_decrypt) != ESP_OK) {
            return -1;
        }
        memcpy(out, tmp + skip, chunk);
        out += chunk;
        addr += chunk;
        size -= chunk;
    }
    return 0;
}

static int esp_ddelta_read_old(void *buf, size_t size)
{
    uint32_t addr = custom_ota_config->base_addr + custom_ota_config->base_offset;
    if (custom_ota_config->base_offset + size > custom_ota_config->base_size) {
        return -1;
    }
    if (flash_read_bytes(addr, buf, size) != 0) {
        return -1;
    }
    custom_ota_config->base_offset += size;
    return (int)size;
}

static int esp_ddelta_seek_old(int64_t skip_value)
{
    int64_t next = (int64_t)custom_ota_config->base_offset + skip_value;
    if (next < 0 || next > custom_ota_config->base_size) {
        return -1;
    }
    custom_ota_config->base_offset = (uint32_t)next;
    return 0;
}

static int esp_ddelta_write_new(void *buf, size_t size)
{
    return custom_ota_engines[DIFF_ENGINE + 1]->input(buf, (int)size);
}

int bootloader_diff_ddelta_init(bootloader_custom_ota_params_t *params)
{
    custom_ota_engines = (bootloader_custom_ota_engine_t **)params->engines;
    custom_ota_config = params->config;
    s_ddelta_complete = false;
    if (esp_ddelta_init(&esp_ddelta_read_old, &esp_ddelta_seek_old, &esp_ddelta_write_new,
                        &get_patch_flush_buf_valid_len, &esp_ddelta_read_patch) != 0) {
        ESP_LOGE(TAG, "ddelta init err");
        return -1;
    }
    return 0;
}

int bootloader_diff_ddelta_input(void *buf, int size)
{
    /* Need-more-input (-2) is only OK while the patch stream is still open.
     * Completion is tracked separately and checked after XZ ends. */
    if (s_ddelta_complete) {
        if (size > 0) {
            ESP_LOGE(TAG, "extra patch data after terminator");
            return 0;
        }
        return size;
    }

    update_patch_flush_buf((uint8_t *)buf, (uint32_t)size);
    int ret = esp_ddelta_apply();
    if (ret == -2 || ret == 0) {
        return size;
    }
    ESP_LOGE(TAG, "patch err");
    return 0;
}

bool bootloader_diff_ddelta_is_complete(void)
{
    return s_ddelta_complete;
}

#endif
