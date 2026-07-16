/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <stdlib.h>
#include <string.h>
#include <sys/cdefs.h>

#include "imu_gesture_inference.h"
#include "imu_gesture_private.h"

typedef struct {
    imu_gesture_detector_t base;

    imu_gesture_inference_config_t config;
    size_t input_count;
    imu_gesture_inference_result_t last_result;

    float *window;
    float *output_scores;

    size_t collected_samples;
} imu_gesture_inference_obj_t;

static void imu_gesture_inference_reset_obj(
    imu_gesture_inference_obj_t *infer)
{
    infer->collected_samples = 0;

    memset(infer->window, 0,
           sizeof(float) * infer->input_count);
    memset(infer->output_scores, 0,
           sizeof(float) * infer->config.model->output_count);
    infer->last_result.label_index = 0;
    infer->last_result.score = 0.0f;

    if (infer->base.sample_queue != NULL) {
        xQueueReset(infer->base.sample_queue);
    }
}

static esp_err_t imu_gesture_inference_run_model(
    imu_gesture_inference_obj_t *infer,
    const float *time_major_window,
    imu_gesture_event_t *out_event)
{
    const bool infer_ok = infer->config.model->model_predict(
                              time_major_window,
                              infer->input_count,
                              infer->output_scores,
                              infer->config.model->output_count);
    if (!infer_ok) {
        *out_event = IMU_GESTURE_EVENT_INFERENCE_MODEL_FAILED;
        return ESP_OK;
    }

    size_t top_index = 0;
    float top_score = infer->output_scores[0];
    for (size_t i = 1; i < infer->config.model->output_count; ++i) {
        if (infer->output_scores[i] > top_score) {
            top_score = infer->output_scores[i];
            top_index = i;
        }
    }

    infer->last_result.label_index = top_index;
    infer->last_result.score = top_score;
    *out_event = IMU_GESTURE_EVENT_INFERENCE_RESULT;
    return ESP_OK;
}

static esp_err_t imu_gesture_inference_process_pending_internal(
    imu_gesture_detector_t *detector,
    size_t *out_processed)
{
    if (detector == NULL ||
            detector->type != IMU_GESTURE_DETECTOR_TYPE_INFERENCE ||
            detector->sample_queue == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    imu_gesture_inference_obj_t *infer =
        __containerof(detector, imu_gesture_inference_obj_t, base);
    size_t processed = 0;

    while (true) {
        imu_gesture_sample_t sample = {};
        if (xQueueReceive(detector->sample_queue, &sample, 0) != pdPASS) {
            break;
        }

        imu_gesture_event_t event = IMU_GESTURE_EVENT_NONE;
        float *dst = infer->window +
                     infer->collected_samples *
                     infer->config.model->input_channels;
        for (size_t i = 0; i < infer->config.model->input_channels; ++i) {
            dst[i] = sample.gyro[i];
        }
        infer->collected_samples++;

        esp_err_t ret = ESP_OK;
        if (infer->collected_samples >= infer->config.model->input_length) {
            ret = imu_gesture_inference_run_model(infer, infer->window, &event);
            const size_t keep_samples =
                infer->config.model->input_length - infer->config.window_step;
            if (keep_samples > 0) {
                memmove(infer->window,
                        infer->window +
                        infer->config.window_step *
                        infer->config.model->input_channels,
                        keep_samples * infer->config.model->input_channels *
                        sizeof(float));
            }
            infer->collected_samples = keep_samples;
        }
        if (ret != ESP_OK) {
            if (out_processed != NULL) {
                *out_processed = processed;
            }
            return ret;
        }

        if (event != IMU_GESTURE_EVENT_NONE && detector->cb != NULL) {
            detector->cb(detector, event, detector->cb_user_data);
        }
        processed++;
    }

    if (out_processed != NULL) {
        *out_processed = processed;
    }

    return ESP_OK;
}

static esp_err_t imu_gesture_inference_reset_detector(
    imu_gesture_detector_t *detector)
{
    if (detector == NULL ||
            detector->type != IMU_GESTURE_DETECTOR_TYPE_INFERENCE) {
        return ESP_ERR_INVALID_ARG;
    }

    imu_gesture_inference_obj_t *infer =
        __containerof(detector, imu_gesture_inference_obj_t, base);
    imu_gesture_inference_reset_obj(infer);
    return ESP_OK;
}

static void imu_gesture_inference_destroy_detector(
    imu_gesture_detector_t *detector)
{
    if (detector == NULL) {
        return;
    }

    imu_gesture_inference_obj_t *infer =
        __containerof(detector, imu_gesture_inference_obj_t, base);
    free(infer->output_scores);
    free(infer->window);
    free(infer);
}

static bool imu_gesture_is_inference_config_valid(
    const imu_gesture_inference_config_t *config)
{
    if (config == NULL || config->model == NULL ||
            config->model->model_init == NULL ||
            config->model->model_predict == NULL) {
        return false;
    }

    if (config->model->input_length == 0 ||
            config->model->input_channels == 0 ||
            config->model->output_count == 0 || config->window_step == 0 ||
            config->sample_queue_len == 0) {
        return false;
    }

    if (config->window_step > config->model->input_length) {
        return false;
    }

    if (config->model->input_channels > 3) {
        return false;
    }

    return true;
}

esp_err_t imu_gesture_inference_detector_create(
    const imu_gesture_inference_config_t *config,
    imu_gesture_detector_handle_t *out_detector)
{
    if (out_detector == NULL || !imu_gesture_is_inference_config_valid(config)) {
        return ESP_ERR_INVALID_ARG;
    }

    if (!config->model->model_init()) {
        return ESP_FAIL;
    }

    imu_gesture_inference_obj_t *infer = calloc(1, sizeof(*infer));
    if (infer == NULL) {
        return ESP_ERR_NO_MEM;
    }

    infer->config = *config;
    infer->input_count = config->model->input_length *
                         config->model->input_channels;

    infer->window = calloc(infer->input_count, sizeof(float));
    infer->output_scores = calloc(config->model->output_count, sizeof(float));
    if (infer->window == NULL || infer->output_scores == NULL) {
        imu_gesture_inference_destroy_detector(&infer->base);
        return ESP_ERR_NO_MEM;
    }

    infer->base.sample_queue = xQueueCreate(config->sample_queue_len,
                                            sizeof(imu_gesture_sample_t));
    if (infer->base.sample_queue == NULL) {
        imu_gesture_inference_destroy_detector(&infer->base);
        return ESP_ERR_NO_MEM;
    }

    infer->base.type = IMU_GESTURE_DETECTOR_TYPE_INFERENCE;
    infer->base.process_pending = imu_gesture_inference_process_pending_internal;
    infer->base.reset = imu_gesture_inference_reset_detector;
    infer->base.destroy = imu_gesture_inference_destroy_detector;

    imu_gesture_inference_reset_obj(infer);

    *out_detector = &infer->base;
    return ESP_OK;
}

esp_err_t imu_gesture_inference_detector_get_last_result(
    imu_gesture_detector_handle_t detector,
    imu_gesture_inference_result_t *out_result)
{
    if (detector == NULL || out_result == NULL ||
            detector->type != IMU_GESTURE_DETECTOR_TYPE_INFERENCE) {
        return ESP_ERR_INVALID_ARG;
    }

    imu_gesture_inference_obj_t *infer =
        __containerof(detector, imu_gesture_inference_obj_t, base);
    *out_result = infer->last_result;
    return ESP_OK;
}

esp_err_t imu_gesture_inference_detector_process_single_shot_buffer(
    imu_gesture_detector_handle_t detector,
    const imu_gesture_sample_t *samples,
    size_t sample_count)
{
    if (detector == NULL || samples == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (detector->type != IMU_GESTURE_DETECTOR_TYPE_INFERENCE) {
        return ESP_ERR_INVALID_ARG;
    }

    imu_gesture_inference_obj_t *infer =
        __containerof(detector, imu_gesture_inference_obj_t, base);
    if (sample_count != infer->config.model->input_length) {
        return ESP_ERR_INVALID_ARG;
    }

    for (size_t sample_idx = 0; sample_idx < sample_count; ++sample_idx) {
        const float *src = samples[sample_idx].gyro;
        float *dst = infer->window +
                     sample_idx * infer->config.model->input_channels;
        for (size_t axis_idx = 0;
                axis_idx < infer->config.model->input_channels;
                ++axis_idx) {
            dst[axis_idx] = src[axis_idx];
        }
    }

    imu_gesture_event_t event = IMU_GESTURE_EVENT_NONE;
    const esp_err_t ret =
        imu_gesture_inference_run_model(infer, infer->window, &event);
    if (ret != ESP_OK) {
        return ret;
    }

    if (event != IMU_GESTURE_EVENT_NONE && detector->cb != NULL) {
        detector->cb(detector, event, detector->cb_user_data);
    }

    return ESP_OK;
}
