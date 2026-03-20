/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "lvgl.h"
#include "app_camera.hpp"
#include "self_learning_classifier.hpp"

#define RECORD_BIT  (1 << 0)
#define RECOG_BIT   (1 << 1)

class AppClassifier {
public:
    /**
     * @brief Construct a new App Classifier object
     *
     * @param cam Camera object
     * @param classifier SelfLearningClassifier object
     */
    AppClassifier(Camera *cam, SelfLearningClassifier *classifier);

    /**
     * @brief Start the app classifier
     *
     */
    void start();
private:
    static void display_task(void *arg);
    static void recognition_task(void *arg);
    static void record_clicked_cb(lv_event_t *e);
    static void recog_clicked_cb(lv_event_t *e);
    static void class_roller_changed_cb(lv_event_t *e);

    Camera *m_cam;
    SelfLearningClassifier *m_classifier;
    EventGroupHandle_t m_evg;
    int m_record_class_id;

    lv_image_dsc_t m_img_dsc;
    cam_fb_t *m_display_fb;
    lv_obj_t *m_recog_result_label;
};
