/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct airmouse_function_handle airmouse_function_handle_t;
typedef struct airmouse_inference_dispatch_state
    airmouse_inference_dispatch_state_t;

typedef enum {
    AIRMOUSE_MOUSE_EVENT_MOVE = 0,
    AIRMOUSE_MOUSE_EVENT_ABS_MOVE,
    AIRMOUSE_MOUSE_EVENT_LEFT_CLICK,
    AIRMOUSE_MOUSE_EVENT_LEFT_DOWN,
    AIRMOUSE_MOUSE_EVENT_LEFT_UP,
    AIRMOUSE_MOUSE_EVENT_RECENTER,
    AIRMOUSE_MOUSE_EVENT_TASK_VIEW,
    AIRMOUSE_MOUSE_EVENT_MEDIA_PLAY_PAUSE,
    AIRMOUSE_MOUSE_EVENT_ESCAPE,
    AIRMOUSE_MOUSE_EVENT_WINDOWS_OSK,
    AIRMOUSE_MOUSE_EVENT_KNOB_CW,
    AIRMOUSE_MOUSE_EVENT_KNOB_CCW,
    AIRMOUSE_MOUSE_EVENT_SPACE_LEFT,
    AIRMOUSE_MOUSE_EVENT_SPACE_RIGHT,
    AIRMOUSE_MOUSE_EVENT_SPACE_UP,
    AIRMOUSE_MOUSE_EVENT_SPACE_DOWN,
} airmouse_mouse_event_type_t;

typedef struct {
    int8_t dx;
    int8_t dy;
} airmouse_mouse_move_event_t;

typedef struct {
    uint16_t x;
    uint16_t y;
} airmouse_mouse_abs_move_event_t;

typedef struct {
    airmouse_mouse_event_type_t type;
    union {
        airmouse_mouse_move_event_t move;
        airmouse_mouse_abs_move_event_t abs_move;
    } data;
} airmouse_mouse_event_t;

void airmouse_hid_dispatch_event(
    airmouse_function_handle_t *ctx,
    const airmouse_mouse_event_t *evt,
    bool *left_button_held,
    airmouse_inference_dispatch_state_t *inference_dispatch_state);

#ifdef __cplusplus
}
#endif
