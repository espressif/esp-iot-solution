/*
 * SPDX-FileCopyrightText: 2024-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef APP_VIDEO_H
#define APP_VIDEO_H

#include <stddef.h>
#include <stdint.h>
#include "sdkconfig.h"
#include "esp_err.h"
#include "driver/i2c_master.h"
#include "linux/videodev2.h"
#include "esp_video_device.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    APP_VIDEO_FMT_RAW8 = V4L2_PIX_FMT_SBGGR8,
    APP_VIDEO_FMT_RAW10 = V4L2_PIX_FMT_SBGGR10,
    APP_VIDEO_FMT_GREY = V4L2_PIX_FMT_GREY,
    APP_VIDEO_FMT_RGB565 = V4L2_PIX_FMT_RGB565,
    APP_VIDEO_FMT_RGB888 = V4L2_PIX_FMT_RGB24,
    APP_VIDEO_FMT_YUV422 = V4L2_PIX_FMT_UYVY,
    APP_VIDEO_FMT_YUV420 = V4L2_PIX_FMT_YUV420,
} video_fmt_t;

#define EXAMPLE_CAM_DEV_PATH    ESP_VIDEO_MIPI_CSI_DEVICE_NAME
#define EXAMPLE_CAM_BUF_NUM     CONFIG_EXAMPLE_CAM_BUF_COUNT

#if CONFIG_LCD_PIXEL_FORMAT_RGB565
#define APP_VIDEO_FMT          APP_VIDEO_FMT_RGB565
#elif CONFIG_LCD_PIXEL_FORMAT_RGB888
#define APP_VIDEO_FMT          APP_VIDEO_FMT_RGB888
#endif

/**
 * Called in the capture task. Return the index safe to requeue: the current
 * frame after a copy, or the previously held frame after display retirement.
 */
typedef int (*app_video_frame_operation_cb_t)(uint8_t *buffer, uint8_t index, uint32_t width,
                                              uint32_t height, size_t bytesused, void *user_data);

/* These helpers manage one capture device; serialize control calls in one task. */

/** Initialize CSI/SCCB. NULL creates the configured I2C bus; an existing bus is borrowed. */
esp_err_t app_video_main(i2c_master_bus_handle_t i2c_bus_handle);

/** Open and negotiate the capture format. Return the file descriptor, or -1 on failure. */
int app_video_open(char *dev, video_fmt_t init_fmt);

/**
 * Set up 2 or 3 buffers once. NULL allocates in the configured MMAP/USERPTR
 * mode using VIDIOC_QUERYBUF capacity; otherwise fb borrows aligned USERPTR
 * buffers of fb_capacity bytes each. Restart reuses the existing buffers.
 * An I/O failure releases the requested buffers and closes video_fd.
 * held_index=-1 queues all buffers. Shared LCD mode holds its initial scanout
 * index and queues the other two; callbacks return the retired held index.
 */
esp_err_t app_video_set_bufs(int video_fd, uint32_t fb_num, const void **fb, int held_index, size_t fb_capacity);

/** Copy borrowed buffer addresses into fb after successful buffer setup. */
esp_err_t app_video_get_bufs(int fb_num, void **fb);

/** Return the queried buffer capacity after successful buffer setup. */
uint32_t app_video_get_buf_size(void);

/** Start capture after buffers and a callback are ready; stop the stream if task creation fails. */
esp_err_t app_video_stream_task_start(int video_fd, int core_id, void *user_data);

/** Requeue existing buffers after stopping; return ESP_ERR_INVALID_STATE while a task is active. */
esp_err_t app_video_stream_task_restart(int video_fd);

/** Request an asynchronous stop after the current frame has been returned to the driver. */
esp_err_t app_video_stream_task_stop(int video_fd);

/** Register a non-NULL callback while the capture task is stopped. */
esp_err_t app_video_register_frame_operation_cb(app_video_frame_operation_cb_t operation_cb);

#ifdef __cplusplus
}
#endif
#endif
