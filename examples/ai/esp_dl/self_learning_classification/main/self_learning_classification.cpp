/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "app_lcd.hpp"
#include "app_camera.hpp"
#include "app_classifier.hpp"
#include "self_learning_classifier.hpp"

extern "C" void app_main(void)
{
    app_lcd_init();

    auto cam = new Camera(VIDEO_PIX_FMT_RGB565, 4, V4L2_MEMORY_MMAP, false);
    auto classifier = new SelfLearningClassifier("imagenet_cls.espdl");
    auto app = new AppClassifier(cam, classifier);
    app->start();
}
