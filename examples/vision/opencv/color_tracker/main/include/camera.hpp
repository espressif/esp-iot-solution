/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <queue>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "esp_cache.h"
#include "esp_timer.h"
#include "esp_video_init.h"
#include "camera_define.hpp"
#include "esp_private/esp_cache_private.h"

/**
 * @brief Camera class for video capture and frame buffer management
 *
 * This class handles camera initialization, video stream control, and frame buffer management
 * using V4L2 interface. It supports both MMAP and USERPTR memory modes for frame buffers.
 */
class Camera {
public:
    /**
     * @brief Construct a new Camera object
     *
     * @param video_pix_fmt Desired pixel format for video capture
     * @param fb_count Number of frame buffers to allocate
     * @param fb_mem_type The V4L2 memory type used for frame buffers
     * @param horizontal_flip Enable horizontal flip of the captured image
     */
    Camera(const video_pix_fmt_t video_pix_fmt,
           const uint8_t fb_count,
           const v4l2_memory fb_mem_type,
           bool horizontal_flip);

    /**
     * @brief Destroy the Camera object and release resources
     *
     */
    ~Camera();

    /**
     * @brief Get a frame buffer from the camera queue
     *
     * @return cam_fb_t* Pointer to the frame buffer if successful, otherwise nullptr.
     */
    cam_fb_t *cam_fb_get();

    /**
     * @brief Peeks into the queue of frame buffers without removing the element
     *
     * @param back If true, peek at the last element, else peek at the first
     * @return cam_fb_t* Pointer to the frame buffer, nullptr if queue is empty
     */
    cam_fb_t *cam_fb_peek(bool back);

    /**
     * @brief Returns a frame buffer to the queue for reuse
     *
     */
    void cam_fb_return();
protected:
    /**
     * @brief Initialize the video camera
     *
     */
    void video_init();

    /**
     * @brief Deinitialize the video camera
     *
     */
    void video_deinit();

    /**
     * @brief Open a specified video capture device
     *
     * @return ESP_OK on success, or ESP_FAIL on failure
     */
    esp_err_t open_video_device();

    /**
     * @brief Close video device
     *
     * @return esp_err_t ESP_OK on success, ESP_FAIL on failure
     */
    esp_err_t close_video_device();

    /**
     * @brief Enable horizontal flip of the captured image
     *
     * @return esp_err_t ESP_OK on success, ESP_FAIL on failure
     */
    esp_err_t set_horizontal_flip();

    /**
     * @brief Set the video format
     *
     * @return esp_err_t ESP_OK on success, ESP_FAIL on failure
     */
    esp_err_t set_video_format();

    /**
     * @brief Initialize frame buffers
     *
     * @return esp_err_t ESP_OK on success, ESP_FAIL on failure
     */
    esp_err_t init_fbs();

    /**
     * @brief Start video streaming
     *
     * @return esp_err_t ESP_OK on success, ESP_FAIL on failure
     */
    esp_err_t start_video_stream();

    /**
     * @brief Stop video streaming
     *
     * @return esp_err_t ESP_OK on success, ESP_FAIL on failure
     */
    esp_err_t stop_video_stream();
private:
    SemaphoreHandle_t m_mutex;                        /*!< Mutex for buffer queue protection */
    video_pix_fmt_t m_video_pix_fmt;                  /*!< Selected pixel format */
    int m_fd;                                         /*!< File descriptor for video device */
    uint8_t m_fb_count;                               /*!< Number of allocated frame buffers */
    v4l2_memory m_fb_mem_type;                        /*!< Frame buffer memory type */
    int m_width;                                      /*!< Captured image width */
    int m_height;                                     /*!< Captured image height */
    cam_fb_t *m_cam_fbs;                              /*!< Array of frame buffers */
    std::queue<struct v4l2_buffer> m_v4l2_buf_queue;  /*!< V4L2 buffer queue */
    std::queue<cam_fb_t *> m_buf_queue;               /*!< Frame buffer pointer queue */
    bool m_horizontal_flip;                           /*!< Horizontal flip enable flag */
};
