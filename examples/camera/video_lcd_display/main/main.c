/*
 * SPDX-FileCopyrightText: 2024-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */
#include <stdbool.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_lcd_mipi_dsi.h"
#include "app_video.h"
#include "app_lcd.h"

static const char *TAG = "app_main";
static esp_lcd_panel_handle_t display_panel;
static bool shared_buffers;

#if CONFIG_EXAMPLE_ENABLE_PRINT_FPS_RATE_VALUE
static int fps_count;
static int64_t start_time;
#endif

static esp_err_t configure_camera_buffers(int fd)
{
    if (shared_buffers) {
        void *buffers[EXAMPLE_LCD_SHARED_BUF_NUM];
        ESP_ERROR_CHECK(esp_lcd_dpi_panel_get_frame_buffer(display_panel, EXAMPLE_LCD_SHARED_BUF_NUM,
                                                           &buffers[0], &buffers[1], &buffers[2]));
        ESP_LOGI(TAG, "Capture buffers: USERPTR, two queued plus one LCD scanout");
        return app_video_set_bufs(fd, EXAMPLE_LCD_SHARED_BUF_NUM, (const void **)buffers, 0, EXAMPLE_LCD_BUF_SIZE);
    }
    return app_video_set_bufs(fd, EXAMPLE_CAM_BUF_NUM, NULL, -1, 0);
}

static int camera_video_frame_operation(uint8_t *buffer, uint8_t index, uint32_t width,
                                        uint32_t height, size_t bytesused, void *user_data)
{
#if CONFIG_EXAMPLE_ENABLE_PRINT_FPS_RATE_VALUE
    if (++fps_count == 50) {
        int64_t now = esp_timer_get_time();
        ESP_LOGI(TAG, "fps: %f", 50.0 * 1000000 / (now - start_time));
        start_time = now;
        fps_count = 0;
    }
#endif

    int retired_index;
    ESP_ERROR_CHECK(app_lcd_draw_bitmap(display_panel, width, height, buffer, &retired_index));
    return shared_buffers ? retired_index : index;
}

void app_main(void)
{
    esp_err_t ret = app_video_main(NULL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "camera initialization failed: 0x%x", ret);
        return;
    }
    int fd = app_video_open(EXAMPLE_CAM_DEV_PATH, APP_VIDEO_FMT);
    if (fd < 0) {
        return;
    }
    struct v4l2_format format = {.type = V4L2_BUF_TYPE_VIDEO_CAPTURE};
    if (ioctl(fd, VIDIOC_G_FMT, &format) != 0) {
        ESP_LOGE(TAG, "failed to get capture geometry");
        close(fd);
        return;
    }
#if !CONFIG_EXAMPLE_USE_MEMORY_MAPPING
    shared_buffers = format.fmt.pix.width == EXAMPLE_LCD_H_RES && format.fmt.pix.height == EXAMPLE_LCD_V_RES &&
                     format.fmt.pix.sizeimage <= EXAMPLE_LCD_BUF_SIZE;
#endif
    int draw_core;
    ESP_ERROR_CHECK(app_lcd_init(&display_panel, shared_buffers, &draw_core));
    ESP_ERROR_CHECK(configure_camera_buffers(fd));
    ESP_ERROR_CHECK(app_video_register_frame_operation_cb(camera_video_frame_operation));
#if CONFIG_EXAMPLE_ENABLE_PRINT_FPS_RATE_VALUE
    start_time = esp_timer_get_time();
#endif
    ESP_ERROR_CHECK(app_video_stream_task_start(fd, draw_core, NULL));
}
