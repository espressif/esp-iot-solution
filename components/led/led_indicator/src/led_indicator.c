/*
 * SPDX-FileCopyrightText: 2023-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <inttypes.h>
#include <math.h>
#include <stdbool.h>
#include <string.h>
#include <sys/queue.h>
#include "esp_log.h"
#include "led_indicator.h"
#include "led_gamma.h"
#include "led_convert.h"
#include "led_common.h"

static const char *TAG = "led_indicator";

#define MAX3(x, y, z) ((x) > (y) ? ((x) > (z) ? (x) : (z)) : ((y) > (z) ? (y) : (z)))

#define BRIGHTNESS_TICKS   CONFIG_BRIGHTNESS_TICKS
#define BRIGHTNESS_MAX     UINT8_MAX
#define BRIGHTNESS_MIN     0
#define NULL_ACTIVE_BLINK  -1
#define NULL_PREEMPT_BLINK -1

typedef struct {
    uint16_t hue;
    uint8_t saturation;
} HS_color_t;

static const HS_color_t temp_table[] = {
    {4, 100},  {8, 100},  {11, 100}, {14, 100}, {16, 100}, {18, 100}, {20, 100}, {22, 100}, {24, 100}, {25, 100},
    {27, 100}, {28, 100}, {30, 100}, {31, 100}, {31, 95},  {30, 89},  {30, 85},  {29, 80},  {29, 76},  {29, 73},
    {29, 69},  {28, 66},  {28, 63},  {28, 60},  {28, 57},  {28, 54},  {28, 52},  {27, 49},  {27, 47},  {27, 45},
    {27, 43},  {27, 41},  {27, 39},  {27, 37},  {27, 35},  {27, 33},  {27, 31},  {27, 30},  {27, 28},  {27, 26},
    {27, 25},  {27, 23},  {27, 22},  {27, 21},  {27, 19},  {27, 18},  {27, 17},  {27, 15},  {28, 14},  {28, 13},
    {28, 12},  {29, 10},  {29, 9},   {30, 8},   {31, 7},   {32, 6},   {34, 5},   {36, 4},   {41, 3},   {49, 2},
    {0, 0},    {294, 2},  {265, 3},  {251, 4},  {242, 5},  {237, 6},  {233, 7},  {231, 8},  {229, 9},  {228, 10},
    {227, 11}, {226, 11}, {226, 12}, {225, 13}, {225, 13}, {224, 14}, {224, 14}, {224, 15}, {224, 15}, {223, 16},
    {223, 16}, {223, 17}, {223, 17}, {223, 17}, {222, 18}, {222, 18}, {222, 19}, {222, 19}, {222, 19}, {222, 19},
    {222, 20}, {222, 20}, {222, 20}, {222, 21}, {222, 21}
};

/**
 * @brief LED indicator object
 *
 */

typedef struct _led_indicator_slist_t {
    SLIST_ENTRY(_led_indicator_slist_t) next;  /*!< Pointer to the next element in the singly linked list */
    _led_indicator_t *p_led_indicator;         /*!< Pointer to the LED indicator structure */
} _led_indicator_slist_t;

static SLIST_HEAD(_led_indicator_head_t, _led_indicator_slist_t) s_led_indicator_slist_head = SLIST_HEAD_INITIALIZER(s_led_indicator_slist_head);

esp_err_t _led_indicator_add_node(_led_indicator_t *p_led_indicator)
{
    LED_INDICATOR_CHECK(p_led_indicator != NULL, "pointer can not be NULL", return ESP_ERR_INVALID_ARG);
    _led_indicator_slist_t *node = calloc(1, sizeof(_led_indicator_slist_t));
    LED_INDICATOR_CHECK(node != NULL, "calloc node failed", return ESP_ERR_NO_MEM);
    node->p_led_indicator = p_led_indicator;
    SLIST_INSERT_HEAD(&s_led_indicator_slist_head, node, next);
    return ESP_OK;
}

static esp_err_t _led_indicator_remove_node(_led_indicator_t *p_led_indicator)
{
    LED_INDICATOR_CHECK(p_led_indicator != NULL, "pointer can not be NULL", return ESP_ERR_INVALID_ARG);
    _led_indicator_slist_t *node;
    SLIST_FOREACH(node, &s_led_indicator_slist_head, next) {
        if (node->p_led_indicator == p_led_indicator) {
            SLIST_REMOVE(&s_led_indicator_slist_head, node, _led_indicator_slist_t, next);
            free(node);
            break;
        }
    }
    return ESP_OK;
}

static uint32_t _ihsv_convert_to_gamma(uint32_t ihsv_value)
{
    led_indicator_ihsv_t ihsv = {
        .value = ihsv_value,
    };
    ihsv.v = led_indicator_get_gamma_value(ihsv.v);
    return ihsv.value;
}

static uint32_t _irgb_convert_to_gamma(uint32_t irgb_value)
{
    led_indicator_irgb_t irgb = {
        .value = irgb_value,
    };
    irgb.r = _ihsv_convert_to_gamma(irgb.r);
    irgb.g = _ihsv_convert_to_gamma(irgb.g);
    irgb.b = _ihsv_convert_to_gamma(irgb.b);
    return irgb.value;
}

/**
 * @brief switch to the first high priority incomplete blink steps
 *
 * @param p_led_indicator pointer to LED indicator
 */
static void _blink_list_switch(_led_indicator_t *p_led_indicator)
{
    if (p_led_indicator->preempt_blink != NULL_PREEMPT_BLINK) {
        p_led_indicator->active_blink = p_led_indicator->preempt_blink; //jump in blink list
        return;
    }
    p_led_indicator->active_blink = NULL_ACTIVE_BLINK; //stop active blink
    for (size_t index = 0; index < p_led_indicator->blink_list_num; index ++) { //find the first incomplete blink
        if (p_led_indicator->p_blink_steps[index] != LED_BLINK_STOP) {
            p_led_indicator->active_blink = index;
            break;
        }
    }
}

/**
 * @brief timer callback to control LED and counter steps
 *
 * @param xTimer handle of the timer instance
 */
static void _blink_list_runner(TimerHandle_t xTimer)
{
    bool leave = false;
    uint32_t hardware_level;
    bool timer_restart = false;
    TickType_t timer_period_ms = 0;
    _led_indicator_t *p_led_indicator = (_led_indicator_t *)pvTimerGetTimerID(xTimer);
    if (p_led_indicator == NULL) {
        return;
    }

    if (pdTRUE != xSemaphoreTake(p_led_indicator->mutex, 0)) {
        // In most cases, the semaphore should be taken successfully.
        // If not, it means that the blinks is changing, or user prepares to delete the indicator.
        xTimerChangePeriod(p_led_indicator->h_timer, pdMS_TO_TICKS(50), 0);
        xTimerStart(p_led_indicator->h_timer, 0);
        ESP_LOGV(TAG, "timeout restart, period: %d ms", 50);
        return;
    }

    while (!leave) {
        if (p_led_indicator->active_blink == NULL_ACTIVE_BLINK) {
            break;
        }

        int active_blink = p_led_indicator->active_blink;
        int active_step = p_led_indicator->p_blink_steps[active_blink];
        const blink_step_t *p_blink_step =  &p_led_indicator->blink_lists[active_blink][active_step];
        led_indicator_ihsv_t p_blink_step_value = {
            .value = p_blink_step->value,
        };

        // No advance loading actions in breathe mode
        if (p_blink_step->type != LED_BLINK_BREATHE && p_blink_step->type != LED_BLINK_RGB_RING && p_blink_step->type != LED_BLINK_HSV_RING) {
            p_led_indicator->p_blink_steps[active_blink] += 1;
        }

        switch (p_blink_step->type) {
        case LED_BLINK_LOOP:
            p_led_indicator->p_blink_steps[active_blink] = 0;
            break;

        case LED_BLINK_STOP:
            p_led_indicator->p_blink_steps[active_blink] = LED_BLINK_STOP;
            if (p_led_indicator->preempt_blink != NULL_PREEMPT_BLINK) {
                p_led_indicator->preempt_blink = NULL_PREEMPT_BLINK;
            }
            _blink_list_switch(p_led_indicator);
            break;

        case LED_BLINK_HOLD: {
            if (!p_led_indicator->hal_indicator_set_on_off) {
                ESP_LOGW(TAG, "LED_BLINK_HOLD Skip: no hal_indicator_set_on_off function");
                break;
            }
            hardware_level = p_blink_step_value.v ? 1 : 0;
            p_led_indicator->hal_indicator_set_on_off(p_led_indicator->hardware_data, hardware_level);

            p_led_indicator->current_fade_value.v = hardware_level ? LED_STATE_ON : LED_STATE_OFF;
            p_led_indicator->last_fade_value = p_led_indicator->current_fade_value;
            if (p_blink_step->hold_time_ms == 0) {
                break;
            }

            leave = true;
            timer_restart = true;
            timer_period_ms = p_blink_step->hold_time_ms;
            break;
        }

        case LED_BLINK_RGB: {
            if (!p_led_indicator->hal_indicator_set_rgb) {
                ESP_LOGW(TAG, "LED_BLINK_RGB Skip: no hal_indicator_set_rgb function");
                break;
            }
            p_led_indicator->hal_indicator_set_rgb(p_led_indicator->hardware_data, _irgb_convert_to_gamma(p_blink_step_value.value));
            p_led_indicator->current_fade_value.value = led_indicator_rgb2hsv(p_blink_step_value.value);

            p_led_indicator->current_fade_value.i = p_blink_step_value.i;
            p_led_indicator->last_fade_value = p_led_indicator->current_fade_value;
            if (p_blink_step->hold_time_ms == 0) {
                break;
            }

            leave = true;
            timer_restart = true;
            timer_period_ms = p_blink_step->hold_time_ms;
            break;
        }

        case LED_BLINK_RGB_RING: {
            if (!p_led_indicator->hal_indicator_set_rgb) {
                ESP_LOGW(TAG, "LED_BLINK_RGB_RING Skip: no hal_indicator_set_hsv function");
                break;
            }

            led_indicator_irgb_t currect_rgb_value, last_rgb_value = {0};
            uint16_t ticks = BRIGHTNESS_TICKS;
            int16_t diff[3] = {0};

            led_indicator_ihsv_t hsv_value = {
                .value = led_indicator_rgb2hsv(p_blink_step_value.value),
            };

            if (p_blink_step->hold_time_ms == 0) {
                p_led_indicator->hal_indicator_set_rgb(p_led_indicator->hardware_data, _irgb_convert_to_gamma(p_blink_step_value.value));
                p_led_indicator->current_fade_value.value = hsv_value.value;
                p_led_indicator->current_fade_value.i = p_blink_step_value.i;
                p_led_indicator->last_fade_value = p_led_indicator->current_fade_value;
                p_led_indicator->p_blink_steps[active_blink] += 1;
                break;
            }

            uint32_t r, g, b = 0;
            /*!< Get the last fade value's RGB */
            led_indicator_hsv2rgb(p_led_indicator->last_fade_value.value, &r, &g, &b);
            last_rgb_value.value = SET_IRGB(p_blink_step_value.i, r, g, b),
            currect_rgb_value.value = p_blink_step_value.value;
            diff[0] = currect_rgb_value.r - last_rgb_value.r;
            diff[1] = currect_rgb_value.g - last_rgb_value.g;
            diff[2] = currect_rgb_value.b - last_rgb_value.b;
            int16_t max_diff = MAX3(abs(diff[0]), abs(diff[1]), abs(diff[2]));

            /*!< Calculate total steps and timer ticks. */
            if (max_diff == 0) {
                ticks = p_blink_step->hold_time_ms;
                p_led_indicator->fade_total_step = 1;
            } else if (p_blink_step->hold_time_ms > ticks * abs(max_diff)) {
                ticks = p_blink_step->hold_time_ms / abs(max_diff) ;
                p_led_indicator->fade_total_step = max_diff;
            } else {
                p_led_indicator->fade_total_step = p_blink_step->hold_time_ms / ticks;
            }

            p_led_indicator->fade_step += 1;
            ESP_LOGD(TAG, "ticks value: %d, total fade step: %d, fade step: %d", ticks, p_led_indicator->fade_total_step, p_led_indicator->fade_step);

            currect_rgb_value.r = (uint8_t)(last_rgb_value.r + diff[0] * p_led_indicator->fade_step * 1.0 / p_led_indicator->fade_total_step);
            currect_rgb_value.g = (uint8_t)(last_rgb_value.g + diff[1] * p_led_indicator->fade_step * 1.0 / p_led_indicator->fade_total_step);
            currect_rgb_value.b = (uint8_t)(last_rgb_value.b + diff[2] * p_led_indicator->fade_step * 1.0 / p_led_indicator->fade_total_step);
            ESP_LOGD(TAG, "currect_rgb_value: [%d, %d, %d]\n", currect_rgb_value.r, currect_rgb_value.g, currect_rgb_value.b);

            p_led_indicator->hal_indicator_set_rgb(p_led_indicator->hardware_data, _irgb_convert_to_gamma(currect_rgb_value.value));

            leave = true;
            timer_restart = true;
            timer_period_ms = ticks;

            if (p_led_indicator->fade_step >= p_led_indicator->fade_total_step) {
                p_led_indicator->fade_step = 0;
                p_led_indicator->fade_total_step = 0;
                p_led_indicator->current_fade_value.value = hsv_value.value;
                p_led_indicator->current_fade_value.i = p_blink_step_value.i;
                p_led_indicator->last_fade_value = p_led_indicator->current_fade_value;
                p_led_indicator->p_blink_steps[active_blink] += 1;
            }

            break;
        }

        case LED_BLINK_HSV: {
            if (!p_led_indicator->hal_indicator_set_hsv) {
                ESP_LOGW(TAG, "LED_BLINK_HSV Skip: no hal_indicator_set_hsv function");
                break;
            }
            p_led_indicator->hal_indicator_set_hsv(p_led_indicator->hardware_data, _ihsv_convert_to_gamma(p_blink_step_value.value));
            p_led_indicator->current_fade_value = p_blink_step_value;
            p_led_indicator->last_fade_value = p_led_indicator->current_fade_value;
            if (p_blink_step->hold_time_ms == 0) {
                break;
            }

            leave = true;
            timer_restart = true;
            timer_period_ms = p_blink_step->hold_time_ms;
            break;
        }

        case LED_BLINK_HSV_RING: {
            if (!p_led_indicator->hal_indicator_set_hsv) {
                ESP_LOGW(TAG, "LED_BLINK_HSV_RING Skip: no hal_indicator_set_hsv function");
                break;
            }

            uint16_t ticks = BRIGHTNESS_TICKS;
            int16_t diff[3] = {0};

            if (p_blink_step->hold_time_ms == 0) {
                p_led_indicator->hal_indicator_set_hsv(p_led_indicator->hardware_data, _ihsv_convert_to_gamma(p_blink_step_value.value));
                p_led_indicator->current_fade_value = p_blink_step_value;
                p_led_indicator->last_fade_value = p_led_indicator->current_fade_value;
                p_led_indicator->p_blink_steps[active_blink] += 1;
                break;
            }

            diff[0] = p_blink_step_value.h - p_led_indicator->last_fade_value.h;
            diff[1] = p_blink_step_value.s - p_led_indicator->last_fade_value.s;
            diff[2] = p_blink_step_value.v - p_led_indicator->last_fade_value.v;
            int16_t max_diff = MAX3(abs(diff[0]), abs(diff[1]), abs(diff[2]));

            if (max_diff == 0) {
                ticks = p_blink_step->hold_time_ms;
                p_led_indicator->fade_total_step = 1;
            } else if (p_blink_step->hold_time_ms > ticks * abs(max_diff)) {
                ticks = p_blink_step->hold_time_ms / abs(max_diff) ;
                p_led_indicator->fade_total_step = max_diff;
            } else {
                p_led_indicator->fade_total_step = p_blink_step->hold_time_ms / ticks;
            }

            p_led_indicator->fade_step += 1;
            ESP_LOGD(TAG, "hsv ring ticks value: %d, total fade step: %d, fade step: %d", ticks, p_led_indicator->fade_total_step, p_led_indicator->fade_step);

            p_led_indicator->current_fade_value.h = (uint32_t)(p_led_indicator->last_fade_value.h + diff[0] * p_led_indicator->fade_step * 1.0 / p_led_indicator->fade_total_step);
            p_led_indicator->current_fade_value.s = (uint8_t)(p_led_indicator->last_fade_value.s + diff[1] * p_led_indicator->fade_step * 1.0 / p_led_indicator->fade_total_step);
            p_led_indicator->current_fade_value.v = (uint8_t)(p_led_indicator->last_fade_value.v + diff[2] * p_led_indicator->fade_step * 1.0 / p_led_indicator->fade_total_step);
            ESP_LOGD(TAG, "current_fade_value: [%d, %d, %d]\n", p_led_indicator->current_fade_value.h, p_led_indicator->current_fade_value.s, p_led_indicator->current_fade_value.v);

            p_led_indicator->current_fade_value.i = p_blink_step_value.i;
            p_led_indicator->hal_indicator_set_hsv(p_led_indicator->hardware_data, _ihsv_convert_to_gamma(p_led_indicator->current_fade_value.value));

            leave = true;
            timer_restart = true;
            timer_period_ms = ticks;

            if (p_led_indicator->fade_step >= p_led_indicator->fade_total_step) {
                p_led_indicator->fade_step = 0;
                p_led_indicator->fade_total_step = 0;
                p_led_indicator->current_fade_value = p_blink_step_value;
                p_led_indicator->last_fade_value = p_blink_step_value;
                p_led_indicator->p_blink_steps[active_blink] += 1;
            }

            break;
        }

        case LED_BLINK_BRIGHTNESS: {
            if (!p_led_indicator->hal_indicator_set_brightness) {
                ESP_LOGW(TAG, "LED_BLINK_BRIGHTNESS Skip: no hal_indicator_set_brightness function");
                break;
            }

            p_led_indicator->current_fade_value.v = p_blink_step_value.v;
            p_led_indicator->current_fade_value.i = p_blink_step_value.i;
            p_led_indicator->hal_indicator_set_brightness(p_led_indicator->hardware_data, _ihsv_convert_to_gamma(p_led_indicator->current_fade_value.value));
            p_led_indicator->last_fade_value = p_led_indicator->current_fade_value;
            if (p_blink_step->hold_time_ms == 0) {
                break;
            }

            leave = true;
            timer_restart = true;
            timer_period_ms = p_blink_step->hold_time_ms;
            break;
        }

        case LED_BLINK_BREATHE: {
            if (!p_led_indicator->hal_indicator_set_brightness) {
                ESP_LOGW(TAG, "LED_BLINK_BREATHE Skip: no hal_indicator_set_brightness function");
                break;
            }

            uint32_t brightness_value = p_blink_step_value.v;
            uint16_t ticks = BRIGHTNESS_TICKS;
            int16_t diff_value = brightness_value - p_led_indicator->last_fade_value.v;
            p_led_indicator->current_fade_value.i = p_blink_step_value.i;

            // diff_brightness > 0: Fade increase
            // diff_brightness < 0: Fade decrease
            if (p_blink_step->hold_time_ms == 0) {
                p_led_indicator->current_fade_value.v = brightness_value;
                p_led_indicator->hal_indicator_set_brightness(p_led_indicator->hardware_data, _ihsv_convert_to_gamma(p_led_indicator->current_fade_value.value));
                p_led_indicator->last_fade_value = p_led_indicator->current_fade_value;
                p_led_indicator->p_blink_steps[active_blink] += 1;
                break;
            }

            if (diff_value >= 0) {
                brightness_value = p_led_indicator->fade_value_count + p_led_indicator->last_fade_value.v;
            } else {
                brightness_value = p_led_indicator->last_fade_value.v - p_led_indicator->fade_value_count;
            }
            p_led_indicator->current_fade_value.v = brightness_value;
            p_led_indicator->hal_indicator_set_brightness(p_led_indicator->hardware_data, _ihsv_convert_to_gamma(p_led_indicator->current_fade_value.value));

            if (diff_value == 0) {
                ticks = p_blink_step->hold_time_ms;
                p_led_indicator->fade_value_count += 1;
            } else if (p_blink_step->hold_time_ms > BRIGHTNESS_TICKS * abs(diff_value)) {
                ticks = p_blink_step->hold_time_ms /  abs(diff_value) ;
                p_led_indicator->fade_value_count += 1;
            } else {
                p_led_indicator->fade_value_count += abs(diff_value) * ticks / p_blink_step->hold_time_ms;
            }

            ESP_LOGD(TAG, "breathe ticks value: %d", ticks);
            leave = true;
            timer_restart = true;
            timer_period_ms = ticks;

            if (p_led_indicator->fade_value_count > abs(diff_value)) {
                p_led_indicator->fade_value_count = BRIGHTNESS_MIN;
                p_led_indicator->current_fade_value.v = p_blink_step_value.v;
                p_led_indicator->last_fade_value = p_led_indicator->current_fade_value;
                p_led_indicator->p_blink_steps[active_blink] += 1;
            }

            break;
        }

        default:
            assert(false && "invalid state");
            break;
        }
    }
    // check if the indicator is deleted
    p_led_indicator = (_led_indicator_t *)pvTimerGetTimerID(xTimer);
    if (p_led_indicator && timer_restart && timer_period_ms) {
        xTimerChangePeriod(p_led_indicator->h_timer, pdMS_TO_TICKS(timer_period_ms), 0);
        xTimerStart(p_led_indicator->h_timer, 0);
        ESP_LOGV(TAG, "timer restart, period: %" PRIu32 " ms", timer_period_ms);
    }
    xSemaphoreGive(p_led_indicator->mutex);
}

_led_indicator_t *_led_indicator_create_com(_led_indicator_com_config_t *cfg)
{
    LED_INDICATOR_CHECK(NULL != cfg, "com config can't be NULL", return  NULL);

    char timer_name[16] = {'\0'};
    snprintf(timer_name, sizeof(timer_name) - 1, "%s%"PRIu32"", "led_tmr_", (uint32_t)cfg->hardware_data);
    _led_indicator_t *p_led_indicator = (_led_indicator_t *)calloc(1, sizeof(_led_indicator_t));
    LED_INDICATOR_CHECK(p_led_indicator != NULL, "calloc indicator memory failed", return NULL);
    p_led_indicator->hardware_data = cfg->hardware_data;
    p_led_indicator->hal_indicator_set_on_off = cfg->hal_indicator_set_on_off;
    p_led_indicator->hal_indicator_deinit = cfg->hal_indicator_deinit;
    p_led_indicator->hal_indicator_set_brightness = cfg->hal_indicator_set_brightness;
    p_led_indicator->hal_indicator_set_rgb = cfg->hal_indicator_set_rgb;
    p_led_indicator->hal_indicator_set_hsv = cfg->hal_indicator_set_hsv;
    p_led_indicator->active_blink = NULL_ACTIVE_BLINK;
    p_led_indicator->max_duty = pow(2, cfg->duty_resolution) - 1;
    p_led_indicator->preempt_blink = NULL_PREEMPT_BLINK;
    p_led_indicator->blink_lists = cfg->blink_lists;
    p_led_indicator->p_blink_steps = (int *)calloc(cfg->blink_list_num, sizeof(int));
    LED_INDICATOR_CHECK_WARNING(p_led_indicator->hal_indicator_set_on_off != NULL, "LED indicator does not have the hal_indicator_set_on_off function",);
    LED_INDICATOR_CHECK_WARNING(p_led_indicator->hal_indicator_deinit != NULL, "LED indicator does not have the hal_indicator_deinit function",);
    LED_INDICATOR_CHECK_WARNING(p_led_indicator->p_blink_steps != NULL, "calloc blink_steps memory failed", goto cleanup_indicator);
    for (size_t j = 0; j < cfg->blink_list_num; j++) {
        *(p_led_indicator->p_blink_steps + j) = LED_BLINK_STOP;
    }

    p_led_indicator->blink_list_num = cfg->blink_list_num;
    p_led_indicator->mutex = xSemaphoreCreateMutex();
    LED_INDICATOR_CHECK(p_led_indicator->mutex != NULL, "create mutex failed", goto cleanup_indicator_blinkstep);
    p_led_indicator->h_timer = xTimerCreate(timer_name, (pdMS_TO_TICKS(100)), pdFALSE, (void *)p_led_indicator, _blink_list_runner);
    LED_INDICATOR_CHECK(p_led_indicator->h_timer != NULL, "LED timer create failed", goto cleanup_all);

    return p_led_indicator;

cleanup_indicator:
    free(p_led_indicator);
    return NULL;
cleanup_indicator_blinkstep:
    free(p_led_indicator->p_blink_steps);
    free(p_led_indicator);
    return NULL;
cleanup_all:
    vSemaphoreDelete(p_led_indicator->mutex);
    free(p_led_indicator->p_blink_steps);
    free(p_led_indicator);
    return NULL;
}

static esp_err_t _led_indicator_delete_com(_led_indicator_t *p_led_indicator)
{
    esp_err_t err;
    vTimerSetTimerID(p_led_indicator->h_timer, NULL);
    // wait until the timmer is stopped before release resources
    int timeout_ms = 200;
    do {
        xTimerStop(p_led_indicator->h_timer, portMAX_DELAY);
        vTaskDelay(10 / portTICK_PERIOD_MS);
        timeout_ms -= 10;
    } while (xTimerIsTimerActive(p_led_indicator->h_timer) == pdTRUE && timeout_ms > 0);

    xSemaphoreTake(p_led_indicator->mutex, portMAX_DELAY);
    xTimerDelete(p_led_indicator->h_timer, portMAX_DELAY);
    p_led_indicator->h_timer = NULL;

    for (int i = 0; i < p_led_indicator->blink_list_num; i++) {
        p_led_indicator->p_blink_steps[i] = LED_BLINK_STOP;
    }

    LED_INDICATOR_CHECK_WARNING(NULL != p_led_indicator->hal_indicator_deinit, "LED indicator not set deinit", goto not_deinit);
    err = p_led_indicator->hal_indicator_deinit(p_led_indicator->hardware_data);
    LED_INDICATOR_CHECK(err == ESP_OK, "LED indicator deinit failed", return ESP_FAIL);
not_deinit:
    _led_indicator_remove_node(p_led_indicator);
    xSemaphoreGive(p_led_indicator->mutex);
    vSemaphoreDelete(p_led_indicator->mutex);
    p_led_indicator->mutex = NULL;
    free(p_led_indicator->p_blink_steps);
    free(p_led_indicator);
    p_led_indicator = NULL;

    return ESP_OK;
}

esp_err_t led_indicator_delete(led_indicator_handle_t handle)
{
    LED_INDICATOR_CHECK(handle != NULL, "invalid p_handle", return ESP_ERR_INVALID_ARG);
    _led_indicator_t *p_led_indicator = (_led_indicator_t *)handle;
    _led_indicator_delete_com(p_led_indicator);
    return ESP_OK;
}

esp_err_t led_indicator_start(led_indicator_handle_t handle, int blink_type)
{
    LED_INDICATOR_CHECK(handle != NULL, "invalid p_handle", return ESP_ERR_INVALID_ARG);
    _led_indicator_t *p_led_indicator = (_led_indicator_t *)handle;
    LED_INDICATOR_CHECK(blink_type >= 0 && blink_type < p_led_indicator->blink_list_num, "blink_type out of range", return ESP_FAIL);
    LED_INDICATOR_CHECK(p_led_indicator->blink_lists[blink_type] != NULL, "undefined blink_type", return ESP_ERR_INVALID_ARG);
    xSemaphoreTake(p_led_indicator->mutex, portMAX_DELAY);
    p_led_indicator->p_blink_steps[blink_type] = 0;
    _blink_list_switch(p_led_indicator);
    xSemaphoreGive(p_led_indicator->mutex);
    if (p_led_indicator->active_blink == blink_type) { //re-run from first step
        xTimerChangePeriod(p_led_indicator->h_timer, 1, 0);
        xTimerStart(p_led_indicator->h_timer, 0);
    }

    return ESP_OK;
}

esp_err_t led_indicator_stop(led_indicator_handle_t handle, int blink_type)
{
    LED_INDICATOR_CHECK(handle != NULL, "invalid p_handle", return ESP_ERR_INVALID_ARG);
    _led_indicator_t *p_led_indicator = (_led_indicator_t *)handle;
    LED_INDICATOR_CHECK(blink_type >= 0 && blink_type < p_led_indicator->blink_list_num, "blink_type out of range", return ESP_FAIL);
    LED_INDICATOR_CHECK(p_led_indicator->blink_lists[blink_type] != NULL, "undefined blink_type", return ESP_ERR_INVALID_ARG);
    xSemaphoreTake(p_led_indicator->mutex, portMAX_DELAY);
    p_led_indicator->p_blink_steps[blink_type] = LED_BLINK_STOP;
    _blink_list_switch(p_led_indicator); //stop and switch to next blink steps
    xSemaphoreGive(p_led_indicator->mutex);

    return ESP_OK;
}

esp_err_t led_indicator_preempt_start(led_indicator_handle_t handle, int blink_type)
{
    LED_INDICATOR_CHECK(handle != NULL, "invalid p_handle", return ESP_ERR_INVALID_ARG);
    _led_indicator_t *p_led_indicator = (_led_indicator_t *)handle;
    LED_INDICATOR_CHECK(blink_type >= 0 && blink_type < p_led_indicator->blink_list_num, "blink_type out of range", return ESP_FAIL);
    LED_INDICATOR_CHECK(p_led_indicator->blink_lists[blink_type] != NULL, "undefined blink_type", return ESP_ERR_INVALID_ARG);

    xSemaphoreTake(p_led_indicator->mutex, portMAX_DELAY);
    if (p_led_indicator->preempt_blink != NULL_PREEMPT_BLINK) {
        p_led_indicator->p_blink_steps[p_led_indicator->preempt_blink] = LED_BLINK_STOP; // Maker sure the last preempt blink is stopped
    }
    p_led_indicator->p_blink_steps[blink_type] = 0;
    p_led_indicator->preempt_blink = blink_type;
    _blink_list_switch(p_led_indicator); //stop and switch to next blink steps
    xSemaphoreGive(p_led_indicator->mutex);

    if (p_led_indicator->active_blink == blink_type) { //re-run from first step
        xTimerChangePeriod(p_led_indicator->h_timer, 1, 0);
        xTimerStart(p_led_indicator->h_timer, 0);
    }
    return ESP_OK;
}

esp_err_t led_indicator_preempt_stop(led_indicator_handle_t handle, int blink_type)
{
    LED_INDICATOR_CHECK(handle != NULL, "invalid p_handle", return ESP_ERR_INVALID_ARG);
    _led_indicator_t *p_led_indicator = (_led_indicator_t *)handle;
    LED_INDICATOR_CHECK(blink_type >= 0 && blink_type < p_led_indicator->blink_list_num, "blink_type out of range", return ESP_FAIL);
    LED_INDICATOR_CHECK(p_led_indicator->blink_lists[blink_type] != NULL, "undefined blink_type", return ESP_ERR_INVALID_ARG);
    xSemaphoreTake(p_led_indicator->mutex, portMAX_DELAY);
    if (p_led_indicator->preempt_blink == blink_type) {
        p_led_indicator->p_blink_steps[blink_type] = LED_BLINK_STOP;
        p_led_indicator->preempt_blink = NULL_PREEMPT_BLINK;
    }
    _blink_list_switch(p_led_indicator); //stop and switch to next blink steps
    xSemaphoreGive(p_led_indicator->mutex);

    return ESP_OK;
}

uint8_t led_indicator_get_brightness(led_indicator_handle_t handle)
{
    LED_INDICATOR_CHECK(handle != NULL, "invalid p_handle", return 0);
    _led_indicator_t *p_led_indicator = (_led_indicator_t *)handle;
    xSemaphoreTake(p_led_indicator->mutex, portMAX_DELAY);
    uint8_t fade_value = p_led_indicator->current_fade_value.v;
    xSemaphoreGive(p_led_indicator->mutex);
    return fade_value;
}

esp_err_t led_indicator_set_on_off(led_indicator_handle_t handle, bool on_off)
{
    LED_INDICATOR_CHECK(handle != NULL, "invalid p_handle", return ESP_ERR_INVALID_ARG);
    _led_indicator_t *p_led_indicator = (_led_indicator_t *)handle;
    if (!p_led_indicator->hal_indicator_set_on_off) {
        ESP_LOGW(TAG, "LED indicator does not have the hal_indicator_set_on_off function");
        return ESP_FAIL;
    }
    xSemaphoreTake(p_led_indicator->mutex, portMAX_DELAY);
    p_led_indicator->hal_indicator_set_on_off(p_led_indicator->hardware_data, on_off);
    p_led_indicator->current_fade_value.v = on_off ? BRIGHTNESS_MAX : BRIGHTNESS_MIN;
    p_led_indicator->last_fade_value = p_led_indicator->current_fade_value;
    xSemaphoreGive(p_led_indicator->mutex);
    return ESP_OK;
}

esp_err_t led_indicator_set_brightness(led_indicator_handle_t handle, uint32_t brightness)
{
    LED_INDICATOR_CHECK(handle != NULL, "invalid p_handle", return ESP_ERR_INVALID_ARG);
    _led_indicator_t *p_led_indicator = (_led_indicator_t *)handle;
    if (!p_led_indicator->hal_indicator_set_brightness) {
        ESP_LOGW(TAG, "LED indicator does not have the hal_indicator_set_brightness function");
        return ESP_FAIL;
    }
    xSemaphoreTake(p_led_indicator->mutex, portMAX_DELAY);
    p_led_indicator->hal_indicator_set_brightness(p_led_indicator->hardware_data, led_indicator_get_gamma_value(brightness));
    led_indicator_ihsv_t ihsv = {
        .value = brightness,
    };
    /*!< Just setting index and brightness */
    p_led_indicator->current_fade_value.i = ihsv.i;
    p_led_indicator->current_fade_value.v = ihsv.v;
    p_led_indicator->last_fade_value = p_led_indicator->current_fade_value;
    xSemaphoreGive(p_led_indicator->mutex);
    return ESP_OK;
}

uint32_t led_indicator_get_hsv(led_indicator_handle_t handle)
{
    LED_INDICATOR_CHECK(handle != NULL, "invalid p_handle", return 0);
    _led_indicator_t *p_led_indicator = (_led_indicator_t *)handle;
    xSemaphoreTake(p_led_indicator->mutex, portMAX_DELAY);
    uint32_t hsv_value = p_led_indicator->current_fade_value.value & 0x1FFFFFFF;
    xSemaphoreGive(p_led_indicator->mutex);
    return hsv_value;
}

esp_err_t led_indicator_set_hsv(led_indicator_handle_t handle, uint32_t ihsv_value)
{
    LED_INDICATOR_CHECK(handle != NULL, "invalid p_handle", return ESP_ERR_INVALID_ARG);
    _led_indicator_t *p_led_indicator = (_led_indicator_t *)handle;
    if (!p_led_indicator->hal_indicator_set_hsv) {
        ESP_LOGW(TAG, "LED indicator does not have the hal_indicator_set_hsv function");
        return ESP_FAIL;
    }
    xSemaphoreTake(p_led_indicator->mutex, portMAX_DELAY);
    p_led_indicator->hal_indicator_set_hsv(p_led_indicator->hardware_data, _ihsv_convert_to_gamma(ihsv_value));
    p_led_indicator->current_fade_value.value = ihsv_value;
    p_led_indicator->last_fade_value.value = ihsv_value;
    xSemaphoreGive(p_led_indicator->mutex);
    return ESP_OK;
}

uint32_t led_indicator_get_rgb(led_indicator_handle_t handle)
{
    LED_INDICATOR_CHECK(handle != NULL, "invalid p_handle", return 0);
    _led_indicator_t *p_led_indicator = (_led_indicator_t *)handle;
    xSemaphoreTake(p_led_indicator->mutex, portMAX_DELAY);
    uint32_t ihsv_value = p_led_indicator->current_fade_value.value;
    xSemaphoreGive(p_led_indicator->mutex);

    uint32_t r, g, b, rgb_value;
    led_indicator_hsv2rgb(ihsv_value, &r, &g, &b);
    rgb_value = (r << 16) | (g << 8) | b;

    return rgb_value;
}

esp_err_t led_indicator_set_rgb(led_indicator_handle_t handle, uint32_t irgb_value)
{
    LED_INDICATOR_CHECK(handle != NULL, "invalid p_handle", return ESP_ERR_INVALID_ARG);
    _led_indicator_t *p_led_indicator = (_led_indicator_t *)handle;
    if (!p_led_indicator->hal_indicator_set_rgb) {
        ESP_LOGW(TAG, "LED indicator does not have the hal_indicator_set_rgb function");
        return ESP_FAIL;
    }
    led_indicator_ihsv_t ihsv = {
        .value = led_indicator_rgb2hsv(irgb_value),
    };
    xSemaphoreTake(p_led_indicator->mutex, portMAX_DELAY);
    p_led_indicator->hal_indicator_set_rgb(p_led_indicator->hardware_data, _irgb_convert_to_gamma(irgb_value));
    p_led_indicator->current_fade_value = ihsv;
    p_led_indicator->current_fade_value.i = GET_INDEX(irgb_value);
    p_led_indicator->last_fade_value = p_led_indicator->current_fade_value;
    xSemaphoreGive(p_led_indicator->mutex);
    return ESP_OK;
}

esp_err_t led_indicator_set_color_temperature(led_indicator_handle_t handle, const uint32_t temperature)
{
    LED_INDICATOR_CHECK(handle != NULL, "invalid p_handle", return ESP_ERR_INVALID_ARG);
    _led_indicator_t *p_led_indicator = (_led_indicator_t *)handle;
    if (!p_led_indicator->hal_indicator_set_hsv) {
        ESP_LOGW(TAG, "LED indicator does not have the hal_indicator_set_hsv function");
        return ESP_FAIL;
    }
    uint16_t hue;
    uint8_t saturation;

    xSemaphoreTake(p_led_indicator->mutex, portMAX_DELAY);
    if ((temperature & 0xFFFFFF) < 600) {
        hue = 0;
        saturation = 100;
    } else if ((temperature & 0xFFFFFF) > 10000) {
        hue = 222;
        saturation = 21 + ((temperature & 0xFFFFFF) - 10000) * 41 / 990000;
    } else {
        hue = temp_table[((temperature & 0xFFFFFF) - 600) / 100].hue;
        saturation = temp_table[((temperature & 0xFFFFFF) - 600) / 100].saturation;
    }
    saturation = (saturation * 255) / 100;

    p_led_indicator->current_fade_value.h = hue;
    p_led_indicator->current_fade_value.s = saturation;
    p_led_indicator->current_fade_value.i = GET_INDEX(temperature);
    p_led_indicator->hal_indicator_set_hsv(p_led_indicator->hardware_data, _ihsv_convert_to_gamma(p_led_indicator->current_fade_value.value));
    p_led_indicator->last_fade_value = p_led_indicator->current_fade_value;
    xSemaphoreGive(p_led_indicator->mutex);
    return ESP_OK;
}
