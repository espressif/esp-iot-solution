/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "app_classifier.hpp"
#include "bsp/esp-bsp.h"
#include "dl_image_preprocessor.hpp"
#include "esp_log.h"
#include <cstdio>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "ui/screens/ui_Screen1.h"

static const char *TAG = "app_classifier";

AppClassifier::AppClassifier(Camera *cam, SelfLearningClassifier *classifier)
    : m_cam(cam)
    , m_classifier(classifier)
    , m_record_class_id(0)
    , m_display_fb(nullptr)
    , m_recog_result_label(nullptr)
{
    m_evg = xEventGroupCreate();
    m_img_dsc = {};
    m_img_dsc.header.magic = LV_IMAGE_HEADER_MAGIC;
    m_img_dsc.header.cf = LV_COLOR_FORMAT_RGB565;
    m_img_dsc.header.flags = 0;

    m_record_class_id = lv_roller_get_selected(ui_ClassRoller);
    if (bsp_display_lock(0)) {
        m_recog_result_label = lv_label_create(ui_Screen1);
        lv_obj_align_to(m_recog_result_label, ui_CamImage, LV_ALIGN_TOP_MID, 0, 4);
        lv_obj_set_style_bg_color(m_recog_result_label, lv_color_hex(0x808080), 0);
        lv_obj_set_style_bg_opa(m_recog_result_label, LV_OPA_COVER, 0);
        lv_obj_set_style_pad_all(m_recog_result_label, 4, 0);
        lv_obj_set_style_text_font(m_recog_result_label, &lv_font_montserrat_20, 0);
        lv_obj_set_style_text_color(m_recog_result_label, lv_color_hex(0xFF0000), 0);
        lv_label_set_text(m_recog_result_label, "");
        bsp_display_unlock();
    }
    lv_obj_add_event_cb(ui_ClassRoller, class_roller_changed_cb, LV_EVENT_VALUE_CHANGED, this);
    lv_obj_add_event_cb(ui_RecordButton, record_clicked_cb, LV_EVENT_CLICKED, this);
    lv_obj_add_event_cb(ui_RecogButton, recog_clicked_cb, LV_EVENT_CLICKED, this);
}

void AppClassifier::start()
{
    xTaskCreatePinnedToCore(display_task, "display", 5 * 1024, this, 5, nullptr, 0);
    xTaskCreatePinnedToCore(recognition_task, "recognition", 8 * 1024, this, 5, nullptr, 1);
}

void AppClassifier::display_task(void *arg)
{
    AppClassifier *self = (AppClassifier *)arg;
    while (1) {
        cam_fb_t *fb = self->m_cam->cam_fb_get();
        if (fb != nullptr) {
            if (self->m_display_fb != nullptr) {
                self->m_cam->cam_fb_return();
            }
            self->m_display_fb = fb;

            self->m_img_dsc.header.w = fb->width;
            self->m_img_dsc.header.h = fb->height;
            self->m_img_dsc.data_size = fb->width * fb->height * 2;
            self->m_img_dsc.data = (const uint8_t *)fb->buf;

            if (bsp_display_lock(0)) {
                lv_image_set_src(ui_CamImage, &self->m_img_dsc);
                bsp_display_unlock();
            }
        }
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

void AppClassifier::recognition_task(void *arg)
{
    AppClassifier *self = (AppClassifier *)arg;
    const EventBits_t bits_to_wait = RECORD_BIT | RECOG_BIT;
    while (1) {
        EventBits_t bits = xEventGroupWaitBits(self->m_evg, bits_to_wait, pdTRUE, pdFALSE, portMAX_DELAY);
        switch (bits) {
        case RECORD_BIT: {
            cam_fb_t *fb = self->m_cam->cam_fb_get();
            if (fb != nullptr) {
                dl::image::img_t img = {
                    .data = fb->buf,
                    .width = static_cast<uint16_t>(fb->width),
                    .height = static_cast<uint16_t>(fb->height),
                    .pix_type = dl::image::DL_IMAGE_PIX_TYPE_RGB565,
                };
                self->m_classifier->enroll(img, self->m_record_class_id);
                self->m_cam->cam_fb_return();
                ESP_LOGI(TAG, "Enrolled to class %d", self->m_record_class_id);
            }
            break;
        }
        case RECOG_BIT: {
            cam_fb_t *fb = self->m_cam->cam_fb_get();
            if (fb != nullptr) {
                dl::image::img_t img = {
                    .data = fb->buf,
                    .width = static_cast<uint16_t>(fb->width),
                    .height = static_cast<uint16_t>(fb->height),
                    .pix_type = dl::image::DL_IMAGE_PIX_TYPE_RGB565,
                };
                PredictResult result = self->m_classifier->predict(img);
                self->m_cam->cam_fb_return();
                char buf[32];
                if (result.class_id >= 0) {
                    snprintf(buf, sizeof(buf), "Class %d %.0f%%", result.class_id, result.confidence);
                    ESP_LOGI(TAG, "Recog: %s", buf);
                } else {
                    snprintf(buf, sizeof(buf), "Unknown");
                }
                if (self->m_recog_result_label != nullptr && bsp_display_lock(0)) {
                    lv_label_set_text(self->m_recog_result_label, buf);
                    bsp_display_unlock();
                }
            }
            break;
        }
        default:
            break;
        }
    }
}

void AppClassifier::class_roller_changed_cb(lv_event_t *e)
{
    AppClassifier *self = (AppClassifier *)lv_event_get_user_data(e);
    self->m_record_class_id = lv_roller_get_selected(ui_ClassRoller);
}

void AppClassifier::record_clicked_cb(lv_event_t *e)
{
    AppClassifier *self = (AppClassifier *)lv_event_get_user_data(e);
    xEventGroupSetBits(self->m_evg, RECORD_BIT);
}

void AppClassifier::recog_clicked_cb(lv_event_t *e)
{
    AppClassifier *self = (AppClassifier *)lv_event_get_user_data(e);
    xEventGroupSetBits(self->m_evg, RECOG_BIT);
}
