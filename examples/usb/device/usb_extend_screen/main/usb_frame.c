/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include "esp_err.h"
#include "esp_log.h"
#include "esp_check.h"
#include "esp_private/usb_phy.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "usb_frame.h"
#if CONFIG_IDF_TARGET_ESP32P4
#include "driver/jpeg_decode.h"
#endif

static QueueHandle_t empty_fb_queue = NULL;
static QueueHandle_t filled_fb_queue = NULL;
static const char *TAG = "usb_frame";

esp_err_t frame_allocate(int nb_of_fb, size_t fb_size)
{
    esp_err_t ret;

    // We will be passing the frame buffers by reference
    empty_fb_queue = xQueueCreate(nb_of_fb, sizeof(frame_t *));
    ESP_RETURN_ON_FALSE(empty_fb_queue, ESP_ERR_NO_MEM, TAG, "Not enough memory for empty_fb_queue %d", nb_of_fb);
    filled_fb_queue = xQueueCreate(nb_of_fb, sizeof(frame_t *));
    ESP_RETURN_ON_FALSE(filled_fb_queue, ESP_ERR_NO_MEM, TAG, "Not enough memory for filled_fb_queue %d", nb_of_fb);
    for (int i = 0; i < nb_of_fb; i++) {
        // Allocate the frame buffer
        frame_t *this_fb = malloc(sizeof(frame_t));
        ESP_RETURN_ON_FALSE(this_fb, ESP_ERR_NO_MEM,  TAG, "Not enough memory for frame buffers %d", fb_size);
#if CONFIG_IDF_TARGET_ESP32P4
        size_t malloc_size = 0;
        jpeg_decode_memory_alloc_cfg_t tx_mem_cfg = {
            .buffer_direction = JPEG_DEC_ALLOC_INPUT_BUFFER,
        };
        uint8_t *this_data = (uint8_t*)jpeg_alloc_decoder_mem(fb_size, &tx_mem_cfg, &malloc_size);
#elif CONFIG_IDF_TARGET_ESP32S3
        uint8_t *this_data = (uint8_t *)heap_caps_aligned_alloc(16, fb_size, MALLOC_CAP_SPIRAM);
#endif
        if (!this_data) {
            free(this_fb);
            ret = ESP_ERR_NO_MEM;
            ESP_LOGE(TAG, "Not enough memory for frame buffers %d", fb_size);
        }

        // Set members to default
        this_fb->data = this_data;
        this_fb->data_buffer_len = fb_size;
        this_fb->data_len = 0;

        // Add the frame to Queue of empty frames
        const BaseType_t result = xQueueSend(empty_fb_queue, &this_fb, 0);
        assert(pdPASS == result);
    }
    return ret;
}

void frame_reset(frame_t *frame)
{
    assert(frame);
    frame->data_len = 0;
}

esp_err_t frame_return_empty(frame_t *frame)
{
    frame_reset(frame);
    BaseType_t result = xQueueSend(empty_fb_queue, &frame, 0);
    ESP_RETURN_ON_FALSE(result == pdPASS, ESP_ERR_NO_MEM, TAG, "Not enough memory empty_fb_queue");
    return ESP_OK;
}

esp_err_t frame_send_filled(frame_t *frame)
{
    frame_reset(frame);
    BaseType_t result = xQueueSend(filled_fb_queue, &frame, 0);
    ESP_RETURN_ON_FALSE(result == pdPASS, ESP_ERR_NO_MEM, TAG, "Not enough memory filled_fb_queue");
    return ESP_OK;
}

esp_err_t frame_add_data(frame_t *frame, const uint8_t *data, size_t data_len)
{
    ESP_RETURN_ON_FALSE(frame && data && data_len, ESP_ERR_INVALID_ARG, TAG, "Invalid arguments");
    if (frame->data_buffer_len < frame->data_len + data_len) {
        ESP_LOGD(TAG, "Frame buffer overflow");
        return ESP_ERR_INVALID_SIZE;
    }

    memcpy(frame->data + frame->data_len, data, data_len);
    frame->data_len += data_len;
    return ESP_OK;
}

frame_t *frame_get_empty(void)
{
    frame_t *this_fb;
    if (xQueueReceive(empty_fb_queue, &this_fb, 0) == pdPASS) {
        return this_fb;
    } else {
        return NULL;
    }
}

frame_t *frame_get_filled(void)
{
    frame_t *this_fb;
    if (xQueueReceive(filled_fb_queue, &this_fb, portMAX_DELAY) == pdPASS) {
        return this_fb;
    } else {
        return NULL;
    }
}
