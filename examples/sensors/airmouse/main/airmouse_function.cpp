/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "airmouse_function.h"
#include "airmouse_hid.h"
#include "sdkconfig.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/semphr.h"

#define TAG "AIRMOUSE"

static QueueHandle_t s_airmouse_imu_queue = NULL;
static QueueHandle_t s_airmouse_gesture_imu_queue = NULL;
static QueueHandle_t s_airmouse_mouse_event_queue = NULL;
static QueueHandle_t s_airmouse_inference_imu_queue = NULL;
static imu_gesture_detector_handle_t s_airmouse_knob_detector = NULL;
static imu_gesture_detector_handle_t s_airmouse_space_switch_detector = NULL;
static airmouse_runtime_gesture_state_t s_airmouse_runtime_gesture_state = {};
static airmouse_inference_dispatch_state_t s_airmouse_inference_dispatch_state = {};
static TaskHandle_t s_airmouse_runtime_gesture_task_handle = NULL;
static TaskHandle_t s_airmouse_runtime_config_task_handle = NULL;
static bool s_airmouse_runtime_config_request_pending = false;
#if CONFIG_AIRMOUSE_GESTURE_INFER_ENABLE
static airmouse_gesture_infer_state_t s_airmouse_gesture_infer_state = {};
static TaskHandle_t s_airmouse_inference_task_handle = NULL;
#endif

static void airmouse_reset_runtime_gesture_pipeline(
    airmouse_function_handle_t *airmouse_ctx)
{
    if (s_airmouse_gesture_imu_queue != NULL) {
        xQueueReset(s_airmouse_gesture_imu_queue);
    }
    if (airmouse_ctx->knob_detector != NULL) {
        (void)imu_gesture_detector_reset(airmouse_ctx->knob_detector);
    }
    if (airmouse_ctx->space_switch_detector != NULL) {
        (void)imu_gesture_detector_reset(airmouse_ctx->space_switch_detector);
    }
    airmouse_runtime_gesture_state_reset(&s_airmouse_runtime_gesture_state);
}

#if CONFIG_AIRMOUSE_GESTURE_INFER_ENABLE
static void airmouse_reset_inference_pipeline(
    airmouse_function_handle_t *airmouse_ctx,
    bool reset_dispatch_state)
{
    if (s_airmouse_inference_imu_queue != NULL) {
        xQueueReset(s_airmouse_inference_imu_queue);
    }
    if (airmouse_ctx->inference_detector != NULL) {
        (void)imu_gesture_detector_reset(airmouse_ctx->inference_detector);
    }
    airmouse_gesture_infer_state_reset(&s_airmouse_gesture_infer_state);
    if (reset_dispatch_state) {
        airmouse_inference_dispatch_state_reset(
            &s_airmouse_inference_dispatch_state);
    }
}
#endif

static void airmouse_reset_sampling_queues(void)
{
    if (s_airmouse_imu_queue != NULL) {
        xQueueReset(s_airmouse_imu_queue);
    }
    if (s_airmouse_gesture_imu_queue != NULL) {
        xQueueReset(s_airmouse_gesture_imu_queue);
    }
    if (s_airmouse_inference_imu_queue != NULL) {
        xQueueReset(s_airmouse_inference_imu_queue);
    }
}

static void airmouse_reset_pose_related_pipelines(
    airmouse_function_handle_t *airmouse_ctx)
{
    airmouse_reset_sampling_queues();
    airmouse_reset_runtime_gesture_pipeline(airmouse_ctx);
#if CONFIG_AIRMOUSE_GESTURE_INFER_ENABLE
    if (s_airmouse_inference_task_handle != NULL) {
        (void)xTaskNotify(s_airmouse_inference_task_handle,
                          AIRMOUSE_INFERENCE_NOTIFY_RESET,
                          eSetBits);
    } else {
        airmouse_reset_inference_pipeline(airmouse_ctx, true);
    }
#endif
}

static void airmouse_runtime_gesture_event_cb(
    imu_gesture_detector_handle_t detector,
    imu_gesture_event_t event,
    void *user_data)
{
    (void)detector;
    (void)user_data;

    const int64_t now_us = esp_timer_get_time();
    airmouse_mouse_event_t evt = {};

    if (!airmouse_runtime_gesture_build_mouse_event(
                &s_airmouse_runtime_gesture_state,
                event,
                now_us,
                &evt)) {
        return;
    }

    switch (evt.type) {
    case AIRMOUSE_MOUSE_EVENT_SPACE_LEFT:
        ESP_LOGI(TAG, "space switch detected: left");
        break;
    case AIRMOUSE_MOUSE_EVENT_SPACE_RIGHT:
        ESP_LOGI(TAG, "space switch detected: right");
        break;
    case AIRMOUSE_MOUSE_EVENT_SPACE_UP:
        ESP_LOGI(TAG, "space switch detected: up");
        break;
    case AIRMOUSE_MOUSE_EVENT_SPACE_DOWN:
        ESP_LOGI(TAG, "space switch detected: down");
        break;
    case AIRMOUSE_MOUSE_EVENT_KNOB_CW:
        ESP_LOGI(TAG, "knob detected: clockwise");
        break;
    case AIRMOUSE_MOUSE_EVENT_KNOB_CCW:
        ESP_LOGI(TAG, "knob detected: counterclockwise");
        break;
    default:
        break;
    }

    airmouse_post_mouse_event(&evt);
}

#if CONFIG_AIRMOUSE_GESTURE_INFER_ENABLE
static void airmouse_inference_event_cb(
    imu_gesture_detector_handle_t detector,
    imu_gesture_event_t event,
    void *user_data)
{
    (void)user_data;

    airmouse_gesture_infer_action_t action =
        airmouse_gesture_infer_handle_detector_event(
            &s_airmouse_gesture_infer_state, detector, event);

    if (action == AIRMOUSE_GESTURE_INFER_ACTION_OPEN_TASK_VIEW ||
            action == AIRMOUSE_GESTURE_INFER_ACTION_MEDIA_PLAY_PAUSE ||
            action == AIRMOUSE_GESTURE_INFER_ACTION_ESCAPE ||
            action == AIRMOUSE_GESTURE_INFER_ACTION_OPEN_WINDOWS_OSK) {
        airmouse_mouse_event_t mouse_event = {};
        if (!airmouse_inference_dispatch_action_to_mouse_event(
                    action,
                    &s_airmouse_inference_dispatch_state,
                    esp_timer_get_time(),
                    &mouse_event)) {
            switch (action) {
            case AIRMOUSE_GESTURE_INFER_ACTION_OPEN_TASK_VIEW:
                ESP_LOGI(TAG, "S label ignored: suppressed after left/right page action");
                break;
            case AIRMOUSE_GESTURE_INFER_ACTION_ESCAPE:
                ESP_LOGI(TAG, "V label ignored: suppressed after up/down page action");
                break;
            case AIRMOUSE_GESTURE_INFER_ACTION_OPEN_WINDOWS_OSK:
                ESP_LOGI(TAG, "Z label ignored: suppressed after left/right page action");
                break;
            default:
                break;
            }
            return;
        }
        airmouse_post_mouse_event(&mouse_event);
    }
}
#endif

QueueHandle_t airmouse_get_imu_queue(void)
{
    return s_airmouse_imu_queue;
}

QueueHandle_t airmouse_get_mouse_event_queue(void)
{
    return s_airmouse_mouse_event_queue;
}

TaskHandle_t *airmouse_get_runtime_gesture_task_handle_ref(void)
{
    return &s_airmouse_runtime_gesture_task_handle;
}

TaskHandle_t *airmouse_get_runtime_config_task_handle_ref(void)
{
    return &s_airmouse_runtime_config_task_handle;
}

TaskHandle_t *airmouse_get_inference_task_handle_ref(void)
{
#if CONFIG_AIRMOUSE_GESTURE_INFER_ENABLE
    return &s_airmouse_inference_task_handle;
#else
    return NULL;
#endif
}

void airmouse_post_mouse_event(const airmouse_mouse_event_t *evt)
{
    if (s_airmouse_mouse_event_queue == NULL || evt == NULL) {
        return;
    }

    BaseType_t qret = xQueueSend(s_airmouse_mouse_event_queue, evt, 0);
    if (qret != pdPASS) {
        if (evt->type == AIRMOUSE_MOUSE_EVENT_MOVE ||
                evt->type == AIRMOUSE_MOUSE_EVENT_ABS_MOVE) {
            return;
        }

        airmouse_mouse_event_t dropped_evt;
        if (xQueueReceive(s_airmouse_mouse_event_queue, &dropped_evt, 0) == pdPASS) {
            if (xQueueSend(s_airmouse_mouse_event_queue, evt, 0) != pdPASS) {
                ESP_LOGW(TAG, "mouse event queue still full, drop event type=%d", (int)evt->type);
            }
        }
    }
}

void airmouse_runtime_config_request(void)
{
    if (s_airmouse_runtime_config_task_handle == NULL) {
        ESP_LOGW(TAG, "runtime config request ignored: task not ready");
        return;
    }

    airmouse_ble_handle_t *ble_handle = airmouse_ble_hid_get_ctx();
    if (ble_handle != NULL) {
        ble_handle->runtime_config_mode = true;
    }

    s_airmouse_runtime_config_request_pending = true;
    (void)xTaskNotify(s_airmouse_runtime_config_task_handle,
                      AIRMOUSE_RUNTIME_CONFIG_NOTIFY_ENTER,
                      eSetBits);
}

void airmouse_runtime_gesture_task(void *pvParameters)
{
    if (pvParameters == NULL) {
        ESP_LOGE(TAG, "runtime gesture task: pvParameters is NULL");
        vTaskDelete(NULL);
        return;
    }

    airmouse_function_handle_t *airmouse_ctx =
        (airmouse_function_handle_t *)pvParameters;

    if (airmouse_ctx->ble_handle == NULL) {
        ESP_LOGE(TAG, "runtime gesture task: ble handle is NULL");
        vTaskDelete(NULL);
        return;
    }

    airmouse_ble_handle_t *ble_handle = airmouse_ctx->ble_handle;

    if (s_airmouse_gesture_imu_queue == NULL) {
        ESP_LOGE(TAG, "runtime gesture task: imu queue is NULL");
        vTaskDelete(NULL);
        return;
    }

    s_airmouse_runtime_gesture_task_handle = xTaskGetCurrentTaskHandle();

    bool inactive_reset_done = false;
    bool active_logged = false;

    ESP_LOGI(TAG, "runtime gesture task started");

    while (1) {
        uint32_t notify_bits = 0;
        (void)xTaskNotifyWait(0, UINT32_MAX, &notify_bits, 0);

        imu_gesture_detector_handle_t knob_detector = airmouse_ctx->knob_detector;
        imu_gesture_detector_handle_t space_switch_detector =
            airmouse_ctx->space_switch_detector;

        if (notify_bits & AIRMOUSE_RUNTIME_GESTURE_NOTIFY_OPEN_PAGE_WINDOW) {
            airmouse_runtime_gesture_state_open_page_window(
                &s_airmouse_runtime_gesture_state,
                esp_timer_get_time(),
                CONFIG_AIRMOUSE_GESTURE_SPACE_SWITCH_WINDOW_MS);
            if (space_switch_detector != NULL) {
                (void)imu_gesture_detector_reset(space_switch_detector);
            }
            ESP_LOGI(TAG, "space switch business window opened");
        }

        airmouse_imu_sample_t sample = {};
        if (!airmouse_ble_mouse_ready(ble_handle)) {
            if (!inactive_reset_done) {
                airmouse_reset_runtime_gesture_pipeline(airmouse_ctx);
                inactive_reset_done = true;
                active_logged = false;
            }
            vTaskDelay(pdMS_TO_TICKS(20));
            continue;
        }

        if (knob_detector == NULL || space_switch_detector == NULL) {
            if (!inactive_reset_done) {
                airmouse_reset_runtime_gesture_pipeline(airmouse_ctx);
                inactive_reset_done = true;
                active_logged = false;
            }
            vTaskDelay(pdMS_TO_TICKS(20));
            continue;
        }

        inactive_reset_done = false;
        if (!active_logged) {
            ESP_LOGI(TAG, "runtime gesture active: mouse ready");
            active_logged = true;
        }

        if (xQueueReceive(s_airmouse_gesture_imu_queue,
                          &sample,
                          pdMS_TO_TICKS(20)) != pdPASS) {
            continue;
        }

        imu_gesture_sample_t runtime_sample = {
            .accel = {sample.accel[0], sample.accel[1], sample.accel[2]},
            .gyro = {sample.gyr[0], sample.gyr[1], sample.gyr[2]},
            .timestamp_us = sample.timestamp_us,
        };

        size_t processed = 0;
        airmouse_runtime_gesture_state_mark_event_pending(
            &s_airmouse_runtime_gesture_state);

        esp_err_t process_ret = ESP_ERR_INVALID_STATE;
        if (space_switch_detector != NULL) {
            process_ret = imu_gesture_detector_push_sample(
                              space_switch_detector, &runtime_sample);
            if (process_ret == ESP_OK) {
                process_ret = imu_gesture_detector_process_pending(
                                  space_switch_detector, &processed);
            }
            if (process_ret != ESP_OK) {
                ESP_LOGW(TAG, "space-switch process pending failed: 0x%x",
                         (unsigned int)process_ret);
            }
        }

        if (!airmouse_runtime_gesture_state_event_was_dispatched(
                    &s_airmouse_runtime_gesture_state) &&
                knob_detector != NULL) {
            processed = 0;
            process_ret = imu_gesture_detector_push_sample(knob_detector,
                                                           &runtime_sample);
            if (process_ret == ESP_OK) {
                process_ret = imu_gesture_detector_process_pending(
                                  knob_detector, &processed);
            }
            if (process_ret != ESP_OK) {
                ESP_LOGW(TAG, "knob process pending failed: 0x%x",
                         (unsigned int)process_ret);
            }
        }
    }
}

#if CONFIG_AIRMOUSE_GESTURE_INFER_ENABLE
void airmouse_inference_task(void *pvParameters)
{
    if (pvParameters == NULL) {
        ESP_LOGE(TAG, "inference task: pvParameters is NULL");
        vTaskDelete(NULL);
        return;
    }

    airmouse_function_handle_t *airmouse_ctx =
        (airmouse_function_handle_t *)pvParameters;

    if (airmouse_ctx->ble_handle == NULL) {
        ESP_LOGE(TAG, "inference task: invalid context handle");
        vTaskDelete(NULL);
        return;
    }

    airmouse_ble_handle_t *ble_handle = airmouse_ctx->ble_handle;

    if (s_airmouse_inference_imu_queue == NULL) {
        ESP_LOGE(TAG, "inference task: imu queue is NULL");
        vTaskDelete(NULL);
        return;
    }

    s_airmouse_inference_task_handle = xTaskGetCurrentTaskHandle();

    bool inactive_reset_done = false;
    bool active_logged = false;

    ESP_LOGI(TAG, "inference task started");

    while (1) {
        uint32_t notify_bits = 0;
        (void)xTaskNotifyWait(0, UINT32_MAX, &notify_bits, 0);

        if (notify_bits & AIRMOUSE_INFERENCE_NOTIFY_SINGLE_SHOT_START) {
            imu_gesture_detector_handle_t inference_detector =
                airmouse_ctx->inference_detector;
            if (!s_airmouse_gesture_infer_state.single_shot_capture_active &&
                    !s_airmouse_gesture_infer_state.single_shot_draining &&
                    s_airmouse_gesture_infer_state.enabled) {
                airmouse_gesture_infer_state_reset(&s_airmouse_gesture_infer_state);
                s_airmouse_gesture_infer_state.single_shot_capture_active = true;
                if (inference_detector != NULL) {
                    (void)imu_gesture_detector_reset(inference_detector);
                }
                ESP_LOGI(TAG, "single-shot inference capture started");
            }
        }

        if (notify_bits & AIRMOUSE_INFERENCE_NOTIFY_SINGLE_SHOT_FINISH) {
            imu_gesture_detector_handle_t inference_detector =
                airmouse_ctx->inference_detector;
            if (s_airmouse_gesture_infer_state.single_shot_capture_active) {
                s_airmouse_gesture_infer_state.single_shot_capture_active = false;
                s_airmouse_gesture_infer_state.single_shot_draining = true;

                if (s_airmouse_gesture_infer_state.single_shot_buffer_count >=
                        AIRMOUSE_GESTURE_MODEL_INPUT_LENGTH) {
                    if (inference_detector != NULL) {
                        esp_err_t single_shot_ret =
                            imu_gesture_inference_detector_process_single_shot_buffer(
                                inference_detector,
                                s_airmouse_gesture_infer_state.single_shot_buffer,
                                AIRMOUSE_GESTURE_MODEL_INPUT_LENGTH);
                        if (single_shot_ret != ESP_OK) {
                            ESP_LOGW(TAG,
                                     "single-shot inference buffer process failed: 0x%x",
                                     (unsigned int)single_shot_ret);
                        }
                    } else {
                        ESP_LOGW(TAG,
                                 "single-shot inference skipped: detector disabled");
                    }
                } else {
                    ESP_LOGW(TAG,
                             "single-shot inference skipped: samples not enough (%d/%d)",
                             (int)s_airmouse_gesture_infer_state.single_shot_buffer_count,
                             AIRMOUSE_GESTURE_MODEL_INPUT_LENGTH);
                }

                if (inference_detector != NULL) {
                    (void)imu_gesture_detector_reset(inference_detector);
                }
                airmouse_gesture_infer_state_reset(&s_airmouse_gesture_infer_state);
            }
        }

        if (notify_bits & AIRMOUSE_INFERENCE_NOTIFY_RESET) {
            airmouse_reset_inference_pipeline(airmouse_ctx, false);
        }

        if (!airmouse_ble_mouse_ready(ble_handle)) {
            if (!inactive_reset_done) {
                airmouse_reset_inference_pipeline(airmouse_ctx, true);
                inactive_reset_done = true;
                active_logged = false;
            }
            vTaskDelay(pdMS_TO_TICKS(20));
            continue;
        }

        imu_gesture_detector_handle_t inference_detector =
            airmouse_ctx->inference_detector;
        if (!s_airmouse_gesture_infer_state.enabled || inference_detector == NULL) {
            if (!inactive_reset_done) {
                airmouse_reset_inference_pipeline(airmouse_ctx, true);
                inactive_reset_done = true;
                active_logged = false;
            }
            vTaskDelay(pdMS_TO_TICKS(20));
            continue;
        }

        inactive_reset_done = false;
        if (!active_logged) {
            ESP_LOGI(TAG, "inference task active: mouse ready");
            active_logged = true;
        }

        airmouse_imu_sample_t sample = {};
        if (xQueueReceive(s_airmouse_inference_imu_queue,
                          &sample,
                          pdMS_TO_TICKS(20)) != pdPASS) {
            continue;
        }

        if (s_airmouse_gesture_infer_state.single_shot_draining) {
            continue;
        }

        imu_gesture_sample_t inference_sample = {
            .accel = {sample.accel[0], sample.accel[1], sample.accel[2]},
            .gyro = {sample.gyr[0], sample.gyr[1], sample.gyr[2]},
            .timestamp_us = sample.timestamp_us,
        };

        size_t processed = 0;
        esp_err_t process_ret = ESP_OK;
        if (s_airmouse_gesture_infer_state.single_shot_capture_active) {
            if (s_airmouse_gesture_infer_state.enabled &&
                    s_airmouse_gesture_infer_state.single_shot_buffer_count <
                    AIRMOUSE_GESTURE_MODEL_INPUT_LENGTH &&
                    s_airmouse_gesture_infer_state.sample_divider > 0 &&
                    ((s_airmouse_gesture_infer_state.single_shot_sample_index++ %
                      s_airmouse_gesture_infer_state.sample_divider) == 0)) {
                s_airmouse_gesture_infer_state
                .single_shot_buffer[s_airmouse_gesture_infer_state
                                    .single_shot_buffer_count++] =
                                        inference_sample;
            }
        } else if (s_airmouse_gesture_infer_state.enabled &&
                   s_airmouse_gesture_infer_state.sample_divider > 0 &&
                   ((s_airmouse_gesture_infer_state.gesture_sample_index++ %
                     s_airmouse_gesture_infer_state.sample_divider) == 0)) {
            process_ret = imu_gesture_detector_push_sample(
                              inference_detector, &inference_sample);
            if (process_ret == ESP_OK) {
                process_ret = imu_gesture_detector_process_pending(
                                  inference_detector, &processed);
            }
            if (process_ret != ESP_OK) {
                ESP_LOGW(TAG,
                         "realtime inference process pending failed: 0x%x",
                         (unsigned int)process_ret);
            }
        }
    }
}
#endif

void airmouse_runtime_config_task(void *pvParameters)
{
    if (pvParameters == NULL) {
        ESP_LOGE(TAG, "runtime config task: pvParameters is NULL");
        vTaskDelete(NULL);
        return;
    }

    airmouse_function_handle_t *airmouse_ctx =
        (airmouse_function_handle_t *)pvParameters;

    if (airmouse_ctx->ble_handle == NULL) {
        ESP_LOGE(TAG, "runtime config task: invalid context handle");
        vTaskDelete(NULL);
        return;
    }

    s_airmouse_runtime_config_task_handle = xTaskGetCurrentTaskHandle();

    ESP_LOGI(TAG, "runtime config task started");

    while (1) {
        uint32_t notify_bits = 0;
        (void)xTaskNotifyWait(0, UINT32_MAX, &notify_bits, portMAX_DELAY);

        if ((notify_bits & AIRMOUSE_RUNTIME_CONFIG_NOTIFY_ENTER) == 0) {
            continue;
        }

        if (airmouse_ctx->ble_handle->runtime_config_mode &&
                !s_airmouse_runtime_config_request_pending) {
            ESP_LOGW(TAG, "runtime config request ignored: already active");
            continue;
        }

        s_airmouse_runtime_config_request_pending = false;
        airmouse_config_t old_config = airmouse_ctx->config;
        bool wifi_started = false;
        bool http_started = false;

        ESP_LOGI(TAG, "enter runtime config mode");

        airmouse_ctx->ble_handle->runtime_config_mode = true;
        airmouse_http_runtime_notice_set(
            "runtime-config",
            "info",
            "Runtime config mode is active. Mouse movement is paused until config is submitted.",
            true);
        airmouse_runtime_gesture_state_reset(&s_airmouse_runtime_gesture_state);
        if (s_airmouse_mouse_event_queue != NULL) {
            xQueueReset(s_airmouse_mouse_event_queue);
        }
        airmouse_inference_dispatch_state_reset(
            &s_airmouse_inference_dispatch_state);

        vTaskDelay(pdMS_TO_TICKS(60));

        esp_err_t ret = airmouse_ble_deinit();
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "runtime config: BLE deinit reported: 0x%x",
                     (unsigned int)ret);
        }

        ret = airmouse_wifi_init_sta();
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "runtime config: wifi init failed: 0x%x",
                     (unsigned int)ret);
            goto runtime_config_cleanup;
        }
        wifi_started = true;

        ret = airmouse_http_server_start(&airmouse_ctx->config);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "runtime config: http server start failed: 0x%x",
                     (unsigned int)ret);
            goto runtime_config_cleanup;
        }
        http_started = true;

        ret = airmouse_http_server_wait_config_done(portMAX_DELAY);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "runtime config: wait config done failed: 0x%x",
                     (unsigned int)ret);
            goto runtime_config_cleanup;
        }

runtime_config_cleanup:
        if (http_started) {
            esp_err_t stop_ret = airmouse_http_server_stop();
            if (stop_ret != ESP_OK) {
                ESP_LOGW(TAG, "runtime config: http server stop failed: 0x%x",
                         (unsigned int)stop_ret);
            }
        }
        if (wifi_started) {
            vTaskDelay(pdMS_TO_TICKS(40));
            esp_err_t wifi_ret = airmouse_wifi_deinit_sta();
            if (wifi_ret != ESP_OK) {
                ESP_LOGW(TAG, "runtime config: wifi deinit failed: 0x%x",
                         (unsigned int)wifi_ret);
            }
        }

        uint32_t change_flags = airmouse_runtime_config_get_change_flags(
                                    &old_config, &airmouse_ctx->config);

        if ((change_flags & AIRMOUSE_CONFIG_CHANGE_HARDWARE) != 0) {
            ESP_LOGW(TAG,
                     "runtime config rejected hardware changes; reboot is required");
            airmouse_ctx->config.hardware = old_config.hardware;
        }

        if ((change_flags & AIRMOUSE_CONFIG_CHANGE_KNOB) != 0) {
            ret = airmouse_gesture_recreate_knob_detector(
                      &airmouse_ctx->config.knob,
                      &airmouse_ctx->knob_detector,
                      airmouse_runtime_gesture_event_cb,
                      NULL);
            if (ret != ESP_OK) {
                ESP_LOGW(TAG,
                         "runtime config failed to apply knob changes: 0x%x; rollback",
                         (unsigned int)ret);
                airmouse_ctx->config.knob = old_config.knob;
            } else {
                s_airmouse_knob_detector = airmouse_ctx->knob_detector;
                ESP_LOGI(TAG, "runtime config applied: knob detector recreated");
            }
        }

        if ((change_flags & AIRMOUSE_CONFIG_CHANGE_SPACE_SWITCH) != 0) {
            ret = airmouse_gesture_recreate_space_switch_detector(
                      &airmouse_ctx->config.space_switch,
                      &airmouse_ctx->space_switch_detector,
                      airmouse_runtime_gesture_event_cb,
                      NULL);
            if (ret != ESP_OK) {
                ESP_LOGW(TAG,
                         "runtime config failed to apply space-switch changes: 0x%x; rollback",
                         (unsigned int)ret);
                airmouse_ctx->config.space_switch = old_config.space_switch;
            } else {
                s_airmouse_space_switch_detector =
                    airmouse_ctx->space_switch_detector;
                ESP_LOGI(TAG,
                         "runtime config applied: space-switch detector recreated");
            }
        }

#if CONFIG_AIRMOUSE_GESTURE_INFER_ENABLE
        if ((change_flags & AIRMOUSE_CONFIG_CHANGE_INFERENCE) != 0) {
            bool detector_enabled = false;

            ret = airmouse_gesture_infer_recreate_detector(
                      &airmouse_ctx->config.inference,
                      &s_airmouse_gesture_infer_state,
                      &airmouse_ctx->inference_detector,
                      airmouse_inference_event_cb,
                      NULL,
                      &detector_enabled);
            if (ret != ESP_OK) {
                ESP_LOGW(TAG,
                         "runtime config failed to apply inference changes: 0x%x; rollback",
                         (unsigned int)ret);
                airmouse_ctx->config.inference = old_config.inference;
            } else {
                ESP_LOGI(TAG,
                         "runtime config applied: inference detector %s",
                         detector_enabled ? "recreated" : "disabled");
            }
        }
#endif

        change_flags = airmouse_runtime_config_get_change_flags(
                           &old_config, &airmouse_ctx->config);

        if ((change_flags & (AIRMOUSE_CONFIG_CHANGE_POSE |
                             AIRMOUSE_CONFIG_CHANGE_KNOB |
                             AIRMOUSE_CONFIG_CHANGE_SPACE_SWITCH |
                             AIRMOUSE_CONFIG_CHANGE_INFERENCE)) != 0) {
            airmouse_ctx->pose_reconfigure_pending = true;
            xSemaphoreGive(airmouse_ctx->pose_reset_sem);
            ESP_LOGI(TAG, "runtime config applied, reset active modules");
        } else {
            ESP_LOGI(TAG, "runtime config finished with no active module change");
        }

        airmouse_ctx->ble_handle->runtime_config_mode = false;
        s_airmouse_runtime_config_request_pending = false;
        airmouse_http_runtime_notice_set("", "", "", false);
        ret = airmouse_ble_init();
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "runtime config: BLE reinit reported: 0x%x",
                     (unsigned int)ret);
        }
        ESP_LOGI(TAG, "exit runtime config mode");
    }
}

void airmouse_bmi270_sampling_task(void *pvParameters)
{
    if (pvParameters == NULL) {
        ESP_LOGE(TAG, "sampling task: pvParameters is NULL");
        vTaskDelete(NULL);
        return;
    }

    airmouse_function_handle_t *airmouse_ctx =
        (airmouse_function_handle_t *)pvParameters;

    if (airmouse_ctx->imu_handle == NULL || airmouse_ctx->ble_handle == NULL) {
        ESP_LOGE(TAG, "sampling task: invalid context handle");
        vTaskDelete(NULL);
        return;
    }

    airmouse_bmi270_handle_t *imu_handle = airmouse_ctx->imu_handle;
    airmouse_ble_handle_t *ble_handle = airmouse_ctx->ble_handle;
#if CONFIG_AIRMOUSE_ENABLE_BMM350
    airmouse_bmm350_handle_t *bmm_handle = airmouse_ctx->bmm_handle;
#endif
    QueueHandle_t imu_queue = airmouse_get_imu_queue();

    if (imu_queue == NULL) {
        ESP_LOGE(TAG, "sampling task: imu queue is NULL");
        vTaskDelete(NULL);
        return;
    }

    const TickType_t xPeriod =
        pdMS_TO_TICKS(CONFIG_AIRMOUSE_IMU_SAMPLE_INTERVAL_MS);
    TickType_t xLastWakeTime = xTaskGetTickCount();
    vTaskDelay(pdMS_TO_TICKS(2000));
    float accel[3];
    float gyr[3];
    while (airmouse_bmi270_read_imu_once(imu_handle, accel, gyr) != ESP_OK) {
        vTaskDelay(pdMS_TO_TICKS(2000));
    }

    ESP_LOGI(TAG, "IMU sampling task started, period = %d ms",
             CONFIG_AIRMOUSE_IMU_SAMPLE_INTERVAL_MS);
    ESP_LOGI(TAG, "portTICK_PERIOD_MS = %d ms, xPeriod ticks = %d",
             portTICK_PERIOD_MS, (int)xPeriod);

    while (1) {
        if (airmouse_ble_mouse_ready(ble_handle)) {
            airmouse_imu_sample_t sample = {};

            esp_err_t ret = airmouse_bmi270_read_imu_once(
                                imu_handle,
                                sample.accel,
                                sample.gyr
                            );

#if CONFIG_AIRMOUSE_ENABLE_BMM350
            sample.mag_valid = false;
            if (bmm_handle != NULL && airmouse_bmm350_is_calibration_valid(bmm_handle)) {
                float raw_mag[3] = {};
                float calibrated_mag[3] = {};
                uint8_t drdy_status = 0;
                struct bmm350_mag_temp_data mag_temp_data = {};
                int8_t bmm_rslt =
                    bmm350_get_interrupt_status(&drdy_status, &bmm_handle->bmm_dev);
                if (bmm_rslt == BMM350_OK && drdy_status != 0) {
                    bmm_rslt = bmm350_get_compensated_mag_xyz_temp_data(
                                   &mag_temp_data, &bmm_handle->bmm_dev);
                }

                if (bmm_rslt == BMM350_OK && drdy_status != 0) {
                    raw_mag[0] =  mag_temp_data.x;
                    raw_mag[1] = -mag_temp_data.y;
                    raw_mag[2] = -mag_temp_data.z;

                    esp_err_t cal_ret =
                        airmouse_bmm350_apply_calibration(bmm_handle,
                                                          raw_mag,
                                                          calibrated_mag);
                    if (cal_ret == ESP_OK) {
                        sample.mag[0] = calibrated_mag[0];
                        sample.mag[1] = calibrated_mag[1];
                        sample.mag[2] = calibrated_mag[2];
                        sample.mag_valid = true;
                    }
                } else if (bmm_rslt != BMM350_OK) {
                    int64_t now_us = esp_timer_get_time();
                    if ((now_us - bmm_handle->last_error_log_us) >= 1000000) {
                        ESP_LOGW(TAG, "BMM350 runtime read failed: %d", bmm_rslt);
                        bmm_handle->last_error_log_us = now_us;
                    }
                }
            }
#endif

            if (ret == ESP_OK) {
                sample.timestamp_us = esp_timer_get_time();  // Timestamp the sample when the read completes.

                BaseType_t qret = xQueueSend(imu_queue, &sample, 0);

                if (qret != pdPASS) {
                    airmouse_imu_sample_t dropped_sample;
                    BaseType_t pop_ret = xQueueReceive(imu_queue, &dropped_sample, 0);
                    if (pop_ret == pdPASS) {
                        qret = xQueueSend(imu_queue, &sample, 0);
                    }

                    if (qret == pdPASS) {
                        ESP_LOGW(TAG, "sampling: queue full, dropped oldest sample");
                    }
                }

                if (s_airmouse_gesture_imu_queue != NULL) {
                    BaseType_t iqret = xQueueSend(s_airmouse_gesture_imu_queue, &sample, 0);
                    if (iqret != pdPASS) {
                        airmouse_imu_sample_t dropped_sample;
                        if (xQueueReceive(s_airmouse_gesture_imu_queue, &dropped_sample, 0) == pdPASS) {
                            (void)xQueueSend(s_airmouse_gesture_imu_queue, &sample, 0);
                            ESP_LOGW(TAG,
                                     "gesture sampling: queue full, dropped oldest sample");
                        }
                    }
                }

                if (s_airmouse_inference_imu_queue != NULL) {
                    BaseType_t iqret = xQueueSend(s_airmouse_inference_imu_queue, &sample, 0);
                    if (iqret != pdPASS) {
                        airmouse_imu_sample_t dropped_sample;
                        if (xQueueReceive(s_airmouse_inference_imu_queue, &dropped_sample, 0) == pdPASS) {
                            (void)xQueueSend(s_airmouse_inference_imu_queue, &sample, 0);
                        }
                    }
                }
            }
        } else {
            airmouse_reset_sampling_queues();
        }
        vTaskDelayUntil(&xLastWakeTime, xPeriod);
    }
}

void airmouse_pose_mapping_quaternion_task(void *pvParameters)
{
    if (pvParameters == nullptr) {
        ESP_LOGE(TAG, "pose task: pvParameters is null");
        vTaskDelete(NULL);
        return;
    }

    airmouse_function_handle_t *airmouse_ctx =
        (airmouse_function_handle_t *)pvParameters;

    if (airmouse_ctx->ble_handle == nullptr || airmouse_ctx->pose_handle == nullptr ||
            airmouse_ctx->btn_handle == nullptr) {
        ESP_LOGE(TAG, "pose task: invalid context handle");
        vTaskDelete(NULL);
        return;
    }

    airmouse_ble_handle_t *ble_handle = airmouse_ctx->ble_handle;
    airmouse_active_pose_handle_t *pose_handle = airmouse_ctx->pose_handle;

    QueueHandle_t imu_queue = airmouse_get_imu_queue();
    if (imu_queue == NULL) {
        ESP_LOGE(TAG, "pose task: imu queue is null");
        vTaskDelete(NULL);
        return;
    }

    if (airmouse_get_mouse_event_queue() == NULL) {
        ESP_LOGE(TAG, "pose task: mouse event queue is null");
        vTaskDelete(NULL);
        return;
    }

    vTaskDelay(pdMS_TO_TICKS(2000));
    airmouse_pose_mapping_quaternion_init(pose_handle,
                                          &airmouse_ctx->config.pose);

    bool inactive_reset_done = false;
    bool active_logged = false;
    bool last_mouse_ready = false;
#if CONFIG_AIRMOUSE_ENABLE_BMM350
    bool quat_mag_path_logged = false;
    int64_t last_quat_mag_missing_log_us = 0;
#endif

    while (1) {
        if (xSemaphoreTake(airmouse_ctx->pose_reset_sem, 0) == pdTRUE) {
            const bool reconfigure_pose = airmouse_ctx->pose_reconfigure_pending;
            airmouse_ctx->pose_reconfigure_pending = false;
            ESP_LOGI(TAG,
                     "pose task: clear imu queue and %s",
                     reconfigure_pose ? "reinitialize pose config" : "request pose recenter");

            airmouse_reset_sampling_queues();

            if (reconfigure_pose) {
                airmouse_pose_mapping_quaternion_init(
                    pose_handle,
                    &airmouse_ctx->config.pose);
            } else {
                (void)airmouse_pose_mapping_request_recenter(pose_handle);
            }
            airmouse_reset_pose_related_pipelines(airmouse_ctx);

            inactive_reset_done = false;
            last_mouse_ready = false;
            continue;
        }

        const bool mouse_ready = airmouse_ble_mouse_ready(ble_handle);
        if (!mouse_ready) {
            if (!inactive_reset_done) {
                (void)airmouse_pose_mapping_request_recenter(pose_handle);
                airmouse_reset_pose_related_pipelines(airmouse_ctx);
                inactive_reset_done = true;
                active_logged = false;
            }
            last_mouse_ready = false;
            vTaskDelay(pdMS_TO_TICKS(20));
            continue;
        }

        if (!last_mouse_ready) {
            xQueueReset(imu_queue);
            (void)airmouse_pose_mapping_request_recenter(pose_handle);
            ESP_LOGI(TAG, "pose task: BLE mouse ready, wait next sample to recenter");
        }
        last_mouse_ready = true;

        inactive_reset_done = false;
        if (!active_logged) {
            ESP_LOGI(TAG, "pose mapping active: mouse ready");
            active_logged = true;
        }

        airmouse_imu_sample_t sample = {};
        BaseType_t qret = xQueueReceive(imu_queue, &sample, pdMS_TO_TICKS(20));
        if (qret != pdPASS) {
            continue;
        }

        int64_t begin_us = esp_timer_get_time();

        esp_err_t ret = ESP_OK;

        airmouse_mouse_event_t evt = {};
        bool should_post_event = false;

        airmouse_pose_mapping_cursor_output_t cursor_out = {};
#if CONFIG_AIRMOUSE_ENABLE_BMM350
        const bool quat_mag_valid =
            (airmouse_ctx->bmm_handle != NULL) && sample.mag_valid;
        if (quat_mag_valid && !quat_mag_path_logged) {
            ESP_LOGI(TAG, "pose mapping: magnetic sample path active");
            quat_mag_path_logged = true;
        } else if (airmouse_ctx->bmm_handle != NULL && !quat_mag_valid) {
            const int64_t now_us = esp_timer_get_time();
            if ((now_us - last_quat_mag_missing_log_us) >= 1000000) {
                ESP_LOGW(TAG,
                         "pose mapping: BMM350 available but current sample has no valid magnetic data");
                last_quat_mag_missing_log_us = now_us;
            }
        }

        ret = airmouse_pose_mapping_update_cursor_from_sample(
                  pose_handle,
                  sample.accel,
                  sample.gyr,
                  quat_mag_valid,
                  quat_mag_valid ? sample.mag : NULL,
                  sample.timestamp_us,
                  &cursor_out);
#else
        ret = airmouse_pose_mapping_update_cursor_from_sample(
                  pose_handle,
                  sample.accel,
                  sample.gyr,
                  false,
                  NULL,
                  sample.timestamp_us,
                  &cursor_out);
#endif
        if (ret == ESP_OK && cursor_out.has_relative) {
            evt.type = AIRMOUSE_MOUSE_EVENT_MOVE;
            evt.data.move.dx = cursor_out.dx;
            evt.data.move.dy = cursor_out.dy;
            should_post_event = true;
        } else if (ret == ESP_OK && cursor_out.has_absolute) {
            evt.type = AIRMOUSE_MOUSE_EVENT_ABS_MOVE;
            evt.data.abs_move.x = cursor_out.abs_x;
            evt.data.abs_move.y = cursor_out.abs_y;
            should_post_event = true;
        }

        if (ret == ESP_OK) {
            if (should_post_event) {
                airmouse_post_mouse_event(&evt);
            }
        } else {
            int64_t end_us = esp_timer_get_time();
            ESP_LOGW(TAG, "update current pose failed, pose exec time = %lld us (%.3f ms)",
                     end_us - begin_us,
                     (end_us - begin_us) / 1000.0f);
        }

    }
}

void airmouse_mouse_task(void *pvParameters)
{
    if (pvParameters == NULL) {
        ESP_LOGE(TAG, "mouse task: pvParameters is NULL");
        vTaskDelete(NULL);
        return;
    }

    airmouse_function_handle_t *airmouse_ctx =
        (airmouse_function_handle_t *)pvParameters;

    if (airmouse_ctx->ble_handle == NULL || airmouse_ctx->pose_reset_sem == NULL) {
        ESP_LOGE(TAG, "mouse task: invalid context handle");
        vTaskDelete(NULL);
        return;
    }

    airmouse_ble_handle_t *ble_handle = airmouse_ctx->ble_handle;

    QueueHandle_t mouse_event_queue = airmouse_get_mouse_event_queue();
    if (mouse_event_queue == NULL) {
        ESP_LOGE(TAG, "mouse task: mouse event queue is NULL");
        vTaskDelete(NULL);
        return;
    }

    bool left_button_held = false;
    bool active_logged = false;

    ESP_LOGI(TAG, "mouse task started");

    while (1) {
        airmouse_mouse_event_t evt;
        BaseType_t qret = xQueueReceive(mouse_event_queue, &evt, pdMS_TO_TICKS(20));
        if (qret != pdPASS) {
            if (!airmouse_ble_mouse_ready(ble_handle)) {
                left_button_held = false;
                active_logged = false;
            }
            continue;
        }
        if (!airmouse_ble_mouse_ready(ble_handle)) {
            left_button_held = false;
            active_logged = false;
            continue;
        }

        if (!active_logged) {
            ESP_LOGI(TAG, "mouse task active: mouse ready");
            active_logged = true;
        }

        airmouse_hid_dispatch_event(
            airmouse_ctx,
            &evt,
            &left_button_held,
            &s_airmouse_inference_dispatch_state);
    }
}

esp_err_t airmouse_function_prepare_resources(airmouse_function_handle_t *ctx)
{
    if (ctx == NULL) {
        ESP_LOGE(TAG, "prepare resources: ctx is NULL");
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t ret = ESP_OK;
    imu_gesture_knob_config_t knob_config = {};
    imu_gesture_space_switch_config_t space_switch_config = {};
#if CONFIG_AIRMOUSE_GESTURE_INFER_ENABLE
    imu_gesture_inference_config_t inference_config = {};
#endif

    s_airmouse_imu_queue = xQueueCreate(8, sizeof(airmouse_imu_sample_t));
    if (s_airmouse_imu_queue == NULL) {
        ESP_LOGE(TAG, "Failed to create IMU queue");
        ret = ESP_FAIL;
        goto fail;
    }

    s_airmouse_gesture_imu_queue = xQueueCreate(64, sizeof(airmouse_imu_sample_t));
    if (s_airmouse_gesture_imu_queue == NULL) {
        ESP_LOGE(TAG, "Failed to create gesture IMU queue");
        ret = ESP_FAIL;
        goto fail;
    }

    s_airmouse_inference_imu_queue = xQueueCreate(64, sizeof(airmouse_imu_sample_t));
    if (s_airmouse_inference_imu_queue == NULL) {
        ESP_LOGE(TAG, "Failed to create inference IMU queue");
        ret = ESP_FAIL;
        goto fail;
    }

    s_airmouse_mouse_event_queue = xQueueCreate(16, sizeof(airmouse_mouse_event_t));
    if (s_airmouse_mouse_event_queue == NULL) {
        ESP_LOGE(TAG, "Failed to create mouse event queue");
        ret = ESP_FAIL;
        goto fail;
    }

    ctx->pose_reset_sem = xSemaphoreCreateBinary();
    if (ctx->pose_reset_sem == NULL) {
        ESP_LOGE(TAG, "Failed to create pose reset semaphore");
        ret = ESP_FAIL;
        goto fail;
    }

#if CONFIG_AIRMOUSE_GESTURE_INFER_ENABLE
    airmouse_gesture_infer_state_init(&s_airmouse_gesture_infer_state,
                                      &ctx->config.inference);
    if (s_airmouse_gesture_infer_state.enabled &&
            !airmouse_gesture_infer_build_detector_config(
                &ctx->config.inference,
                &inference_config)) {
        s_airmouse_gesture_infer_state.enabled = false;
    }
#endif

    if (!airmouse_gesture_build_knob_detector_config(&ctx->config.knob,
                                                     &knob_config)) {
        ESP_LOGE(TAG, "Failed to build knob detector config");
        ret = ESP_ERR_INVALID_ARG;
        goto fail;
    }

    if (!airmouse_gesture_build_space_switch_detector_config(
                &ctx->config.space_switch, &space_switch_config)) {
        ESP_LOGE(TAG, "Failed to build space-switch detector config");
        ret = ESP_ERR_INVALID_ARG;
        goto fail;
    }

    ret = imu_gesture_knob_detector_create(
              &knob_config, &s_airmouse_knob_detector);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize knob detector");
        goto fail;
    }

    ret = imu_gesture_space_switch_detector_create(
              &space_switch_config, &s_airmouse_space_switch_detector);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize space-switch detector");
        goto fail;
    }

#if CONFIG_AIRMOUSE_GESTURE_INFER_ENABLE
    if (s_airmouse_gesture_infer_state.enabled) {
        ret = imu_gesture_inference_detector_create(
                  &inference_config, &ctx->inference_detector);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to initialize inference detector");
            goto fail;
        }
    }
#endif

    ctx->knob_detector = s_airmouse_knob_detector;
    ctx->space_switch_detector = s_airmouse_space_switch_detector;
    airmouse_runtime_gesture_state_init(&s_airmouse_runtime_gesture_state);
    airmouse_inference_dispatch_state_init(&s_airmouse_inference_dispatch_state);

    ret = imu_gesture_detector_register_cb(ctx->knob_detector,
                                           airmouse_runtime_gesture_event_cb,
                                           NULL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register knob detector callback");
        goto fail;
    }

    ret = imu_gesture_detector_register_cb(ctx->space_switch_detector,
                                           airmouse_runtime_gesture_event_cb,
                                           NULL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register space-switch detector callback");
        goto fail;
    }

#if CONFIG_AIRMOUSE_GESTURE_INFER_ENABLE
    if (ctx->inference_detector != NULL) {
        ret = imu_gesture_detector_register_cb(ctx->inference_detector,
                                               airmouse_inference_event_cb,
                                               NULL);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to register inference detector callback");
            goto fail;
        }
    }
#endif

    (void)imu_gesture_detector_reset(ctx->knob_detector);
    (void)imu_gesture_detector_reset(ctx->space_switch_detector);
    airmouse_runtime_gesture_state_reset(&s_airmouse_runtime_gesture_state);
#if CONFIG_AIRMOUSE_GESTURE_INFER_ENABLE
    if (ctx->inference_detector != NULL) {
        (void)imu_gesture_detector_reset(ctx->inference_detector);
    }
    airmouse_gesture_infer_state_reset(&s_airmouse_gesture_infer_state);
    airmouse_inference_dispatch_state_reset(&s_airmouse_inference_dispatch_state);
    s_airmouse_inference_task_handle = NULL;
#endif
    s_airmouse_runtime_gesture_task_handle = NULL;
    s_airmouse_runtime_config_task_handle = NULL;

    return ESP_OK;

fail:
#if CONFIG_AIRMOUSE_GESTURE_INFER_ENABLE
    if (ctx->inference_detector != NULL) {
        (void)imu_gesture_detector_del(ctx->inference_detector);
        ctx->inference_detector = NULL;
    }
#endif
    if (s_airmouse_space_switch_detector != NULL) {
        (void)imu_gesture_detector_del(s_airmouse_space_switch_detector);
        s_airmouse_space_switch_detector = NULL;
    }
    if (s_airmouse_knob_detector != NULL) {
        (void)imu_gesture_detector_del(s_airmouse_knob_detector);
        s_airmouse_knob_detector = NULL;
    }

    ctx->knob_detector = NULL;
    ctx->space_switch_detector = NULL;

    if (ctx->pose_reset_sem != NULL) {
        vSemaphoreDelete(ctx->pose_reset_sem);
        ctx->pose_reset_sem = NULL;
    }
    if (s_airmouse_mouse_event_queue != NULL) {
        vQueueDelete(s_airmouse_mouse_event_queue);
        s_airmouse_mouse_event_queue = NULL;
    }
    if (s_airmouse_inference_imu_queue != NULL) {
        vQueueDelete(s_airmouse_inference_imu_queue);
        s_airmouse_inference_imu_queue = NULL;
    }
    if (s_airmouse_gesture_imu_queue != NULL) {
        vQueueDelete(s_airmouse_gesture_imu_queue);
        s_airmouse_gesture_imu_queue = NULL;
    }
    if (s_airmouse_imu_queue != NULL) {
        vQueueDelete(s_airmouse_imu_queue);
        s_airmouse_imu_queue = NULL;
    }

    return ret;
}

esp_err_t airmouse_function_start(airmouse_function_handle_t *ctx)
{
    if (ctx == NULL) {
        ESP_LOGE(TAG, "airmouse_function_start: ctx is NULL");
        return ESP_ERR_INVALID_ARG;
    }

#if CONFIG_AIRMOUSE_GESTURE_INFER_ENABLE
    const bool infer_detector_required = s_airmouse_gesture_infer_state.enabled;
#endif
    if (ctx->ble_handle == NULL || ctx->imu_handle == NULL || ctx->pose_handle == NULL ||
            ctx->btn_handle == NULL || ctx->knob_detector == NULL ||
            ctx->space_switch_detector == NULL
#if CONFIG_AIRMOUSE_GESTURE_INFER_ENABLE
            || (infer_detector_required && ctx->inference_detector == NULL)
#endif
       ) {
        ESP_LOGE(TAG, "airmouse_function_start: invalid sub-handle");
        return ESP_ERR_INVALID_ARG;
    }

    (void)imu_gesture_detector_reset(ctx->knob_detector);
    (void)imu_gesture_detector_reset(ctx->space_switch_detector);
    airmouse_runtime_gesture_state_reset(&s_airmouse_runtime_gesture_state);
#if CONFIG_AIRMOUSE_GESTURE_INFER_ENABLE
    if (ctx->inference_detector != NULL) {
        (void)imu_gesture_detector_reset(ctx->inference_detector);
    }
    airmouse_gesture_infer_state_reset(&s_airmouse_gesture_infer_state);
    airmouse_inference_dispatch_state_reset(&s_airmouse_inference_dispatch_state);
    s_airmouse_inference_task_handle = NULL;
#endif
    s_airmouse_runtime_gesture_task_handle = NULL;
    s_airmouse_runtime_config_task_handle = NULL;

    TaskHandle_t imu_task_handle = NULL;
    TaskHandle_t pose_task_handle = NULL;
    TaskHandle_t mouse_task_handle = NULL;
    TaskHandle_t runtime_gesture_task_handle = NULL;
    TaskHandle_t runtime_config_task_handle = NULL;
#if CONFIG_AIRMOUSE_GESTURE_INFER_ENABLE
    TaskHandle_t inference_task_handle = NULL;
#endif
    esp_err_t err = ESP_FAIL;

    BaseType_t ret = xTaskCreate(
                         airmouse_bmi270_sampling_task,
                         "airmouse_imu_task",
                         4096,
                         ctx,
                         6,
                         &imu_task_handle
                     );
    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create IMU sampling task");
        goto fail_detectors_and_queues;
    }

    ret = xTaskCreate(
              airmouse_pose_mapping_quaternion_task,
              "airmouse_pose_task",
              4096,
              ctx,
              5,
              &pose_task_handle
          );
    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create pose mapping task");
        goto fail_imu_task;
    }

    ret = xTaskCreate(
              airmouse_mouse_task,
              "airmouse_mouse_task",
              2048,
              ctx,
              5,
              &mouse_task_handle
          );
    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create mouse task");
        goto fail_pose_task;
    }

    ret = xTaskCreate(
              airmouse_runtime_gesture_task,
              "airmouse_runtime_gesture_task",
              4096,
              ctx,
              4,
              &runtime_gesture_task_handle
          );
    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create runtime gesture task");
        goto fail_mouse_task;
    }

    ret = xTaskCreate(
              airmouse_runtime_config_task,
              "airmouse_runtime_config_task",
              4096,
              ctx,
              4,
              &runtime_config_task_handle
          );
    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create runtime config task");
        goto fail_runtime_gesture_task;
    }

#if CONFIG_AIRMOUSE_GESTURE_INFER_ENABLE
    if (ctx->inference_detector != NULL) {
        ret = xTaskCreate(
                  airmouse_inference_task,
                  "airmouse_inference_task",
                  4096,
                  ctx,
                  4,
                  &inference_task_handle
              );
        if (ret != pdPASS) {
            ESP_LOGE(TAG, "Failed to create inference task");
            goto fail_runtime_config_task;
        }
    }
#endif

    ESP_LOGI(TAG, "airmouse tasks started successfully");
    return ESP_OK;

#if CONFIG_AIRMOUSE_GESTURE_INFER_ENABLE
fail_runtime_config_task:
    if (runtime_config_task_handle != NULL) {
        vTaskDelete(runtime_config_task_handle);
        runtime_config_task_handle = NULL;
    }
#endif
fail_runtime_gesture_task:
    if (runtime_gesture_task_handle != NULL) {
        vTaskDelete(runtime_gesture_task_handle);
        runtime_gesture_task_handle = NULL;
    }
fail_mouse_task:
    if (mouse_task_handle != NULL) {
        vTaskDelete(mouse_task_handle);
        mouse_task_handle = NULL;
    }
fail_pose_task:
    if (pose_task_handle != NULL) {
        vTaskDelete(pose_task_handle);
        pose_task_handle = NULL;
    }
fail_imu_task:
    if (imu_task_handle != NULL) {
        vTaskDelete(imu_task_handle);
        imu_task_handle = NULL;
    }
fail_detectors_and_queues:
#if CONFIG_AIRMOUSE_GESTURE_INFER_ENABLE
    (void)imu_gesture_detector_del(ctx->inference_detector);
    ctx->inference_detector = NULL;
#endif
    (void)imu_gesture_detector_del(ctx->space_switch_detector);
    (void)imu_gesture_detector_del(ctx->knob_detector);
    s_airmouse_space_switch_detector = NULL;
    s_airmouse_knob_detector = NULL;
    ctx->space_switch_detector = NULL;
    ctx->knob_detector = NULL;
    if (ctx->pose_reset_sem != NULL) {
        vSemaphoreDelete(ctx->pose_reset_sem);
        ctx->pose_reset_sem = NULL;
    }
    if (s_airmouse_inference_imu_queue != NULL) {
        vQueueDelete(s_airmouse_inference_imu_queue);
        s_airmouse_inference_imu_queue = NULL;
    }
    if (s_airmouse_mouse_event_queue != NULL) {
        vQueueDelete(s_airmouse_mouse_event_queue);
        s_airmouse_mouse_event_queue = NULL;
    }
    if (s_airmouse_gesture_imu_queue != NULL) {
        vQueueDelete(s_airmouse_gesture_imu_queue);
        s_airmouse_gesture_imu_queue = NULL;
    }
    if (s_airmouse_imu_queue != NULL) {
        vQueueDelete(s_airmouse_imu_queue);
        s_airmouse_imu_queue = NULL;
    }

    return err;
}
