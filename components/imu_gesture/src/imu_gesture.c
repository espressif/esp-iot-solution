/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <stdlib.h>

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

#include "imu_gesture.h"
#include "imu_gesture_private.h"

esp_err_t imu_gesture_detector_push_sample(
    imu_gesture_detector_handle_t detector,
    const imu_gesture_sample_t *sample)
{
    if (detector == NULL || sample == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (detector->sample_queue == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    if (xQueueSend(detector->sample_queue, sample, 0) == pdPASS) {
        return ESP_OK;
    }

    imu_gesture_sample_t dropped_sample = {};
    if (xQueueReceive(detector->sample_queue, &dropped_sample, 0) != pdPASS) {
        return ESP_FAIL;
    }

    return (xQueueSend(detector->sample_queue, sample, 0) == pdPASS) ? ESP_OK
           : ESP_FAIL;
}

esp_err_t imu_gesture_detector_process_pending(
    imu_gesture_detector_handle_t detector,
    size_t *out_processed)
{
    if (detector == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (detector->sample_queue == NULL || detector->process_pending == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    return detector->process_pending(detector, out_processed);
}

esp_err_t imu_gesture_detector_reset(
    imu_gesture_detector_handle_t detector)
{
    if (detector == NULL || detector->reset == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    return detector->reset(detector);
}

esp_err_t imu_gesture_detector_del(
    imu_gesture_detector_handle_t detector)
{
    if (detector == NULL) {
        return ESP_OK;
    }

    if (detector->sample_queue != NULL) {
        vQueueDelete(detector->sample_queue);
        detector->sample_queue = NULL;
    }

    if (detector->destroy != NULL) {
        detector->destroy(detector);
        return ESP_OK;
    }

    free(detector);
    return ESP_OK;
}

esp_err_t imu_gesture_detector_register_cb(
    imu_gesture_detector_handle_t detector,
    imu_gesture_cb_t cb,
    void *user_data)
{
    if (detector == NULL || cb == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    detector->cb = cb;
    detector->cb_user_data = user_data;
    return ESP_OK;
}

esp_err_t imu_gesture_detector_unregister_cb(
    imu_gesture_detector_handle_t detector)
{
    if (detector == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    detector->cb = NULL;
    detector->cb_user_data = NULL;
    return ESP_OK;
}
