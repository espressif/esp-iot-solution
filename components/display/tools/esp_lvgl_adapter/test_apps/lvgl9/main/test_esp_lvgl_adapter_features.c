/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_check.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "esp_err.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "unity.h"
#include "unity_test_runner.h"
#include "unity_test_utils_memory.h"
#include "hw_init.h"
#include "esp_lv_adapter.h"
#include "esp_lv_adapter_display.h"
#include "esp_lv_adapter_input.h"
#include "lvgl.h"

#if defined(CONFIG_ESP_LVGL_ADAPTER_ENABLE_DECODER) || defined(CONFIG_ESP_LVGL_ADAPTER_ENABLE_FREETYPE)
#include "esp_mmap_assets.h"
#endif

#if CONFIG_ESP_LVGL_ADAPTER_ENABLE_DECODER
#include "mmap_generate_images.h"
#endif

#if CONFIG_ESP_LVGL_ADAPTER_ENABLE_FREETYPE
#include "mmap_generate_fonts.h"
#endif

static const char *TAG = "adapter_test";

/* lv_init()/lv_deinit() has a known residual leak (~1.5 KB per cycle) */
#define TEST_MEMORY_LEAK_THRESHOLD  (2000)
#define TEST_DISPLAY_TIME_MS        (1500)
#define TEST_IMAGE_DISPLAY_TIME_MS  (800)
#define TEST_COLOR_DISPLAY_TIME_MS  (400)
#define LVGL_LOCK_TIMEOUT_MS        (1000)

#if defined(CONFIG_ESP_LVGL_ADAPTER_ENABLE_DECODER) || defined(CONFIG_ESP_LVGL_ADAPTER_ENABLE_FREETYPE)
typedef struct {
    mmap_assets_handle_t mmap_handle;
    esp_lv_fs_handle_t fs_handle;
    char drive_letter;
    bool mounted;
} asset_ctx_t;
#endif

#if CONFIG_ESP_LVGL_ADAPTER_ENABLE_DECODER
static asset_ctx_t s_image_assets = { .drive_letter = 'I' };
#endif

#if CONFIG_ESP_LVGL_ADAPTER_ENABLE_FREETYPE
static asset_ctx_t s_font_assets = { .drive_letter = 'F' };
#endif

static lv_display_t *s_disp = NULL;
#if HW_USE_TOUCH
static lv_indev_t *s_touch = NULL;
#endif
#if HW_USE_ENCODER
static lv_indev_t *s_encoder = NULL;
#endif

static size_t s_before_free_8bit;
static size_t s_before_free_32bit;

static esp_lv_adapter_rotation_t get_configured_rotation(void)
{
#if CONFIG_EXAMPLE_DISPLAY_ROTATION_0
    return ESP_LV_ADAPTER_ROTATE_0;
#elif CONFIG_EXAMPLE_DISPLAY_ROTATION_90
    return ESP_LV_ADAPTER_ROTATE_90;
#elif CONFIG_EXAMPLE_DISPLAY_ROTATION_180
    return ESP_LV_ADAPTER_ROTATE_180;
#elif CONFIG_EXAMPLE_DISPLAY_ROTATION_270
    return ESP_LV_ADAPTER_ROTATE_270;
#else
    return ESP_LV_ADAPTER_ROTATE_0;
#endif
}

/*
 * When set, select_tear_avoid_mode() returns TRIPLE_PARTIAL so the flush
 * regression exercises display_bridge_pipeline_wait_free_buf() rather than
 * DOUBLE_FULL's double_wait_sem path.
 */
static bool s_force_pipeline_tear_mode;

static esp_lv_adapter_tear_avoid_mode_t select_tear_avoid_mode(void)
{
    if (s_force_pipeline_tear_mode) {
        return ESP_LV_ADAPTER_TEAR_AVOID_MODE_TRIPLE_PARTIAL;
    }
#if CONFIG_EXAMPLE_LCD_INTERFACE_MIPI_DSI
    /* eco4 / P4 defaults: DEFAULT_MIPI_DSI expands to TRIPLE_PARTIAL. */
    return ESP_LV_ADAPTER_TEAR_AVOID_MODE_DEFAULT_MIPI_DSI;
#elif CONFIG_EXAMPLE_LCD_INTERFACE_RGB
    return ESP_LV_ADAPTER_TEAR_AVOID_MODE_DEFAULT_RGB;
#else
    return ESP_LV_ADAPTER_TEAR_AVOID_MODE_DEFAULT;
#endif
}

#if defined(CONFIG_ESP_LVGL_ADAPTER_ENABLE_DECODER) || defined(CONFIG_ESP_LVGL_ADAPTER_ENABLE_FREETYPE)
static esp_err_t mount_asset_partition(asset_ctx_t *ctx, const char *partition_label,
                                       int max_files, uint32_t checksum, char drive_letter)
{
    if (ctx->mounted) {
        return ESP_OK;
    }

    const mmap_assets_config_t mmap_cfg = {
        .partition_label = partition_label,
        .max_files = max_files,
        .checksum = checksum,
        .flags = { .mmap_enable = 1 },
    };

    ESP_RETURN_ON_ERROR(mmap_assets_new(&mmap_cfg, &ctx->mmap_handle), TAG,
                        "Failed to init mmap for '%s'", partition_label);

    size_t file_count = mmap_assets_get_stored_files(ctx->mmap_handle);
    if (file_count == 0) {
        mmap_assets_del(ctx->mmap_handle);
        ctx->mmap_handle = NULL;
        return ESP_ERR_NOT_FOUND;
    }

    const fs_cfg_t fs_cfg = {
        .fs_letter = drive_letter,
        .fs_nums = (int)file_count,
        .fs_assets = ctx->mmap_handle,
    };

    esp_err_t ret = esp_lv_adapter_fs_mount(&fs_cfg, &ctx->fs_handle);
    if (ret != ESP_OK) {
        mmap_assets_del(ctx->mmap_handle);
        ctx->mmap_handle = NULL;
        return ret;
    }

    ctx->drive_letter = drive_letter;
    ctx->mounted = true;
    ESP_LOGI(TAG, "Mounted '%s' as '%c:' (%zu files)", partition_label, drive_letter, file_count);
    return ESP_OK;
}

static void unmount_asset_partition(asset_ctx_t *ctx)
{
    if (!ctx->mounted) {
        return;
    }
    if (ctx->fs_handle) {
        esp_lv_adapter_fs_unmount(ctx->fs_handle);
        ctx->fs_handle = NULL;
    }
    if (ctx->mmap_handle) {
        mmap_assets_del(ctx->mmap_handle);
        ctx->mmap_handle = NULL;
    }
    ctx->mounted = false;
}
#endif

#if CONFIG_ESP_LVGL_ADAPTER_ENABLE_DECODER
static esp_err_t mount_image_assets(void)
{
    return mount_asset_partition(&s_image_assets, "images",
                                 MMAP_IMAGES_FILES, MMAP_IMAGES_CHECKSUM, 'I');
}
#endif

#if CONFIG_ESP_LVGL_ADAPTER_ENABLE_FREETYPE
static esp_err_t mount_font_assets(void)
{
    return mount_asset_partition(&s_font_assets, "fonts",
                                 MMAP_FONTS_FILES, MMAP_FONTS_CHECKSUM, 'F');
}
#endif

static void unmount_all_assets(void)
{
#if CONFIG_ESP_LVGL_ADAPTER_ENABLE_DECODER
    unmount_asset_partition(&s_image_assets);
#endif
#if CONFIG_ESP_LVGL_ADAPTER_ENABLE_FREETYPE
    unmount_asset_partition(&s_font_assets);
#endif
}

static void test_init_display(void)
{
    esp_lcd_panel_handle_t panel = NULL;
    esp_lcd_panel_io_handle_t io = NULL;
    esp_lv_adapter_rotation_t rotation = get_configured_rotation();
    esp_lv_adapter_tear_avoid_mode_t tear_avoid_mode = select_tear_avoid_mode();

    TEST_ESP_OK(hw_lcd_init(&panel, &io, tear_avoid_mode, rotation));

    const esp_lv_adapter_config_t adapter_config = ESP_LV_ADAPTER_DEFAULT_CONFIG();
    TEST_ESP_OK(esp_lv_adapter_init(&adapter_config));

#if CONFIG_EXAMPLE_LCD_INTERFACE_MIPI_DSI
    esp_lv_adapter_display_config_t display_config = ESP_LV_ADAPTER_DISPLAY_MIPI_DEFAULT_CONFIG(
                                                         panel, io, HW_LCD_H_RES, HW_LCD_V_RES, rotation);
#elif CONFIG_EXAMPLE_LCD_INTERFACE_RGB
    esp_lv_adapter_display_config_t display_config = ESP_LV_ADAPTER_DISPLAY_RGB_DEFAULT_CONFIG(
                                                         panel, io, HW_LCD_H_RES, HW_LCD_V_RES, rotation);
#elif CONFIG_EXAMPLE_LCD_INTERFACE_SPI_WITHOUT_PSRAM
    esp_lv_adapter_display_config_t display_config = ESP_LV_ADAPTER_DISPLAY_SPI_WITHOUT_PSRAM_DEFAULT_CONFIG(
                                                         panel, io, HW_LCD_H_RES, HW_LCD_V_RES, rotation);
#else
    esp_lv_adapter_display_config_t display_config = ESP_LV_ADAPTER_DISPLAY_SPI_WITH_PSRAM_DEFAULT_CONFIG(
                                                         panel, io, HW_LCD_H_RES, HW_LCD_V_RES, rotation);
#endif

    /* Keep the default display profile; only override the selected tear mode. */
    display_config.tear_avoid_mode = tear_avoid_mode;
    s_disp = esp_lv_adapter_register_display(&display_config);
    TEST_ASSERT_NOT_NULL(s_disp);

#if HW_USE_TOUCH
    esp_lcd_touch_handle_t touch_handle = NULL;
    TEST_ESP_OK(hw_touch_init(&touch_handle, rotation));

    const esp_lv_adapter_touch_config_t touch_cfg = {
        .disp = s_disp,
        .handle = touch_handle,
        .scale = { .x = 1.0f, .y = 1.0f },
    };
    s_touch = esp_lv_adapter_register_touch(&touch_cfg);
    TEST_ASSERT_NOT_NULL(s_touch);
#elif HW_USE_ENCODER
    const esp_lv_adapter_encoder_config_t encoder_cfg = {
        .disp = s_disp,
        .encoder_a_b = hw_knob_get_config(),
        .encoder_enter = hw_knob_get_button(),
    };
    s_encoder = esp_lv_adapter_register_encoder(&encoder_cfg);
    TEST_ASSERT_NOT_NULL(s_encoder);
#endif

    TEST_ESP_OK(esp_lv_adapter_start());
}

static void test_deinit_display(void)
{
    unmount_all_assets();

    if (esp_lv_adapter_is_initialized()) {
        TEST_ESP_OK(esp_lv_adapter_deinit());
    }

    s_disp = NULL;
#if HW_USE_TOUCH
    s_touch = NULL;
#endif
#if HW_USE_ENCODER
    s_encoder = NULL;
#endif

    TEST_ESP_OK(hw_lcd_deinit());
}

void setUp(void)
{
    s_before_free_8bit = heap_caps_get_free_size(MALLOC_CAP_8BIT);
    s_before_free_32bit = heap_caps_get_free_size(MALLOC_CAP_32BIT);
    test_init_display();
}

void tearDown(void)
{
    s_force_pipeline_tear_mode = false;
    test_deinit_display();
    size_t after_free_8bit = heap_caps_get_free_size(MALLOC_CAP_8BIT);
    size_t after_free_32bit = heap_caps_get_free_size(MALLOC_CAP_32BIT);
    unity_utils_check_leak(s_before_free_8bit, after_free_8bit, "8BIT", TEST_MEMORY_LEAK_THRESHOLD);
    unity_utils_check_leak(s_before_free_32bit, after_free_32bit, "32BIT", TEST_MEMORY_LEAK_THRESHOLD);
}

TEST_CASE("adapter basic demo", "[adapter][basic]")
{
    ESP_LOGI(TAG, "Running basic adapter test");

    TEST_ESP_OK(esp_lv_adapter_lock(pdMS_TO_TICKS(LVGL_LOCK_TIMEOUT_MS)));

    lv_obj_t *scr = lv_display_get_screen_active(s_disp);
    lv_obj_clean(scr);

    lv_obj_t *title = lv_label_create(scr);
    lv_label_set_text(title, "LVGL Adapter Test");
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 12);

    lv_obj_t *info = lv_label_create(scr);
    lv_label_set_text_fmt(info,
                          "Resolution: %dx%d\n"
                          "Touch: %s\n"
                          "Encoder: %s",
                          (int)lv_display_get_horizontal_resolution(s_disp),
                          (int)lv_display_get_vertical_resolution(s_disp),
                          HW_USE_TOUCH ? "YES" : "NO",
                          HW_USE_ENCODER ? "YES" : "NO");
    lv_obj_center(info);

    esp_lv_adapter_unlock();
    vTaskDelay(pdMS_TO_TICKS(TEST_DISPLAY_TIME_MS));

    TEST_ESP_OK(esp_lv_adapter_lock(pdMS_TO_TICKS(LVGL_LOCK_TIMEOUT_MS)));
    lv_obj_clean(lv_display_get_screen_active(s_disp));
    esp_lv_adapter_unlock();
    ESP_LOGI(TAG, "Basic adapter test completed");
}

#if CONFIG_ESP_LVGL_ADAPTER_ENABLE_DECODER
TEST_CASE("adapter image decode", "[adapter][decode]")
{
    ESP_LOGI(TAG, "Running image decoder test");
    TEST_ESP_OK(mount_image_assets());

    const char *images[] = {
        "I:red_png.png",
        "I:red_jpg.jpg"
    };

    TEST_ESP_OK(esp_lv_adapter_lock(pdMS_TO_TICKS(LVGL_LOCK_TIMEOUT_MS)));
    lv_obj_clean(lv_display_get_screen_active(s_disp));
    lv_obj_t *img = lv_img_create(lv_display_get_screen_active(s_disp));
    lv_obj_center(img);
    esp_lv_adapter_unlock();

    for (size_t i = 0; i < sizeof(images) / sizeof(images[0]); ++i) {
        ESP_LOGI(TAG, "Loading image: %s", images[i]);

        TEST_ESP_OK(esp_lv_adapter_lock(pdMS_TO_TICKS(LVGL_LOCK_TIMEOUT_MS)));
        lv_img_set_src(img, images[i]);
        esp_lv_adapter_unlock();

        vTaskDelay(pdMS_TO_TICKS(TEST_IMAGE_DISPLAY_TIME_MS));
    }

    TEST_ESP_OK(esp_lv_adapter_lock(pdMS_TO_TICKS(LVGL_LOCK_TIMEOUT_MS)));
    lv_obj_clean(lv_display_get_screen_active(s_disp));
    esp_lv_adapter_unlock();
    ESP_LOGI(TAG, "Image decoder test completed");
}
#endif

TEST_CASE("adapter dummy draw", "[adapter][dummy]")
{
    ESP_LOGI(TAG, "Running dummy draw test");
#if CONFIG_IDF_TARGET_ESP32C3
    const size_t pixel_count = 80 * 80;
#else
    const size_t pixel_count = (size_t)HW_LCD_H_RES * HW_LCD_V_RES;
#endif

    uint16_t *framebuffer = NULL;
#if CONFIG_IDF_TARGET_ESP32C3
    framebuffer = heap_caps_malloc(pixel_count * sizeof(uint16_t), MALLOC_CAP_8BIT);
#else
    framebuffer = heap_caps_malloc(pixel_count * sizeof(uint16_t),
                                   MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!framebuffer) {
        framebuffer = heap_caps_malloc(pixel_count * sizeof(uint16_t), MALLOC_CAP_8BIT);
    }
#endif
    TEST_ASSERT_NOT_NULL(framebuffer);

    const uint16_t colors[] = { 0xF800, 0x07E0, 0x001F };
    const char *color_names[] = { "Red", "Green", "Blue" };

    TEST_ESP_OK(esp_lv_adapter_set_dummy_draw(s_disp, true));

    for (size_t i = 0; i < sizeof(colors) / sizeof(colors[0]); ++i) {
        ESP_LOGI(TAG, "Drawing %s screen", color_names[i]);
        for (size_t px = 0; px < pixel_count; ++px) {
            framebuffer[px] = colors[i];
        }
        TEST_ESP_OK(esp_lv_adapter_dummy_draw_blit(s_disp, 0, 0, HW_LCD_H_RES, HW_LCD_V_RES, framebuffer, true));
        vTaskDelay(pdMS_TO_TICKS(TEST_COLOR_DISPLAY_TIME_MS));
    }

    TEST_ESP_OK(esp_lv_adapter_set_dummy_draw(s_disp, false));
    free(framebuffer);
    vTaskDelay(pdMS_TO_TICKS(300));
    ESP_LOGI(TAG, "Dummy draw test completed");
}

#if CONFIG_ESP_LVGL_ADAPTER_ENABLE_FREETYPE
TEST_CASE("adapter freetype font", "[adapter][freetype]")
{
    ESP_LOGI(TAG, "Running FreeType font test");
    TEST_ESP_OK(mount_font_assets());

    const char *font_path = "F:DejaVuSans.ttf";
    esp_lv_adapter_ft_font_handle_t font30_handle = NULL;
    esp_lv_adapter_ft_font_handle_t font48_handle = NULL;

    ESP_LOGI(TAG, "Initializing 30px font");
    const esp_lv_adapter_ft_font_config_t cfg30 = ESP_LV_ADAPTER_FT_FONT_FILE_CONFIG(
                                                      font_path, 30, ESP_LV_ADAPTER_FT_FONT_STYLE_NORMAL);
    TEST_ESP_OK(esp_lv_adapter_ft_font_init(&cfg30, &font30_handle));

    ESP_LOGI(TAG, "Initializing 48px font");
    const esp_lv_adapter_ft_font_config_t cfg48 = ESP_LV_ADAPTER_FT_FONT_FILE_CONFIG(
                                                      font_path, 48, ESP_LV_ADAPTER_FT_FONT_STYLE_NORMAL);
    TEST_ESP_OK(esp_lv_adapter_ft_font_init(&cfg48, &font48_handle));

    const lv_font_t *font30 = esp_lv_adapter_ft_font_get(font30_handle);
    const lv_font_t *font48 = esp_lv_adapter_ft_font_get(font48_handle);
    TEST_ASSERT_NOT_NULL(font30);
    TEST_ASSERT_NOT_NULL(font48);

    TEST_ESP_OK(esp_lv_adapter_lock(pdMS_TO_TICKS(LVGL_LOCK_TIMEOUT_MS)));

    lv_obj_t *scr = lv_display_get_screen_active(s_disp);
    lv_obj_clean(scr);
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x30363D), 0);

    lv_obj_t *title = lv_label_create(scr);
    lv_obj_set_style_text_font(title, font30, 0);
    lv_obj_set_style_text_color(title, lv_color_hex(0xFFFFFF), 0);
    lv_label_set_text(title, "FreeType Test");
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 12);

    lv_obj_t *small = lv_label_create(scr);
    lv_obj_set_style_text_font(small, font30, 0);
    lv_obj_set_style_text_color(small, lv_color_hex(0xFFFFFF), 0);
    lv_label_set_text(small, "Size 30");
    lv_obj_align(small, LV_ALIGN_CENTER, 0, -30);

    lv_obj_t *large = lv_label_create(scr);
    lv_obj_set_style_text_font(large, font48, 0);
    lv_obj_set_style_text_color(large, lv_color_hex(0xF0E130), 0);
    lv_label_set_text(large, "Size 48");
    lv_obj_align(large, LV_ALIGN_CENTER, 0, 30);

    esp_lv_adapter_unlock();
    vTaskDelay(pdMS_TO_TICKS(TEST_DISPLAY_TIME_MS));

    TEST_ESP_OK(esp_lv_adapter_lock(pdMS_TO_TICKS(LVGL_LOCK_TIMEOUT_MS)));
    lv_obj_clean(lv_display_get_screen_active(s_disp));
    esp_lv_adapter_unlock();

    ESP_LOGI(TAG, "Cleaning up fonts");
    TEST_ESP_OK(esp_lv_adapter_ft_font_deinit(font48_handle));
    TEST_ESP_OK(esp_lv_adapter_ft_font_deinit(font30_handle));
    ESP_LOGI(TAG, "FreeType font test completed");
}
#endif

/*
 * Minimal regression for the flush pipeline notification target.
 *
 * LVGL may call the display flush from a task that is not the adapter task.
 * The pipeline waits on a bridge semaphore.  The ISR gives that semaphore
 * and does not clear or increment this task's notification index 0.
 * The test app disables LV_USE_FREERTOS_TASK_NOTIFY so LVGL's draw-thread
 * sync does not consume that same index.
 *
 * This task posts one index-0 notification, then performs a synchronous
 * refresh.  The notification must still be readable afterwards, including
 * when a free buffer is already available and the wait returns without
 * blocking.  A missed wakeup still stalls after the pipeline buffers are busy.
 *
 * Tear mode must be a wait_free_buf() path (TRIPLE_PARTIAL / DOUBLE_PARTIAL /
 * TRIPLE_FULL).  DOUBLE_FULL waits on double_wait_sem instead.  eco4's
 * DEFAULT_MIPI_DSI already expands to TRIPLE_PARTIAL; the test forces that
 * mode explicitly.
 */
static SemaphoreHandle_t s_flush_done;
static volatile bool s_flush_ok;
static esp_timer_handle_t s_flush_watchdog;

static void flush_watchdog_cb(void *arg)
{
    (void)arg;
    ESP_LOGE(TAG, "flush task watchdog expired; resetting");
    esp_restart();
}

static void non_adapter_flush_task(void *arg)
{
    (void)arg;
    bool ok = true;

    for (int i = 0; i < 8; i++) {
        (void)xTaskNotifyGive(xTaskGetCurrentTaskHandle());

        esp_err_t err = esp_lv_adapter_lock(LVGL_LOCK_TIMEOUT_MS);
        if (err != ESP_OK) {
            ok = false;
            break;
        }

        lv_obj_t *scr = lv_display_get_screen_active(s_disp);
        if (scr == NULL) {
            esp_lv_adapter_unlock();
            ok = false;
            break;
        }

        /* Force a full drawable region, then flush synchronously from this task. */
        lv_obj_invalidate(scr);
        lv_refr_now(s_disp);
        esp_lv_adapter_unlock();

        if (ulTaskNotifyTake(pdTRUE, 0) == 0) {
            ok = false;
            break;
        }

        vTaskDelay(pdMS_TO_TICKS(2));
    }

    s_flush_ok = ok;
    xSemaphoreGive(s_flush_done);
    /* Stay parked until the test task deletes us. Self-delete would defer
     * TCB/stack reclaim to the idle task and race tearDown leak checks. */
    for (;;) {
        vTaskDelay(portMAX_DELAY);
    }
}

static SemaphoreHandle_t s_dummy_done;
static volatile bool s_dummy_ok;

static void dummy_draw_concurrent_task(void *arg)
{
    (void)arg;
    bool ok = true;
    const size_t fb_bytes = (size_t)HW_LCD_H_RES * HW_LCD_V_RES * sizeof(uint16_t);

    for (int i = 0; i < 8; i++) {
        void *fb = esp_lv_adapter_dummy_draw_get_free_buf(s_disp);
        if (fb == NULL) {
            ok = false;
            break;
        }
        memset(fb, (i & 1) ? 0x1F : 0xF8, fb_bytes);
        esp_err_t err = esp_lv_adapter_dummy_draw_flush_buf(s_disp, fb);
        if (err != ESP_OK) {
            ok = false;
            break;
        }
        vTaskDelay(pdMS_TO_TICKS(2));
    }

    s_dummy_ok = ok;
    xSemaphoreGive(s_dummy_done);
    for (;;) {
        vTaskDelay(portMAX_DELAY);
    }
}

TEST_CASE("adapter dummy draw concurrent with flush", "[concurrent]")
{
    ESP_LOGI(TAG, "Running dummy-draw/flush concurrency test");

#if !CONFIG_EXAMPLE_LCD_INTERFACE_MIPI_DSI && !CONFIG_EXAMPLE_LCD_INTERFACE_RGB
    TEST_IGNORE_MESSAGE("concurrency regression needs MIPI/RGB pipeline tear mode");
#endif

    test_deinit_display();
    s_force_pipeline_tear_mode = true;
    test_init_display();
    TEST_ESP_OK(esp_lv_adapter_set_dummy_draw(s_disp, true));

    s_flush_done = xSemaphoreCreateBinary();
    s_dummy_done = xSemaphoreCreateBinary();
    TEST_ASSERT_NOT_NULL(s_flush_done);
    TEST_ASSERT_NOT_NULL(s_dummy_done);
    s_flush_ok = false;
    s_dummy_ok = false;

    const esp_timer_create_args_t watchdog_args = {
        .callback = flush_watchdog_cb,
        .name = "flush_wd",
    };
    TEST_ESP_OK(esp_timer_create(&watchdog_args, &s_flush_watchdog));
    TEST_ESP_OK(esp_timer_start_once(s_flush_watchdog, 8000000));

    TaskHandle_t flush_task = NULL;
    TaskHandle_t dummy_task = NULL;
    TEST_ASSERT_EQUAL(pdPASS, xTaskCreate(non_adapter_flush_task, "flush_task",
                                          16384, NULL, 5, &flush_task));
    TEST_ASSERT_EQUAL(pdPASS, xTaskCreate(dummy_draw_concurrent_task, "dummy_task",
                                          16384, NULL, 5, &dummy_task));
    TEST_ASSERT_NOT_NULL(flush_task);
    TEST_ASSERT_NOT_NULL(dummy_task);

    const bool flush_done = xSemaphoreTake(s_flush_done, pdMS_TO_TICKS(10000)) == pdTRUE;
    const bool dummy_done = xSemaphoreTake(s_dummy_done, pdMS_TO_TICKS(10000)) == pdTRUE;
    TEST_ESP_OK(esp_timer_stop(s_flush_watchdog));
    TEST_ESP_OK(esp_timer_delete(s_flush_watchdog));
    s_flush_watchdog = NULL;
    vTaskDelete(flush_task);
    vTaskDelete(dummy_task);

    TEST_ASSERT_TRUE_MESSAGE(flush_done, "flush blocked during dummy draw");
    TEST_ASSERT_TRUE_MESSAGE(dummy_done, "dummy draw blocked during flush");
    TEST_ASSERT_TRUE_MESSAGE(s_flush_ok, "flush task failed");
    TEST_ASSERT_TRUE_MESSAGE(s_dummy_ok, "dummy draw task failed");

    vSemaphoreDelete(s_flush_done);
    vSemaphoreDelete(s_dummy_done);
    s_flush_done = NULL;
    s_dummy_done = NULL;
    s_force_pipeline_tear_mode = false;
    ESP_LOGI(TAG, "Dummy-draw/flush concurrency test completed");
}

TEST_CASE("adapter flush from non-adapter task", "[flush]")
{
    ESP_LOGI(TAG, "Running non-adapter-task flush test");

#if !CONFIG_EXAMPLE_LCD_INTERFACE_MIPI_DSI && !CONFIG_EXAMPLE_LCD_INTERFACE_RGB
    TEST_IGNORE_MESSAGE("flush notify regression needs MIPI/RGB wait_free_buf tear mode");
#endif

    /* Re-init with an explicit pipeline tear mode (keep sync lv_refr_now). */
    test_deinit_display();
    s_force_pipeline_tear_mode = true;
    test_init_display();
    TEST_ASSERT_EQUAL(ESP_LV_ADAPTER_TEAR_AVOID_MODE_TRIPLE_PARTIAL, select_tear_avoid_mode());

    s_flush_done = xSemaphoreCreateBinary();
    TEST_ASSERT_NOT_NULL(s_flush_done);
    s_flush_ok = false;

    const esp_timer_create_args_t watchdog_args = {
        .callback = flush_watchdog_cb,
        .name = "flush_wd",
    };
    TEST_ESP_OK(esp_timer_create(&watchdog_args, &s_flush_watchdog));
    /* Reset the test app if the bug is present; deleting a task that owns
     * the LVGL mutex would leave teardown deadlocked. */
    TEST_ESP_OK(esp_timer_start_once(s_flush_watchdog, 8000000));

    TaskHandle_t task = NULL;
    TEST_ASSERT_EQUAL(pdPASS, xTaskCreate(non_adapter_flush_task, "flush_task",
                                          16384, NULL, 5, &task));
    TEST_ASSERT_NOT_NULL(task);

    const bool completed = xSemaphoreTake(s_flush_done, pdMS_TO_TICKS(10000)) == pdTRUE;
    TEST_ESP_OK(esp_timer_stop(s_flush_watchdog));
    TEST_ESP_OK(esp_timer_delete(s_flush_watchdog));
    s_flush_watchdog = NULL;
    vTaskDelete(task);
    task = NULL;

    TEST_ASSERT_TRUE_MESSAGE(completed, "flush task blocked waiting for frame completion");
    TEST_ASSERT_TRUE_MESSAGE(s_flush_ok, "flush task failed");

    vSemaphoreDelete(s_flush_done);
    s_flush_done = NULL;
    s_force_pipeline_tear_mode = false;
    ESP_LOGI(TAG, "Non-adapter flush task completed");
}

void app_main(void)
{
    ESP_LOGI(TAG, "ESP LVGL Adapter Feature Test Suite");

    /* Warm-up: absorb one-time lazy allocations (I2C bus, GPIO ISR, touch
     * driver handle, etc.) so they don't skew the first test's leak check. */
    test_init_display();
    test_deinit_display();

    unity_run_menu();
}
