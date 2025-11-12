/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file test_esp_lv_adapter_features.c
 * @brief ESP LVGL Adapter Feature Test Suite - Validates display, decoder,
 *        FreeType, dummy draw, and input device support
 */

#include <stdint.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_check.h"
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

#define TEST_MEMORY_LEAK_THRESHOLD  (3000)
#define TEST_DISPLAY_TIME_MS        (1500)
#define TEST_IMAGE_DISPLAY_TIME_MS  (800)
#define TEST_COLOR_DISPLAY_TIME_MS  (400)
#define LVGL_LOCK_TIMEOUT_MS        (1000)

#if defined(CONFIG_ESP_LVGL_ADAPTER_ENABLE_DECODER) || defined(CONFIG_ESP_LVGL_ADAPTER_ENABLE_FREETYPE)
typedef struct {
    mmap_assets_handle_t mmap_handle;
    esp_lv_fs_handle_t fs_handle;      /**< LVGL filesystem handle */
    char drive_letter;                  /**< Drive letter for filesystem mounting (e.g., 'I' for images) */
    bool mounted;                       /**< Mount status flag */
} asset_ctx_t;
#endif

/* ============================================================================
 * Static Variables
 * ============================================================================ */

/** @brief Image assets context (images partition) */
#if CONFIG_ESP_LVGL_ADAPTER_ENABLE_DECODER
static asset_ctx_t s_image_assets = { .drive_letter = 'I' };
#endif

/** @brief Font assets context (fonts partition) */
#if CONFIG_ESP_LVGL_ADAPTER_ENABLE_FREETYPE
static asset_ctx_t s_font_assets = { .drive_letter = 'F' };
#endif

/** @brief LVGL display handle */
static lv_display_t *s_disp = NULL;

/** @brief Touch input device handle */
#if HW_USE_TOUCH
static lv_indev_t *s_touch = NULL;
#endif

/** @brief Encoder input device handle */
#if HW_USE_ENCODER
static lv_indev_t *s_encoder = NULL;
#endif

/** @brief Heap memory tracking for leak detection */
static size_t s_before_free_8bit;
static size_t s_before_free_32bit;

/* ============================================================================
 * Helper Functions
 * ============================================================================ */

/**
 * @brief Get the configured display rotation from Kconfig
 *
 * Reads the CONFIG_EXAMPLE_DISPLAY_ROTATION_XXX configuration options
 * and returns the corresponding rotation enum value.
 *
 * @return esp_lv_adapter_rotation_t The configured rotation angle
 */
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

/**
 * @brief Select the appropriate tearing effect mode based on LCD interface
 *
 * Different LCD interfaces (MIPI DSI, RGB, SPI) require different tearing
 * effect configurations for optimal performance and visual quality.
 *
 * @return esp_lv_adapter_tear_avoid_mode_t The tearing mode for the current LCD interface
 */
static esp_lv_adapter_tear_avoid_mode_t select_tear_avoid_mode(void)
{
#if CONFIG_EXAMPLE_LCD_INTERFACE_MIPI_DSI
    return ESP_LV_ADAPTER_TEAR_AVOID_MODE_DEFAULT_MIPI_DSI;
#elif CONFIG_EXAMPLE_LCD_INTERFACE_RGB
    return ESP_LV_ADAPTER_TEAR_AVOID_MODE_DEFAULT_RGB;
#else
    return ESP_LV_ADAPTER_TEAR_AVOID_MODE_DEFAULT;
#endif
}

#if defined(CONFIG_ESP_LVGL_ADAPTER_ENABLE_DECODER) || defined(CONFIG_ESP_LVGL_ADAPTER_ENABLE_FREETYPE)
/**
 * @brief Mount a memory-mapped asset partition to LVGL filesystem
 *
 * This function performs the following operations:
 * 1. Initializes memory-mapped access to the flash partition
 * 2. Verifies that files exist in the partition
 * 3. Mounts the partition as an LVGL filesystem with specified drive letter
 *
 * @param[in,out] ctx Pointer to asset context to be populated
 * @param[in] partition_label Flash partition label (e.g., "images", "fonts")
 * @param[in] max_files Maximum number of files expected in partition
 * @param[in] checksum Expected checksum for partition validation
 * @param[in] drive_letter Filesystem drive letter (e.g., 'I' for images)
 *
 * @return
 *     - ESP_OK: Success
 *     - ESP_ERR_NOT_FOUND: No files found in partition
 *     - Other: Error from mmap_assets_new() or esp_lv_adapter_fs_mount()
 */
static esp_err_t mount_asset_partition(asset_ctx_t *ctx, const char *partition_label,
                                       int max_files, uint32_t checksum, char drive_letter)
{
    /* Skip if already mounted */
    if (ctx->mounted) {
        return ESP_OK;
    }

    /* Configure memory-mapped assets */
    const mmap_assets_config_t mmap_cfg = {
        .partition_label = partition_label,
        .max_files = max_files,
        .checksum = checksum,
        .flags = { .mmap_enable = 1 },
    };

    /* Initialize memory-mapped asset access */
    ESP_RETURN_ON_ERROR(mmap_assets_new(&mmap_cfg, &ctx->mmap_handle), TAG,
                        "Failed to initialize mmap for partition '%s'", partition_label);

    /* Verify partition contains files */
    size_t file_count = mmap_assets_get_stored_files(ctx->mmap_handle);
    if (file_count == 0) {
        mmap_assets_del(ctx->mmap_handle);
        ctx->mmap_handle = NULL;
        ESP_LOGW(TAG, "No files found in partition '%s'", partition_label);
        return ESP_ERR_NOT_FOUND;
    }

    ESP_LOGI(TAG, "Found %zu files in partition '%s'", file_count, partition_label);

    /* Configure LVGL filesystem */
    const fs_cfg_t fs_cfg = {
        .fs_letter = drive_letter,
        .fs_nums = (int)file_count,
        .fs_assets = ctx->mmap_handle,
    };

    /* Mount to LVGL filesystem */
    esp_err_t ret = esp_lv_adapter_fs_mount(&fs_cfg, &ctx->fs_handle);
    if (ret != ESP_OK) {
        mmap_assets_del(ctx->mmap_handle);
        ctx->mmap_handle = NULL;
        ESP_LOGE(TAG, "Failed to mount filesystem for partition '%s'", partition_label);
        return ret;
    }

    ctx->drive_letter = drive_letter;
    ctx->mounted = true;
    ESP_LOGI(TAG, "Mounted partition '%s' as drive '%c:'", partition_label, drive_letter);
    return ESP_OK;
}

/**
 * @brief Unmount an asset partition and free resources
 *
 * Safely unmounts the LVGL filesystem and releases memory-mapped resources.
 * Safe to call multiple times or on non-mounted partitions.
 *
 * @param[in,out] ctx Pointer to asset context to be cleaned up
 */
static void unmount_asset_partition(asset_ctx_t *ctx)
{
    if (!ctx->mounted) {
        return;
    }

    /* Unmount LVGL filesystem */
    if (ctx->fs_handle) {
        esp_lv_adapter_fs_unmount(ctx->fs_handle);
        ctx->fs_handle = NULL;
    }

    /* Release memory-mapped resources */
    if (ctx->mmap_handle) {
        mmap_assets_del(ctx->mmap_handle);
        ctx->mmap_handle = NULL;
    }

    ctx->mounted = false;
    ESP_LOGI(TAG, "Unmounted asset partition '%c:'", ctx->drive_letter);
}
#endif /* CONFIG_ESP_LVGL_ADAPTER_ENABLE_DECODER || CONFIG_ESP_LVGL_ADAPTER_ENABLE_FREETYPE */

#if CONFIG_ESP_LVGL_ADAPTER_ENABLE_DECODER
/**
 * @brief Mount the images partition for decoder testing
 *
 * Mounts the 'images' partition containing test PNG and JPG files
 * to drive letter 'I:'
 *
 * @return
 *     - ESP_OK: Success
 *     - ESP_ERR_NOT_FOUND: Images partition not found or empty
 *     - Other: Error from mount_asset_partition()
 */
static esp_err_t mount_image_assets(void)
{
    return mount_asset_partition(&s_image_assets, "images",
                                 MMAP_IMAGES_FILES, MMAP_IMAGES_CHECKSUM, 'I');
}
#endif

#if CONFIG_ESP_LVGL_ADAPTER_ENABLE_FREETYPE
/**
 * @brief Mount the fonts partition for FreeType testing
 *
 * Mounts the 'fonts' partition containing TrueType font files
 * to drive letter 'F:'
 *
 * @return
 *     - ESP_OK: Success
 *     - ESP_ERR_NOT_FOUND: Fonts partition not found or empty
 *     - Other: Error from mount_asset_partition()
 */
static esp_err_t mount_font_assets(void)
{
    return mount_asset_partition(&s_font_assets, "fonts",
                                 MMAP_FONTS_FILES, MMAP_FONTS_CHECKSUM, 'F');
}
#endif

/**
 * @brief Unmount all mounted asset partitions
 *
 * Cleanup function to unmount all asset partitions that were mounted
 * during the test session. Safe to call even if partitions were not mounted.
 */
static void unmount_all_assets(void)
{
#if CONFIG_ESP_LVGL_ADAPTER_ENABLE_DECODER
    unmount_asset_partition(&s_image_assets);
#endif
#if CONFIG_ESP_LVGL_ADAPTER_ENABLE_FREETYPE
    unmount_asset_partition(&s_font_assets);
#endif
}

/**
 * @brief Initialize display and input devices for testing
 *
 * Performs complete initialization sequence:
 * 1. Initialize LCD panel hardware
 * 2. Initialize LVGL adapter
 * 3. Register display with appropriate interface configuration
 * 4. Register input device (touch or encoder)
 * 5. Start LVGL adapter task
 *
 * @note This function uses Unity assertions and will fail the test on error
 */
static void test_init_display(void)
{
    esp_lcd_panel_handle_t panel = NULL;
    esp_lcd_panel_io_handle_t io = NULL;
    esp_lv_adapter_rotation_t rotation = get_configured_rotation();
    esp_lv_adapter_tear_avoid_mode_t tear_avoid_mode = select_tear_avoid_mode();

    ESP_LOGI(TAG, "Initializing LCD: %dx%d, rotation=%d", HW_LCD_H_RES, HW_LCD_V_RES, rotation);
    TEST_ESP_OK(hw_lcd_init(&panel, &io, tear_avoid_mode, rotation));

    ESP_LOGI(TAG, "Initializing LVGL adapter");
    const esp_lv_adapter_config_t adapter_config = ESP_LV_ADAPTER_DEFAULT_CONFIG();
    TEST_ESP_OK(esp_lv_adapter_init(&adapter_config));

    /* Select display configuration based on LCD interface type */
#if CONFIG_EXAMPLE_LCD_INTERFACE_MIPI_DSI
    ESP_LOGI(TAG, "Registering MIPI DSI display");
    esp_lv_adapter_display_config_t display_config = ESP_LV_ADAPTER_DISPLAY_MIPI_DEFAULT_CONFIG(
                                                         panel, io, HW_LCD_H_RES, HW_LCD_V_RES, rotation);
#elif CONFIG_EXAMPLE_LCD_INTERFACE_RGB
    ESP_LOGI(TAG, "Registering RGB display");
    esp_lv_adapter_display_config_t display_config = ESP_LV_ADAPTER_DISPLAY_RGB_DEFAULT_CONFIG(
                                                         panel, io, HW_LCD_H_RES, HW_LCD_V_RES, rotation);
#elif CONFIG_EXAMPLE_LCD_INTERFACE_SPI_WITHOUT_PSRAM
    ESP_LOGI(TAG, "Registering SPI display (without PSRAM)");
    esp_lv_adapter_display_config_t display_config = ESP_LV_ADAPTER_DISPLAY_SPI_WITHOUT_PSRAM_DEFAULT_CONFIG(
                                                         panel, io, HW_LCD_H_RES, HW_LCD_V_RES, rotation);
#else
    ESP_LOGI(TAG, "Registering SPI display (with PSRAM)");
    esp_lv_adapter_display_config_t display_config = ESP_LV_ADAPTER_DISPLAY_SPI_WITH_PSRAM_DEFAULT_CONFIG(
                                                         panel, io, HW_LCD_H_RES, HW_LCD_V_RES, rotation);
#endif

    s_disp = esp_lv_adapter_register_display(&display_config);
    TEST_ASSERT_NOT_NULL(s_disp);
    ESP_LOGI(TAG, "Display registered successfully");

    /* Initialize input device if available */
#if HW_USE_TOUCH
    ESP_LOGI(TAG, "Initializing touch input");
    esp_lcd_touch_handle_t touch_handle = NULL;
    TEST_ESP_OK(hw_touch_init(&touch_handle, rotation));

    const esp_lv_adapter_touch_config_t touch_cfg = {
        .disp = s_disp,
        .handle = touch_handle,
        .scale = { .x = 1.0f, .y = 1.0f },
    };
    s_touch = esp_lv_adapter_register_touch(&touch_cfg);
    TEST_ASSERT_NOT_NULL(s_touch);
    ESP_LOGI(TAG, "Touch input registered successfully");

#elif HW_USE_ENCODER
    ESP_LOGI(TAG, "Initializing encoder input");
    const esp_lv_adapter_encoder_config_t encoder_cfg = {
        .disp = s_disp,
        .encoder_a_b = hw_knob_get_config(),
        .encoder_enter = hw_knob_get_button(),
    };
    s_encoder = esp_lv_adapter_register_encoder(&encoder_cfg);
    TEST_ASSERT_NOT_NULL(s_encoder);
    ESP_LOGI(TAG, "Encoder input registered successfully");
#endif

    ESP_LOGI(TAG, "Starting LVGL adapter");
    TEST_ESP_OK(esp_lv_adapter_start());
}

/* ============================================================================
 * Unity Test Hooks
 * ============================================================================ */

/**
 * @brief Unity setUp function called before each test case
 *
 * Records the current free heap sizes for memory leak detection
 */
void setUp(void)
{
    s_before_free_8bit = heap_caps_get_free_size(MALLOC_CAP_8BIT);
    s_before_free_32bit = heap_caps_get_free_size(MALLOC_CAP_32BIT);
}

/**
 * @brief Unity tearDown function called after each test case
 *
 * Validates that memory usage is within acceptable threshold
 */
void tearDown(void)
{
    size_t after_free_8bit = heap_caps_get_free_size(MALLOC_CAP_8BIT);
    size_t after_free_32bit = heap_caps_get_free_size(MALLOC_CAP_32BIT);
    unity_utils_check_leak(s_before_free_8bit, after_free_8bit, "8BIT", TEST_MEMORY_LEAK_THRESHOLD);
    unity_utils_check_leak(s_before_free_32bit, after_free_32bit, "32BIT", TEST_MEMORY_LEAK_THRESHOLD);
}

/* ============================================================================
 * Test Cases
 * ============================================================================ */

/**
 * @brief Test basic LVGL adapter functionality
 *
 * Validates:
 * - Screen access and clearing
 * - Label creation and text rendering
 * - Object alignment
 * - Lock/unlock mechanism
 *
 * @note Tagged as [adapter][basic] for selective test execution
 */
TEST_CASE("adapter basic demo", "[adapter][basic]")
{
    ESP_LOGI(TAG, "Running basic adapter test");

    TEST_ESP_OK(esp_lv_adapter_lock(pdMS_TO_TICKS(LVGL_LOCK_TIMEOUT_MS)));

    lv_obj_t *scr = lv_disp_get_scr_act(s_disp);
    lv_obj_clean(scr);

    /* Create title label */
    lv_obj_t *title = lv_label_create(scr);
    lv_label_set_text(title, "LVGL Adapter Test");
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 12);

    /* Create info label with system information */
    lv_obj_t *info = lv_label_create(scr);
    lv_label_set_text_fmt(info,
                          "Resolution: %dx%d\n"
                          "Touch: %s\n"
                          "Encoder: %s",
                          (int)lv_disp_get_hor_res(s_disp),
                          (int)lv_disp_get_ver_res(s_disp),
                          HW_USE_TOUCH ? "YES" : "NO",
                          HW_USE_ENCODER ? "YES" : "NO");
    lv_obj_center(info);

    esp_lv_adapter_unlock();

    /* Display for 1.5 seconds */
    vTaskDelay(pdMS_TO_TICKS(TEST_DISPLAY_TIME_MS));

    /* Clean up */
    TEST_ESP_OK(esp_lv_adapter_lock(pdMS_TO_TICKS(LVGL_LOCK_TIMEOUT_MS)));
    lv_obj_clean(lv_disp_get_scr_act(s_disp));
    esp_lv_adapter_unlock();

    ESP_LOGI(TAG, "Basic adapter test completed");
}

#if CONFIG_ESP_LVGL_ADAPTER_ENABLE_DECODER
/**
 * @brief Test image decoder functionality
 *
 * Validates:
 * - Asset partition mounting
 * - Image loading from filesystem
 * - PNG and JPG decoding
 * - Image display and switching
 *
 * @note Tagged as [adapter][decode] for selective test execution
 * @note Requires CONFIG_ESP_LVGL_ADAPTER_ENABLE_DECODER=y
 */
TEST_CASE("adapter image decode", "[adapter][decode]")
{
    ESP_LOGI(TAG, "Running image decoder test");

    /* Mount image assets partition */
    TEST_ESP_OK(mount_image_assets());

    const char *k_images[] = {
        "I:red_png.png",  /* PNG format test */
        "I:red_jpg.jpg"   /* JPG format test */
    };

    /* Create image widget */
    TEST_ESP_OK(esp_lv_adapter_lock(pdMS_TO_TICKS(LVGL_LOCK_TIMEOUT_MS)));
    lv_obj_clean(lv_disp_get_scr_act(s_disp));
    lv_obj_t *img = lv_img_create(lv_scr_act());
    lv_obj_center(img);
    esp_lv_adapter_unlock();

    /* Test each image format */
    for (size_t i = 0; i < sizeof(k_images) / sizeof(k_images[0]); ++i) {
        ESP_LOGI(TAG, "Loading image: %s", k_images[i]);

        TEST_ESP_OK(esp_lv_adapter_lock(pdMS_TO_TICKS(LVGL_LOCK_TIMEOUT_MS)));
        lv_img_set_src(img, k_images[i]);
        esp_lv_adapter_unlock();

        /* Display each image for 800ms */
        vTaskDelay(pdMS_TO_TICKS(TEST_IMAGE_DISPLAY_TIME_MS));
    }

    /* Clean up */
    TEST_ESP_OK(esp_lv_adapter_lock(pdMS_TO_TICKS(LVGL_LOCK_TIMEOUT_MS)));
    lv_obj_clean(lv_disp_get_scr_act(s_disp));
    esp_lv_adapter_unlock();

    ESP_LOGI(TAG, "Image decoder test completed");
}
#endif /* CONFIG_ESP_LVGL_ADAPTER_ENABLE_DECODER */

/**
 * @brief Test dummy draw mode functionality
 *
 * Dummy draw mode allows bypassing LVGL rendering and directly writing
 * framebuffers to the display. Useful for benchmarking and testing.
 *
 * Validates:
 * - Framebuffer allocation (PSRAM preferred)
 * - Dummy draw mode enable/disable
 * - Direct framebuffer blitting
 * - Color rendering (red, green, blue)
 *
 * @note Tagged as [adapter][dummy] for selective test execution
 */
TEST_CASE("adapter dummy draw", "[adapter][dummy]")
{
    ESP_LOGI(TAG, "Running dummy draw test");

#if CONFIG_IDF_TARGET_ESP32C3
    /* ESP32-C3 has limited internal RAM, use smaller test area */
    const size_t test_width = 80;
    const size_t test_height = 80;
    const size_t pixel_count = test_width * test_height;
    ESP_LOGI(TAG, "ESP32-C3: Using reduced test area %zux%zu", test_width, test_height);
#else
    const size_t pixel_count = (size_t)HW_LCD_H_RES * HW_LCD_V_RES;
#endif

    /* Allocate framebuffer - prefer PSRAM if available, otherwise use internal RAM */
    uint16_t *framebuffer = NULL;
#if CONFIG_IDF_TARGET_ESP32C3
    /* ESP32-C3 has no PSRAM, use internal RAM directly */
    framebuffer = heap_caps_malloc(pixel_count * sizeof(uint16_t), MALLOC_CAP_8BIT);
#else
    /* Try PSRAM first for other targets */
    framebuffer = heap_caps_malloc(pixel_count * sizeof(uint16_t),
                                   MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!framebuffer) {
        ESP_LOGW(TAG, "PSRAM allocation failed, using internal RAM");
        framebuffer = heap_caps_malloc(pixel_count * sizeof(uint16_t), MALLOC_CAP_8BIT);
    }
#endif
    TEST_ASSERT_NOT_NULL(framebuffer);
    ESP_LOGI(TAG, "Allocated %zu byte framebuffer", pixel_count * sizeof(uint16_t));

    /* Test colors: Red, Green, Blue in RGB565 format */
    const uint16_t colors[] = { 0xF800, 0x07E0, 0x001F };
    const char *color_names[] = { "Red", "Green", "Blue" };

    /* Enable dummy draw mode */
    TEST_ESP_OK(esp_lv_adapter_set_dummy_draw(s_disp, true));

    /* Render each color */
    for (size_t i = 0; i < sizeof(colors) / sizeof(colors[0]); ++i) {
        ESP_LOGI(TAG, "Drawing %s screen", color_names[i]);

        /* Fill framebuffer with color */
        for (size_t px = 0; px < pixel_count; ++px) {
            framebuffer[px] = colors[i];
        }

        /* Blit framebuffer to display (full screen) */
        TEST_ESP_OK(esp_lv_adapter_dummy_draw_blit(s_disp, 0, 0, HW_LCD_H_RES, HW_LCD_V_RES, framebuffer, true));
        vTaskDelay(pdMS_TO_TICKS(TEST_COLOR_DISPLAY_TIME_MS));
    }

    /* Disable dummy draw mode */
    TEST_ESP_OK(esp_lv_adapter_set_dummy_draw(s_disp, false));
    free(framebuffer);

    /* Give SPI driver time to cleanup resources */
    vTaskDelay(pdMS_TO_TICKS(300));

    ESP_LOGI(TAG, "Dummy draw test completed");
}

#if CONFIG_ESP_LVGL_ADAPTER_ENABLE_FREETYPE
/**
 * @brief Test FreeType font rendering
 *
 * Validates:
 * - Font asset partition mounting via esp_mmap_assets
 * - Zero-copy font loading using memory-mapped pointers
 * - Multiple font size rendering (30px, 48px)
 * - Font style application
 * - Text color and positioning
 *
 * @note LVGL v8 limitation: FreeType doesn't support LVGL virtual filesystem (lv_fs)
 * @note Solution: Use esp_mmap_assets pointers directly (zero-copy, no RAM overhead)
 * @note Stack requirement: Calling task needs >= 32KB stack (ensure CONFIG_ESP_MAIN_TASK_STACK_SIZE >= 32768)
 * @note LVGL v9: Can use virtual file system paths like "F:font.ttf" directly
 * @note Tagged as [adapter][freetype] for selective test execution
 * @note Requires CONFIG_ESP_LVGL_ADAPTER_ENABLE_FREETYPE=y
 */
TEST_CASE("adapter freetype font", "[adapter][freetype]")
{
    ESP_LOGI(TAG, "Running FreeType font test");

    /* Mount font assets partition (mmap for zero-copy access) */
    TEST_ESP_OK(mount_font_assets());

    /* Get font data pointer directly from mmap (zero-copy, no RAM allocation) */
    const uint8_t *font_data = mmap_assets_get_mem(s_font_assets.mmap_handle, MMAP_FONTS_DEJAVUSANS_TTF);
    int font_size = mmap_assets_get_size(s_font_assets.mmap_handle, MMAP_FONTS_DEJAVUSANS_TTF);
    TEST_ASSERT_NOT_NULL(font_data);
    TEST_ASSERT_GREATER_THAN(0, font_size);
    ESP_LOGI(TAG, "Font mmap pointer: %p, size: %d bytes", font_data, font_size);

    esp_lv_adapter_ft_font_handle_t font30_handle = NULL;
    esp_lv_adapter_ft_font_handle_t font48_handle = NULL;

    /* Initialize 30px font */
    ESP_LOGI(TAG, "Initializing 30px font");
    const esp_lv_adapter_ft_font_config_t cfg30 = {
        .name = "DejaVuSans.ttf",
        .mem = font_data,
        .mem_size = font_size,
        .size = 30,
        .style = ESP_LV_ADAPTER_FT_FONT_STYLE_NORMAL
    };
    TEST_ESP_OK(esp_lv_adapter_ft_font_init(&cfg30, &font30_handle));

    /* Initialize 48px font */
    ESP_LOGI(TAG, "Initializing 48px font");
    const esp_lv_adapter_ft_font_config_t cfg48 = {
        .name = "DejaVuSans.ttf",
        .mem = font_data,
        .mem_size = font_size,
        .size = 48,
        .style = ESP_LV_ADAPTER_FT_FONT_STYLE_NORMAL
    };
    TEST_ESP_OK(esp_lv_adapter_ft_font_init(&cfg48, &font48_handle));

    /* Get LVGL font objects */
    const lv_font_t *font30 = esp_lv_adapter_ft_font_get(font30_handle);
    const lv_font_t *font48 = esp_lv_adapter_ft_font_get(font48_handle);
    TEST_ASSERT_NOT_NULL(font30);
    TEST_ASSERT_NOT_NULL(font48);

    /* Create UI elements */
    TEST_ESP_OK(esp_lv_adapter_lock(pdMS_TO_TICKS(LVGL_LOCK_TIMEOUT_MS)));

    lv_obj_t *scr = lv_disp_get_scr_act(s_disp);
    lv_obj_clean(scr);
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x30363D), 0);

    /* Title label */
    lv_obj_t *title = lv_label_create(scr);
    lv_obj_set_style_text_font(title, font30, 0);
    lv_obj_set_style_text_color(title, lv_color_hex(0xFFFFFF), 0);
    lv_label_set_text(title, "FreeType Test");
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 12);

    /* Small font sample */
    lv_obj_t *small = lv_label_create(scr);
    lv_obj_set_style_text_font(small, font30, 0);
    lv_obj_set_style_text_color(small, lv_color_hex(0xFFFFFF), 0);
    lv_label_set_text(small, "Size 30");
    lv_obj_align(small, LV_ALIGN_CENTER, 0, -30);

    /* Large font sample */
    lv_obj_t *large = lv_label_create(scr);
    lv_obj_set_style_text_font(large, font48, 0);
    lv_obj_set_style_text_color(large, lv_color_hex(0xF0E130), 0);
    lv_label_set_text(large, "Size 48");
    lv_obj_align(large, LV_ALIGN_CENTER, 0, 30);

    esp_lv_adapter_unlock();

    /* Display for 1.5 seconds */
    vTaskDelay(pdMS_TO_TICKS(TEST_DISPLAY_TIME_MS));

    /* Clean up UI */
    TEST_ESP_OK(esp_lv_adapter_lock(pdMS_TO_TICKS(LVGL_LOCK_TIMEOUT_MS)));
    lv_obj_clean(lv_disp_get_scr_act(s_disp));
    esp_lv_adapter_unlock();

    /* Clean up fonts */
    ESP_LOGI(TAG, "Cleaning up fonts");
    TEST_ESP_OK(esp_lv_adapter_ft_font_deinit(font48_handle));
    TEST_ESP_OK(esp_lv_adapter_ft_font_deinit(font30_handle));

    ESP_LOGI(TAG, "FreeType font test completed");
}
#endif /* CONFIG_ESP_LVGL_ADAPTER_ENABLE_FREETYPE */

/* ============================================================================
 * Application Entry Point
 * ============================================================================ */

/**
 * @brief Application main entry point
 *
 * Initializes the display subsystem and starts the Unity test menu.
 * Users can select which tests to run via serial console.
 */
void app_main(void)
{
    ESP_LOGI(TAG, "====================================");
    ESP_LOGI(TAG, "ESP LVGL Adapter Feature Test Suite");
    ESP_LOGI(TAG, "====================================");

    /* Initialize display and input devices */
    test_init_display();

    /* Start Unity test runner (interactive menu) */
    unity_run_menu();

    /* Clean up mounted assets */
    unmount_all_assets();

    ESP_LOGI(TAG, "All tests completed");
}
