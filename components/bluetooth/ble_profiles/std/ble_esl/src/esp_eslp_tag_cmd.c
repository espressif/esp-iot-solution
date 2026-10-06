/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/** @file
 *  @brief Tag Display / LED / Sensor ECP & PAwR command handling
 */

#include <string.h>

#include "esp_event.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "sdkconfig.h"

#include "esp_esl.h"
#include "esp_eslp_tag.h"
#include "esp_eslp_tag_cmd_priv.h"

static const char *TAG = "ble_eslp_tag_cmd";

/** Absolute Time delta beyond 48 days is treated as implausible */
#define ESLP_IMPLAUSIBLE_TIME_MS    (48UL * 24UL * 3600UL * 1000UL)

#ifndef CONFIG_BLE_ESL_MAX_DISPLAYS
#define ESLP_MAX_DISPLAYS           1
#else
#define ESLP_MAX_DISPLAYS           CONFIG_BLE_ESL_MAX_DISPLAYS
#endif

#ifndef CONFIG_BLE_ESL_MAX_LEDS
#define ESLP_MAX_LEDS               1
#else
#define ESLP_MAX_LEDS               CONFIG_BLE_ESL_MAX_LEDS
#endif

typedef struct {
    bool has_active_image;
    uint8_t current_image;
    bool timed_pending;
    uint8_t timed_image;
    uint32_t timed_abs;
    esp_timer_handle_t timed_timer;
} eslp_display_slot_t;

typedef struct {
    bool active;
    bool timed_pending;
    uint8_t settings[10];   /*!< color_brightness + flashing(7) + repeat(2) */
    uint32_t timed_abs;
    esp_timer_handle_t timed_timer;
    esp_timer_handle_t pattern_timer;
} eslp_led_slot_t;

typedef struct {
    bool inited;
    eslp_display_slot_t displays[ESLP_MAX_DISPLAYS];
    eslp_led_slot_t leds[ESLP_MAX_LEDS];
    bool sensor_pending;
    uint8_t sensor_pending_index;
} eslp_tag_cmd_ctx_t;

static eslp_tag_cmd_ctx_t s_cmd;
static uint16_t *s_basic_state_ptr;

static void post_event(esp_ble_eslp_event_t id, const void *data, size_t len)
{
    esp_err_t ret = esp_event_post(BLE_ESLP_EVENTS, id, data, len, portMAX_DELAY);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "post event %d failed: %s", (int)id, esp_err_to_name(ret));
    }
}

static void set_error(eslp_tag_cmd_result_t *r, uint8_t code)
{
    r->has_response = true;
    r->rsp[0] = BLE_ESL_RESP_ERROR;
    r->rsp[1] = code;
    r->rsp_len = 2;
}

static void set_display_state(eslp_tag_cmd_result_t *r, uint8_t display_index, uint8_t image_index)
{
    r->has_response = true;
    r->rsp[0] = BLE_ESL_RESP_DISPLAY_STATE;
    r->rsp[1] = display_index;
    r->rsp[2] = image_index;
    r->rsp_len = 3;
}

static void set_led_state(eslp_tag_cmd_result_t *r, uint8_t led_index)
{
    r->has_response = true;
    r->rsp[0] = BLE_ESL_RESP_LED_STATE;
    r->rsp[1] = led_index;
    r->rsp_len = 2;
}

static void refresh_pending_bits(void)
{
    if (!s_basic_state_ptr) {
        return;
    }
    uint16_t bs = *s_basic_state_ptr;
    bool pending_disp = false;
    bool pending_led = false;
    bool active_led = false;

    for (int i = 0; i < ESLP_MAX_DISPLAYS; i++) {
        if (s_cmd.displays[i].timed_pending) {
            pending_disp = true;
        }
    }
    for (int i = 0; i < ESLP_MAX_LEDS; i++) {
        if (s_cmd.leds[i].timed_pending) {
            pending_led = true;
        }
        if (s_cmd.leds[i].active) {
            active_led = true;
        }
    }

    if (pending_disp) {
        bs |= BLE_ESL_BASIC_STATE_PENDING_DISP_UPDATE;
    } else {
        bs &= (uint16_t)~BLE_ESL_BASIC_STATE_PENDING_DISP_UPDATE;
    }
    if (pending_led) {
        bs |= BLE_ESL_BASIC_STATE_PENDING_LED_UPDATE;
    } else {
        bs &= (uint16_t)~BLE_ESL_BASIC_STATE_PENDING_LED_UPDATE;
    }
    if (active_led) {
        bs |= BLE_ESL_BASIC_STATE_ACTIVE_LED;
    } else {
        bs &= (uint16_t)~BLE_ESL_BASIC_STATE_ACTIVE_LED;
    }
    *s_basic_state_ptr = bs;
}

void eslp_tag_cmd_sync_basic_state(uint16_t *basic_state)
{
    s_basic_state_ptr = basic_state;
    refresh_pending_bits();
}

static uint8_t num_displays(void)
{
#ifdef CONFIG_BLE_ESL_DISPLAY_INFO
    esp_ble_esl_display_info_t tmp[ESLP_MAX_DISPLAYS];
    uint8_t count = 0;
    if (esp_ble_esl_get_display_info(tmp, ESLP_MAX_DISPLAYS, &count) == ESP_OK) {
        return count;
    }
#endif
    return 0;
}

static uint8_t max_image_index(void)
{
#ifdef CONFIG_BLE_ESL_IMAGE_INFO
    esp_ble_esl_image_info_t info = {0};
    if (esp_ble_esl_get_image_info(&info) == ESP_OK) {
        return info.max_image_index;
    }
#endif
#ifdef CONFIG_BLE_ESL_MAX_IMAGES
    return (uint8_t)(CONFIG_BLE_ESL_MAX_IMAGES - 1);
#else
    return 0;
#endif
}

static uint8_t num_leds(void)
{
#ifdef CONFIG_BLE_ESL_LED_INFO
    uint8_t leds[ESLP_MAX_LEDS];
    uint8_t count = 0;
    if (esp_ble_esl_get_led_info(leds, ESLP_MAX_LEDS, &count) == ESP_OK) {
        return count;
    }
#endif
    return 0;
}

static bool image_available(uint8_t image_index)
{
#ifdef CONFIG_BLE_ESL_OTS_SUPPORT
    const uint8_t *data = NULL;
    uint32_t len = 0;
    return esp_ble_eslp_get_image(image_index, &data, &len) == ESP_OK && data && len > 0;
#else
    (void)image_index;
    return true;
#endif
}

static uint32_t time_delta_ms(uint32_t absolute_time, uint32_t now)
{
    if (absolute_time >= now) {
        return absolute_time - now;
    }
    return (0xFFFFFFFFUL - now) + absolute_time + 1UL;
}

static void parse_led_settings(uint8_t led_index, const uint8_t settings[10],
                               esp_ble_eslp_led_control_t *out, bool is_off)
{
    uint8_t cb = settings[0];
    out->led_index = led_index;
    out->color_red = (uint8_t)(cb & 0x03);
    out->color_green = (uint8_t)((cb >> 2) & 0x03);
    out->color_blue = (uint8_t)((cb >> 4) & 0x03);
    out->brightness = (uint8_t)((cb >> 6) & 0x03);
    memcpy(out->flashing_pattern, &settings[1], 7);
    uint16_t repeat = (uint16_t)(settings[8] | ((uint16_t)settings[9] << 8));
    out->repeat_type = (uint8_t)(repeat & 0x01);
    out->repeats_duration = (uint16_t)((repeat >> 1) & 0x7FFF);
    out->is_off = is_off;
}

static uint64_t led_pattern_duration_us(const esp_ble_eslp_led_control_t *p)
{
    if (p->repeats_duration == 0) {
        return 0;
    }
    if (p->repeat_type == 1) {
        return (uint64_t)p->repeats_duration * 1000000ULL;
    }
    /* Count mode: approximate using on/off periods from flashing_pattern[5]/[6] */
    uint8_t bit_off = p->flashing_pattern[5];
    uint8_t bit_on = p->flashing_pattern[6];
    uint32_t cycle_ms = (uint32_t)bit_on * 2u * 20u + (uint32_t)bit_off * 2u * 20u;
    if (cycle_ms == 0) {
        cycle_ms = 100;
    }
    return (uint64_t)cycle_ms * (uint64_t)p->repeats_duration * 1000ULL;
}

static void apply_led(uint8_t led_index, const uint8_t settings[10]);

static void led_pattern_timer_cb(void *arg)
{
    uintptr_t led_index = (uintptr_t)arg;
    if (led_index >= ESLP_MAX_LEDS || !s_cmd.inited) {
        return;
    }
    eslp_led_slot_t *led = &s_cmd.leds[led_index];
    led->active = false;
    refresh_pending_bits();

    esp_ble_eslp_led_control_t ev = {0};
    parse_led_settings((uint8_t)led_index, led->settings, &ev, true);
    post_event(BLE_ESLP_EVENT_LED_CONTROL, &ev, sizeof(ev));
}

static void apply_led(uint8_t led_index, const uint8_t settings[10])
{
    eslp_led_slot_t *led = &s_cmd.leds[led_index];
    if (led->pattern_timer) {
        (void)esp_timer_stop(led->pattern_timer);
    }
    memcpy(led->settings, settings, sizeof(led->settings));

    esp_ble_eslp_led_control_t ev = {0};
    parse_led_settings(led_index, settings, &ev, false);

    if (ev.repeat_type == 0 && ev.repeats_duration == 0) {
        led->active = false;
        ev.is_off = true;
        refresh_pending_bits();
        post_event(BLE_ESLP_EVENT_LED_CONTROL, &ev, sizeof(ev));
        return;
    }

    led->active = true;
    refresh_pending_bits();
    post_event(BLE_ESLP_EVENT_LED_CONTROL, &ev, sizeof(ev));

    uint64_t dur = led_pattern_duration_us(&ev);
    if (dur > 0 && led->pattern_timer) {
        (void)esp_timer_start_once(led->pattern_timer, dur);
    }
}

static void display_timed_cb(void *arg)
{
    uintptr_t display_index = (uintptr_t)arg;
    if (display_index >= ESLP_MAX_DISPLAYS || !s_cmd.inited) {
        return;
    }
    eslp_display_slot_t *disp = &s_cmd.displays[display_index];
    if (!disp->timed_pending) {
        return;
    }
    disp->timed_pending = false;
    disp->has_active_image = true;
    disp->current_image = disp->timed_image;
    refresh_pending_bits();

    esp_ble_eslp_display_image_t ev = {
        .display_index = (uint8_t)display_index,
        .image_index = disp->current_image,
    };
    post_event(BLE_ESLP_EVENT_DISPLAY_IMAGE, &ev, sizeof(ev));
}

static void led_timed_cb(void *arg)
{
    uintptr_t led_index = (uintptr_t)arg;
    if (led_index >= ESLP_MAX_LEDS || !s_cmd.inited) {
        return;
    }
    eslp_led_slot_t *led = &s_cmd.leds[led_index];
    if (!led->timed_pending) {
        return;
    }
    led->timed_pending = false;
    refresh_pending_bits();
    apply_led((uint8_t)led_index, led->settings);
}

static esp_err_t handle_display_image(const uint8_t *params, uint8_t params_len,
                                      eslp_tag_cmd_result_t *result)
{
    /* params: [esl_id, display_index, image_index] */
    if (params_len < 3) {
        set_error(result, BLE_ESL_ERR_INVALID_PARAMS);
        return ESP_OK;
    }
    uint8_t display_count = num_displays();
    if (display_count == 0) {
        set_error(result, BLE_ESL_ERR_INVALID_OPCODE);
        return ESP_OK;
    }
    uint8_t display_index = params[1];
    uint8_t image_index = params[2];
    if (display_index >= display_count || display_index >= ESLP_MAX_DISPLAYS) {
        set_error(result, BLE_ESL_ERR_INVALID_PARAMS);
        return ESP_OK;
    }
    if (image_index > max_image_index()) {
        set_error(result, BLE_ESL_ERR_INVALID_IMAGE_INDEX);
        return ESP_OK;
    }
    if (!image_available(image_index)) {
        set_error(result, BLE_ESL_ERR_IMAGE_NOT_AVAILABLE);
        return ESP_OK;
    }

    eslp_display_slot_t *disp = &s_cmd.displays[display_index];
    if (disp->timed_pending && disp->timed_timer) {
        (void)esp_timer_stop(disp->timed_timer);
        disp->timed_pending = false;
    }
    disp->has_active_image = true;
    disp->current_image = image_index;
    refresh_pending_bits();

    esp_ble_eslp_display_image_t ev = {
        .display_index = display_index,
        .image_index = image_index,
    };
    post_event(BLE_ESLP_EVENT_DISPLAY_IMAGE, &ev, sizeof(ev));
    set_display_state(result, display_index, image_index);
    return ESP_OK;
}

static esp_err_t handle_refresh_display(const uint8_t *params, uint8_t params_len,
                                        eslp_tag_cmd_result_t *result)
{
    /* params: [esl_id, display_index] */
    if (params_len < 2) {
        set_error(result, BLE_ESL_ERR_INVALID_PARAMS);
        return ESP_OK;
    }
    uint8_t display_count = num_displays();
    if (display_count == 0) {
        set_error(result, BLE_ESL_ERR_INVALID_OPCODE);
        return ESP_OK;
    }
    uint8_t display_index = params[1];
    if (display_index >= display_count || display_index >= ESLP_MAX_DISPLAYS) {
        set_error(result, BLE_ESL_ERR_INVALID_PARAMS);
        return ESP_OK;
    }
    eslp_display_slot_t *disp = &s_cmd.displays[display_index];
    if (!disp->has_active_image) {
        set_error(result, BLE_ESL_ERR_IMAGE_NOT_AVAILABLE);
        return ESP_OK;
    }

    esp_ble_eslp_refresh_display_t ev = { .display_index = display_index };
    post_event(BLE_ESLP_EVENT_REFRESH_DISPLAY, &ev, sizeof(ev));
    set_display_state(result, display_index, disp->current_image);
    return ESP_OK;
}

static esp_err_t handle_display_timed(const uint8_t *params, uint8_t params_len,
                                      eslp_tag_cmd_result_t *result)
{
    /* params: [esl_id, display_index, image_index, abs_time(4 LE)] */
    if (params_len < 7) {
        set_error(result, BLE_ESL_ERR_INVALID_PARAMS);
        return ESP_OK;
    }
    uint8_t display_count = num_displays();
    if (display_count == 0) {
        set_error(result, BLE_ESL_ERR_INVALID_OPCODE);
        return ESP_OK;
    }
    uint8_t display_index = params[1];
    uint8_t image_index = params[2];
    uint32_t abs_time = (uint32_t)params[3] | ((uint32_t)params[4] << 8) |
                        ((uint32_t)params[5] << 16) | ((uint32_t)params[6] << 24);

    if (display_index >= display_count || display_index >= ESLP_MAX_DISPLAYS) {
        set_error(result, BLE_ESL_ERR_INVALID_PARAMS);
        return ESP_OK;
    }

    eslp_display_slot_t *disp = &s_cmd.displays[display_index];

    if (abs_time == 0) {
        if (disp->timed_pending && disp->timed_timer) {
            (void)esp_timer_stop(disp->timed_timer);
            disp->timed_pending = false;
            refresh_pending_bits();
        }
        /* Display State echoes command Display_Index / Image_Index. */
        set_display_state(result, display_index, image_index);
        return ESP_OK;
    }

    if (image_index > max_image_index()) {
        set_error(result, BLE_ESL_ERR_INVALID_IMAGE_INDEX);
        return ESP_OK;
    }
    if (!image_available(image_index)) {
        set_error(result, BLE_ESL_ERR_IMAGE_NOT_AVAILABLE);
        return ESP_OK;
    }

    esp_ble_esl_abs_time_t now = 0;
    (void)esp_ble_esl_get_current_abs_time(&now);
    uint32_t delta = time_delta_ms(abs_time, now);
    if (delta > ESLP_IMPLAUSIBLE_TIME_MS) {
        set_error(result, BLE_ESL_ERR_IMPLAUSIBLE_ABS_TIME);
        return ESP_OK;
    }

    if (disp->timed_pending && disp->timed_abs != abs_time) {
        set_error(result, BLE_ESL_ERR_QUEUE_FULL);
        return ESP_OK;
    }

    disp->timed_pending = true;
    disp->timed_image = image_index;
    disp->timed_abs = abs_time;
    refresh_pending_bits();

    if (disp->timed_timer) {
        (void)esp_timer_stop(disp->timed_timer);
        esp_err_t ret = esp_timer_start_once(disp->timed_timer, (uint64_t)delta * 1000ULL);
        if (ret != ESP_OK) {
            disp->timed_pending = false;
            refresh_pending_bits();
            set_error(result, BLE_ESL_ERR_INSUFFICIENT_RESOURCES);
            return ESP_OK;
        }
    }

    /* Immediate Display State with command indices (not yet displayed). */
    set_display_state(result, display_index, image_index);
    return ESP_OK;
}

static esp_err_t handle_led_control(const uint8_t *params, uint8_t params_len,
                                    eslp_tag_cmd_result_t *result)
{
    /* params: [esl_id, led_index, settings(10)] */
    if (params_len < 12) {
        set_error(result, BLE_ESL_ERR_INVALID_PARAMS);
        return ESP_OK;
    }
    uint8_t nl = num_leds();
    if (nl == 0) {
        set_error(result, BLE_ESL_ERR_INVALID_OPCODE);
        return ESP_OK;
    }
    uint8_t led_index = params[1];
    if (led_index >= nl || led_index >= ESLP_MAX_LEDS) {
        set_error(result, BLE_ESL_ERR_INVALID_PARAMS);
        return ESP_OK;
    }

    const uint8_t *settings = &params[2];
    uint8_t bit_off = settings[6];
    uint8_t bit_on = settings[7];
    uint16_t repeat = (uint16_t)(settings[8] | ((uint16_t)settings[9] << 8));
    uint16_t repeats_duration = (uint16_t)((repeat >> 1) & 0x7FFF);
    if (repeats_duration > 0 && (bit_on == 0 || bit_off == 0)) {
        set_error(result, BLE_ESL_ERR_INVALID_PARAMS);
        return ESP_OK;
    }

    eslp_led_slot_t *led = &s_cmd.leds[led_index];
    if (led->timed_pending && led->timed_timer) {
        (void)esp_timer_stop(led->timed_timer);
        led->timed_pending = false;
    }

    apply_led(led_index, settings);
    set_led_state(result, led_index);
    return ESP_OK;
}

static esp_err_t handle_led_timed(const uint8_t *params, uint8_t params_len,
                                  eslp_tag_cmd_result_t *result)
{
    /* params: [esl_id, led_index, settings(10), abs_time(4)] */
    if (params_len < 16) {
        set_error(result, BLE_ESL_ERR_INVALID_PARAMS);
        return ESP_OK;
    }
    uint8_t nl = num_leds();
    if (nl == 0) {
        set_error(result, BLE_ESL_ERR_INVALID_OPCODE);
        return ESP_OK;
    }
    uint8_t led_index = params[1];
    if (led_index >= nl || led_index >= ESLP_MAX_LEDS) {
        set_error(result, BLE_ESL_ERR_INVALID_PARAMS);
        return ESP_OK;
    }

    const uint8_t *settings = &params[2];
    uint32_t abs_time = (uint32_t)params[12] | ((uint32_t)params[13] << 8) |
                        ((uint32_t)params[14] << 16) | ((uint32_t)params[15] << 24);

    eslp_led_slot_t *led = &s_cmd.leds[led_index];

    if (abs_time == 0) {
        if (led->timed_pending && led->timed_timer) {
            (void)esp_timer_stop(led->timed_timer);
            led->timed_pending = false;
            refresh_pending_bits();
        }
        set_led_state(result, led_index);
        return ESP_OK;
    }

    esp_ble_esl_abs_time_t now = 0;
    (void)esp_ble_esl_get_current_abs_time(&now);
    uint32_t delta = time_delta_ms(abs_time, now);
    if (delta > ESLP_IMPLAUSIBLE_TIME_MS) {
        set_error(result, BLE_ESL_ERR_IMPLAUSIBLE_ABS_TIME);
        return ESP_OK;
    }

    if (led->timed_pending && led->timed_abs != abs_time) {
        set_error(result, BLE_ESL_ERR_QUEUE_FULL);
        return ESP_OK;
    }

    memcpy(led->settings, settings, sizeof(led->settings));
    led->timed_pending = true;
    led->timed_abs = abs_time;
    refresh_pending_bits();

    if (led->timed_timer) {
        (void)esp_timer_stop(led->timed_timer);
        esp_err_t ret = esp_timer_start_once(led->timed_timer, (uint64_t)delta * 1000ULL);
        if (ret != ESP_OK) {
            led->timed_pending = false;
            refresh_pending_bits();
            set_error(result, BLE_ESL_ERR_INSUFFICIENT_RESOURCES);
            return ESP_OK;
        }
    }

    set_led_state(result, led_index);
    return ESP_OK;
}

static uint8_t num_sensors(void)
{
#ifdef CONFIG_BLE_ESL_SENSOR_INFO
    uint8_t buf[CONFIG_BLE_ESL_SENSOR_INFO_MAX_LEN];
    uint16_t len = 0;
    if (esp_ble_esl_get_sensor_info(buf, sizeof(buf), &len) != ESP_OK || len == 0) {
        return 0;
    }
    uint8_t count = 0;
    uint16_t off = 0;
    while (off < len) {
        uint8_t size = buf[off];
        uint16_t need = (size == BLE_ESL_SENSOR_SIZE_SHORT) ? BLE_ESL_SENSOR_SHORT_LEN :
                        (size == BLE_ESL_SENSOR_SIZE_LONG) ? BLE_ESL_SENSOR_LONG_LEN : 0;
        if (need == 0 || (uint16_t)(off + need) > len) {
            break;
        }
        count++;
        off = (uint16_t)(off + need);
    }
    return count;
#else
    return 0;
#endif
}

static esp_err_t handle_read_sensor(const uint8_t *params, uint8_t params_len,
                                    eslp_tag_cmd_result_t *result, bool pawr)
{
    /* params: [esl_id, sensor_index] */
    if (params_len < 2) {
        set_error(result, BLE_ESL_ERR_INVALID_PARAMS);
        return ESP_OK;
    }
#ifndef CONFIG_BLE_ESL_SENSOR_INFO
    set_error(result, BLE_ESL_ERR_INVALID_OPCODE);
    return ESP_OK;
#else
    uint8_t nsensors = num_sensors();
    if (nsensors == 0) {
        set_error(result, BLE_ESL_ERR_INVALID_OPCODE);
        return ESP_OK;
    }

    uint8_t sensor_index = params[1];
    if (sensor_index >= nsensors) {
        set_error(result, BLE_ESL_ERR_INVALID_PARAMS);
        return ESP_OK;
    }

    esp_ble_eslp_sensor_read_t ev = { .sensor_index = sensor_index };
    post_event(BLE_ESLP_EVENT_SENSOR_READ, &ev, sizeof(ev));

    if (pawr) {
        /* PAwR timing: ask AP to retry over ECP */
        set_error(result, BLE_ESL_ERR_RETRY);
        return ESP_OK;
    }

    s_cmd.sensor_pending = true;
    s_cmd.sensor_pending_index = sensor_index;
    result->has_response = false;
    return ESP_OK;
#endif
}

static void tag_cmd_delete_timers(void)
{
    for (int i = 0; i < ESLP_MAX_DISPLAYS; i++) {
        if (s_cmd.displays[i].timed_timer) {
            (void)esp_timer_delete(s_cmd.displays[i].timed_timer);
            s_cmd.displays[i].timed_timer = NULL;
        }
    }
    for (int i = 0; i < ESLP_MAX_LEDS; i++) {
        if (s_cmd.leds[i].timed_timer) {
            (void)esp_timer_delete(s_cmd.leds[i].timed_timer);
            s_cmd.leds[i].timed_timer = NULL;
        }
        if (s_cmd.leds[i].pattern_timer) {
            (void)esp_timer_delete(s_cmd.leds[i].pattern_timer);
            s_cmd.leds[i].pattern_timer = NULL;
        }
    }
}

esp_err_t eslp_tag_cmd_init(void)
{
    if (s_cmd.inited) {
        return ESP_ERR_INVALID_STATE;
    }
    memset(&s_cmd, 0, sizeof(s_cmd));

    for (int i = 0; i < ESLP_MAX_DISPLAYS; i++) {
        const esp_timer_create_args_t args = {
            .callback = display_timed_cb,
            .arg = (void *)(uintptr_t)i,
            .dispatch_method = ESP_TIMER_TASK,
            .name = "eslp_disp",
        };
        if (esp_timer_create(&args, &s_cmd.displays[i].timed_timer) != ESP_OK) {
            ESP_LOGE(TAG, "display %d timed timer create failed", i);
            tag_cmd_delete_timers();
            return ESP_ERR_NO_MEM;
        }
    }
    for (int i = 0; i < ESLP_MAX_LEDS; i++) {
        const esp_timer_create_args_t targs = {
            .callback = led_timed_cb,
            .arg = (void *)(uintptr_t)i,
            .dispatch_method = ESP_TIMER_TASK,
            .name = "eslp_led_t",
        };
        const esp_timer_create_args_t pargs = {
            .callback = led_pattern_timer_cb,
            .arg = (void *)(uintptr_t)i,
            .dispatch_method = ESP_TIMER_TASK,
            .name = "eslp_led_p",
        };
        if (esp_timer_create(&targs, &s_cmd.leds[i].timed_timer) != ESP_OK) {
            ESP_LOGE(TAG, "LED %d timed timer create failed", i);
            tag_cmd_delete_timers();
            return ESP_ERR_NO_MEM;
        }
        if (esp_timer_create(&pargs, &s_cmd.leds[i].pattern_timer) != ESP_OK) {
            ESP_LOGE(TAG, "LED %d pattern timer create failed", i);
            tag_cmd_delete_timers();
            return ESP_ERR_NO_MEM;
        }
    }

    s_cmd.inited = true;
    return ESP_OK;
}

esp_err_t eslp_tag_cmd_deinit(void)
{
    if (!s_cmd.inited) {
        return ESP_ERR_INVALID_STATE;
    }
    eslp_tag_cmd_cancel_pending();
    tag_cmd_delete_timers();
    memset(&s_cmd, 0, sizeof(s_cmd));
    s_basic_state_ptr = NULL;
    return ESP_OK;
}

void eslp_tag_cmd_cancel_pending(void)
{
    if (!s_cmd.inited) {
        return;
    }
    for (int i = 0; i < ESLP_MAX_DISPLAYS; i++) {
        if (s_cmd.displays[i].timed_timer) {
            (void)esp_timer_stop(s_cmd.displays[i].timed_timer);
        }
        s_cmd.displays[i].timed_pending = false;
    }
    for (int i = 0; i < ESLP_MAX_LEDS; i++) {
        if (s_cmd.leds[i].timed_timer) {
            (void)esp_timer_stop(s_cmd.leds[i].timed_timer);
        }
        if (s_cmd.leds[i].pattern_timer) {
            (void)esp_timer_stop(s_cmd.leds[i].pattern_timer);
        }
        s_cmd.leds[i].timed_pending = false;
        s_cmd.leds[i].active = false;
    }
    s_cmd.sensor_pending = false;
    refresh_pending_bits();
}

esp_err_t eslp_tag_cmd_handle(const esp_ble_eslp_ecp_command_t *cmd,
                              eslp_tag_cmd_result_t *result, bool pawr)
{
    if (!s_cmd.inited || !cmd || !result) {
        return ESP_ERR_INVALID_STATE;
    }
    memset(result, 0, sizeof(*result));

    switch (cmd->opcode) {
    case BLE_ESL_CMD_DISPLAY_IMAGE:
        return handle_display_image(cmd->params, cmd->params_len, result);
    case BLE_ESL_CMD_REFRESH_DISPLAY:
        return handle_refresh_display(cmd->params, cmd->params_len, result);
    case BLE_ESL_CMD_DISPLAY_TIMED_IMAGE:
        return handle_display_timed(cmd->params, cmd->params_len, result);
    case BLE_ESL_CMD_LED_CONTROL:
        return handle_led_control(cmd->params, cmd->params_len, result);
    case BLE_ESL_CMD_LED_TIMED_CONTROL:
        return handle_led_timed(cmd->params, cmd->params_len, result);
    case BLE_ESL_CMD_READ_SENSOR:
        return handle_read_sensor(cmd->params, cmd->params_len, result, pawr);
    default:
        return ESP_ERR_NOT_SUPPORTED;
    }
}

esp_err_t esp_ble_eslp_indicate_display_state(uint8_t display_index, uint8_t image_index)
{
    uint8_t tlv[3] = { BLE_ESL_RESP_DISPLAY_STATE, display_index, image_index };
    return esp_ble_eslp_indicate_ecp_response(tlv, sizeof(tlv));
}

esp_err_t esp_ble_eslp_indicate_led_state(uint8_t led_index)
{
    uint8_t tlv[2] = { BLE_ESL_RESP_LED_STATE, led_index };
    return esp_ble_eslp_indicate_ecp_response(tlv, sizeof(tlv));
}

esp_err_t esp_ble_eslp_report_sensor_data(uint8_t sensor_index, uint8_t error_code,
                                          const uint8_t *data, uint8_t data_len)
{
    if (!s_cmd.inited) {
        return ESP_ERR_INVALID_STATE;
    }
    if (!s_cmd.sensor_pending || s_cmd.sensor_pending_index != sensor_index) {
        return ESP_ERR_INVALID_STATE;
    }
    s_cmd.sensor_pending = false;

    if (error_code != 0) {
        return esp_ble_eslp_indicate_error(error_code);
    }
    if (!data || data_len == 0 || data_len > 15) {
        return ESP_ERR_INVALID_ARG;
    }

    /* Sensor Value: Tag=0xE, Length = data_len (params = sensor_index + data) */
    uint8_t params_len = (uint8_t)(1 + data_len);
    uint8_t length_nibble = (uint8_t)(params_len - 1);
    uint8_t tlv[BLE_ESL_ECP_MAX_SIZE];
    tlv[0] = (uint8_t)((length_nibble << 4) | (BLE_ESL_RESP_SENSOR_VALUE & 0x0F));
    tlv[1] = sensor_index;
    memcpy(&tlv[2], data, data_len);
    return esp_ble_eslp_indicate_ecp_response(tlv, (uint16_t)(2 + data_len));
}
