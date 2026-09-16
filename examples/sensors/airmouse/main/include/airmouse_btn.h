/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <stdbool.h>
#include "esp_err.h"
#include "iot_button.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "airmouse_hid.h"
#include "button_gpio.h"
#include "airmouse_config.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    button_handle_t btn;
    QueueHandle_t mouse_event_queue;
    TaskHandle_t *runtime_gesture_task_handle_ref;
    TaskHandle_t *inference_task_handle_ref;
} airmouse_btn_handle_t;

airmouse_btn_handle_t *airmouse_btn_init(QueueHandle_t mouse_event_queue,
                                         const airmouse_hardware_config_t *hardware_config,
                                         TaskHandle_t *runtime_gesture_task_handle_ref,
                                         TaskHandle_t *inference_task_handle_ref);

#ifdef __cplusplus
}
#endif
