/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <stdlib.h>
#include "esp_log.h"
#include "airmouse_btn.h"
#include "airmouse_function.h"
#include "airmouse_hid.h"

static const char *TAG = "AIRMOUSE_BTN";

static void airmouse_btn_send_mouse_event(airmouse_btn_handle_t *handle,
                                          airmouse_mouse_event_type_t type)
{
    if (handle == NULL || handle->mouse_event_queue == NULL) {
        return;
    }

    airmouse_mouse_event_t evt = {0};
    evt.type = type;
    airmouse_post_mouse_event(&evt);
}

static void airmouse_btn_short_press_cb(void *arg, void *data)
{
    (void)arg;
    airmouse_btn_handle_t *handle = (airmouse_btn_handle_t *)data;
    airmouse_btn_send_mouse_event(handle, AIRMOUSE_MOUSE_EVENT_LEFT_CLICK);
}

static void airmouse_btn_double_press_cb(void *arg, void *data)
{
    (void)arg;
    airmouse_btn_handle_t *handle = (airmouse_btn_handle_t *)data;
    airmouse_btn_send_mouse_event(handle, AIRMOUSE_MOUSE_EVENT_RECENTER);
}

static void airmouse_btn_multiple_press_cb(void *arg, void *data)
{
    (void)arg;
    airmouse_btn_handle_t *handle = (airmouse_btn_handle_t *)data;
    if (handle != NULL && handle->runtime_gesture_task_handle_ref != NULL &&
            *handle->runtime_gesture_task_handle_ref != NULL) {
        (void)xTaskNotify(*handle->runtime_gesture_task_handle_ref,
                          AIRMOUSE_RUNTIME_GESTURE_NOTIFY_OPEN_PAGE_WINDOW,
                          eSetBits);
    }
}

static void airmouse_btn_long_press_start_cb(void *arg, void *data)
{
    (void)arg;
    airmouse_btn_handle_t *handle = (airmouse_btn_handle_t *)data;
#if CONFIG_AIRMOUSE_GESTURE_INFER_ENABLE
    if (handle != NULL && handle->inference_task_handle_ref != NULL &&
            *handle->inference_task_handle_ref != NULL) {
        (void)xTaskNotify(*handle->inference_task_handle_ref,
                          AIRMOUSE_INFERENCE_NOTIFY_SINGLE_SHOT_START,
                          eSetBits);
    }
#else
    (void)handle;
#endif
}

static void airmouse_btn_press_up_cb(void *arg, void *data)
{
    (void)arg;
    airmouse_btn_handle_t *handle = (airmouse_btn_handle_t *)data;
#if CONFIG_AIRMOUSE_GESTURE_INFER_ENABLE
    if (handle != NULL && handle->inference_task_handle_ref != NULL &&
            *handle->inference_task_handle_ref != NULL) {
        (void)xTaskNotify(*handle->inference_task_handle_ref,
                          AIRMOUSE_INFERENCE_NOTIFY_SINGLE_SHOT_FINISH,
                          eSetBits);
    }
#else
    (void)handle;
#endif
}

airmouse_btn_handle_t *airmouse_btn_init(QueueHandle_t mouse_event_queue,
                                         const airmouse_hardware_config_t *hardware_config,
                                         TaskHandle_t *runtime_gesture_task_handle_ref,
                                         TaskHandle_t *inference_task_handle_ref)
{
    esp_err_t ret = ESP_OK;
    if (mouse_event_queue == NULL || hardware_config == NULL) {
        ESP_LOGE(TAG, "mouse_event_queue or hardware_config is NULL");
        return NULL;
    }

    airmouse_btn_handle_t *handle =
        (airmouse_btn_handle_t *)calloc(1, sizeof(airmouse_btn_handle_t));
    if (handle == NULL) {
        ESP_LOGE(TAG, "failed to allocate button handle");
        return NULL;
    }

    handle->mouse_event_queue = mouse_event_queue;
    handle->runtime_gesture_task_handle_ref = runtime_gesture_task_handle_ref;
    handle->inference_task_handle_ref = inference_task_handle_ref;
    button_config_t btn_cfg = {
        .long_press_time = 300,
    };

    button_gpio_config_t btn_gpio_cfg = {
        .gpio_num = hardware_config->reset_button,
        .active_level = 0,
    };

    ret = iot_button_new_gpio_device(&btn_cfg, &btn_gpio_cfg, &handle->btn);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "failed to create button");
        free(handle);
        return NULL;
    }

    ret = iot_button_register_cb(handle->btn,
                                 BUTTON_SINGLE_CLICK,
                                 NULL,
                                 airmouse_btn_short_press_cb,
                                 handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "failed to register short press callback");
        free(handle);
        return NULL;
    }

    ret = iot_button_register_cb(handle->btn,
                                 BUTTON_DOUBLE_CLICK,
                                 NULL,
                                 airmouse_btn_double_press_cb,
                                 handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "failed to register double press callback");
        free(handle);
        return NULL;
    }

    button_event_args_t multiple_click_args = {
        .multiple_clicks = {
            .clicks = 3,
        },
    };
    ret = iot_button_register_cb(handle->btn,
                                 BUTTON_MULTIPLE_CLICK,
                                 &multiple_click_args,
                                 airmouse_btn_multiple_press_cb,
                                 handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "failed to register triple-click callback");
        free(handle);
        return NULL;
    }

    ret = iot_button_register_cb(handle->btn,
                                 BUTTON_LONG_PRESS_START,
                                 NULL,
                                 airmouse_btn_long_press_start_cb,
                                 handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "failed to register long press start callback");
        free(handle);
        return NULL;
    }

    ret = iot_button_register_cb(handle->btn,
                                 BUTTON_PRESS_UP,
                                 NULL,
                                 airmouse_btn_press_up_cb,
                                 handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "failed to register press up callback");
        free(handle);
        return NULL;
    }

    ESP_LOGI(TAG, "button GPIO%d initialized successfully", btn_gpio_cfg.gpio_num);
    ESP_LOGI(TAG,
             "button init ok: single click=left click, double click=recenter, multiple click=space-switch window, long press=hold-to-capture single-shot inference");
    return handle;
}
