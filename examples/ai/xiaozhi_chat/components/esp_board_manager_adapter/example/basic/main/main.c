/*
 * SPDX-FileCopyrightText: 2025-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <string.h>
#include "esp_log.h"
#include "esp_err.h"
#include "esp_codec_dev.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_board_manager_adapter.h"
#include "lvgl.h"
#include "esp_lvgl_port.h"

static const char *TAG = "BOARD_MANAGER_ADAPTER_EXAMPLE";

static lv_obj_t *counter_label = NULL;
static int counter_value = 0;

static void button_click_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_CLICKED) {
        counter_value++;
        if (counter_label) {
            lvgl_port_lock(0);
            lv_label_set_text_fmt(counter_label, "%d", counter_value);
            lvgl_port_unlock();
        }
        ESP_LOGI(TAG, "Button clicked, counter: %d", counter_value);
    }
}

void app_main(void)
{
    esp_err_t ret;
    esp_board_manager_adapter_info_t bsp_info;

    ESP_LOGI(TAG, "ESP BSP Manager Example");

    // Get BSP Manager information
    esp_board_manager_adapter_config_t config = ESP_BOARD_MANAGER_ADAPTER_CONFIG_DEFAULT();
    config.enable_lvgl = true;
    config.enable_audio = true;
    config.enable_sdcard = true;
    config.enable_lcd = true;
    config.enable_lcd_backlight = true;
    config.enable_touch = true;
    ret = esp_board_manager_adapter_init(&config, &bsp_info);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get BSP manager info: %s", esp_err_to_name(ret));
        return;
    }

    // Print BSP information
    ESP_LOGI(TAG, "=== BSP Manager Information ===");
    ESP_LOGI(TAG, "Play device: %p", bsp_info.play_dev);
    ESP_LOGI(TAG, "Record device: %p", bsp_info.rec_dev);
    ESP_LOGI(TAG, "LCD panel: %p", bsp_info.lcd_panel);
    ESP_LOGI(TAG, "Brightness handle: %p", bsp_info.brightness_handle);
    ESP_LOGI(TAG, "SD card handle: %p", bsp_info.sdcard_handle);
    ESP_LOGI(TAG, "Board sample rate: %d Hz", bsp_info.sample_rate);
    ESP_LOGI(TAG, "Board sample bits: %d", bsp_info.sample_bits);
    ESP_LOGI(TAG, "Board channels: %d", bsp_info.channels);
    ESP_LOGI(TAG, "Board mic layout: %.*s", 8, bsp_info.mic_layout);

    ESP_LOGI(TAG, "Example completed successfully");

    // LVGL example: Create a label and button, increment counter on button click
    if (bsp_info.lcd_panel != NULL && config.enable_lvgl) {
        ESP_LOGI(TAG, "Creating LVGL UI...");

        // Wait a bit for LVGL task to fully start
        vTaskDelay(pdMS_TO_TICKS(100));

        // Lock LVGL before creating UI objects
        if (lvgl_port_lock(5000)) {
            lv_obj_t *scr = lv_screen_active();

            // Create a label to display the counter
            counter_label = lv_label_create(scr);
            lv_label_set_text_fmt(counter_label, "%d", counter_value);
            lv_obj_align(counter_label, LV_ALIGN_CENTER, 0, -50);

            // Create a button
            lv_obj_t *btn = lv_button_create(scr);
            lv_obj_set_size(btn, 120, 50);
            lv_obj_align(btn, LV_ALIGN_CENTER, 0, 50);
            lv_obj_add_event_cb(btn, button_click_cb, LV_EVENT_CLICKED, NULL);

            // Add label to button
            lv_obj_t *btn_label = lv_label_create(btn);
            lv_label_set_text(btn_label, "Click");
            lv_obj_center(btn_label);

            // Unlock LVGL
            lvgl_port_unlock();

            ESP_LOGI(TAG, "LVGL UI created successfully");
        } else {
            ESP_LOGE(TAG, "Failed to lock LVGL port");
        }
    } else {
        ESP_LOGW(TAG, "LVGL not enabled or LCD not available");
    }

    esp_codec_dev_sample_info_t fs = {
        .sample_rate = 16000,
        .bits_per_sample = 16,
        .channel = 2,
    };
    ret = esp_codec_dev_open(bsp_info.play_dev, &fs);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open playback device: %s", esp_err_to_name(ret));
        return;
    }
    ret = esp_codec_dev_open(bsp_info.rec_dev, &fs);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open recording device: %s", esp_err_to_name(ret));
        return;
    }
    char *data = (char *)malloc(1024);
    if (data == NULL) {
        ESP_LOGE(TAG, "Failed to allocate memory");
        return;
    }

    while (1) {
        ret = esp_codec_dev_read(bsp_info.rec_dev, data, 1024);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to read data: %s", esp_err_to_name(ret));
            return;
        }
        ret = esp_codec_dev_write(bsp_info.play_dev, data, 1024);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to write data: %s", esp_err_to_name(ret));
            return;
        }
    }
}
