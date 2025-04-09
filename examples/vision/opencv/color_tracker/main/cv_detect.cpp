/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <opencv2/core/utility.hpp>
#include <opencv2/imgproc.hpp>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "cv_detect.hpp"

static const char *TAG = "cv_detect";

void CvDetect::draw_rectangle_rgb(uint8_t *buffer, int width, int height,
                                  int x1, int y1, int x2, int y2,
                                  uint8_t r, uint8_t g, uint8_t b,
                                  int thickness)
{
    if (x1 < 0) {
        x1 = 0;
    }
    if (y1 < 0) {
        y1 = 0;
    }
    if (x2 >= width) {
        x2 = width - 1;
    }
    if (y2 >= height) {
        y2 = height - 1;
    }

    for (int t = 0; t < thickness; ++t) {
        for (int x = x1; x <= x2; ++x) {
            if (y1 + t >= 0 && y1 + t < height) {
                int index = ((y1 + t) * width + x) * 3;
                buffer[index] = b;
                buffer[index + 1] = g;
                buffer[index + 2] = r;
            }
            if (y2 - t >= 0 && y2 - t < height) {
                int index = ((y2 - t) * width + x) * 3;
                buffer[index] = b;
                buffer[index + 1] = g;
                buffer[index + 2] = r;
            }
        }

        for (int y = y1; y <= y2; ++y) {
            if (x1 + t >= 0 && x1 + t < width) {
                int index = ((y) * width + (x1 + t)) * 3;
                buffer[index] = b;
                buffer[index + 1] = g;
                buffer[index + 2] = r;
            }
            if (x2 - t >= 0 && x2 - t < width) {
                int index = ((y) * width + (x2 - t)) * 3;
                buffer[index] = b;
                buffer[index + 1] = g;
                buffer[index + 2] = r;
            }
        }
    }
}

void CvDetect::detect(cam_fb_t *fb)
{
    long long start_time = esp_timer_get_time();
    cv::Mat image(fb->height, fb->width, CV_8UC3, (void*)fb->buf);
    cv::Mat hsv;

    cv::cvtColor(image, hsv, cv::COLOR_BGR2HSV);

    cv::Scalar primary_color_lower(CONFIG_PRIMARY_COLOR_LOWER_H, CONFIG_PRIMARY_COLOR_LOWER_S, CONFIG_PRIMARY_COLOR_LOWER_V);
    cv::Scalar primary_color_upper(CONFIG_PRIMARY_COLOR_UPPER_H, CONFIG_PRIMARY_COLOR_UPPER_S, CONFIG_PRIMARY_COLOR_UPPER_V);
    cv::Scalar secondary_color_lower(CONFIG_SECONDARY_COLOR_LOWER_H, CONFIG_SECONDARY_COLOR_LOWER_S, CONFIG_SECONDARY_COLOR_LOWER_V);
    cv::Scalar secondary_color_upper(CONFIG_SECONDARY_COLOR_UPPER_H, CONFIG_SECONDARY_COLOR_UPPER_S, CONFIG_SECONDARY_COLOR_UPPER_V);

    cv::Mat mask1, mask2, mask;
    cv::inRange(hsv, primary_color_lower, primary_color_upper, mask1);
    cv::inRange(hsv, secondary_color_lower, secondary_color_upper, mask2);
    mask = mask1 | mask2;

    cv::Mat kernel = cv::getStructuringElement(cv::MORPH_RECT, cv::Size(5, 5));
    cv::morphologyEx(mask, mask, cv::MORPH_OPEN, kernel);
    cv::morphologyEx(mask, mask, cv::MORPH_CLOSE, kernel);

    std::vector<std::vector<cv::Point>> contours;
    std::vector<cv::Vec4i> hierarchy;
    cv::findContours(mask, contours, hierarchy, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);
    long long duration_time = esp_timer_get_time() - start_time;
    ESP_LOGD(TAG, "Detect time: %lld ms", duration_time / 1000);

    xSemaphoreTake(m_mutex, portMAX_DELAY);
    m_results.clear();
    for (size_t i = 0; i < contours.size(); i++) {
        double area = cv::contourArea(contours[i]);
        if (area > CONFIG_EXAMPLE_CV_CONTOUR_FILTER_AREA_THRESHOLD) {
            cv::Rect boundingBox = cv::boundingRect(contours[i]);
            result_t result = {boundingBox.x, boundingBox.y, boundingBox.width, boundingBox.height};
            ESP_LOGD(TAG, "Detected at: X=%d, Y=%d, Width=%d, Height=%d",
                     boundingBox.x, boundingBox.y, boundingBox.width, boundingBox.height);
            m_results.push_back(result);
        }
    }
    xSemaphoreGive(m_mutex);
}

void CvDetect::lcd_refresh_task(void *args)
{
    CvDetect *self = (CvDetect *)args;
    while (1) {
        self->m_cam->cam_fb_get();
        cam_fb_t *fb = self->m_cam->cam_fb_peek(false);

        std::vector<result_t> results;
        xSemaphoreTake(self->m_mutex, portMAX_DELAY);
        results = self->m_results;
        xSemaphoreGive(self->m_mutex);

        for (const auto &result : results) {
            self->draw_rectangle_rgb((uint8_t *)fb->buf, fb->width, fb->height,
                                     result.x, result.y,
                                     result.x + result.width, result.y + result.height, 0, 0, 255, 10);
        }
        esp_lcd_panel_draw_bitmap(self->m_lcd_panel, 0, 0, fb->width, fb->height, fb->buf);
        self->m_cam->cam_fb_return();
        vTaskDelay(pdMS_TO_TICKS(5));
    }
}

void CvDetect::cv_detect_task(void *args)
{
    CvDetect *self = (CvDetect *)args;
    while (1) {
        self->m_cam->cam_fb_get();
        cam_fb_t *fb = self->m_cam->cam_fb_peek(false);
        self->detect(fb);
        self->m_cam->cam_fb_return();
        vTaskDelay(pdMS_TO_TICKS(1));
    }
}

CvDetect::CvDetect(esp_lcd_panel_handle_t lcd_panel,
                   Camera *cam) :
    m_lcd_panel(lcd_panel),
    m_cam(cam)
{
    m_mutex = xSemaphoreCreateMutex();
    m_is_running = false;
}

CvDetect::~CvDetect()
{
    vSemaphoreDelete(m_mutex);
    m_mutex = nullptr;
    m_is_running = false;
}

void CvDetect::run()
{
    if (m_is_running) {
        return;
    }

    if (xTaskCreatePinnedToCore(lcd_refresh_task, "lcd_refresh_task", 4 * 1024, this, 10, NULL, 1) != pdPASS) {
        ESP_LOGE(TAG, "Failed to create lcd refresh task");
        return;
    }

    if (xTaskCreatePinnedToCore(cv_detect_task, "cv_detect_task", 10 * 1024, this, 10, NULL, 0) != pdPASS) {
        ESP_LOGE(TAG, "Failed to create cv detect task");
        return;
    }

    m_is_running = true;
}
