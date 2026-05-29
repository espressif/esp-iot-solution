/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * V1 envelope protocol: see ../json_format.md
 *
 * This file is the UI adapter for the OpenCode companion demo. ble_protocol.c owns the
 * JSONL protocol and permission state; this module translates those events into
 * board animations, short tip text, and one-key user decisions.
 */

#include <inttypes.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

#include "board.h"
#include "ble_protocol.h"
#include "emote_gen_player.h"
#include "gfx.h"
#include "iot_button.h"
#include "emote_demo.h"

static const char *TAG = "emote_demo";

/* Keep display strings bounded. BLE/OpenCode payloads can contain long command
 * lines, but the wearable should only render a compact, human-scannable hint. */
#define DEMO_TIP_BUF_SIZE                768
#define DEMO_MSG_QUEUE_LEN               8
#define DEMO_WORKER_TASK_STACK           8192
#define DEMO_WORKER_TASK_PRIORITY        5
#define DEMO_SESSION_STATUS_BUSY         "busy"
#define DEMO_SESSION_STATUS_IDLE         "idle"
#define DEMO_SESSION_STATUS_RETRY        "retry"
#define DEMO_TIP_TEXT_WORKING            "Working..."
#define DEMO_TIP_TEXT_RETRY              "Retry..."
#define DEMO_PERMISSION_DECISION_ONCE    "once"
#define DEMO_PERMISSION_DECISION_REJECT  "reject"

/** Tip label: offset from @ref GFX_ALIGN_TOP_MID (positive = down). */
#ifndef DEMO_TIP_LABEL_Y_OFS
#define DEMO_TIP_LABEL_Y_OFS             30
#endif

/** Emote `name` for once / single-click (asset index, e.g. smile). */
#ifndef DEMO_DECISION_NAME_ALLOW
#define DEMO_DECISION_NAME_ALLOW         "smile_05s"
#endif
/** Emote `name` for idle state after a prompt/status is cleared. */
#ifndef DEMO_IDLE_NAME
#define DEMO_IDLE_NAME                   DEMO_DECISION_NAME_ALLOW
#endif
/** Emote `name` while OpenCode is busy doing work. */
#ifndef DEMO_BUSY_NAME
#define DEMO_BUSY_NAME                   "leisure_05s_"
#endif
/** Emote `name` for reject / long-press (asset index, e.g. cry). */
#ifndef DEMO_DECISION_NAME_DENY
#define DEMO_DECISION_NAME_DENY          "cry_10s_10s"
#endif
/** Emote `name` for permission wait timeout (no key in 30s); distinct from once/reject. */
#ifndef DEMO_DECISION_NAME_TIMEOUT
#define DEMO_DECISION_NAME_TIMEOUT       "sigh_20s_20s"
#endif
/** Emote `name` while hook permission UI is shown (must exist in index). */
#ifndef DEMO_QUESTION_NAME
#define DEMO_QUESTION_NAME               "question_05s"
#endif

typedef enum {
    DEMO_MSG_ANIM_NAME = 0,
    DEMO_MSG_TIP_TEXT,
    DEMO_MSG_CLEAR_TIP,
} demo_msg_kind_t;

typedef struct {
    /* Messages are deliberately small value types so BLE/protocol callbacks can
     * enqueue UI work without owning the emote_gen_player timing directly. */
    demo_msg_kind_t kind;
    emote_gen_player_handle_t player;
    union {
        char anim_name[EMOTE_GEN_PLAYER_STR_MAX];
        char tip_text[DEMO_TIP_BUF_SIZE];
    } u;
} demo_msg_t;

static emote_gen_player_handle_t s_player;
static QueueHandle_t s_msg_queue;
static TaskHandle_t s_worker_task;

static void emote_demo_post_animation_fade(const char *name);
static void emote_demo_post_clear_tip(const char *log_context);
static void emote_demo_permission_key_event_cb(const board_touch_event_t *ev);
static void emote_demo_ui_worker_task(void *arg);
static void emote_demo_ui_worker_start_once(void);
static void emote_demo_board_touch_cb(void *user_data, const board_touch_event_t *ev);
static void emote_demo_log_animation_index(emote_gen_player_handle_t player);
static bool emote_demo_runtime_start(emote_gen_player_handle_t player);

static void emote_demo_post_animation_fade(const char *name)
{
    /* Request an animation transition asynchronously. Dropping the request on a
     * full queue is preferable to blocking the BLE/parser path. */
    if (name == NULL || s_msg_queue == NULL || s_player == NULL) {
        return;
    }
    demo_msg_t msg = { .kind = DEMO_MSG_ANIM_NAME, .player = s_player };
    (void)strncpy(msg.u.anim_name, name, sizeof(msg.u.anim_name) - 1);
    msg.u.anim_name[sizeof(msg.u.anim_name) - 1] = '\0';
    if (xQueueSend(s_msg_queue, &msg, 0) != pdTRUE) {
        ESP_LOGW(TAG, "queue full, drop anim %s", name);
    }
}

static void emote_demo_post_clear_tip(const char *log_context)
{
    if (s_msg_queue == NULL || s_player == NULL) {
        return;
    }

    demo_msg_t msg = { .kind = DEMO_MSG_CLEAR_TIP, .player = s_player };
    if (xQueueSend(s_msg_queue, &msg, 0) != pdTRUE) {
        ESP_LOGW(TAG, "tip queue full (%s)", log_context);
    }
}

/**
 * Key: single-click = DEMO_PERMISSION_DECISION_ONCE, long-press (first threshold) = DEMO_PERMISSION_DECISION_REJECT.
 * Only acts when a permission request is pending (v1 envelope protocol).
 * "Always" is not exposed with the current single-key default (see json_format.md).
 * Then play once/reject emote via fade.
 */
static void emote_demo_permission_key_event_cb(const board_touch_event_t *ev)
{
    if (ev->src != BOARD_TOUCH_SOURCE_KEY) {
        return;
    }
    const button_event_t be = (button_event_t)ev->code;
    if (be != BUTTON_SINGLE_CLICK && be != BUTTON_LONG_PRESS_START) {
        return;
    }
    if (!ble_protocol_permission_is_pending()) {
        return;
    }
    const bool once = (be == BUTTON_SINGLE_CLICK);
    ble_protocol_submit_permission(once ? DEMO_PERMISSION_DECISION_ONCE : DEMO_PERMISSION_DECISION_REJECT);
    emote_demo_post_animation_fade(once ? DEMO_DECISION_NAME_ALLOW : DEMO_DECISION_NAME_DENY);
    emote_demo_post_clear_tip("hook decision");
}

static void emote_demo_ui_worker_task(void *arg)
{
    /* The UI code never calls emote_gen_player directly from BLE/parser callbacks.
     * It posts small messages to s_msg_queue so rendering/animation operations are
     * serialized by emote_demo_ui_worker_task(). This keeps BLE callback latency low and avoids
     * mixing transport parsing with display-side work. */
    (void)arg;
    demo_msg_t msg;
    for (;;) {
        if (xQueueReceive(s_msg_queue, &msg, portMAX_DELAY) != pdTRUE) {
            continue;
        }
        if (msg.player == NULL) {
            continue;
        }
        switch (msg.kind) {
        case DEMO_MSG_ANIM_NAME:
            ESP_LOGI(TAG, "anim: %s", msg.u.anim_name);
            (void)emote_gen_player_anim_fade_name(msg.player, msg.u.anim_name);
            break;
        case DEMO_MSG_TIP_TEXT:
            ESP_LOGI(TAG, "tip: %s", msg.u.tip_text);
            (void)emote_gen_player_set_tip_text(msg.player, msg.u.tip_text);
            break;
        case DEMO_MSG_CLEAR_TIP:
            ESP_LOGI(TAG, "tip: clear");
            (void)emote_gen_player_set_tip_text(msg.player, NULL);
            break;
        }
    }
}

static void emote_demo_ui_worker_start_once(void)
{
    /* Start once after the emote player is available. The queue becomes the
     * boundary between protocol events and display-side operations. */
    if (s_msg_queue == NULL) {
        s_msg_queue = xQueueCreate(DEMO_MSG_QUEUE_LEN, sizeof(demo_msg_t));
    }
    if (s_worker_task == NULL && s_msg_queue != NULL) {
        (void)xTaskCreate(emote_demo_ui_worker_task, "emote_demo", DEMO_WORKER_TASK_STACK, NULL,
                          DEMO_WORKER_TASK_PRIORITY, &s_worker_task);
    }
}

void emote_demo_show_permission_timeout(void)
{
    /* Permission timeout is displayed as a separate expression so users can
     * distinguish "no answer" from an explicit reject long-press. */
    if (s_msg_queue == NULL || s_player == NULL) {
        return;
    }
    emote_demo_post_clear_tip("hook timeout");
    emote_demo_post_animation_fade(DEMO_DECISION_NAME_TIMEOUT);
}

void emote_demo_clear_permission(void)
{
    /* Clear both the tip label and the prompt animation. This is used for idle
     * status, cancellation, disconnect cleanup, and stale wait-task cleanup. */
    if (s_msg_queue == NULL || s_player == NULL) {
        return;
    }
    emote_demo_post_clear_tip("hook clear");
    emote_demo_post_animation_fade(DEMO_IDLE_NAME);
}

void emote_demo_show_permission(const char *perm_type, const char *title, const char *meta_compact)
{
    /* Show one compact permission prompt. The protocol layer already validates
     * the payload; this layer only chooses what fits on the wearable: permission
     * type plus metadata such as command/path/url, falling back to title. */
    if (s_msg_queue == NULL || s_player == NULL) {
        return;
    }
    const char *pt = perm_type ? perm_type : "";
    const char *tl = title ? title : "";
    const char *mc = (meta_compact && meta_compact[0] != '\0') ? meta_compact : NULL;
    const char *detail = (mc != NULL) ? mc : tl;

    emote_demo_post_animation_fade(DEMO_QUESTION_NAME);
    demo_msg_t msg = { .kind = DEMO_MSG_TIP_TEXT, .player = s_player };
    (void)snprintf(msg.u.tip_text, sizeof(msg.u.tip_text), "%s:%s", pt, detail);
    msg.u.tip_text[sizeof(msg.u.tip_text) - 1] = '\0';
    if (xQueueSend(s_msg_queue, &msg, 0) != pdTRUE) {
        ESP_LOGW(TAG, "tip queue full (hook)");
    }
}

void emote_demo_show_session_status(const char *status_type)
{
    /* Best-effort OpenCode session sync. These states are not permissions and
     * do not require a BLE reply; they simply make the device feel connected to
     * the editor session. */
    if (status_type == NULL || s_msg_queue == NULL || s_player == NULL) {
        return;
    }
    demo_msg_t msg = { .kind = DEMO_MSG_TIP_TEXT, .player = s_player };
    if (strcmp(status_type, DEMO_SESSION_STATUS_BUSY) == 0) {
        (void)snprintf(msg.u.tip_text, sizeof(msg.u.tip_text), DEMO_TIP_TEXT_WORKING);
        emote_demo_post_animation_fade(DEMO_BUSY_NAME);
    } else if (strcmp(status_type, DEMO_SESSION_STATUS_IDLE) == 0) {
        emote_demo_post_clear_tip("session idle");
        emote_demo_post_animation_fade(DEMO_IDLE_NAME);
        return;
    } else if (strcmp(status_type, DEMO_SESSION_STATUS_RETRY) == 0) {
        (void)snprintf(msg.u.tip_text, sizeof(msg.u.tip_text), DEMO_TIP_TEXT_RETRY);
        emote_demo_post_animation_fade(DEMO_QUESTION_NAME);
    } else {
        return; /* unknown status type – ignore */
    }
    msg.u.tip_text[sizeof(msg.u.tip_text) - 1] = '\0';
    if (xQueueSend(s_msg_queue, &msg, 0) != pdTRUE) {
        ESP_LOGW(TAG, "tip queue full (session status)");
    }
}

static void emote_demo_board_touch_cb(void *user_data, const board_touch_event_t *ev)
{
    /* Route only the physical key into permission decisions. Panel touch events
     * can still drive other demos, but the OpenCode toy keeps approval/reject
     * on a single deliberate input path. */
    (void)user_data;
    if (ev == NULL || s_msg_queue == NULL) {
        return;
    }
    if (ev->src == BOARD_TOUCH_SOURCE_KEY) {
        emote_demo_permission_key_event_cb(ev);
    }
}

static void emote_demo_log_animation_index(emote_gen_player_handle_t player)
{
    size_t index_count = emote_gen_player_get_index_count(player);

    for (size_t i = 0; i < index_count; i++) {
        const emote_gen_player_index_entry_t *e = emote_gen_player_get_index_entry(player, i);
        if (e == NULL) {
            ESP_LOGE(TAG, "index[%zu] missing", i);
            continue;
        }
        ESP_LOGD(TAG,
                 "index[%2zu] name:%-20s pos:(%" PRIi16 ",%" PRIi16 ") has_xy:%d stop:%" PRIi32 " has_loop:%d loop:[%" PRIi32
                 ",%" PRIi32 "]",
                 i, e->name, e->play.x, e->play.y, (int)e->play.has_xy, e->play.stop_frame, (int)e->play.has_loop_range,
                 e->play.loop_start, e->play.loop_end);
    }
    ESP_LOGI(TAG, "index.json: loaded %zu entries", index_count);
}

static bool emote_demo_runtime_start(emote_gen_player_handle_t player)
{
    /* Once the board/display is up, bind the global player, start the UI worker,
     * align the tip label, and register the touch callback used for permissions. */
    size_t index_count = emote_gen_player_get_index_count(player);

    emote_demo_log_animation_index(player);
    if (index_count == 0) {
        ESP_LOGE(TAG, "no index entries");
        return false;
    }

    ESP_LOGI(TAG, "demo start");
    s_player = player;

    emote_demo_ui_worker_start_once();
    if (s_msg_queue == NULL || s_worker_task == NULL) {
        ESP_LOGE(TAG, "runtime worker failed to start");
        s_player = NULL;
        return false;
    }

    gfx_obj_t *tip = emote_gen_player_get_tip_label(player);
    if (tip == NULL) {
        ESP_LOGW(TAG, "tip label missing");
    } else {
        gfx_handle_t gh = emote_gen_player_get_gfx_handle(player);
        if (gh != NULL && gfx_emote_lock(gh) == ESP_OK) {
            (void)gfx_obj_align(tip, GFX_ALIGN_TOP_MID, 0, DEMO_TIP_LABEL_Y_OFS);
            gfx_emote_unlock(gh);
        }
    }
    (void)emote_gen_player_anim_now_name(player, DEMO_IDLE_NAME);
    board_set_touch_callback(emote_demo_board_touch_cb, player);

    return true;
}

void emote_demo_run(void)
{
    /* Board entry point called from app_main after BLE/protocol setup. The board
     * layer owns panel/touch/display initialization; this file owns the demo
     * behavior layered on top of the emote player. */
    board_app_t app;
    s_player = NULL;

    if (board_start(&app) != ESP_OK) {
        ESP_LOGE(TAG, "board_start failed");
        return;
    }
    if (!emote_demo_runtime_start(app.player)) {
        board_stop(&app);
    }
}
