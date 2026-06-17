/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "unity.h"

#include <string.h>

#include "imu_gesture.h"
#include "imu_gesture_inference.h"
#include "imu_gesture_knob.h"
#include "imu_gesture_space_switch.h"

typedef struct {
    int count;
    imu_gesture_event_t last_event;
} test_callback_state_t;

static bool s_test_model_init_ok = true;
static bool s_test_model_predict_ok = true;
static size_t s_test_model_top_index = 0;
static float s_test_model_top_score = 0.95f;
static float s_last_model_input[12] = {0};
static size_t s_last_model_input_len = 0;

static void test_runtime_cb(imu_gesture_detector_handle_t detector,
                            imu_gesture_event_t event,
                            void *user_data)
{
    (void)detector;

    test_callback_state_t *state = user_data;
    if (state == NULL) {
        return;
    }

    state->count++;
    state->last_event = event;
}

static bool test_model_init(void)
{
    return s_test_model_init_ok;
}

static bool test_model_predict(const float *input,
                               size_t input_len,
                               float *output,
                               size_t output_len)
{
    if (!s_test_model_predict_ok) {
        return false;
    }

    s_last_model_input_len = input_len;
    if (input != NULL && input_len <= (sizeof(s_last_model_input) / sizeof(float))) {
        memcpy(s_last_model_input, input, sizeof(float) * input_len);
    }

    memset(output, 0, sizeof(float) * output_len);
    output[s_test_model_top_index] = s_test_model_top_score;
    return true;
}

static imu_gesture_knob_config_t test_default_knob_config(void)
{
    return (imu_gesture_knob_config_t)IMU_GESTURE_KNOB_DEFAULT_CONFIG();
}

static imu_gesture_space_switch_config_t test_default_space_switch_config(void)
{
    return (imu_gesture_space_switch_config_t)
           IMU_GESTURE_SPACE_SWITCH_DEFAULT_CONFIG();
}

static imu_gesture_inference_config_t test_default_inference_config(void)
{
    static const imu_gesture_inference_model_t s_model = {
        .input_length = 4,
        .input_channels = 3,
        .output_count = 5,
        .model_init = test_model_init,
        .model_predict = test_model_predict,
    };
    imu_gesture_inference_config_t config = {
        .model = &s_model,
        .window_step = 2,
        .sample_queue_len = 8,
    };
    return config;
}

static esp_err_t test_push_and_process(
    imu_gesture_detector_handle_t detector,
    const imu_gesture_sample_t *sample)
{
    size_t processed = 0;

    TEST_ASSERT_EQUAL(ESP_OK, imu_gesture_detector_push_sample(detector, sample));
    return imu_gesture_detector_process_pending(detector, &processed);
}

static void test_feed_knob_sequence(imu_gesture_detector_handle_t detector,
                                    imu_gesture_axis_t axis,
                                    float primary_gyro_dps,
                                    const float stable_accel[3],
                                    int64_t start_us)
{
    imu_gesture_sample_t sample = {
        .accel = {0.0f, 0.0f, 1.0f},
        .gyro = {0.0f, 0.0f, 0.0f},
        .timestamp_us = start_us,
    };
    sample.gyro[(int)axis] = primary_gyro_dps;
    TEST_ASSERT_EQUAL(ESP_OK, test_push_and_process(detector, &sample));

    sample.accel[0] = stable_accel[0];
    sample.accel[1] = stable_accel[1];
    sample.accel[2] = stable_accel[2];
    sample.gyro[0] = 0.0f;
    sample.gyro[1] = 0.0f;
    sample.gyro[2] = 0.0f;
    sample.timestamp_us = start_us + 1000;
    TEST_ASSERT_EQUAL(ESP_OK, test_push_and_process(detector, &sample));

    sample.timestamp_us = start_us + 120000;
    TEST_ASSERT_EQUAL(ESP_OK, test_push_and_process(detector, &sample));
}

void setUp(void)
{
    s_test_model_init_ok = true;
    s_test_model_predict_ok = true;
    s_test_model_top_index = 0;
    s_test_model_top_score = 0.95f;
    memset(s_last_model_input, 0, sizeof(s_last_model_input));
    s_last_model_input_len = 0;
}

void tearDown(void)
{
}

void test_imu_gesture_knob_detector_create_reports_event(void)
{
    imu_gesture_detector_handle_t detector = NULL;
    imu_gesture_knob_config_t config = test_default_knob_config();
    test_callback_state_t cb_state = {};

    TEST_ASSERT_EQUAL(ESP_OK,
                      imu_gesture_knob_detector_create(&config, &detector));
    TEST_ASSERT_EQUAL(
        ESP_OK,
        imu_gesture_detector_register_cb(detector, test_runtime_cb, &cb_state));

    const float stable_accel[3] = {0.7f, 0.0f, 0.71f};
    test_feed_knob_sequence(detector,
                            config.axis,
                            60.0f * config.cw_sign,
                            stable_accel,
                            1000);

    TEST_ASSERT_EQUAL(1, cb_state.count);
    TEST_ASSERT_EQUAL(IMU_GESTURE_EVENT_KNOB_CW, cb_state.last_event);
    TEST_ASSERT_EQUAL(ESP_OK, imu_gesture_detector_del(detector));
}

void test_imu_gesture_space_switch_detector_create_reports_event(void)
{
    imu_gesture_detector_handle_t detector = NULL;
    imu_gesture_space_switch_config_t config =
        test_default_space_switch_config();
    test_callback_state_t cb_state = {};
    const imu_gesture_sample_t sample = {
        .accel = {0.0f, 0.0f, 1.0f},
        .gyro = {0.0f, 20.0f, 320.0f},
        .timestamp_us = 1000,
    };

    TEST_ASSERT_EQUAL(
        ESP_OK,
        imu_gesture_space_switch_detector_create(&config, &detector));
    TEST_ASSERT_EQUAL(
        ESP_OK,
        imu_gesture_detector_register_cb(detector, test_runtime_cb, &cb_state));
    TEST_ASSERT_EQUAL(ESP_OK, test_push_and_process(detector, &sample));
    TEST_ASSERT_EQUAL(1, cb_state.count);
    TEST_ASSERT_EQUAL(IMU_GESTURE_EVENT_SPACE_LEFT, cb_state.last_event);
    TEST_ASSERT_EQUAL(ESP_OK, imu_gesture_detector_del(detector));
}

void test_imu_gesture_queue_full_drops_oldest_sample(void)
{
    if (IMU_GESTURE_KNOB_DEFAULT_SAMPLE_QUEUE_LEN == 0) {
        return;
    }

    imu_gesture_detector_handle_t detector = NULL;
    imu_gesture_knob_config_t config = test_default_knob_config();
    test_callback_state_t cb_state = {};
    const float stable_accel[3] = {0.7f, 0.0f, 0.71f};
    const imu_gesture_sample_t stale_sample = {
        .accel = {0.0f, 0.0f, 1.0f},
        .gyro = {0.0f, 0.0f, 0.0f},
        .timestamp_us = 1000,
    };
    imu_gesture_sample_t fresh_sample = stale_sample;
    fresh_sample.timestamp_us = 2000;
    fresh_sample.gyro[(int)config.axis] = 80.0f;

    TEST_ASSERT_EQUAL(ESP_OK,
                      imu_gesture_knob_detector_create(&config, &detector));
    TEST_ASSERT_EQUAL(
        ESP_OK,
        imu_gesture_detector_register_cb(detector, test_runtime_cb, &cb_state));

    for (int i = 0; i < (int)IMU_GESTURE_KNOB_DEFAULT_SAMPLE_QUEUE_LEN; ++i) {
        TEST_ASSERT_EQUAL(
            ESP_OK,
            imu_gesture_detector_push_sample(detector, &stale_sample));
    }
    TEST_ASSERT_EQUAL(
        ESP_OK,
        imu_gesture_detector_push_sample(detector, &fresh_sample));

    size_t processed = 0;
    TEST_ASSERT_EQUAL(
        ESP_OK,
        imu_gesture_detector_process_pending(detector, &processed));

    imu_gesture_sample_t stable_sample = {
        .accel = {stable_accel[0], stable_accel[1], stable_accel[2]},
        .gyro = {0.0f, 0.0f, 0.0f},
        .timestamp_us = 3000,
    };
    TEST_ASSERT_EQUAL(ESP_OK, test_push_and_process(detector, &stable_sample));
    stable_sample.timestamp_us = 123000;
    TEST_ASSERT_EQUAL(ESP_OK, test_push_and_process(detector, &stable_sample));

    TEST_ASSERT_EQUAL(1, cb_state.count);
    TEST_ASSERT_EQUAL(IMU_GESTURE_EVENT_KNOB_CW, cb_state.last_event);
    TEST_ASSERT_EQUAL(ESP_OK, imu_gesture_detector_del(detector));
}

void test_imu_gesture_reset_clears_knob_state(void)
{
    imu_gesture_detector_handle_t detector = NULL;
    imu_gesture_knob_config_t config = test_default_knob_config();
    test_callback_state_t cb_state = {};
    const float stable_accel[3] = {0.7f, 0.0f, 0.71f};
    imu_gesture_sample_t sample = {
        .accel = {0.0f, 0.0f, 1.0f},
        .gyro = {0.0f, 0.0f, 0.0f},
        .timestamp_us = 1000,
    };
    sample.gyro[(int)config.axis] = 80.0f;

    TEST_ASSERT_EQUAL(ESP_OK,
                      imu_gesture_knob_detector_create(&config, &detector));
    TEST_ASSERT_EQUAL(
        ESP_OK,
        imu_gesture_detector_register_cb(detector, test_runtime_cb, &cb_state));

    TEST_ASSERT_EQUAL(ESP_OK, test_push_and_process(detector, &sample));
    TEST_ASSERT_EQUAL(ESP_OK, imu_gesture_detector_reset(detector));

    sample.accel[0] = stable_accel[0];
    sample.accel[1] = stable_accel[1];
    sample.accel[2] = stable_accel[2];
    sample.gyro[0] = 0.0f;
    sample.gyro[1] = 0.0f;
    sample.gyro[2] = 0.0f;
    sample.timestamp_us = 2000;
    TEST_ASSERT_EQUAL(ESP_OK, test_push_and_process(detector, &sample));
    sample.timestamp_us = 125000;
    TEST_ASSERT_EQUAL(ESP_OK, test_push_and_process(detector, &sample));

    TEST_ASSERT_EQUAL(0, cb_state.count);
    TEST_ASSERT_EQUAL(ESP_OK, imu_gesture_detector_del(detector));
}

void test_imu_gesture_inference_detector_reports_realtime_result(void)
{
    imu_gesture_detector_handle_t detector = NULL;
    imu_gesture_inference_result_t result = {};
    imu_gesture_inference_config_t config = test_default_inference_config();
    test_callback_state_t cb_state = {};

    TEST_ASSERT_EQUAL(
        ESP_OK,
        imu_gesture_inference_detector_create(&config, &detector));
    TEST_ASSERT_EQUAL(
        ESP_OK,
        imu_gesture_detector_register_cb(detector, test_runtime_cb, &cb_state));

    for (int i = 0; i < (int)config.model->input_length; ++i) {
        imu_gesture_sample_t sample = {
            .accel = {0.0f, 0.0f, 1.0f},
            .gyro = {(float)i, (float)i + 1.0f, (float)i + 2.0f},
            .timestamp_us = i * 1000,
        };
        TEST_ASSERT_EQUAL(ESP_OK, test_push_and_process(detector, &sample));
    }

    TEST_ASSERT_EQUAL(1, cb_state.count);
    TEST_ASSERT_EQUAL(IMU_GESTURE_EVENT_INFERENCE_RESULT, cb_state.last_event);
    TEST_ASSERT_EQUAL(
        ESP_OK,
        imu_gesture_inference_detector_get_last_result(detector, &result));
    static const float expected_input[] = {
        0.0f, 1.0f, 2.0f,
        1.0f, 2.0f, 3.0f,
        2.0f, 3.0f, 4.0f,
        3.0f, 4.0f, 5.0f,
    };
    TEST_ASSERT_EQUAL(sizeof(expected_input) / sizeof(float), s_last_model_input_len);
    TEST_ASSERT_EQUAL_FLOAT_ARRAY(expected_input,
                                  s_last_model_input,
                                  s_last_model_input_len);
    TEST_ASSERT_EQUAL(s_test_model_top_index, result.label_index);
    TEST_ASSERT_FLOAT_WITHIN(0.0001f,
                             s_test_model_top_score,
                             result.score);
    TEST_ASSERT_EQUAL(ESP_OK, imu_gesture_detector_del(detector));
}

void test_imu_gesture_inference_detector_process_single_shot_buffer_reports_result(void)
{
    imu_gesture_detector_handle_t detector = NULL;
    imu_gesture_inference_result_t result = {};
    imu_gesture_inference_config_t config = test_default_inference_config();
    test_callback_state_t cb_state = {};

    TEST_ASSERT_EQUAL(
        ESP_OK,
        imu_gesture_inference_detector_create(&config, &detector));
    TEST_ASSERT_EQUAL(
        ESP_OK,
        imu_gesture_detector_register_cb(detector, test_runtime_cb, &cb_state));

    imu_gesture_sample_t samples[4] = {};
    for (int i = 0; i < 4; ++i) {
        samples[i] = (imu_gesture_sample_t) {
            .accel = {0.0f, 0.0f, 1.0f},
            .gyro = {(float)i, (float)i + 1.0f, (float)i + 2.0f},
            .timestamp_us = i * 1000,
        };
    }

    TEST_ASSERT_EQUAL(
        ESP_OK,
        imu_gesture_inference_detector_process_single_shot_buffer(
            detector, samples, 4));
    TEST_ASSERT_EQUAL(1, cb_state.count);
    TEST_ASSERT_EQUAL(IMU_GESTURE_EVENT_INFERENCE_RESULT,
                      cb_state.last_event);
    TEST_ASSERT_EQUAL(
        ESP_OK,
        imu_gesture_inference_detector_get_last_result(detector, &result));
    static const float expected_input[] = {
        0.0f, 1.0f, 2.0f,
        1.0f, 2.0f, 3.0f,
        2.0f, 3.0f, 4.0f,
        3.0f, 4.0f, 5.0f,
    };
    TEST_ASSERT_EQUAL(sizeof(expected_input) / sizeof(float), s_last_model_input_len);
    TEST_ASSERT_EQUAL_FLOAT_ARRAY(expected_input,
                                  s_last_model_input,
                                  s_last_model_input_len);
    TEST_ASSERT_EQUAL(s_test_model_top_index, result.label_index);
    TEST_ASSERT_FLOAT_WITHIN(0.0001f,
                             s_test_model_top_score,
                             result.score);
    TEST_ASSERT_EQUAL(ESP_OK, imu_gesture_detector_del(detector));
}

void test_imu_gesture_inference_detector_reports_model_failed(void)
{
    imu_gesture_detector_handle_t detector = NULL;
    imu_gesture_inference_result_t result = {};
    imu_gesture_inference_config_t config = test_default_inference_config();
    test_callback_state_t cb_state = {};

    s_test_model_predict_ok = false;

    TEST_ASSERT_EQUAL(
        ESP_OK,
        imu_gesture_inference_detector_create(&config, &detector));
    TEST_ASSERT_EQUAL(
        ESP_OK,
        imu_gesture_detector_register_cb(detector, test_runtime_cb, &cb_state));

    for (int i = 0; i < (int)config.model->input_length; ++i) {
        imu_gesture_sample_t sample = {
            .accel = {0.0f, 0.0f, 1.0f},
            .gyro = {(float)i, (float)i, (float)i},
            .timestamp_us = i * 1000,
        };
        TEST_ASSERT_EQUAL(ESP_OK, test_push_and_process(detector, &sample));
    }

    TEST_ASSERT_EQUAL(1, cb_state.count);
    TEST_ASSERT_EQUAL(IMU_GESTURE_EVENT_INFERENCE_MODEL_FAILED,
                      cb_state.last_event);
    TEST_ASSERT_EQUAL(
        ESP_OK,
        imu_gesture_inference_detector_get_last_result(detector, &result));
    TEST_ASSERT_EQUAL(0u, result.label_index);
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 0.0f, result.score);
    TEST_ASSERT_EQUAL(ESP_OK, imu_gesture_detector_del(detector));
}

void test_imu_gesture_invalid_state_without_queue(void)
{
    if (IMU_GESTURE_KNOB_DEFAULT_SAMPLE_QUEUE_LEN != 0) {
        return;
    }

    imu_gesture_detector_handle_t detector = NULL;
    imu_gesture_knob_config_t config = test_default_knob_config();
    const imu_gesture_sample_t sample = {
        .accel = {0.0f, 0.0f, 1.0f},
        .gyro = {0.0f, 0.0f, 0.0f},
        .timestamp_us = 1000,
    };
    size_t processed = 0;

    TEST_ASSERT_EQUAL(ESP_OK,
                      imu_gesture_knob_detector_create(&config, &detector));
    TEST_ASSERT_EQUAL(
        ESP_ERR_INVALID_STATE,
        imu_gesture_detector_push_sample(detector, &sample));
    TEST_ASSERT_EQUAL(
        ESP_ERR_INVALID_STATE,
        imu_gesture_detector_process_pending(detector, &processed));
    TEST_ASSERT_EQUAL(ESP_OK, imu_gesture_detector_del(detector));
}
