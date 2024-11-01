/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
/*
 * SPDX-License-Identifier: CC0-1.0
 */

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_timer.h"

#include "esp_err.h"
#include "esp_log.h"
#include "esp_check.h"
#include "esp_lcd_panel_ops.h"

#include "lvgl.h"
#include "esp_lv_fs.h"
#include "esp_lv_decoder.h"
#include "bsp/display.h"

#include "mmap_generate_Drive_A.h"
#include "mmap_generate_Drive_B.h"
#include "mmap_generate_Drive_C.h"
#include "mmap_generate_Drive_D.h"
#include "mmap_generate_Drive_E.h"
#include "mmap_generate_Drive_F.h"
#include "mmap_generate_Drive_G.h"
#include "mmap_generate_Drive_H.h"
#include "mmap_generate_Drive_I.h"
#include "mmap_generate_Drive_J.h"

static const char *TAG = "perf_decoder";

#define TEST_LCD_H_RES      110
#define TEST_LCD_V_RES      110
#define TEST_COUNTERS       1

#define SCHEDULE_WAIT_MS    3000
#define PARTITION_NUM       10

typedef struct {
    const char *partition_label;
    int max_files;
    int checksum;
} asset_config_data_t;

typedef struct {
    int64_t start;
    int64_t acc;
    char str1[15];
    char str2[15];
} PerfCounter;

static lv_disp_drv_t *lv_disp_drv = NULL;
static lv_disp_draw_buf_t *lv_disp_buf = NULL;
static SemaphoreHandle_t lv_flush_sync_sem;
static esp_lcd_panel_handle_t panel_handle = NULL;

static mmap_assets_handle_t mmap_drive_handle[PARTITION_NUM];
static esp_lv_fs_handle_t fs_drive_handle[PARTITION_NUM];
static PerfCounter perf_counters;

static const asset_config_data_t asset_configs[PARTITION_NUM] = {
    {"assets_A", MMAP_DRIVE_A_FILES, MMAP_DRIVE_A_CHECKSUM},
    {"assets_B", MMAP_DRIVE_B_FILES, MMAP_DRIVE_B_CHECKSUM},
    {"assets_C", MMAP_DRIVE_C_FILES, MMAP_DRIVE_C_CHECKSUM},
    {"assets_D", MMAP_DRIVE_D_FILES, MMAP_DRIVE_D_CHECKSUM},
    {"assets_E", MMAP_DRIVE_E_FILES, MMAP_DRIVE_E_CHECKSUM},
    {"assets_F", MMAP_DRIVE_F_FILES, MMAP_DRIVE_F_CHECKSUM},
    {"assets_G", MMAP_DRIVE_G_FILES, MMAP_DRIVE_G_CHECKSUM},
    {"assets_H", MMAP_DRIVE_H_FILES, MMAP_DRIVE_H_CHECKSUM},
    {"assets_I", MMAP_DRIVE_I_FILES, MMAP_DRIVE_I_CHECKSUM},
    {"assets_J", MMAP_DRIVE_J_FILES, MMAP_DRIVE_J_CHECKSUM},
};

static void perfmon_start(const char* fmt1, const char* fmt2, ...)
{
    va_list args;
    va_start(args, fmt2);
    vsnprintf(perf_counters.str1, sizeof(perf_counters.str1), fmt1, args);
    vsnprintf(perf_counters.str2, sizeof(perf_counters.str2), fmt2, args);
    va_end(args);

    perf_counters.start = esp_timer_get_time();
}

static void perfmon_end(int count)
{
    perf_counters.acc = esp_timer_get_time() - perf_counters.start;
    printf("Perf ctr, [%-15s][%-15s]: %.2f ms\n",
           perf_counters.str1, perf_counters.str2, ((float)perf_counters.acc / count) / 1000);
}

static void test_display_lcd_init()
{
    esp_lcd_panel_io_handle_t io_handle = NULL;
    const bsp_display_config_t bsp_disp_cfg = {
        .max_transfer_sz = BSP_LCD_H_RES * 80 * sizeof(uint16_t),
    };
    ESP_ERROR_CHECK(bsp_display_new(&bsp_disp_cfg, &panel_handle, &io_handle));

    esp_lcd_panel_disp_on_off(panel_handle, true);
    bsp_display_backlight_on();
}

esp_err_t test_mmap_drive_new(void)
{
    for (int i = 0; i < PARTITION_NUM; i++) {
        const mmap_assets_config_t asset_cfg = {
            .partition_label = asset_configs[i].partition_label,
            .max_files = asset_configs[i].max_files,
            .checksum = asset_configs[i].checksum,
            .flags = {.mmap_enable = true}
        };
        esp_err_t ret = mmap_assets_new(&asset_cfg, &mmap_drive_handle[i]);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to initialize %s", asset_configs[i].partition_label);
            return ret;
        }
    }

    return ESP_OK;
}

esp_err_t test_mmap_drive_del(void)
{
    for (int i = 0; i < PARTITION_NUM; i++) {
        esp_err_t ret = mmap_assets_del(mmap_drive_handle[i]);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to delete %s", asset_configs[i].partition_label);
        }
    }
    return ESP_OK;
}

esp_err_t test_lv_fs_add(void)
{
    fs_cfg_t fs_cfg;

    for (int i = 0; i < PARTITION_NUM; i++) {
        fs_cfg.fs_letter = 'A' + i;
        fs_cfg.fs_assets = mmap_drive_handle[i];
        fs_cfg.fs_nums = asset_configs[i].max_files;

        esp_err_t ret = esp_lv_fs_desc_init(&fs_cfg, &fs_drive_handle[i]);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to initialize FS for %s", asset_configs[i].partition_label);
            return ret;
        }
    }
    return ESP_OK;
}

esp_err_t test_lv_fs_del(void)
{
    for (int i = 0; i < PARTITION_NUM; i++) {
        esp_err_t ret = esp_lv_fs_desc_deinit(fs_drive_handle[i]);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to deinit FS for %s", asset_configs[i].partition_label);
        }
    }
    return ESP_OK;
}

static void test_flush_callback(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *color_map)
{
    if (color_map[0].full != 0xF7BE && color_map[0].full != 0xFFFF) {
        xSemaphoreGive(lv_flush_sync_sem);
    }
    esp_lcd_panel_draw_bitmap(panel_handle, area->x1, area->y1, area->x2 + 1, area->y2 + 1, color_map);
    lv_disp_flush_ready(drv);
}

void test_performance_run(lv_obj_t *img, const char *str1, const char *str2, const char *img_src)
{
    lv_img_set_src(img, NULL);
    lv_refr_now(NULL);

    perfmon_start(str1, str2);
    for (int i = 0; i < TEST_COUNTERS; i++) {
        lv_img_set_src(img, (lv_img_dsc_t *)img_src);
        lv_refr_now(NULL);
        if (xSemaphoreTake(lv_flush_sync_sem, pdMS_TO_TICKS(3000)) != pdTRUE) {
            ESP_LOGE(TAG, "decoder failed");
        }
    }
    perfmon_end(TEST_COUNTERS);
}

void test_lv_disp_add()
{
    lv_init();

    lv_disp_buf = heap_caps_malloc(sizeof(lv_disp_draw_buf_t), MALLOC_CAP_DEFAULT);
    assert(lv_disp_buf);

    uint32_t buffer_size = TEST_LCD_H_RES * TEST_LCD_V_RES;
    lv_color_t *buf1 = heap_caps_malloc(buffer_size * sizeof(lv_color_t), MALLOC_CAP_DEFAULT);
    assert(buf1);
    lv_disp_draw_buf_init(lv_disp_buf, buf1, NULL, buffer_size);

    lv_disp_drv = heap_caps_malloc(sizeof(lv_disp_drv_t), MALLOC_CAP_DEFAULT);
    assert(lv_disp_drv);
    lv_disp_drv_init(lv_disp_drv);
    lv_disp_drv->hor_res = TEST_LCD_H_RES;
    lv_disp_drv->ver_res = TEST_LCD_V_RES;
    lv_disp_drv->flush_cb = test_flush_callback;
    lv_disp_drv->draw_buf = lv_disp_buf;
    lv_disp_drv_register(lv_disp_drv);

    lv_flush_sync_sem = xSemaphoreCreateBinary();
    assert(lv_flush_sync_sem);
}

void test_lv_disp_del()
{
    free(lv_disp_drv->draw_buf->buf1);
    free(lv_disp_drv->draw_buf);
    free(lv_disp_drv);
#if LV_ENABLE_GC || !LV_MEM_CUSTOM
    /* Deinitialize LVGL */
    lv_deinit();
#endif
    vSemaphoreDelete(lv_flush_sync_sem);
}

void test_perf_decoder_fs_esp(void)
{
    esp_lv_decoder_handle_t decoder_handle = NULL;

    test_lv_disp_add();

    esp_err_t ret_fs = test_lv_fs_add();
    if (ret_fs != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize flash FS");
        return;
    }

    ret_fs = esp_lv_decoder_init(&decoder_handle);
    if (ret_fs != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize SPNG decoder");
        return;
    }

    lv_obj_t *obj_bg = lv_obj_create(lv_scr_act());
    lv_obj_set_size(obj_bg, LV_HOR_RES, LV_VER_RES);
    lv_obj_set_align(obj_bg, LV_ALIGN_CENTER);
    lv_obj_clear_flag(obj_bg, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(obj_bg, lv_color_hex(0x000000), 0);
    lv_obj_set_style_border_width(obj_bg, 0, 0);

    lv_obj_t *img = lv_img_create(obj_bg);
    lv_obj_set_align(img, LV_ALIGN_CENTER);

    char path[30];
    static lv_img_dsc_t img_dsc;

    for (int list = 0; list < PARTITION_NUM; list++) {
        for (int i = 0; i < mmap_assets_get_stored_files(mmap_drive_handle[list]); i++) {

            snprintf(path, sizeof(path), "%c:%s", 'A' + list, mmap_assets_get_name(mmap_drive_handle[list], i));

            img_dsc.data_size = mmap_assets_get_size(mmap_drive_handle[list], i);
            img_dsc.data = mmap_assets_get_mem(mmap_drive_handle[list], i);
            if (strstr(path, ".bin")) {
                memcpy(&img_dsc.header, mmap_assets_get_mem(mmap_drive_handle[list], i), sizeof(lv_img_header_t));
                img_dsc.data += sizeof(lv_img_header_t);
                img_dsc.data_size -= sizeof(lv_img_header_t);
            }

            test_performance_run(img, path, "Mem", (const void *)&img_dsc);
            vTaskDelay(pdMS_TO_TICKS(SCHEDULE_WAIT_MS));

            test_performance_run(img, path, "File", path);
            vTaskDelay(pdMS_TO_TICKS(SCHEDULE_WAIT_MS));
        }
    }

    esp_lv_decoder_deinit(decoder_handle);

    test_lv_disp_del();
    test_lv_fs_del();
}

void app_main(void)
{
    ESP_LOGI(TAG, "Perf ctr target: %s", CONFIG_IDF_TARGET);

    test_display_lcd_init();

    esp_err_t ret_drive = test_mmap_drive_new();
    if (ret_drive != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize mmap drives");
        return;
    }

    test_perf_decoder_fs_esp();

    ret_drive = test_mmap_drive_del();
    if (ret_drive != ESP_OK) {
        ESP_LOGE(TAG, "Failed to delete mmap drives");
    }
}
