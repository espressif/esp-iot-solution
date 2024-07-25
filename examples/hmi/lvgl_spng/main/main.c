/*
 * SPDX-FileCopyrightText: 2021-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_err.h"
#include "esp_log.h"

#include "bsp/esp-bsp.h"
#include "esp_lv_spng.h"
#include "assets_generate.h"

static const char *TAG = "example";

#define MEMORY_MONITOR 1

static mmap_assets_handle_t asset_handle;
static esp_lv_spng_decoder_handle_t spng_decoder;

#if MEMORY_MONITOR
static void monitor_task(void *arg)
{
    (void) arg;
    const int STATS_TICKS = pdMS_TO_TICKS(5 * 1000);

    while (true) {
        ESP_LOGI(TAG, "System Info Trace");
        printf("\tDescription\tInternal\tSPIRAM\n");
        printf("Current Free Memory\t%d\t\t%d\n",
               heap_caps_get_free_size(MALLOC_CAP_8BIT | MALLOC_CAP_INTERNAL),
               heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
        printf("Largest Free Block\t%d\t\t%d\n",
               heap_caps_get_largest_free_block(MALLOC_CAP_8BIT | MALLOC_CAP_INTERNAL),
               heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM));
        printf("Min. Ever Free Size\t%d\t\t%d\n",
               heap_caps_get_minimum_free_size(MALLOC_CAP_8BIT | MALLOC_CAP_INTERNAL),
               heap_caps_get_minimum_free_size(MALLOC_CAP_SPIRAM));

        vTaskDelay(STATS_TICKS);
    }

    vTaskDelete(NULL);
}

static void sys_monitor_start(void)
{
    BaseType_t ret_val = xTaskCreatePinnedToCore(monitor_task, "Monitor Task", 4 * 1024, NULL, configMAX_PRIORITIES - 3, NULL, 0);
    ESP_ERROR_CHECK_WITHOUT_ABORT((pdPASS == ret_val) ? ESP_OK : ESP_FAIL);
}
#endif

void image_mmap_init()
{
    const mmap_assets_config_t config = {
        .partition_label = "assets",
        .max_files = TOTAL_MMAP_FILES,
        .checksum = MMAP_CHECKSUM,
    };

    ESP_ERROR_CHECK(mmap_assets_new(&config, &asset_handle));

    for (int i = 0; i < TOTAL_MMAP_FILES; i++) {
        const char* name = mmap_assets_get_name(asset_handle, i);
        const void* mem = mmap_assets_get_mem(asset_handle, i);
        int size = mmap_assets_get_size(asset_handle, i);
        int width = mmap_assets_get_width(asset_handle, i);
        int height = mmap_assets_get_height(asset_handle, i);

        ESP_LOGI(TAG, "Name:[%s], Addr:[%p], Size:[%d], Width:[%d], Height:[%d]", name, mem, size, width, height);
    }
}

void app_main(void)
{
    static lv_img_dsc_t img_dsc;
    static uint8_t list = 0;

    image_mmap_init();

    ESP_LOGI(TAG, "screen size:[%d,%d]", LV_HOR_RES, LV_VER_RES);

    bsp_display_start();
    esp_lv_split_png_init(&spng_decoder);

    /* Set default display brightness */
    bsp_display_brightness_set(100);

    bsp_display_lock(0);

    lv_obj_t *obj_bg = lv_obj_create(lv_scr_act());
    lv_obj_set_size(obj_bg, LV_HOR_RES, LV_VER_RES);
    lv_obj_set_align(obj_bg, LV_ALIGN_CENTER);
    lv_obj_clear_flag(obj_bg, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(obj_bg, lv_color_hex(0xFF0000), 0);
    lv_obj_set_style_border_width(obj_bg, 0, 0);

    lv_obj_t *obj_img = lv_img_create(obj_bg);
    lv_obj_set_align(obj_img, LV_ALIGN_CENTER);

    bsp_display_unlock();

#if MEMORY_MONITOR
    sys_monitor_start();
#endif

    while (1) {
        bsp_display_lock(0);
        img_dsc.data_size = mmap_assets_get_size(asset_handle, (list) % TOTAL_MMAP_FILES);
        img_dsc.data = mmap_assets_get_mem(asset_handle, (list) % TOTAL_MMAP_FILES);
        lv_img_set_src(obj_img, &img_dsc);
        bsp_display_unlock();
        list++;
        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    mmap_assets_del(asset_handle);
    esp_lv_split_png_deinit(spng_decoder);
}
