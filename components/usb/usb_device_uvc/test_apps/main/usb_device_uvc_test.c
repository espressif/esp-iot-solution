/*
 * SPDX-FileCopyrightText: 2024-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "unity.h"
#include "unity_test_runner.h"
#include "unity_test_utils_memory.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "usb_device_uvc.h"
#include "esp_timer.h"

static const char *TAG = "usb_device_uvc_test";

// Some resources are lazy allocated in GPTimer driver, the threshold is left for that case
#define TEST_MEMORY_LEAK_THRESHOLD (-300)
#define UVC_MAX_FRAMESIZE_SIZE     (60*1024)
#define WIDTH   CONFIG_UVC_CAM1_FRAMESIZE_WIDTH
#define HEIGHT  CONFIG_UVC_CAM1_FRAMESIZE_HEIGT
#define TEST_COUNT (15)

extern const unsigned char jpg_start[] asm("_binary_esp_1280_720_jpg_start");
extern const unsigned char jpg_end[]   asm("_binary_esp_1280_720_jpg_end");

static uvc_fb_t s_fb;

static void camera_stop_cb(void *cb_ctx)
{
    ESP_LOGI(TAG, "camera stop");
}

static esp_err_t camera_start_cb(uvc_format_t format, int width, int height, int rate, void *cb_ctx)
{
    (void)cb_ctx;
    ESP_LOGI(TAG, "Camera: Start");
    ESP_LOGI(TAG, "Format: %d, width: %d, height: %d, rate: %d", format, width, height, rate);

    return ESP_OK;
}

static void camera_fb_return_cb(uvc_fb_t *fb, void *cb_ctx)
{
    (void)cb_ctx;
    assert(fb == &s_fb);
}

static uvc_fb_t* camera_fb_get_cb(void *cb_ctx)
{
    (void)cb_ctx;
    uint64_t us = (uint64_t)esp_timer_get_time();
    s_fb.buf = (uint8_t *)jpg_start;
    s_fb.len = jpg_end - jpg_start;
    s_fb.width = WIDTH;
    s_fb.height = HEIGHT;
    s_fb.format = UVC_FORMAT_JPEG;
    s_fb.timestamp.tv_sec = us / 1000000UL;
    s_fb.timestamp.tv_usec = us % 1000000UL;

    if (s_fb.len > UVC_MAX_FRAMESIZE_SIZE) {
        ESP_LOGE(TAG, "Frame size %d is larger than max frame size %d", s_fb.len, UVC_MAX_FRAMESIZE_SIZE);
        return NULL;
    }

    return &s_fb;
}

TEST_CASE("usb_device_uvc_test", "[usb_device_uvc]")
{

    uint8_t *uvc_buffer = (uint8_t *)malloc(UVC_MAX_FRAMESIZE_SIZE);
    TEST_ASSERT_NOT_NULL(uvc_buffer);

    uvc_device_config_t config = {
        .uvc_buffer = uvc_buffer,
        .uvc_buffer_size = UVC_MAX_FRAMESIZE_SIZE,
        .start_cb = camera_start_cb,
        .fb_get_cb = camera_fb_get_cb,
        .fb_return_cb = camera_fb_return_cb,
        .stop_cb = camera_stop_cb,
        .cb_ctx = NULL,
    };
    uvc_device_config(0, &config);
    uvc_device_init();
    for (int i = 0; i < TEST_COUNT; i++) {
        vTaskDelay(1000 / portTICK_PERIOD_MS);
        ESP_LOGI(TAG, "UVC Device Test: %d", i);
    }
    uvc_device_deinit();
    free(uvc_buffer);
}

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

    //  _   _ _   _ _____     _____ _____ _____ _____
    // | | | | | | /  __ \   |_   _|  ___/  ___|_   _|
    // | | | | | | | /  \/_____| | | |__ \ `--.  | |
    // | | | | | | | |  |______| | |  __| `--. \ | |
    // | |_| \ \_/ / \__/\     | | | |___/\__/ / | |
    //  \___/ \___/ \____/     \_/ \____/\____/  \_/
    printf(" _   _ _   _ _____     _____ _____ _____ _____ \n");
    printf("| | | | | | /  __ \\   |_   _|  ___/  ___|_   _|\n");
    printf("| | | | | | | /  \\/_____| | | |__ \\ `--.  | |  \n");
    printf("| | | | | | | |  |______| | |  __| `--. \\ | |  \n");
    printf("| |_| \\ \\_/ / \\__/\\     | | | |___/\\__/ / | |  \n");
    printf(" \\___/ \\___/ \\____/     \\_/ \\____/\\____/  \\_/  \n");
    unity_run_menu();
}
