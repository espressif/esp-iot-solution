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
#include "lvgl.h"

static const char *TAG = "decoder";

#define TEST_LCD_H_RES      100
#define TEST_LCD_V_RES      100

LV_IMG_DECLARE(color_spng);
LV_IMG_DECLARE(color_png);

LV_IMG_DECLARE(color_sjpg);
LV_IMG_DECLARE(color_jpg);

static SemaphoreHandle_t flush_sem;

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

static void test_flush_callback(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *color_map)
{
    ESP_LOGI(TAG, "flush_cb, 0x%04X:0x%04X, [%d,%d,%d,%d]", color_map[0].full, color_map[1].full, area->x1, area->y1, area->x2, area->y2);
    if (color_map[0].full == 0xF800) {
        xSemaphoreGive(flush_sem);
        ESP_LOGI(TAG, "decoder finished");
    }

    lv_disp_flush_ready(drv);
}

static void test_lvgl_init(lv_disp_drv_t **disp_drv, lv_disp_draw_buf_t **disp_buf)
{
    /* LVGL init */
    lv_init();

    *disp_buf = heap_caps_malloc(sizeof(lv_disp_draw_buf_t), MALLOC_CAP_DEFAULT);
    TEST_ASSERT_NOT_NULL(*disp_buf);

    /* Initialize LVGL draw buffers */
    uint32_t buffer_size = TEST_LCD_H_RES * TEST_LCD_V_RES;
    lv_color_t *buf1 = heap_caps_malloc(buffer_size * sizeof(lv_color_t), MALLOC_CAP_DEFAULT);
    TEST_ASSERT_NOT_NULL(buf1);
    lv_disp_draw_buf_init(*disp_buf, buf1, NULL, buffer_size);

    *disp_drv = heap_caps_malloc(sizeof(lv_disp_drv_t), MALLOC_CAP_DEFAULT);
    TEST_ASSERT_NOT_NULL(*disp_drv);
    /* Descriptor of a display driver */
    lv_disp_drv_init(*disp_drv);
    (*disp_drv)->hor_res = TEST_LCD_H_RES;
    (*disp_drv)->ver_res = TEST_LCD_V_RES;
    (*disp_drv)->flush_cb = test_flush_callback;
    (*disp_drv)->draw_buf = *disp_buf;

    /* Finally register the driver */
    lv_disp_drv_register(*disp_drv);

    // Initialize the semaphores
    flush_sem = xSemaphoreCreateBinary();
    TEST_ASSERT_NOT_NULL(flush_sem);
}

static void test_lvgl_deinit(lv_disp_drv_t *disp_drv, lv_disp_draw_buf_t *disp_buf)
{
    free(disp_drv->draw_buf->buf1);
    free(disp_drv->draw_buf);
    free(disp_drv);
    lv_deinit();
    // Delete the semaphores
    vSemaphoreDelete(flush_sem);
}

TEST_CASE("Validate decoder functionality in LVGL", "[decoder][C_ARRAY png]")
{
    lv_disp_drv_t *disp_drv = NULL;
    lv_disp_draw_buf_t *disp_buf = NULL;
    esp_lv_decoder_handle_t decoder_handle = NULL;

    /* Initialize LVGL */
    test_lvgl_init(&disp_drv, &disp_buf);
    TEST_ESP_OK(esp_lv_decoder_init(&decoder_handle));

    lv_obj_t *img = lv_img_create(lv_scr_act());
    lv_obj_set_align(img, LV_ALIGN_TOP_LEFT);
    lv_img_set_src(img, &color_png);
    lv_refr_now(NULL);

    // Wait for flush to complete
    TEST_ASSERT_TRUE(xSemaphoreTake(flush_sem, pdMS_TO_TICKS(3 * 1000)));

    TEST_ESP_OK(esp_lv_decoder_deinit(decoder_handle));
    test_lvgl_deinit(disp_drv, disp_buf);
}

TEST_CASE("Validate decoder functionality in LVGL", "[decoder][C_ARRAY spng]")
{
    lv_disp_drv_t *disp_drv = NULL;
    lv_disp_draw_buf_t *disp_buf = NULL;
    esp_lv_decoder_handle_t decoder_handle = NULL;

    /* Initialize LVGL */
    test_lvgl_init(&disp_drv, &disp_buf);
    TEST_ESP_OK(esp_lv_decoder_init(&decoder_handle));

    lv_obj_t *img = lv_img_create(lv_scr_act());
    lv_obj_set_align(img, LV_ALIGN_TOP_LEFT);
    lv_img_set_src(img, &color_spng);
    lv_refr_now(NULL);

    // Wait for flush to complete
    TEST_ASSERT_TRUE(xSemaphoreTake(flush_sem, pdMS_TO_TICKS(3 * 1000)));

    TEST_ESP_OK(esp_lv_decoder_deinit(decoder_handle));
    test_lvgl_deinit(disp_drv, disp_buf);
}

TEST_CASE("Validate decoder functionality in LVGL", "[decoder][File png]")
{
    lv_disp_drv_t *disp_drv = NULL;
    lv_disp_draw_buf_t *disp_buf = NULL;
    esp_lv_decoder_handle_t decoder_handle = NULL;

    TEST_ESP_OK(test_spiffs_init());

    /* Initialize LVGL */
    test_lvgl_init(&disp_drv, &disp_buf);
    TEST_ESP_OK(esp_lv_decoder_init(&decoder_handle));

    lv_obj_t *img = lv_img_create(lv_scr_act());
    lv_obj_set_align(img, LV_ALIGN_TOP_LEFT);
    lv_img_set_src(img, "A:/assets/color_png.png");
    lv_refr_now(NULL);

    // Wait for flush to complete
    TEST_ASSERT_TRUE(xSemaphoreTake(flush_sem, pdMS_TO_TICKS(3 * 1000)));

    TEST_ESP_OK(esp_lv_decoder_deinit(decoder_handle));
    test_lvgl_deinit(disp_drv, disp_buf);
    TEST_ESP_OK(test_spiffs_deinit());
}

TEST_CASE("Validate decoder functionality in LVGL", "[decoder][C_ARRAY jpg]")
{
    lv_disp_drv_t *disp_drv = NULL;
    lv_disp_draw_buf_t *disp_buf = NULL;
    esp_lv_decoder_handle_t decoder_handle = NULL;

    /* Initialize LVGL */
    test_lvgl_init(&disp_drv, &disp_buf);
    TEST_ESP_OK(esp_lv_decoder_init(&decoder_handle));

    lv_obj_t *img = lv_img_create(lv_scr_act());
    lv_obj_set_align(img, LV_ALIGN_TOP_LEFT);
    lv_img_set_src(img, &color_jpg);
    lv_refr_now(NULL);

    // Wait for flush to complete
    TEST_ASSERT_TRUE(xSemaphoreTake(flush_sem, pdMS_TO_TICKS(3 * 1000)));

    TEST_ESP_OK(esp_lv_decoder_deinit(decoder_handle));
    test_lvgl_deinit(disp_drv, disp_buf);
}

TEST_CASE("Validate decoder functionality in LVGL", "[decoder][C_ARRAY sjpg]")
{
    lv_disp_drv_t *disp_drv = NULL;
    lv_disp_draw_buf_t *disp_buf = NULL;
    esp_lv_decoder_handle_t decoder_handle = NULL;

    /* Initialize LVGL */
    test_lvgl_init(&disp_drv, &disp_buf);
    TEST_ESP_OK(esp_lv_decoder_init(&decoder_handle));

    lv_obj_t *img = lv_img_create(lv_scr_act());
    lv_obj_set_align(img, LV_ALIGN_TOP_LEFT);
    lv_img_set_src(img, &color_sjpg);
    lv_refr_now(NULL);

    // Wait for flush to complete
    TEST_ASSERT_TRUE(xSemaphoreTake(flush_sem, pdMS_TO_TICKS(3 * 1000)));

    TEST_ESP_OK(esp_lv_decoder_deinit(decoder_handle));
    test_lvgl_deinit(disp_drv, disp_buf);
}

TEST_CASE("Validate decoder functionality in LVGL", "[decoder][File jpg]")
{
    lv_disp_drv_t *disp_drv = NULL;
    lv_disp_draw_buf_t *disp_buf = NULL;
    esp_lv_decoder_handle_t decoder_handle = NULL;

    TEST_ESP_OK(test_spiffs_init());

    /* Initialize LVGL */
    test_lvgl_init(&disp_drv, &disp_buf);
    TEST_ESP_OK(esp_lv_decoder_init(&decoder_handle));

    lv_obj_t *img = lv_img_create(lv_scr_act());
    lv_obj_set_align(img, LV_ALIGN_TOP_LEFT);
    lv_img_set_src(img, "A:/assets/color_jpg.jpg");
    lv_refr_now(NULL);

    // Wait for flush to complete
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

    /* Initialize LVGL */
    test_lvgl_init(&disp_drv, &disp_buf);
    TEST_ESP_OK(esp_lv_decoder_init(&decoder_handle));

    lv_obj_t *img = lv_img_create(lv_scr_act());
    lv_obj_set_align(img, LV_ALIGN_TOP_LEFT);
    lv_img_set_src(img, "A:/assets/color_qoi.qoi");
    lv_refr_now(NULL);

    // Wait for flush to complete
    TEST_ASSERT_TRUE(xSemaphoreTake(flush_sem, pdMS_TO_TICKS(3 * 1000)));

    TEST_ESP_OK(esp_lv_decoder_deinit(decoder_handle));
    test_lvgl_deinit(disp_drv, disp_buf);
    TEST_ESP_OK(test_spiffs_deinit());
}

// Some resources are lazy allocated in the LCD driver, the threadhold is left for that case
#define TEST_MEMORY_LEAK_THRESHOLD  (600)

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
    printf("ESP SPNG decoder TEST \n");
    unity_run_menu();
}
