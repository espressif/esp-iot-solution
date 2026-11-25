/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 * SPDX-License-Identifier: Apache-2.0
 */
#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "sdkconfig.h"
#include "esp_eaf_player.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_lv_eaf_player_config.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

extern const uint8_t all_modes_bin_start[] asm("_binary_all_modes_bin_start");
extern const uint8_t all_modes_bin_end[] asm("_binary_all_modes_bin_end");

static const char *TAG = "eaf_all_modes";

typedef struct {
    char name[48];
    uint32_t offset, size, reference, frame_bytes;
    uint16_t frames, width, height;
    uint8_t layout, output, max_error, software_from, reject_from, codec, mode, reserved;
    uint8_t padding[2];
} fixture_t;
_Static_assert(sizeof(fixture_t) == 80, "Fixture index layout");

static bool reject_frame(const fixture_t *c, unsigned frame)
{
    if (c->reject_from != 255 && frame >= c->reject_from) {
        return true;
    }
#if !ESP_LV_EAF_ENABLE_SW_JPEG
    if (c->codec == 2 || c->codec == 6) {
#if !ESP_LV_EAF_ENABLE_HW_JPEG
        return true;
#else
        return c->software_from != 255 && frame >= c->software_from;
#endif
    }
#endif
    return false;
}

static unsigned difference(unsigned a, unsigned b)
{
    return a > b ? a - b : b - a;
}

static void compare_frame(const fixture_t *c, esp_eaf_player_t *player, unsigned index)
{
    const esp_eaf_frame_t *frame = esp_eaf_player_get_frame(player);
    assert(frame && frame->pixels && frame->index == index);
    size_t pixels = (size_t)c->width * c->height;
    const uint8_t *actual = frame->pixels;
    const uint8_t *reference = all_modes_bin_start + c->reference + index * c->frame_bytes;
    size_t color_bytes = pixels * (c->layout == 0 ? 4 : 2);
    assert(frame->pixel_bytes == color_bytes);
    assert((frame->alpha != NULL) == (c->layout == 2));
    if (c->max_error == 0) {
        if (memcmp(actual, reference, color_bytes) != 0) {
            ESP_LOGE(TAG, "PIXEL MISMATCH case=%s frame=%u", c->name, index);
            abort();
        }
    } else if (c->layout == 0) {
        for (size_t i = 0; i < color_bytes; i++) {
            if (difference(actual[i], reference[i]) > c->max_error) {
                ESP_LOGE(TAG, "BOUND case=%s frame=%u byte=%u got=%u expected=%u max=%u",
                         c->name, index, (unsigned)i, actual[i], reference[i], c->max_error);
                abort();
            }
        }
    } else {
        for (size_t i = 0; i < pixels; i++) {
            unsigned a = actual[2 * i] | actual[2 * i + 1] << 8;
            unsigned b = reference[2 * i] | reference[2 * i + 1] << 8;
            if (difference(a >> 11, b >> 11) * 8 > c->max_error ||
                    difference((a >> 5) & 63, (b >> 5) & 63) * 4 > c->max_error ||
                    difference(a & 31, b & 31) * 8 > c->max_error) {
                ESP_LOGE(TAG, "BOUND565 case=%s frame=%u pixel=%u got=%04x expected=%04x max=%u",
                         c->name, index, (unsigned)i, a, b, c->max_error);
                abort();
            }
        }
    }
    if (c->layout == 2) {
        assert(memcmp(frame->alpha, reference + color_bytes, pixels) == 0);
    }
    if (frame->dirty.valid) {
        assert(frame->dirty.x1 <= frame->dirty.x2 && frame->dirty.y1 <= frame->dirty.y2);
        assert(frame->dirty.x2 < c->width && frame->dirty.y2 < c->height);
    }
}

static void seek_and_compare(const fixture_t *c, esp_eaf_player_t *player, unsigned index)
{
    esp_err_t ret = esp_eaf_player_seek(player, index);
    if (reject_frame(c, index)) {
        assert(ret != ESP_OK);
        return;
    }
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "SEEK case=%s frame=%u ret=0x%x", c->name, index, (unsigned)ret);
        abort();
    }
    compare_frame(c, player, index);
}

static void verify_case(const fixture_t *c, size_t pack_size)
{
    assert(memchr(c->name, 0, sizeof(c->name)));
    assert(c->frames && c->layout < 3 && c->width && c->height);
    assert(c->offset <= pack_size && c->size <= pack_size - c->offset);
    assert(c->reference <= pack_size && (size_t)c->frames * c->frame_bytes <= pack_size - c->reference);
    esp_eaf_player_config_t config = ESP_EAF_PLAYER_CONFIG_DEFAULT();
    config.output_format = c->output;
    esp_eaf_player_t *player = NULL, *other = NULL;
    esp_err_t ret = esp_eaf_player_open_memory(all_modes_bin_start + c->offset, c->size, &config, &player);
    if (reject_frame(c, 0)) {
        assert(ret != ESP_OK && !player);
        return;
    }
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "OPEN case=%s ret=0x%x", c->name, (unsigned)ret);
        abort();
    }
    const esp_eaf_stream_info_t *info = esp_eaf_player_get_info(player);
    assert(info && info->frame_count == c->frames && info->width == c->width && info->height == c->height);
    compare_frame(c, player, 0);
    ESP_ERROR_CHECK(esp_eaf_player_open_memory(all_modes_bin_start + c->offset, c->size, &config, &other));
    for (unsigned i = 0; i < c->frames; i++) {
        seek_and_compare(c, player, i);
        seek_and_compare(c, other, c->frames - i - 1);
    }
    seek_and_compare(c, player, 0);
    if (c->frames > 1) {
        ret = esp_eaf_player_next(player);
        if (reject_frame(c, 1)) {
            assert(ret != ESP_OK);
        } else {
            ESP_ERROR_CHECK(ret);
            compare_frame(c, player, 1);
        }
    }
    assert(esp_eaf_player_seek(player, c->frames) != ESP_OK);
    esp_eaf_player_release_cache(player);
    seek_and_compare(c, player, 0);
    seek_and_compare(c, other, 0);
    esp_eaf_player_close(other);
    esp_eaf_player_close(player);
}

void eaf_test_all_modes(void)
{
    size_t pack_size = all_modes_bin_end - all_modes_bin_start;
    uint32_t header[4];
    assert(pack_size >= sizeof(header));
    memcpy(header, all_modes_bin_start, sizeof(header));
    assert(memcmp(header, "TEST", 4) == 0 && header[1] == 1 && header[3] == pack_size);
    assert(header[2] <= (pack_size - sizeof(header)) / sizeof(fixture_t));
    esp_log_level_set("eaf4", ESP_LOG_WARN);
    esp_log_level_set("eaf_codec_support", ESP_LOG_WARN);
    ESP_LOGI(TAG, "START target=%s cases=%u sw_jpeg=%d hw_jpeg=%d",
             CONFIG_IDF_TARGET, (unsigned)header[2], ESP_LV_EAF_ENABLE_SW_JPEG, ESP_LV_EAF_ENABLE_HW_JPEG);
    size_t baseline = 0;
    for (unsigned round = 1;; round++) {
        for (unsigned i = 0; i < header[2]; i++) {
            fixture_t c;
            memcpy(&c, all_modes_bin_start + sizeof(header) + i * sizeof(c), sizeof(c));
            verify_case(&c, pack_size);
            if (round == 1) {
                ESP_LOGI(TAG, "PASS case=%s codec=%u mode=%u frames=%u max_error=%u",
                         c.name, c.codec, c.mode, c.frames, c.max_error);
            }
        }
        vTaskDelay(pdMS_TO_TICKS(1000));
        size_t free_heap = heap_caps_get_free_size(MALLOC_CAP_8BIT);
        if (round == 1) {
            baseline = free_heap;
        }
        assert(free_heap + 1024 >= baseline);
        ESP_LOGI(TAG, "ROUND PASS target=%s round=%u cases=%u heap=%u baseline=%u",
                 CONFIG_IDF_TARGET, round, (unsigned)header[2], (unsigned)free_heap, (unsigned)baseline);
    }
}
