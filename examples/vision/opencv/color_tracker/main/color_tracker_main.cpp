/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lcd.hpp"
#include "camera.hpp"
#include "cv_detect.hpp"

static esp_lcd_panel_handle_t display_panel;

extern "C" void app_main(void)
{
    ESP_ERROR_CHECK(lcd_init(&display_panel));
    auto cam = new Camera(VIDEO_PIX_FMT_RGB888, 4, V4L2_MEMORY_MMAP, true);
    auto cv_detect = new CvDetect(display_panel, cam);
    cv_detect->run();
}
