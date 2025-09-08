/*
 * SPDX-FileCopyrightText: 2024-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_check.h"
#include "esp_spiffs.h"
#include <stdio.h>

#include "unity.h"
#include "unity_test_runner.h"
#include "unity_test_utils_memory.h"

#include "lvgl_private.h"
#include "esp_lv_decoder.h"
#include "esp_lv_decoder_config.h"
#include "lvgl.h"
#include "esp_mmap_assets.h"
#include "mmap_generate_qoi.h"
#include "mmap_generate_spng.h"
#include "mmap_generate_sjpg.h"
#if ESP_LV_ENABLE_PJPG
#include "mmap_generate_pjpg.h"
#endif

static const char *TAG = "decoder";

#define TEST_LCD_H_RES      100
#define TEST_LCD_V_RES      100

static SemaphoreHandle_t flush_sem;

static void test_flush_callback(lv_display_t *drv, const lv_area_t *area, uint8_t *color_map)
{
    uint16_t *color = (uint16_t *)color_map;

    if (*color == 0xF800) {
        xSemaphoreGive(flush_sem);
        ESP_LOGI(TAG, "decoder finished");
    }

    lv_disp_flush_ready(drv);
}

static lv_display_t *test_lvgl_init()
{
    /* LVGL init */
    lv_init();

    /* Initialize LVGL draw buffers */
    uint32_t buffer_size = TEST_LCD_H_RES * TEST_LCD_V_RES;
    lv_color_t *buf1 = heap_caps_malloc(buffer_size * sizeof(lv_color_t), MALLOC_CAP_DEFAULT);
    TEST_ASSERT_NOT_NULL(buf1);

    lv_display_t *disp = lv_display_create(TEST_LCD_H_RES, TEST_LCD_V_RES);

    lv_display_set_buffers(disp, buf1, NULL, buffer_size * sizeof(lv_color_t), LV_DISPLAY_RENDER_MODE_PARTIAL);

    lv_display_set_color_format(disp, LV_COLOR_FORMAT_RGB565);
    lv_display_set_flush_cb(disp, test_flush_callback);

    // Initialize the semaphores
    flush_sem = xSemaphoreCreateBinary();
    TEST_ASSERT_NOT_NULL(flush_sem);

    return disp;
}

static void test_lvgl_deinit(lv_display_t *disp)
{
    free(disp->_static_buf1.data);
    lv_disp_remove(disp);
    lv_deinit();
    // Delete the semaphores
    vSemaphoreDelete(flush_sem);
}

#if !CONFIG_IDF_TARGET_ESP32C3

static esp_err_t test_spiffs_init(void)
{
    esp_vfs_spiffs_conf_t conf = {
        .base_path = "/assets",
        .partition_label = "assets",
        .max_files = 1,
    };

    return esp_vfs_spiffs_register(&conf);
}

static esp_err_t test_spiffs_deinit(void)
{
    return esp_vfs_spiffs_unregister("assets");
}

TEST_CASE("Validate decoder functionality in LVGL", "[decoder][File jpg]")
{
    esp_lv_decoder_handle_t decoder_handle = NULL;

    TEST_ESP_OK(test_spiffs_init());

    /* Initialize LVGL */
    lv_display_t *disp = test_lvgl_init();
    TEST_ESP_OK(esp_lv_decoder_init(&decoder_handle));

    lv_obj_t *img = lv_img_create(lv_scr_act());
    lv_obj_set_align(img, LV_ALIGN_TOP_LEFT);
    const char *image_paths[] = {
        "A:/assets/red_jpg.jpg",
    };

    for (size_t i = 0; i < sizeof(image_paths) / sizeof(image_paths[0]); i++) {
        lv_img_set_src(img, image_paths[i]);
        lv_refr_now(NULL);

        // Wait for flush to complete
        TEST_ASSERT_TRUE(xSemaphoreTake(flush_sem, pdMS_TO_TICKS(3 * 1000)));
    }

    TEST_ESP_OK(esp_lv_decoder_deinit(decoder_handle));
    test_lvgl_deinit(disp);
    TEST_ESP_OK(test_spiffs_deinit());
}

/* memory-embedded tests removed; using mmap-based tests instead */

/* PJPG tests switched to mmap-based below */

TEST_CASE("Validate decoder functionality in LVGL", "[decoder][File png]")
{
    esp_lv_decoder_handle_t decoder_handle = NULL;

    TEST_ESP_OK(test_spiffs_init());

    lv_display_t *disp = test_lvgl_init();
    lv_display_set_flush_cb(disp, test_flush_callback);
    TEST_ESP_OK(esp_lv_decoder_init(&decoder_handle));

    lv_obj_t *img = lv_img_create(lv_scr_act());
    lv_obj_set_align(img, LV_ALIGN_TOP_LEFT);
    const char *image_paths[] = {
        "A:/assets/red_png.png",
    };

    for (size_t i = 0; i < sizeof(image_paths) / sizeof(image_paths[0]); i++) {
        lv_img_set_src(img, image_paths[i]);
        lv_refr_now(NULL);
        TEST_ASSERT_TRUE(xSemaphoreTake(flush_sem, pdMS_TO_TICKS(5 * 1000)));
    }

    TEST_ESP_OK(esp_lv_decoder_deinit(decoder_handle));
    test_lvgl_deinit(disp);
    TEST_ESP_OK(test_spiffs_deinit());
}

/* QOI tests switched to mmap-based below */

/* PNG memory test removed; use SPIFFS file test */

TEST_CASE("Validate decoder functionality in LVGL", "[decoder][MMAP qoi]")
{
    esp_lv_decoder_handle_t decoder_handle = NULL;
    lv_display_t *disp = test_lvgl_init();
    lv_display_set_flush_cb(disp, test_flush_callback);
    TEST_ESP_OK(esp_lv_decoder_init(&decoder_handle));

    mmap_assets_handle_t assets = NULL;
    mmap_assets_config_t cfg = {
        .partition_label = "qoi",
        .max_files = MMAP_QOI_FILES,
        .checksum = MMAP_QOI_CHECKSUM,
        .flags = {
            .mmap_enable = 1,
            .app_bin_check = 0,
        },
    };
    TEST_ESP_OK(mmap_assets_new(&cfg, &assets));

    lv_obj_t *img = lv_img_create(lv_scr_act());
    lv_obj_set_align(img, LV_ALIGN_TOP_LEFT);

    lv_image_dsc_t dsc = {0};
    dsc.data = mmap_assets_get_mem(assets, 0);
    dsc.data_size = mmap_assets_get_size(assets, 0);

    lv_img_set_src(img, &dsc);
    lv_refr_now(NULL);
    TEST_ASSERT_TRUE(xSemaphoreTake(flush_sem, pdMS_TO_TICKS(5 * 1000)));

    TEST_ESP_OK(mmap_assets_del(assets));
    TEST_ESP_OK(esp_lv_decoder_deinit(decoder_handle));
    test_lvgl_deinit(disp);
}

#endif /* !CONFIG_IDF_TARGET_ESP32C3 */

TEST_CASE("Validate decoder functionality in LVGL", "[decoder][MMAP spng]")
{
    esp_lv_decoder_handle_t decoder_handle = NULL;
    lv_display_t *disp = test_lvgl_init();
    lv_display_set_flush_cb(disp, test_flush_callback);
    TEST_ESP_OK(esp_lv_decoder_init(&decoder_handle));

    mmap_assets_handle_t assets = NULL;
    mmap_assets_config_t cfg = {
        .partition_label = "spng",
        .max_files = MMAP_SPNG_FILES,
        .checksum = MMAP_SPNG_CHECKSUM,
        .flags = {
            .mmap_enable = 1,
            .app_bin_check = 0,
        },
    };
    TEST_ESP_OK(mmap_assets_new(&cfg, &assets));

    lv_obj_t *img = lv_img_create(lv_scr_act());
    lv_obj_set_align(img, LV_ALIGN_TOP_LEFT);

    lv_image_dsc_t dsc = {0};
    dsc.data = mmap_assets_get_mem(assets, 0);
    dsc.data_size = mmap_assets_get_size(assets, 0);

    lv_img_set_src(img, &dsc);
    lv_refr_now(NULL);
    TEST_ASSERT_TRUE(xSemaphoreTake(flush_sem, pdMS_TO_TICKS(5 * 1000)));

    TEST_ESP_OK(mmap_assets_del(assets));
    TEST_ESP_OK(esp_lv_decoder_deinit(decoder_handle));
    test_lvgl_deinit(disp);
}

TEST_CASE("Validate decoder functionality in LVGL", "[decoder][MMAP sjpg]")
{
    esp_lv_decoder_handle_t decoder_handle = NULL;
    lv_display_t *disp = test_lvgl_init();
    lv_display_set_flush_cb(disp, test_flush_callback);
    TEST_ESP_OK(esp_lv_decoder_init(&decoder_handle));

    mmap_assets_handle_t assets = NULL;
    mmap_assets_config_t cfg = {
        .partition_label = "sjpg",
        .max_files = MMAP_SJPG_FILES,
        .checksum = MMAP_SJPG_CHECKSUM,
        .flags = {
            .mmap_enable = 1,
            .app_bin_check = 0,
        },
    };
    TEST_ESP_OK(mmap_assets_new(&cfg, &assets));

    lv_obj_t *img = lv_img_create(lv_scr_act());
    lv_obj_set_align(img, LV_ALIGN_TOP_LEFT);

    lv_image_dsc_t dsc = {0};
    dsc.data = mmap_assets_get_mem(assets, 0);
    dsc.data_size = mmap_assets_get_size(assets, 0);

    lv_img_set_src(img, &dsc);
    lv_refr_now(NULL);
    TEST_ASSERT_TRUE(xSemaphoreTake(flush_sem, pdMS_TO_TICKS(5 * 1000)));

    TEST_ESP_OK(mmap_assets_del(assets));
    TEST_ESP_OK(esp_lv_decoder_deinit(decoder_handle));
    test_lvgl_deinit(disp);
}

#if !CONFIG_IDF_TARGET_ESP32C3
#if ESP_LV_ENABLE_PJPG
TEST_CASE("Validate decoder functionality in LVGL", "[decoder][MMAP pjpg]")
{
    esp_lv_decoder_handle_t decoder_handle = NULL;
    lv_display_t *disp = test_lvgl_init();
    lv_display_set_flush_cb(disp, test_flush_callback);
    TEST_ESP_OK(esp_lv_decoder_init(&decoder_handle));

    mmap_assets_handle_t assets = NULL;
    mmap_assets_config_t cfg = {
        .partition_label = "pjpg",
        .max_files = MMAP_PJPG_FILES,
        .checksum = MMAP_PJPG_CHECKSUM,
        .flags = {
            .mmap_enable = 1,
            .app_bin_check = 0,
        },
    };
    TEST_ESP_OK(mmap_assets_new(&cfg, &assets));

    lv_obj_t *img = lv_img_create(lv_scr_act());
    lv_obj_set_align(img, LV_ALIGN_TOP_LEFT);

    lv_image_dsc_t dsc = {0};
    dsc.data = mmap_assets_get_mem(assets, 0);
    dsc.data_size = mmap_assets_get_size(assets, 0);

    lv_img_set_src(img, &dsc);
    lv_refr_now(NULL);
    TEST_ASSERT_TRUE(xSemaphoreTake(flush_sem, pdMS_TO_TICKS(5 * 1000)));

    TEST_ESP_OK(mmap_assets_del(assets));
    TEST_ESP_OK(esp_lv_decoder_deinit(decoder_handle));
    test_lvgl_deinit(disp);
}
#endif
#endif /* !CONFIG_IDF_TARGET_ESP32C3 */

// Some resources are lazy allocated in the LCD driver, the threadhold is left for that case
#define TEST_MEMORY_LEAK_THRESHOLD  (1500)

static size_t before_free_8bit;
static size_t before_free_32bit;

void setUp(void)
{
    before_free_8bit = heap_caps_get_free_size(MALLOC_CAP_8BIT);
    before_free_32bit = heap_caps_get_free_size(MALLOC_CAP_32BIT);
}

void tearDown(void)
{
    size_t after_free_8bit = heap_caps_get_free_size(MALLOC_CAP_8BIT);
    size_t after_free_32bit = heap_caps_get_free_size(MALLOC_CAP_32BIT);
    unity_utils_check_leak(before_free_8bit, after_free_8bit, "8BIT", TEST_MEMORY_LEAK_THRESHOLD);
    unity_utils_check_leak(before_free_32bit, after_free_32bit, "32BIT", TEST_MEMORY_LEAK_THRESHOLD);
}

void app_main(void)
{
    printf("ESP LVGL9 decoder TEST \n");

    unity_run_menu();
}
