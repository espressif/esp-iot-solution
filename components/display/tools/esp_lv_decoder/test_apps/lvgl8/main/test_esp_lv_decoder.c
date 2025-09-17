/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
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

#include "unity.h"
#include "unity_test_runner.h"
#include "unity_test_utils_memory.h"

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

static void test_flush_callback(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *color_map)
{
    if (color_map[0].full == 0xF800) {
        xSemaphoreGive(flush_sem);
        ESP_LOGI(TAG, "decoder finished (red detected)");
    }

    lv_disp_flush_ready(drv);
}

static void test_lvgl_init(lv_disp_drv_t **disp_drv, lv_disp_draw_buf_t **disp_buf)
{
    lv_init();

    *disp_buf = heap_caps_malloc(sizeof(lv_disp_draw_buf_t), MALLOC_CAP_DEFAULT);
    TEST_ASSERT_NOT_NULL(*disp_buf);

    uint32_t buffer_size = TEST_LCD_H_RES * TEST_LCD_V_RES;
    lv_color_t *buf1 = heap_caps_malloc(buffer_size * sizeof(lv_color_t), MALLOC_CAP_DEFAULT);
    TEST_ASSERT_NOT_NULL(buf1);
    lv_disp_draw_buf_init(*disp_buf, buf1, NULL, buffer_size);

    *disp_drv = heap_caps_malloc(sizeof(lv_disp_drv_t), MALLOC_CAP_DEFAULT);
    TEST_ASSERT_NOT_NULL(*disp_drv);
    lv_disp_drv_init(*disp_drv);
    (*disp_drv)->hor_res = TEST_LCD_H_RES;
    (*disp_drv)->ver_res = TEST_LCD_V_RES;
    (*disp_drv)->flush_cb = test_flush_callback;
    (*disp_drv)->draw_buf = *disp_buf;

    lv_disp_drv_register(*disp_drv);

    flush_sem = xSemaphoreCreateBinary();
    TEST_ASSERT_NOT_NULL(flush_sem);
}

static void test_lvgl_deinit(lv_disp_drv_t *disp_drv, lv_disp_draw_buf_t *disp_buf)
{
    free(disp_drv->draw_buf->buf1);
    free(disp_drv->draw_buf);
    free(disp_drv);
    lv_deinit();
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

TEST_CASE("Validate decoder functionality in LVGL", "[decoder][File png]")
{
    lv_disp_drv_t *disp_drv = NULL;
    lv_disp_draw_buf_t *disp_buf = NULL;
    esp_lv_decoder_handle_t decoder_handle = NULL;

    TEST_ESP_OK(test_spiffs_init());

    test_lvgl_init(&disp_drv, &disp_buf);
    TEST_ESP_OK(esp_lv_decoder_init(&decoder_handle));

    lv_obj_t *img = lv_img_create(lv_scr_act());
    lv_obj_set_align(img, LV_ALIGN_TOP_LEFT);
    lv_img_set_src(img, "A:/assets/red_png.png");
    lv_refr_now(NULL);

    TEST_ASSERT_TRUE(xSemaphoreTake(flush_sem, pdMS_TO_TICKS(3 * 1000)));

    TEST_ESP_OK(esp_lv_decoder_deinit(decoder_handle));
    test_lvgl_deinit(disp_drv, disp_buf);
    TEST_ESP_OK(test_spiffs_deinit());
}

TEST_CASE("Validate decoder functionality in LVGL", "[decoder][File jpg]")
{
    lv_disp_drv_t *disp_drv = NULL;
    lv_disp_draw_buf_t *disp_buf = NULL;
    esp_lv_decoder_handle_t decoder_handle = NULL;

    TEST_ESP_OK(test_spiffs_init());

    test_lvgl_init(&disp_drv, &disp_buf);
    TEST_ESP_OK(esp_lv_decoder_init(&decoder_handle));

    lv_obj_t *img = lv_img_create(lv_scr_act());
    lv_obj_set_align(img, LV_ALIGN_TOP_LEFT);
    lv_img_set_src(img, "A:/assets/red_jpg.jpg");
    lv_refr_now(NULL);

    TEST_ASSERT_TRUE(xSemaphoreTake(flush_sem, pdMS_TO_TICKS(3 * 1000)));

    TEST_ESP_OK(esp_lv_decoder_deinit(decoder_handle));
    test_lvgl_deinit(disp_drv, disp_buf);
    TEST_ESP_OK(test_spiffs_deinit());
}

TEST_CASE("Validate decoder functionality in LVGL", "[decoder][File qoi]")
{
    lv_disp_drv_t *disp_drv = NULL;
    lv_disp_draw_buf_t *disp_buf = NULL;
    esp_lv_decoder_handle_t decoder_handle = NULL;

    TEST_ESP_OK(test_spiffs_init());

    test_lvgl_init(&disp_drv, &disp_buf);
    TEST_ESP_OK(esp_lv_decoder_init(&decoder_handle));

    lv_obj_t *img = lv_img_create(lv_scr_act());
    lv_obj_set_align(img, LV_ALIGN_TOP_LEFT);
    /* Switch to mmap for qoi */
    mmap_assets_handle_t assets = NULL;
    mmap_assets_config_t cfg = {
        .partition_label = "qoi",
        .max_files = MMAP_QOI_FILES,
        .checksum = MMAP_QOI_CHECKSUM,
        .flags = { .mmap_enable = 1, .app_bin_check = 0 },
    };
    TEST_ESP_OK(mmap_assets_new(&cfg, &assets));
    lv_img_dsc_t d = {0};
    d.data = mmap_assets_get_mem(assets, 0);
    d.data_size = mmap_assets_get_size(assets, 0);
    lv_img_set_src(img, &d);
    lv_refr_now(NULL);

    TEST_ASSERT_TRUE(xSemaphoreTake(flush_sem, pdMS_TO_TICKS(3 * 1000)));

    TEST_ESP_OK(mmap_assets_del(assets));
    TEST_ESP_OK(esp_lv_decoder_deinit(decoder_handle));
    test_lvgl_deinit(disp_drv, disp_buf);
    TEST_ESP_OK(test_spiffs_deinit());
}

#if ESP_LV_ENABLE_PJPG
TEST_CASE("Validate decoder functionality in LVGL", "[decoder][File pjpg]")
{
    lv_disp_drv_t *disp_drv = NULL;
    lv_disp_draw_buf_t *disp_buf = NULL;
    esp_lv_decoder_handle_t decoder_handle = NULL;

    TEST_ESP_OK(test_spiffs_init());

    test_lvgl_init(&disp_drv, &disp_buf);
    TEST_ESP_OK(esp_lv_decoder_init(&decoder_handle));

    lv_obj_t *img = lv_img_create(lv_scr_act());
    lv_obj_set_align(img, LV_ALIGN_TOP_LEFT);
    mmap_assets_handle_t a2 = NULL;
    mmap_assets_config_t c2 = {
        .partition_label = "pjpg",
        .max_files = MMAP_PJPG_FILES,
        .checksum = MMAP_PJPG_CHECKSUM,
        .flags = { .mmap_enable = 1, .app_bin_check = 0 },
    };
    TEST_ESP_OK(mmap_assets_new(&c2, &a2));
    lv_img_dsc_t d2 = {0};
    d2.data = mmap_assets_get_mem(a2, 0);
    d2.data_size = mmap_assets_get_size(a2, 0);
    lv_img_set_src(img, &d2);
    lv_refr_now(NULL);

    TEST_ASSERT_TRUE(xSemaphoreTake(flush_sem, pdMS_TO_TICKS(3 * 1000)));

    TEST_ESP_OK(mmap_assets_del(a2));
    TEST_ESP_OK(esp_lv_decoder_deinit(decoder_handle));
    test_lvgl_deinit(disp_drv, disp_buf);
    TEST_ESP_OK(test_spiffs_deinit());
}
#endif
#endif /* !CONFIG_IDF_TARGET_ESP32C3 */

TEST_CASE("Validate decoder functionality in LVGL", "[decoder][MMAP spng]")
{
    lv_disp_drv_t *disp_drv = NULL;
    lv_disp_draw_buf_t *disp_buf = NULL;
    esp_lv_decoder_handle_t decoder_handle = NULL;

    test_lvgl_init(&disp_drv, &disp_buf);
    TEST_ESP_OK(esp_lv_decoder_init(&decoder_handle));

    lv_obj_t *img = lv_img_create(lv_scr_act());
    lv_obj_set_align(img, LV_ALIGN_TOP_LEFT);

    mmap_assets_handle_t assets = NULL;
    mmap_assets_config_t cfg = {
        .partition_label = "spng",
        .max_files = MMAP_SPNG_FILES,
        .checksum = MMAP_SPNG_CHECKSUM,
        .flags = { .mmap_enable = 1, .app_bin_check = 0 },
    };
    TEST_ESP_OK(mmap_assets_new(&cfg, &assets));

    lv_img_dsc_t d = {0};
    d.data = mmap_assets_get_mem(assets, 0);
    d.data_size = mmap_assets_get_size(assets, 0);
    lv_img_set_src(img, &d);
    lv_refr_now(NULL);

    TEST_ASSERT_TRUE(xSemaphoreTake(flush_sem, pdMS_TO_TICKS(3 * 1000)));

    TEST_ESP_OK(mmap_assets_del(assets));
    TEST_ESP_OK(esp_lv_decoder_deinit(decoder_handle));
    test_lvgl_deinit(disp_drv, disp_buf);
}

TEST_CASE("Validate decoder functionality in LVGL", "[decoder][MMAP sjpg]")
{
    lv_disp_drv_t *disp_drv = NULL;
    lv_disp_draw_buf_t *disp_buf = NULL;
    esp_lv_decoder_handle_t decoder_handle = NULL;

    test_lvgl_init(&disp_drv, &disp_buf);
    TEST_ESP_OK(esp_lv_decoder_init(&decoder_handle));

    lv_obj_t *img = lv_img_create(lv_scr_act());
    lv_obj_set_align(img, LV_ALIGN_TOP_LEFT);

    mmap_assets_handle_t assets = NULL;
    mmap_assets_config_t cfg = {
        .partition_label = "sjpg",
        .max_files = MMAP_SJPG_FILES,
        .checksum = MMAP_SJPG_CHECKSUM,
        .flags = { .mmap_enable = 1, .app_bin_check = 0 },
    };
    TEST_ESP_OK(mmap_assets_new(&cfg, &assets));

    lv_img_dsc_t d = {0};
    d.data = mmap_assets_get_mem(assets, 0);
    d.data_size = mmap_assets_get_size(assets, 0);
    lv_img_set_src(img, &d);
    lv_refr_now(NULL);

    TEST_ASSERT_TRUE(xSemaphoreTake(flush_sem, pdMS_TO_TICKS(3 * 1000)));

    TEST_ESP_OK(mmap_assets_del(assets));
    TEST_ESP_OK(esp_lv_decoder_deinit(decoder_handle));
    test_lvgl_deinit(disp_drv, disp_buf);
}

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
    printf("ESP LVGL8 decoder TEST \n");

    unity_run_menu();
}
