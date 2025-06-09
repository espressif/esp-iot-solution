/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <inttypes.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "unity.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_idf_version.h"
#include "esp_spiffs.h"
#include "avi_player.h"

static const char *TAG = "avi_player_test";

// Some resources are lazy allocated in GPTimer driver, the threshold is left for that case
#define TEST_MEMORY_LEAK_THRESHOLD (-300)

static bool end_play = false;

void video_write(frame_data_t *data, void *arg)
{
    TEST_ASSERT_TRUE(data->type == FRAME_TYPE_VIDEO);
    ESP_LOGI(TAG, "Video write: %d", data->data_bytes);
}

void audio_write(frame_data_t *data, void *arg)
{
    TEST_ASSERT_TRUE(data->type == FRAME_TYPE_AUDIO);
    ESP_LOGI(TAG, "Audio write: %d", data->data_bytes);
}

void audio_set_clock(uint32_t rate, uint32_t bits_cfg, uint32_t ch, void *arg)
{
    ESP_LOGI(TAG, "Audio set clock, rate %"PRIu32", bits %"PRIu32", ch %"PRIu32"", rate, bits_cfg, ch);
}

void avi_play_end(void *arg)
{
    ESP_LOGI(TAG, "Play end");
    end_play = true;
}

TEST_CASE("avi_player_test", "[avi_player]")
{
    end_play = false;
    avi_player_config_t config = {
        .buffer_size = 60 * 1024,
        .audio_cb = audio_write,
        .video_cb = video_write,
        .audio_set_clock_cb = audio_set_clock,
        .avi_play_end_cb = avi_play_end,
        .stack_size = 4096,
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 1, 0)
        // It must not be set to `true` when reading data from flash.
        .stack_in_psram = false,
#endif
    };

    avi_player_handle_t handle;
    avi_player_init(config, &handle);

    avi_player_play_from_file(handle, "/spiffs/p4_introduce.avi");

    while (!end_play) {
        vTaskDelay(500 / portTICK_PERIOD_MS);
    }
    avi_player_deinit(handle);
    vTaskDelay(500 / portTICK_PERIOD_MS);
}

static size_t before_free_8bit;
static size_t before_free_32bit;

static void check_leak(size_t before_free, size_t after_free, const char *type)
{
    ssize_t delta = after_free - before_free;
    printf("MALLOC_CAP_%s: Before %u bytes free, After %u bytes free (delta %d)\n", type, before_free, after_free, delta);
    TEST_ASSERT_MESSAGE(delta >= TEST_MEMORY_LEAK_THRESHOLD, "memory leak");
}

void setUp(void)
{
    before_free_8bit = heap_caps_get_free_size(MALLOC_CAP_8BIT);
    before_free_32bit = heap_caps_get_free_size(MALLOC_CAP_32BIT);
}

void tearDown(void)
{
    size_t after_free_8bit = heap_caps_get_free_size(MALLOC_CAP_8BIT);
    size_t after_free_32bit = heap_caps_get_free_size(MALLOC_CAP_32BIT);
    check_leak(before_free_8bit, after_free_8bit, "8BIT");
    check_leak(before_free_32bit, after_free_32bit, "32BIT");
}

void app_main(void)
{
    /*   ___  _   _ _____  ______ _       _____   _____________
     *  / _ \| | | |_   _| | ___ \ |     / _ \ \ / /  ___| ___ \
     * / /_\ \ | | | | |   | |_/ / |    / /_\ \ V /| |__ | |_/ /
     * |  _  | | | | | |   |  __/| |    |  _  |\ / |  __||    /
     * | | | \ \_/ /_| |_  | |   | |____| | | || | | |___| |\ \
     * \_| |_/\___/ \___/  \_|   \_____/\_| |_/\_/ \____/\_| \_|
     */
    printf("  ___  _   _ _____  ______ _       _____   _____________ \n");
    printf(" / _ \\| | | |_   _| | ___ \\ |     / _ \\ \\ / /  ___| ___ \\\n");
    printf("/ /_\\ \\ | | | | |   | |_/ / |    / /_\\ \\ V /| |__ | |_/ /\n");
    printf("|  _  | | | | | |   |  __/| |    |  _  |\\ / |  __||    / \n");
    printf("| | | \\ \\_/ /_| |_  | |   | |____| | | || | | |___| |\\ \\ \n");
    printf("\\_| |_/\\___/ \\___/  \\_|   \\_____/\\_| |_/\\_/ \\____/\\_| \\_|\n");

    esp_vfs_spiffs_conf_t conf = {
        .base_path = "/spiffs",
        .partition_label = "avi",
        .max_files = 5,   // This decides the maximum number of files that can be created on the storage
        .format_if_mount_failed = false
    };

    ESP_ERROR_CHECK(esp_vfs_spiffs_register(&conf));

    unity_run_menu();
}
