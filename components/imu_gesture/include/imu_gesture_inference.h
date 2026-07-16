/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "imu_gesture_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Model initialization function for an inference detector
 *
 * @return
 *      - true: Model initialization succeeds
 *      - false: Model initialization fails
 */
typedef bool (*imu_gesture_model_init_fn)(void);

/**
 * @brief Model prediction function for an inference detector
 *
 * @param input: Time-major model input buffer flattened in [L, C] order
 * @param input_len: Number of float elements in @p input
 * @param output: Output score buffer to be filled by the model
 * @param output_len: Number of float elements available in @p output
 *
 * @return
 *      - true: Model prediction succeeds
 *      - false: Model prediction fails
 */
typedef bool (*imu_gesture_model_predict_fn)(const float *input, size_t input_len, float *output, size_t output_len);

/**
 * @brief Latest successful inference result stored by an inference detector
 */
typedef struct imu_gesture_inference_result_t {
    uint32_t label_index; /*!< Latest top label index */
    float score;        /*!< Latest top score */
} imu_gesture_inference_result_t;

/**
 * @brief Compile-time model description used by an inference detector
 *
 * This descriptor groups the model's static facts so exported model
 * components can expose a single object that applications pass into the
 * runtime detector config. The object is expected to have static or otherwise
 * detector-long lifetime.
 */
typedef struct imu_gesture_inference_model_t {
    uint32_t input_length;                      /*!< Number of samples required by the model input window */
    uint32_t input_channels;                    /*!< Number of channels for each sample in the model input */
    uint32_t output_count;                      /*!< Number of output scores produced by the model */
    imu_gesture_model_init_fn model_init;       /*!< Model initialization callback */
    imu_gesture_model_predict_fn model_predict; /*!< Model prediction callback */
} imu_gesture_inference_model_t;

/**
 * @brief Inference detector runtime configuration
 *
 * The configuration selects one compile-time model descriptor and adds the
 * detector's runtime queue/window policy.
 */
typedef struct imu_gesture_inference_config_t {
    const imu_gesture_inference_model_t *model; /*!< Static model descriptor */
    uint32_t window_step;                       /*!< Sliding step in samples for realtime inference */
    uint32_t sample_queue_len;                  /*!< Internal detector sample queue length */
} imu_gesture_inference_config_t;

/**
 * @brief Create an inference detector instance
 *
 * @param config: Inference detector configuration
 * @param detector_handle: Returned detector handle
 *
 * @return
 *      - ESP_OK: Create inference detector successfully
 *      - ESP_ERR_INVALID_ARG: Create inference detector failed because of invalid parameters
 *      - ESP_ERR_NO_MEM: Create inference detector failed because of insufficient memory
 *      - ESP_FAIL: Create inference detector failed because of other error
 */
esp_err_t imu_gesture_inference_detector_create(const imu_gesture_inference_config_t *config, imu_gesture_detector_handle_t *detector_handle);

/**
 * @brief Get the latest successful inference result cached by the detector
 *
 * @param detector: Inference detector handle
 * @param result: Returned latest result
 *
 * @return
 *      - ESP_OK: Latest result copied successfully
 *      - ESP_ERR_INVALID_ARG: Invalid detector or output pointer
 */
esp_err_t imu_gesture_inference_detector_get_last_result(imu_gesture_detector_handle_t detector, imu_gesture_inference_result_t *result);

/**
 * @brief Run one single-shot inference from an application-owned sample buffer
 *        This API is intended for one-shot inference only. The caller must
 *        provide exactly one full model window in time order. The detector
 *        copies that buffer into the detector's internal time-major window,
 *        passes the assembled [L, C] window directly to @c model_predict(),
 *        invokes the model once, caches the latest successful result inside
 *        the detector, and dispatches the detector callback if an event is
 *        emitted. It does
 *        not consume the detector's realtime queue, but it does reuse the
 *        detector's internal window storage, so applications should reset the
 *        detector afterward if they want realtime inference to resume from a
 *        clean window.
 *
 * @param detector: Inference detector handle
 * @param samples: Buffer of IMU samples in time order
 * @param sample_count: Number of samples in @p samples, must equal input_length
 *
 * @return
 *      - ESP_OK: One-shot buffer processed successfully (model failures are re
 *        ported via IMU_GESTURE_EVENT_INFERENCE_MODEL_FAILED).
 *      - ESP_ERR_INVALID_ARG: Invalid detector, buffer, or sample count
 */
esp_err_t imu_gesture_inference_detector_process_single_shot_buffer(imu_gesture_detector_handle_t detector, const imu_gesture_sample_t *samples, size_t sample_count);

#ifdef __cplusplus
}
#endif
