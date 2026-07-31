/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "airmouse_hid.h"

#include "airmouse_ble.h"
#include "airmouse_function.h"
#include "airmouse_gesture_inference.h"
#include "esp_hidd_prf_api.h"
#include "esp_timer.h"
#include "freertos/queue.h"
#include "hid_dev.h"

static const TickType_t kKeyboardShortcutHoldTicks = pdMS_TO_TICKS(20);
static const int64_t kPageSwitchSuppressDurationUs = 2000000LL;

static void airmouse_send_keyboard_tap(
    uint16_t conn_id,
    uint8_t modifier,
    uint8_t key)
{
    uint8_t keys[1] = {key};

    esp_hidd_send_keyboard_value(conn_id, modifier, keys, 1);
    vTaskDelay(kKeyboardShortcutHoldTicks);
    esp_hidd_send_keyboard_value(conn_id, 0, NULL, 0);
}

static void airmouse_send_consumer_tap(
    uint16_t conn_id,
    uint16_t usage)
{
    esp_hidd_send_consumer_value(conn_id, usage, true);
    vTaskDelay(pdMS_TO_TICKS(20));
    esp_hidd_send_consumer_value(conn_id, usage, false);
}

void airmouse_hid_dispatch_event(
    airmouse_function_handle_t *ctx,
    const airmouse_mouse_event_t *evt,
    bool *left_button_held,
    airmouse_inference_dispatch_state_t *inference_dispatch_state)
{
    if (ctx == NULL || ctx->ble_handle == NULL || evt == NULL ||
            left_button_held == NULL) {
        return;
    }

    airmouse_ble_handle_t *ble_handle = ctx->ble_handle;

    switch (evt->type) {
    case AIRMOUSE_MOUSE_EVENT_MOVE: {
        uint8_t button_mask = *left_button_held ? 0x01 : 0x00;
        esp_hidd_send_mouse_value(ble_handle->hid_conn_id,
                                  button_mask,
                                  evt->data.move.dx,
                                  evt->data.move.dy);
        break;
    }

    case AIRMOUSE_MOUSE_EVENT_ABS_MOVE: {
        uint8_t button_mask = *left_button_held ? 0x01 : 0x00;
        esp_hidd_send_abs_mouse_value(ble_handle->hid_conn_id,
                                      button_mask,
                                      evt->data.abs_move.x,
                                      evt->data.abs_move.y);
        break;
    }

    case AIRMOUSE_MOUSE_EVENT_LEFT_CLICK:
        esp_hidd_send_mouse_value(ble_handle->hid_conn_id, 0x01, 0, 0);
        vTaskDelay(pdMS_TO_TICKS(20));
        esp_hidd_send_mouse_value(ble_handle->hid_conn_id, 0x00, 0, 0);
        break;

    case AIRMOUSE_MOUSE_EVENT_LEFT_DOWN:
        if (!*left_button_held) {
            esp_hidd_send_mouse_value(ble_handle->hid_conn_id, 0x01, 0, 0);
            *left_button_held = true;
        }
        break;

    case AIRMOUSE_MOUSE_EVENT_LEFT_UP:
        if (*left_button_held) {
            esp_hidd_send_mouse_value(ble_handle->hid_conn_id, 0x00, 0, 0);
            *left_button_held = false;
        }
        break;

    case AIRMOUSE_MOUSE_EVENT_RECENTER:
        if (*left_button_held) {
            esp_hidd_send_mouse_value(ble_handle->hid_conn_id, 0x00, 0, 0);
            *left_button_held = false;
        }

        if (airmouse_get_mouse_event_queue() != NULL) {
            xQueueReset(airmouse_get_mouse_event_queue());
        }
        esp_hidd_send_abs_mouse_value(ble_handle->hid_conn_id, 0, 16383, 16383);
        ctx->pose_reconfigure_pending = false;
        xSemaphoreGive(ctx->pose_reset_sem);
        break;

    case AIRMOUSE_MOUSE_EVENT_TASK_VIEW:
        airmouse_send_keyboard_tap(ble_handle->hid_conn_id,
                                   LEFT_GUI_KEY_MASK,
                                   HID_KEY_TAB);
        break;

    case AIRMOUSE_MOUSE_EVENT_MEDIA_PLAY_PAUSE:
        airmouse_send_consumer_tap(ble_handle->hid_conn_id,
                                   HID_CONSUMER_PLAY_PAUSE);
        break;

    case AIRMOUSE_MOUSE_EVENT_ESCAPE:
        airmouse_send_keyboard_tap(ble_handle->hid_conn_id, 0, HID_KEY_ESCAPE);
        break;

    case AIRMOUSE_MOUSE_EVENT_WINDOWS_OSK:
        airmouse_send_keyboard_tap(
            ble_handle->hid_conn_id,
            LEFT_CONTROL_KEY_MASK | LEFT_GUI_KEY_MASK,
            HID_KEY_O);
        break;

    case AIRMOUSE_MOUSE_EVENT_KNOB_CW:
        esp_hidd_send_mouse_scroll_value(ble_handle->hid_conn_id, 0x00, 1);
        break;

    case AIRMOUSE_MOUSE_EVENT_KNOB_CCW:
        esp_hidd_send_mouse_scroll_value(ble_handle->hid_conn_id, 0x00, -1);
        break;

    case AIRMOUSE_MOUSE_EVENT_SPACE_LEFT:
        airmouse_send_keyboard_tap(ble_handle->hid_conn_id,
                                   LEFT_ALT_KEY_MASK,
                                   HID_KEY_LEFT_ARROW);
        airmouse_inference_dispatch_note_page_switch(
            inference_dispatch_state,
            evt->type,
            esp_timer_get_time(),
            kPageSwitchSuppressDurationUs);
        break;

    case AIRMOUSE_MOUSE_EVENT_SPACE_RIGHT:
        airmouse_send_keyboard_tap(ble_handle->hid_conn_id,
                                   LEFT_ALT_KEY_MASK,
                                   HID_KEY_RIGHT_ARROW);
        airmouse_inference_dispatch_note_page_switch(
            inference_dispatch_state,
            evt->type,
            esp_timer_get_time(),
            kPageSwitchSuppressDurationUs);
        break;

    case AIRMOUSE_MOUSE_EVENT_SPACE_UP:
        airmouse_runtime_config_request();
        break;

    case AIRMOUSE_MOUSE_EVENT_SPACE_DOWN:
        airmouse_send_keyboard_tap(
            ble_handle->hid_conn_id,
            LEFT_CONTROL_KEY_MASK | LEFT_SHIFT_KEY_MASK,
            HID_KEY_TAB);
        airmouse_inference_dispatch_note_page_switch(
            inference_dispatch_state,
            evt->type,
            esp_timer_get_time(),
            kPageSwitchSuppressDurationUs);
        break;

    default:
        break;
    }
}
