/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef APP_VIDEO_H
#define APP_VIDEO_H

#include "esp_err.h"
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
    APP_VIDEO_FMT_YUV422 = V4L2_PIX_FMT_YUV422P,
    APP_VIDEO_FMT_YUV420 = V4L2_PIX_FMT_YUV420,
} video_fmt_t;

#define EXAMPLE_CAM_DEV_PATH                (ESP_VIDEO_MIPI_CSI_DEVICE_NAME)
#define EXAMPLE_CAM_BUF_NUM                 (CONFIG_EXAMPLE_CAM_BUF_COUNT)

#if CONFIG_LCD_PIXEL_FORMAT_RGB565
#define APP_VIDEO_FMT              (APP_VIDEO_FMT_RGB565)
#elif CONFIG_LCD_PIXEL_FORMAT_RGB888
#define APP_VIDEO_FMT              (APP_VIDEO_FMT_RGB888)
#endif

typedef void (*app_video_frame_operation_cb_t)(uint8_t *camera_buf, uint8_t camera_buf_index, uint32_t camera_buf_hes, uint32_t camera_buf_ves, size_t camera_buf_len, void *user_data);

/**
 * @brief Initialize the video camera.
 *
 * Configures SCCB settings for I2C communication. Overrides defaults if an
 * I2C bus handle is provided, then initializes the camera.
 *
 * @param i2c_bus_handle Handle for the I2C bus (can be NULL).
 * @return ESP_OK on success, or an error code on failure.
 */
esp_err_t app_video_main(i2c_master_bus_handle_t i2c_bus_handle);

/**
 * @brief Opens a specified video capture device and configures its format.
 *
 * This function initializes the video capture by opening the device at the given path,
 * querying its capabilities, and retrieving the current format. If the current pixel format
 * differs from the specified initial format, it sets the desired format. Additionally, it applies
 * vertical or horizontal mirroring if enabled in the configuration.
 *
 * @param dev The device path of the video capture (e.g., "/dev/video0").
 * @param init_fmt The desired pixel format for the video capture.
 * @return Returns the file descriptor for the opened video device on success;
 *         returns -1 on failure, indicating an error occurred during the operation.
 */
int app_video_open(char *dev, video_fmt_t init_fmt);

/**
 * @brief Set up video capture buffers.
 *
 * Configures the video device to use the specified number of buffers for
 * capturing video frames. Ensures the buffer count is within acceptable limits
 * and allocates buffers either via memory-mapped I/O or user pointers.
 * Closes the device on failure.
 *
 * @param video_fd File descriptor for the video device.
 * @param fb_num Number of frame buffers to allocate.
 * @param fb Array of pointers to user-provided frame buffers (if applicable).
 * @return ESP_OK on success, or ESP_FAIL on failure.
 */
esp_err_t app_video_set_bufs(int video_fd, uint32_t fb_num, const void **fb);

/**
 * @brief Retrieve video capture buffers.
 *
 * Fills the provided array with pointers to the allocated frame buffers for
 * capturing video frames. Checks that the specified buffer count is within
 * acceptable limits.
 *
 * @param fb_num Number of frame buffers to retrieve.
 * @param fb Array of pointers to receive the frame buffers.
 * @return ESP_OK on success, or ESP_FAIL on failure.
 */
esp_err_t app_video_get_bufs(int fb_num, void **fb);

/**
 * @brief Get the size of the video buffer.
 *
 * Calculates and returns the size of the video buffer based on the
 * camera's width, height, and pixel format (RGB565 or RGB888).
 *
 * @return Size of the video buffer in bytes.
 */
uint32_t app_video_get_buf_size(void);

/**
 * @brief Start the video stream task.
 *
 * Initiates the video streaming by starting the video stream and creating
 * a FreeRTOS task to handle the streaming process on a specified core.
 * Stops the video stream if task creation fails.
 *
 * @param video_fd File descriptor for the video device.
 * @param core_id Core ID to which the task will be pinned.
 * @return ESP_OK on success, or ESP_FAIL on failure.
 */
esp_err_t app_video_stream_task_start(int video_fd, int core_id, void *user_data);

/**
 * @brief Restart the video stream task.
 *
 * This function stops the current video stream task and restarts it with the specified
 * video device file descriptor. It first sets the necessary buffers for the video stream,
 * then attempts to start the video stream task on the specified core.
 *
 * @param video_fd File descriptor for the video device.
 * @return
 * - ESP_OK on successful restart of the video stream task.
 * - ESP_FAIL if there was an error while restarting the task.
 */
esp_err_t app_video_stream_task_restart(int video_fd);

/**
 * @brief Stop the video stream task.
 *
 * Deletes the video stream task if it is running and stops the video stream.
 * Ensures the task handle is reset to NULL after deletion.
 *
 * @param video_fd File descriptor for the video device.
 * @return ESP_OK on success.
 */
esp_err_t app_video_stream_task_stop(int video_fd);

/**
 * @brief Register a callback for frame operations.
 *
 * Sets the user-defined callback function to be invoked for frame operations
 * during video processing.
 *
 * @param operation_cb Callback function to register.
 * @return ESP_OK on success.
 */
esp_err_t app_video_register_frame_operation_cb(app_video_frame_operation_cb_t operation_cb);

#ifdef __cplusplus
}
#endif
#endif
