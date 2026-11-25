/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
/* SPDX-License-Identifier: Apache-2.0 */
#include <assert.h>
#include <string.h>
#include "esp_lv_eaf_player_config.h"
#include "esp_eaf_player.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

extern const uint8_t legacy_eaf_start[] asm("_binary_legacy_eaf_start");
extern const uint8_t legacy_eaf_end[] asm("_binary_legacy_eaf_end");
extern const uint8_t lossless_eaf4_start[] asm("_binary_lossless_eaf4_start");
extern const uint8_t lossless_eaf4_end[] asm("_binary_lossless_eaf4_end");

static void verify(const uint8_t *start, const uint8_t *end)
{
    esp_eaf_player_t *player = NULL;
    esp_eaf_player_config_t config = ESP_EAF_PLAYER_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_eaf_player_open_memory(start, end - start, &config, &player));
    const esp_eaf_stream_info_t *info = esp_eaf_player_get_info(player);
    const esp_eaf_frame_t *frame = esp_eaf_player_get_frame(player);
    assert(info && info->frame_count > 0 && info->width > 0 && info->height > 0);
    assert(frame && frame->pixels && frame->pixel_bytes > 0);
    ESP_ERROR_CHECK(esp_eaf_player_next(player));
    esp_eaf_player_close(player);
}

static void verify_legacy_bgra8888(void)
{
    esp_eaf_player_t *player = NULL;
    esp_eaf_player_config_t config = ESP_EAF_PLAYER_CONFIG_DEFAULT();
    config.output_format = ESP_EAF_OUT_BGRA8888;
    ESP_ERROR_CHECK(esp_eaf_player_open_memory(legacy_eaf_start,
                                               legacy_eaf_end - legacy_eaf_start,
                                               &config, &player));
    const esp_eaf_stream_info_t *info = esp_eaf_player_get_info(player);
    const esp_eaf_frame_t *frame = esp_eaf_player_get_frame(player);
    assert(info->pixel_format == ESP_EAF_OUT_BGRA8888);
    assert(info->has_alpha);
    assert(frame->stride == (size_t)info->width * 4);
    assert(frame->pixel_bytes == (size_t)info->width * info->height * 4);
    assert(frame->alpha == NULL);
    esp_eaf_player_close(player);
}

static void verify_lossless_pixels(void)
{
    esp_eaf_player_t *player = NULL;
    esp_eaf_player_config_t config = ESP_EAF_PLAYER_CONFIG_DEFAULT();
    config.output_format = ESP_EAF_OUT_BGRA8888;
    ESP_ERROR_CHECK(esp_eaf_player_open_memory(lossless_eaf4_start,
                                               lossless_eaf4_end - lossless_eaf4_start,
                                               &config, &player));
    const esp_eaf_stream_info_t *info = esp_eaf_player_get_info(player);
    assert(info && info->frame_count == 2 && info->period_ms == 67);
    for (unsigned index = 0; index < 2; index++) {
        ESP_ERROR_CHECK(esp_eaf_player_seek(player, index));
        const esp_eaf_frame_t *frame = esp_eaf_player_get_frame(player);
        for (unsigned y = 0; y < 16; y++) {
            for (unsigned x = 0; x < 16; x++) {
                const uint8_t *pixel = (const uint8_t *)frame->pixels + y * frame->stride + x * 4;
                uint8_t red = 0xd0;
                if (index == 1 && x >= 4 && x < 8 && y >= 4 && y < 8) {
                    red ^= 0x3f;
                }
                assert(pixel[0] == x * 12 && pixel[1] == y * 12);
                assert(pixel[2] == red && pixel[3] == 0x60 + ((x + y) & 0x7f));
            }
        }
        assert(frame->dirty.valid);
        assert(frame->dirty.x1 == (index ? 4 : 0) && frame->dirty.y1 == (index ? 4 : 0));
        assert(frame->dirty.x2 == (index ? 7 : 15) && frame->dirty.y2 == (index ? 7 : 15));
    }
    ESP_ERROR_CHECK(esp_eaf_player_seek(player, 0));
    assert(((const uint8_t *)esp_eaf_player_get_frame(player)->pixels)[(4 * 16 + 4) * 4 + 2] == 0xd0);
    esp_eaf_player_close(player);
}

extern const uint8_t jpeg_eaf_start[] asm("_binary_jpeg_eaf_start");
extern const uint8_t jpeg_eaf_end[] asm("_binary_jpeg_eaf_end");
extern const uint8_t unaligned_eaf_start[] asm("_binary_unaligned_eaf_start");
extern const uint8_t unaligned_eaf_end[] asm("_binary_unaligned_eaf_end");

extern const uint8_t direct_eaf_start[] asm("_binary_direct_eaf_start");
extern const uint8_t direct_eaf_end[] asm("_binary_direct_eaf_end");

static void verify_jpeg(void)
{
    for (unsigned iteration = 0; iteration < 3; iteration++) {
        esp_eaf_player_t *player = NULL;
        esp_err_t ret = esp_eaf_player_open_memory(jpeg_eaf_start, jpeg_eaf_end - jpeg_eaf_start, NULL, &player);
#if ESP_LV_EAF_ENABLE_SW_JPEG || ESP_LV_EAF_ENABLE_HW_JPEG
        assert(ret == ESP_OK && player);
        const esp_eaf_frame_t *frame = esp_eaf_player_get_frame(player);
        uint16_t first = ((const uint16_t *)frame->pixels)[0];
        assert((first >> 11) > ((first >> 5) & 63) / 2);
        ESP_ERROR_CHECK(esp_eaf_player_next(player));
        frame = esp_eaf_player_get_frame(player);
        uint16_t green = ((const uint16_t *)frame->pixels)[0];
        assert(((green >> 5) & 63) / 2 > (green >> 11));
        assert(((const uint16_t *)frame->pixels)[8 * 32 + 24] == first);
        ESP_ERROR_CHECK(esp_eaf_player_seek(player, 0));
        assert(((const uint16_t *)esp_eaf_player_get_frame(player)->pixels)[0] == first);
        esp_eaf_player_close(player);
#else
        assert(ret != ESP_OK && !player);
#endif
        ret = esp_eaf_player_open_memory(direct_eaf_start, direct_eaf_end - direct_eaf_start, NULL, &player);
#if ESP_LV_EAF_ENABLE_SW_JPEG || ESP_LV_EAF_ENABLE_HW_JPEG
        assert(ret == ESP_OK && player);
        uint16_t direct = ((const uint16_t *)esp_eaf_player_get_frame(player)->pixels)[0];
        assert((direct >> 11) > ((direct >> 5) & 63) / 2);
        esp_eaf_player_close(player);
#else
        assert(ret != ESP_OK && !player);
#endif
        ret = esp_eaf_player_open_memory(unaligned_eaf_start, unaligned_eaf_end - unaligned_eaf_start, NULL, &player);
#if ESP_LV_EAF_ENABLE_SW_JPEG
        assert(ret == ESP_OK && player); /* Unsupported hardware width falls back to software. */
        esp_eaf_player_close(player);
#else
        assert(ret != ESP_OK && !player);
#endif
    }
}

static void concurrent_decode(void *done)
{
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    verify_jpeg();
    xSemaphoreGive((SemaphoreHandle_t)done);
    vTaskDelete(NULL);
}

static void verify_concurrent_players(void)
{
    SemaphoreHandle_t done = xSemaphoreCreateCounting(2, 0);
    assert(done);
    TaskHandle_t first, second;
    assert(xTaskCreatePinnedToCore(concurrent_decode, "eaf_first", 8192, done, 5, &first, 0) == pdPASS);
    assert(xTaskCreatePinnedToCore(concurrent_decode, "eaf_second", 8192, done, 5, &second, portNUM_PROCESSORS - 1) == pdPASS);
    xTaskNotifyGive(first);
    xTaskNotifyGive(second);
    assert(xSemaphoreTake(done, pdMS_TO_TICKS(10000)));
    assert(xSemaphoreTake(done, pdMS_TO_TICKS(10000)));
    vSemaphoreDelete(done);
}

#if CONFIG_EAF_TEST_ALL_MODES
void eaf_test_all_modes(void);
#endif

void app_main(void)
{
    verify(legacy_eaf_start, legacy_eaf_end);
    verify_legacy_bgra8888();
    verify(lossless_eaf4_start, lossless_eaf4_end);
    verify_lossless_pixels();
    verify_concurrent_players();
    ESP_LOGI("eaf_core_test", "GUI-independent EAF core test passed");
#if CONFIG_EAF_TEST_ALL_MODES
    eaf_test_all_modes();
#endif
}
