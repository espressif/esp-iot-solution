/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */
#include "esp_err.h"
#include "esp_log.h"
#include "esp_video_init.h"
#include "esp_lcd_mipi_dsi.h"
#include "esp_lcd_panel_ops.h"
#include "esp_cache.h"
#include "esp_heap_caps.h"
#include "esp_private/esp_cache_private.h"
#include "esp_timer.h"
#include "driver/ppa.h"
#include "app_video.h"
#include "app_lcd.h"

#define ALIGN_UP(num, align)    (((num) + ((align) - 1)) & ~((align) - 1))

static void camera_video_frame_operation(uint8_t *camera_buf, uint8_t camera_buf_index, uint32_t camera_buf_hes, uint32_t camera_buf_ves, size_t camera_buf_len, void *user_data);

static const char *TAG = "app_main";

static esp_lcd_panel_handle_t display_panel;
static ppa_client_handle_t ppa_srm_handle = NULL;
static size_t data_cache_line_size = 0;
static void *lcd_buffer[EXAMPLE_LCD_BUF_NUM];

#if CONFIG_EXAMPLE_ENABLE_PRINT_FPS_RATE_VALUE
static int fps_count;
static int64_t start_time;
#endif

void app_main(void)
{
    // Initialize the LCD
    ESP_ERROR_CHECK(app_lcd_init(&display_panel));

    // Initialize the PPA
    ppa_client_config_t ppa_srm_config = {
        .oper_type = PPA_OPERATION_SRM,
    };
    ESP_ERROR_CHECK(ppa_register_client(&ppa_srm_config, &ppa_srm_handle));
    ESP_ERROR_CHECK(esp_cache_get_alignment(MALLOC_CAP_SPIRAM, &data_cache_line_size));

    // Initialize the video camera
    esp_err_t ret = app_video_main(NULL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "video main init failed with error 0x%x", ret);
        return;
    }

    // Open the video device
    int video_cam_fd0 = app_video_open(EXAMPLE_CAM_DEV_PATH, APP_VIDEO_FMT);
    if (video_cam_fd0 < 0) {
        ESP_LOGE(TAG, "video cam open failed");
        return;
    }

    // Get the LCD frame buffer
#if EXAMPLE_LCD_BUF_NUM == 2
    ESP_ERROR_CHECK(esp_lcd_dpi_panel_get_frame_buffer(display_panel, 2, &lcd_buffer[0], &lcd_buffer[1]));
#else
    ESP_ERROR_CHECK(esp_lcd_dpi_panel_get_frame_buffer(display_panel, 3, &lcd_buffer[0], &lcd_buffer[1], &lcd_buffer[2]));
#endif

    // Set the video buffer
#if CONFIG_EXAMPLE_USE_MEMORY_MAPPING
    ESP_LOGI(TAG, "Using map buffer");
    ESP_ERROR_CHECK(app_video_set_bufs(video_cam_fd0, EXAMPLE_CAM_BUF_NUM, NULL)); // When setting the camera video buffer, it can be written as NULL to automatically allocate the buffer using mapping
#else
    ESP_LOGI(TAG, "Using user defined buffer");
#if CONFIG_CAMERA_SC2336_MIPI_RAW8_1024x600_30FPS
    ESP_ERROR_CHECK(app_video_set_bufs(video_cam_fd0, EXAMPLE_CAM_BUF_NUM, (void *)lcd_buffer));
#else
    void *camera_buf[EXAMPLE_CAM_BUF_NUM];
    for (int i = 0; i < EXAMPLE_CAM_BUF_NUM; i++) {
        camera_buf[i] = heap_caps_aligned_calloc(data_cache_line_size, 1, app_video_get_buf_size(), MALLOC_CAP_SPIRAM);
    }
    ESP_ERROR_CHECK(app_video_set_bufs(video_cam_fd0, EXAMPLE_CAM_BUF_NUM, (void *)camera_buf));
#endif
#endif

    // Register the video frame operation callback
    ESP_ERROR_CHECK(app_video_register_frame_operation_cb(camera_video_frame_operation));

    // Start the camera stream task
    ESP_ERROR_CHECK(app_video_stream_task_start(video_cam_fd0, 0, NULL));

#if CONFIG_EXAMPLE_ENABLE_PRINT_FPS_RATE_VALUE
    start_time = esp_timer_get_time();  // Get the initial time for frame rate statistics
#endif
}

static void camera_video_frame_operation(uint8_t *camera_buf, uint8_t camera_buf_index, uint32_t camera_buf_hes, uint32_t camera_buf_ves, size_t camera_buf_len, void *user_data)
{
#if CONFIG_EXAMPLE_ENABLE_PRINT_FPS_RATE_VALUE
    fps_count++;
    if (fps_count == 50) {
        int64_t end_time = esp_timer_get_time();
        ESP_LOGI(TAG, "fps: %f", 1000000.0 / ((end_time - start_time) / 50.0));
        start_time = end_time;
        fps_count = 0;

        ESP_LOGI(TAG, "camera_buf_hes: %lu, camera_buf_ves: %lu, camera_buf_len: %d KB", camera_buf_hes, camera_buf_ves, camera_buf_len / 1024);
    }
#endif

    ppa_srm_oper_config_t srm_config = {
        .in.buffer = camera_buf,
        .in.pic_w = camera_buf_hes,
        .in.pic_h = camera_buf_ves,
        .in.block_w = camera_buf_hes,
        .in.block_h = camera_buf_ves,
        .in.block_offset_x = (camera_buf_hes > EXAMPLE_LCD_H_RES) ? (camera_buf_hes - EXAMPLE_LCD_H_RES) / 2 : 0,
           .in.block_offset_y = (camera_buf_ves > EXAMPLE_LCD_V_RES) ? (camera_buf_ves - EXAMPLE_LCD_V_RES) / 2 : 0,
           .in.srm_cm = APP_VIDEO_FMT == APP_VIDEO_FMT_RGB565 ? PPA_SRM_COLOR_MODE_RGB565 : PPA_SRM_COLOR_MODE_RGB888,
           .out.buffer = lcd_buffer[camera_buf_index],
           .out.buffer_size = ALIGN_UP(EXAMPLE_LCD_H_RES * EXAMPLE_LCD_V_RES * (APP_VIDEO_FMT == APP_VIDEO_FMT_RGB565 ? 2 : 3), data_cache_line_size),
           .out.pic_w = EXAMPLE_LCD_H_RES,
           .out.pic_h = EXAMPLE_LCD_V_RES,
           .out.block_offset_x = 0,
           .out.block_offset_y = 0,
           .out.srm_cm = APP_VIDEO_FMT == APP_VIDEO_FMT_RGB565 ? PPA_SRM_COLOR_MODE_RGB565 : PPA_SRM_COLOR_MODE_RGB888,
           .rotation_angle = PPA_SRM_ROTATION_ANGLE_0,
           .scale_x = 1,
           .scale_y = 1,
           .rgb_swap = 0,
           .byte_swap = 0,
           .mode = PPA_TRANS_MODE_BLOCKING,
    };

    if (camera_buf_hes > EXAMPLE_LCD_H_RES || camera_buf_ves > EXAMPLE_LCD_V_RES) {
        // The resolution of the camera does not match the LCD resolution. Image processing can be done using PPA, but there will be some frame rate loss

        srm_config.in.block_w = (camera_buf_hes > EXAMPLE_LCD_H_RES) ? EXAMPLE_LCD_H_RES : camera_buf_hes;
        srm_config.in.block_h = (camera_buf_ves > EXAMPLE_LCD_V_RES) ? EXAMPLE_LCD_V_RES : camera_buf_ves;

        ESP_ERROR_CHECK(ppa_do_scale_rotate_mirror(ppa_srm_handle, &srm_config));

        ESP_ERROR_CHECK(esp_lcd_panel_draw_bitmap(display_panel, 0, 0, srm_config.in.block_w, srm_config.in.block_h, lcd_buffer[camera_buf_index]));
    } else {
        ESP_ERROR_CHECK(esp_lcd_panel_draw_bitmap(display_panel, 0, 0, camera_buf_hes, camera_buf_ves, camera_buf));
    }
}
