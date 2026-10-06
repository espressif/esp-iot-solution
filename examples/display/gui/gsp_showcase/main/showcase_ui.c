/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 * SPDX-License-Identifier: Apache-2.0
 */

#include "showcase_ui.h"

#include <stdbool.h>
#include <stdatomic.h>
#include <stdio.h>

#define GSP_BUNDLE_ENABLE_RAW_IDS 1
#include "bundle_gsp.h"

#ifdef SHOWCASE_PC
#define GSP_BUNDLE_SCENE_CONTROLS GSP_BRIDGE_BUNDLE_SCENE_CONTROLS
#define GSP_BUNDLE_SCENE_MEDIA GSP_BRIDGE_BUNDLE_SCENE_MEDIA
#define GSP_BUNDLE_SCENE_MOTION GSP_BRIDGE_BUNDLE_SCENE_MOTION
#define GSP_BUNDLE_SCENE_DATA GSP_BRIDGE_BUNDLE_SCENE_DATA
#endif

static atomic_uint_fast16_t s_scene = ATOMIC_VAR_INIT(GSP_BUNDLE_SCENE_CONTROLS);
static atomic_int s_level = ATOMIC_VAR_INIT(42);
static atomic_bool s_playing = ATOMIC_VAR_INIT(true);
static atomic_bool s_power = ATOMIC_VAR_INIT(true);
static atomic_bool s_alerts = ATOMIC_VAR_INIT(true);
static atomic_bool s_choice_a = ATOMIC_VAR_INIT(true);
static atomic_int s_image_status = ATOMIC_VAR_INIT(0);
static atomic_bool s_image_submitted = ATOMIC_VAR_INIT(false);
static uint32_t s_sample;
static const uint8_t *s_badge;
static size_t s_badge_size;
static void *s_timer;
#ifdef GSP_DATA_TEMPLATE_DEVICES
static esp_gsp_list_t s_devices_list = ESP_GSP_LIST_NONE;
#endif
static void report_error(const char *operation, esp_gsp_err_t err);

#ifdef SHOWCASE_KNOB_UI
static atomic_bool s_editing = ATOMIC_VAR_INIT(false);
static atomic_int s_focus = ATOMIC_VAR_INIT(0);
static const char *const s_focus_names[] = {
    "Edit: Add 10", "Edit: Reset", "Edit: Level -10", "Edit: Level +10",
    "Edit: Power", "Edit: Alerts", "Edit: Option A", "Edit: Option B",
};

static void show_focus(esp_gsp_handle_t ui)
{
    bool editing = atomic_load(&s_editing);
    report_error("focus", gsp_controls_focus_label_set_text(ui,
                                                            editing ? s_focus_names[atomic_load(&s_focus)] : "Input and state"));
    report_error("knob hint", gsp_controls_knob_hint_set_text(ui,
                                                              editing ? "Turn: focus  Click: apply  Hold: done" :
                                                              "Turn: page  Click: +10  Hold: edit"));
}
#endif

static void report_error(const char *operation, esp_gsp_err_t err)
{
    if (err != ESP_GSP_OK) {
        fprintf(stderr, "gsp_showcase: %s failed: %d\n", operation, (int)err);
    }
}

static void set_level(esp_gsp_handle_t ui, int32_t value)
{
    if (value < 0) {
        value = 0;
    }
    if (value > 100) {
        value = 100;
    }
    atomic_store(&s_level, value);
    char text[24];
    snprintf(text, sizeof(text), "Level %ld%%", (long)value);
    report_error("slider", gsp_controls_level_set_value(ui, value));
    report_error("progress", gsp_controls_level_progress_set_value(ui, value));
    report_error("level text", gsp_controls_level_readout_set_text(ui, text));
}

static const char *image_status_text(void)
{
#ifdef SHOWCASE_KNOB_UI
    switch (atomic_load(&s_image_status)) {
    case 1: return "PNG: loading  Click: replace";
    case 2: return "PNG: ready  Click: replace";
    case 3: return "PNG: failed  Click: replace";
    default: return "PNG: original  Click: replace";
    }
#else
    switch (atomic_load(&s_image_status)) {
    case 1: return "PNG: loading";
    case 2: return "PNG: ready";
    case 3: return "PNG: failed";
    default: return "PNG: original";
    }
#endif
}

static void image_complete(esp_gsp_handle_t ui, uint16_t bind, gsp_err_t status, void *ctx)
{
    (void)ui;
    (void)bind;
    (void)ctx;
    /* Decode/render callbacks must remain non-blocking. The timer updates the label. */
    if (status != GSP_ERR_CANCELLED) {
        atomic_store(&s_image_status, status == ESP_GSP_OK ? 2 : 3);
    }
}

static esp_gsp_err_t show_runtime_png(esp_gsp_handle_t ui)
{
    if (s_badge == NULL || s_badge_size == 0) {
        return ESP_GSP_ERR_INVALID_ARG;
    }
    atomic_store(&s_image_status, 1);
    report_error("image status", gsp_media_image_status_set_text(ui, image_status_text()));
    const esp_gsp_image_options_t options = {
        .ownership = ESP_GSP_IMAGE_COPY,
        .on_complete = image_complete,
    };
    esp_gsp_err_t err = esp_gsp_set_image_ex(ui, GSP_MEDIA_BIND_LIVE_IMAGE,
                                             s_badge, s_badge_size, &options);
    if (err != ESP_GSP_OK) {
        atomic_store(&s_image_status, 3);
        report_error("image status", gsp_media_image_status_set_text(ui, image_status_text()));
    } else {
        atomic_store(&s_image_submitted, true);
    }
    return err;
}

static bool is_nav_controls(const esp_gsp_event_t *e)
{
    return gsp_controls_event_is_nav_controls(e) || gsp_media_event_is_nav_controls(e) ||
           gsp_motion_event_is_nav_controls(e) || gsp_data_event_is_nav_controls(e);
}
static bool is_nav_media(const esp_gsp_event_t *e)
{
    return gsp_controls_event_is_nav_media(e) || gsp_media_event_is_nav_media(e) ||
           gsp_motion_event_is_nav_media(e) || gsp_data_event_is_nav_media(e);
}
static bool is_nav_motion(const esp_gsp_event_t *e)
{
    return gsp_controls_event_is_nav_motion(e) || gsp_media_event_is_nav_motion(e) ||
           gsp_motion_event_is_nav_motion(e) || gsp_data_event_is_nav_motion(e);
}
static bool is_nav_data(const esp_gsp_event_t *e)
{
    return gsp_controls_event_is_nav_data(e) || gsp_media_event_is_nav_data(e) ||
           gsp_motion_event_is_nav_data(e) || gsp_data_event_is_nav_data(e);
}

static void goto_scene(esp_gsp_handle_t ui, uint16_t scene)
{
    uint16_t current = atomic_load(&s_scene);
    if (scene == current) {
        return;
    }
#ifdef SHOWCASE_KNOB_UI
    const esp_gsp_transition_t transition = ESP_GSP_NO_TRANSITION;
#else
    const esp_gsp_transition_t transition = scene > current ? ESP_GSP_SLIDE_LEFT : ESP_GSP_SLIDE_RIGHT;
#endif
    report_error("scene navigation", esp_gsp_goto_scene(ui, scene, transition));
}

static void toggle_panel(esp_gsp_handle_t ui)
{
    bool open = false;
    esp_gsp_err_t err = esp_gsp_drawer_is_open(ui, GSP_CONTROLS_OBJ_KEY_QUICK_DRAWER, &open);
    if (err != ESP_GSP_OK) {
        report_error("query Quick Panel", err);
        return;
    }
    report_error("toggle Quick Panel", open ?
                 esp_gsp_drawer_close(ui, GSP_CONTROLS_OBJ_KEY_QUICK_DRAWER, true) :
                 esp_gsp_drawer_open(ui, GSP_CONTROLS_OBJ_KEY_QUICK_DRAWER, true));
}

static void showcase_event(esp_gsp_handle_t ui, const esp_gsp_event_t *event, void *ctx)
{
    (void)ctx;
    if (event == NULL) {
        return;
    }
    if (event->type == ESP_GSP_EVENT_SCENE_CHANGED) {
        atomic_store(&s_scene, event->scene_id);
        if (event->scene_id == GSP_BUNDLE_SCENE_CONTROLS) {
            set_level(ui, atomic_load(&s_level));
            report_error("power", gsp_controls_power_set_checked(ui, atomic_load(&s_power)));
            report_error("alerts", gsp_controls_alerts_set_checked(ui, atomic_load(&s_alerts)));
            report_error("radio A", gsp_controls_choice_a_set_checked(ui, atomic_load(&s_choice_a)));
            report_error("radio B", gsp_controls_choice_b_set_checked(ui, !atomic_load(&s_choice_a)));
#ifdef SHOWCASE_KNOB_UI
            atomic_store(&s_editing, false);
            show_focus(ui);
#endif
        } else if (event->scene_id == GSP_BUNDLE_SCENE_MEDIA) {
            if (atomic_load(&s_image_submitted)) {
                report_error("runtime PNG", show_runtime_png(ui));
            } else {
                report_error("image status", gsp_media_image_status_set_text(ui, image_status_text()));
            }
        } else if (event->scene_id == GSP_BUNDLE_SCENE_MOTION) {
            report_error("motion", gsp_motion_charge_set_playing(ui, atomic_load(&s_playing)));
#ifdef GSP_DATA_TEMPLATE_DEVICES
        } else if (event->scene_id == GSP_BUNDLE_SCENE_DATA && s_devices_list == ESP_GSP_LIST_NONE) {
            /* Bind once after this scene exists; NULL serves its authored items. */
            s_devices_list = gsp_data_devices_bind(ui, NULL, NULL);
            if (s_devices_list == ESP_GSP_LIST_NONE) {
                report_error("devices list", ESP_GSP_FAIL);
            }
#endif
        }
        return;
    }
    if (event->type != ESP_GSP_EVENT_CALL) {
        return;
    }

    if (is_nav_controls(event)) {
        goto_scene(ui, GSP_BUNDLE_SCENE_CONTROLS);
    } else if (is_nav_media(event)) {
        goto_scene(ui, GSP_BUNDLE_SCENE_MEDIA);
    } else if (is_nav_motion(event)) {
        goto_scene(ui, GSP_BUNDLE_SCENE_MOTION);
    } else if (is_nav_data(event)) {
        goto_scene(ui, GSP_BUNDLE_SCENE_DATA);
    } else if (gsp_controls_event_is_increment(event)) {
        set_level(ui, atomic_load(&s_level) + 10);
    } else if (gsp_controls_event_is_reset(event)) {
        set_level(ui, 0);
    } else if (gsp_controls_event_is_level_changed(event)) {
        set_level(ui, (int32_t)event->arg);
    } else if (gsp_controls_event_is_power_changed(event)) {
        bool checked = false;
        if (gsp_controls_power_get_checked(ui, &checked) == ESP_GSP_OK) {
            atomic_store(&s_power, checked);
        }
    } else if (gsp_controls_event_is_alerts_changed(event)) {
        bool checked = false;
        if (gsp_controls_alerts_get_checked(ui, &checked) == ESP_GSP_OK) {
            atomic_store(&s_alerts, checked);
        }
    } else if (gsp_controls_event_is_select_a(event) || gsp_controls_event_is_select_b(event)) {
        bool a = gsp_controls_event_is_select_a(event);
        atomic_store(&s_choice_a, a);
        report_error("radio A", gsp_controls_choice_a_set_checked(ui, a));
        report_error("radio B", gsp_controls_choice_b_set_checked(ui, !a));
    } else if (gsp_media_event_is_replace_image(event)) {
        report_error("runtime PNG", show_runtime_png(ui));
    } else if (gsp_motion_event_is_toggle_motion(event)) {
        bool playing = !atomic_load(&s_playing);
        atomic_store(&s_playing, playing);
        report_error("motion", gsp_motion_charge_set_playing(ui, playing));
    } else if (gsp_motion_event_is_animate_value(event)) {
        report_error("value animation", esp_gsp_component_animate_value(
                         ui, GSP_MOTION_OBJ_KEY_CHARGE, 20, 95, 1000, ESP_GSP_EASE_OUT));
    }
}

static void showcase_tick(esp_gsp_handle_t ui, void *ctx)
{
    (void)ctx;
    uint16_t scene = atomic_load(&s_scene);
    if (scene == GSP_BUNDLE_SCENE_MEDIA) {
        static int last_image_status = -1;
        int status = atomic_load(&s_image_status);
        if (status != last_image_status) {
            report_error("image status", gsp_media_image_status_set_text(ui, image_status_text()));
            last_image_status = status;
        }
    } else if (scene == GSP_BUNDLE_SCENE_MOTION) {
        int32_t value = 0;
        if (gsp_motion_charge_get_value(ui, &value) == ESP_GSP_OK) {
            char text[16];
            snprintf(text, sizeof(text), "%ld%%", (long)value);
            report_error("charge text", gsp_motion_charge_readout_set_text(ui, text));
        }
    } else if (scene == GSP_BUNDLE_SCENE_DATA) {
        s_sample = (s_sample + 1U) % 10000U;
        int32_t value = (int32_t)((s_sample * 17U + 23U) % 100U);
        char text[24];
        snprintf(text, sizeof(text), "Sample %lu", (unsigned long)s_sample);
        report_error("sample text", gsp_data_sample_readout_set_text(ui, text));
        report_error("sample bar", gsp_data_sample_progress_set_value(ui, value));
        report_error("chart", gsp_data_trend_append(ui, value));
    }
}

esp_gsp_err_t showcase_ui_init(esp_gsp_handle_t ui, const uint8_t *badge, size_t badge_size)
{
    if (ui == NULL || badge == NULL || badge_size == 0) {
        return ESP_GSP_ERR_INVALID_ARG;
    }
    s_badge = badge;
    s_badge_size = badge_size;
    esp_gsp_err_t err = esp_gsp_on_event(ui, showcase_event, NULL);
    if (err != ESP_GSP_OK) {
        return err;
    }
    set_level(ui, atomic_load(&s_level));
#ifdef SHOWCASE_KNOB_UI
    show_focus(ui);
#endif
    s_timer = esp_gsp_timer_create(ui, 500, showcase_tick, NULL);
    return s_timer != NULL ? ESP_GSP_OK : ESP_GSP_ERR_NO_MEM;
}

void showcase_ui_deinit(esp_gsp_handle_t ui)
{
    if (s_timer != NULL) {
        (void)esp_gsp_timer_delete(ui, s_timer);
        s_timer = NULL;
    }
    s_badge = NULL;
    s_badge_size = 0;
#ifdef GSP_DATA_TEMPLATE_DEVICES
    s_devices_list = ESP_GSP_LIST_NONE;
#endif
}

void showcase_ui_knob_turn(esp_gsp_handle_t ui, int direction)
{
#ifdef SHOWCASE_KNOB_UI
    if (atomic_load(&s_editing) && atomic_load(&s_scene) == GSP_BUNDLE_SCENE_CONTROLS) {
        const int count = (int)(sizeof(s_focus_names) / sizeof(s_focus_names[0]));
        int next = (atomic_load(&s_focus) + (direction > 0 ? 1 : count - 1)) % count;
        atomic_store(&s_focus, next);
        show_focus(ui);
        return;
    }
#endif
    uint16_t current = atomic_load(&s_scene);
    goto_scene(ui, (current + (direction > 0 ? 1U : 3U)) % 4U);
}

void showcase_ui_knob_click(esp_gsp_handle_t ui)
{
    uint16_t scene = atomic_load(&s_scene);
    if (scene == GSP_BUNDLE_SCENE_CONTROLS) {
#ifdef SHOWCASE_KNOB_UI
        if (atomic_load(&s_editing)) {
            switch (atomic_load(&s_focus)) {
            case 0: case 3: set_level(ui, atomic_load(&s_level) + 10); break;
            case 1: set_level(ui, 0); break;
            case 2: set_level(ui, atomic_load(&s_level) - 10); break;
            case 4:
                atomic_store(&s_power, !atomic_load(&s_power));
                report_error("power", gsp_controls_power_set_checked(ui, atomic_load(&s_power)));
                break;
            case 5:
                atomic_store(&s_alerts, !atomic_load(&s_alerts));
                report_error("alerts", gsp_controls_alerts_set_checked(ui, atomic_load(&s_alerts)));
                break;
            case 6: case 7:
                atomic_store(&s_choice_a, atomic_load(&s_focus) == 6);
                report_error("radio A", gsp_controls_choice_a_set_checked(ui, atomic_load(&s_choice_a)));
                report_error("radio B", gsp_controls_choice_b_set_checked(ui, !atomic_load(&s_choice_a)));
                break;
            }
            return;
        }
#endif
        set_level(ui, atomic_load(&s_level) + 10);
    } else if (scene == GSP_BUNDLE_SCENE_MEDIA) {
        report_error("runtime PNG", show_runtime_png(ui));
    } else if (scene == GSP_BUNDLE_SCENE_MOTION) {
        bool playing = !atomic_load(&s_playing);
        atomic_store(&s_playing, playing);
        report_error("motion", gsp_motion_charge_set_playing(ui, playing));
    } else {
        toggle_panel(ui);
    }
}

void showcase_ui_knob_long(esp_gsp_handle_t ui)
{
#ifdef SHOWCASE_KNOB_UI
    if (atomic_load(&s_scene) == GSP_BUNDLE_SCENE_CONTROLS) {
        bool editing = !atomic_load(&s_editing);
        atomic_store(&s_editing, editing);
        show_focus(ui);
        return;
    }
#endif
    toggle_panel(ui);
}
