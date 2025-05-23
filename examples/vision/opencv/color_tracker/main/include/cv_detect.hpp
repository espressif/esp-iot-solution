/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <queue>
#include "lcd.hpp"
#include "camera.hpp"

/**
 * @brief Computer Vision detection and display handler
 *
 * This class handles object detection using OpenCV algorithms and displays results
 * on LCD panel. It manages two FreeRTOS tasks:
 * - Detection task: Performs image processing and object detection
 * - Refresh task: Handles LCD display updates with detection results
 */
class CvDetect {
public:
    /**
     * @brief Structure to store detection results
     *
     */
    typedef struct {
        int x;       /*!< X coordinate of detected object */
        int y;       /*!< Y coordinate of detected object */
        int width;   /*!< Width of detected object bounding box */
        int height;  /*!< Height of detected object bounding box */
    } result_t;

    /**
     * @brief Construct a new Cv Detect object
     *
     * @param lcd_panel Handle to LCD panel for display operations
     * @param cam Pointer to initialized Camera object for frame capture
     */
    CvDetect(esp_lcd_panel_handle_t lcd_panel, Camera *cam);

    /**
     * @brief Destroy the Cv Detect object
     *
     */
    ~CvDetect();

    /**
     * @brief Start detection and display tasks
     *
     */
    void run();

    /**
     * @brief Perform object detection on a frame buffer
     *
     * @param fb Pointer to frame buffer containing image data
     */
    void detect(cam_fb_t *fb);
private:
    /**
     * @brief LCD refresh task
     *
     * @param args The user data given from task creating
     */
    static void lcd_refresh_task(void *args);

    /**
     * @brief CV detection task
     *
     * @param args The user data given from task creating
     */
    static void cv_detect_task(void *args);

    /**
     * @brief Draw rectangle on image buffer
     *
     * @param buffer Pointer to image buffer
     * @param width Image width in pixels
     * @param height Image height in pixels
     * @param x1 Left coordinate of rectangle
     * @param y1 Top coordinate of rectangle
     * @param x2 Right coordinate of rectangle
     * @param y2 Bottom coordinate of rectangle
     * @param r Red component (0-255)
     * @param g Green component (0-255)
     * @param b Blue component (0-255)
     * @param thickness Border thickness in pixels
     */
    void draw_rectangle_rgb(uint8_t *buffer, int width, int height,
                            int x1, int y1, int x2, int y2,
                            uint8_t r, uint8_t g, uint8_t b,
                            int thickness);

    esp_lcd_panel_handle_t m_lcd_panel; /*!< LCD panel handle for display operations */
    Camera *m_cam;                      /*!< Camera pointer for frame capture */
    std::vector<result_t> m_results;    /*!< Storage for detection results */
    SemaphoreHandle_t m_mutex;          /*!< Mutex for thread-safe result access */
    bool m_is_running;                  /*!< Flag to indicate if detection is running */
};
