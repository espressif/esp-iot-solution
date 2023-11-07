/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <inttypes.h>
#include <math.h>
#include <stdbool.h>
#include <string.h>
#include <sys/queue.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"
#include "freertos/semphr.h"
#include "led_custom.h"
#include "led_indicator.h"
#include "led_indicator_blink_default.h"
#include "led_gpio.h"
#include "led_ledc.h"
#include "led_strips.h"
#include "led_gamma.h"

static const char *TAG = "led_indicator";

#define LED_INDICATOR_CHECK(a, str, action) if(!(a)) { \
        ESP_LOGE(TAG,"%s:%d (%s):%s", __FILE__, __LINE__, __FUNCTION__, str); \
        action; \
    }

#define LED_INDICATOR_CHECK_WARNING(a, str, action) if(!(a)) { \
        ESP_LOGW(TAG,"%s:%d (%s):%s", __FILE__, __LINE__, __FUNCTION__, str); \
        action; \
    }

#define BRIGHTNESS_TICKS   CONFIG_BRIGHTNESS_TICKS
#define BRIGHTNESS_MAX     UINT8_MAX
#define BRIGHTNESS_MIN     0
#define NULL_ACTIVE_BLINK  -1
#define NULL_PREEMPT_BLINK -1

static const char *led_indicator_mode_str[4] = {"GPIO mode", "LEDC mode", "LED Strips mode","custom mode"};

/**
 * @brief LED indicator object
 *
 */
typedef struct {
    bool is_active_level_high;                                                            /*!< Set true if GPIO level is high when light is ON, otherwise false. */
    esp_err_t (*hal_indicator_set_on_off)(void *hardware_data, bool on_off);              /*!< Pointer function for setting on or off */
    esp_err_t (*hal_indicator_deinit)(void *hardware_data);                               /*!< Pointer function for Deinitialization */
    esp_err_t (*hal_indicator_set_brightness)(void *hardware_data, uint32_t brightness);  /*!< Pointer function for setting brightness, must be supported by hardware */
    esp_err_t (*hal_indicator_set_rgb)(void *hardware, uint32_t rgb_value);               /*!< Pointer function for setting rgb, must be supported by hardware */
    esp_err_t (*hal_indicator_set_hsv)(void *hardware, uint32_t hsv_value);               /*!< Pointer function for setting hsv, must be supported by hardware */
    void *hardware_data;               /*!< Hardware data of the LED indicator */
    led_indicator_mode_t mode;         /*!< LED work mode, eg. GPIO or pwm mode */
    int active_blink;                  /*!< Active blink list*/
    int preempt_blink;                 /*!< Highest priority blink list*/
    int *p_blink_steps;                /*!< Stage of each blink list */
    uint32_t current_fade_value;       /*!< Current fade value */
    uint32_t last_fade_value;          /*!< Save the last value. */
    uint16_t fade_value_count;         /*!< Count the number of fade */
    uint32_t max_duty;                 /*!< Max duty cycle from duty_resolution : 2^duty_resolution -1 */
    SemaphoreHandle_t mutex;           /*!< Mutex to achieve thread-safe */
    TimerHandle_t h_timer;             /*!< LED timer handle, invalid if works in pwm mode */
    blink_step_t const **blink_lists;  /*!< User defined LED blink lists */
    uint16_t blink_list_num;           /*!< Number of blink lists */
} _led_indicator_t;

typedef struct _led_indicator_slist_t {
    SLIST_ENTRY(_led_indicator_slist_t) next;  /*!< Pointer to the next element in the singly linked list */
    _led_indicator_t *p_led_indicator;         /*!< Pointer to the LED indicator structure */
} _led_indicator_slist_t;

typedef struct _led_indicator_com_config {
    bool is_active_level_high;                                                            /*!< Set true if GPIO level is high when light is ON, otherwise false. */
    esp_err_t (*hal_indicator_set_on_off)(void *hardware_data, bool on_off);              /*!< Pointer function for setting on or off */
    esp_err_t (*hal_indicator_deinit)(void *hardware_data);                               /*!< Pointer function for Deinitialization */
    esp_err_t (*hal_indicator_set_brightness)(void *hardware_data, uint32_t brightness);  /*!< Pointer function for setting brightness, must be supported by hardware */
    esp_err_t (*hal_indicator_set_rgb)(void *hardware, uint32_t rgb_value);               /*!< Pointer function for setting rgb, must be supported by hardware */
    esp_err_t (*hal_indicator_set_hsv)(void *hardware, uint32_t hsv_value);               /*!< Pointer function for setting hsv, must be supported by hardware */
    void *hardware_data;                  /*!< GPIO number of the LED indicator */
    blink_step_t const **blink_lists;     /*!< User defined LED blink lists */
    uint16_t blink_list_num;              /*!< Number of blink lists */
    led_indicator_duty_t duty_resolution; /*!< Resolution of duty setting in number of bits. The range of duty values is [0, (2**duty_resolution) -1]. If the brightness cannot be set, set this as 1. */
} _led_indicator_com_config_t;
static SLIST_HEAD(_led_indicator_head_t, _led_indicator_slist_t) s_led_indicator_slist_head = SLIST_HEAD_INITIALIZER(s_led_indicator_slist_head);

static esp_err_t _led_indicator_add_node(_led_indicator_t *p_led_indicator)
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
            hardware_level = p_blink_step->value ? p_led_indicator->is_active_level_high : !p_led_indicator->is_active_level_high;
            p_led_indicator->hal_indicator_set_on_off(p_led_indicator->hardware_data, hardware_level);
            SET_BRIGHTNESS(p_led_indicator->current_fade_value, p_blink_step->value ? LED_STATE_ON : LED_STATE_OFF);
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
            p_led_indicator->hal_indicator_set_rgb(p_led_indicator->hardware_data, p_blink_step->value);
            p_led_indicator->current_fade_value = led_indicator_strips_rgb2hsv(p_blink_step->value);
            SET_INDEX(p_led_indicator->current_fade_value, GET_INDEX(p_blink_step->value));
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

            uint32_t hsv_value = led_indicator_strips_rgb2hsv(p_blink_step->value);
            uint16_t ticks = BRIGHTNESS_TICKS;
            int16_t diff_value = GET_HUE(hsv_value) - GET_HUE(p_led_indicator->last_fade_value);

            if (p_blink_step->hold_time_ms == 0) {
                p_led_indicator->hal_indicator_set_rgb(p_led_indicator->hardware_data, p_blink_step->value);
                p_led_indicator->current_fade_value = hsv_value;
                SET_INDEX(p_led_indicator->current_fade_value, GET_INDEX(p_blink_step->value));
                p_led_indicator->last_fade_value = p_led_indicator->current_fade_value;
                p_led_indicator->p_blink_steps[active_blink] += 1;
                break;
            }

            if (diff_value >= 0) {
                SET_HUE(p_led_indicator->current_fade_value, p_led_indicator->fade_value_count + GET_HUE(p_led_indicator->last_fade_value));
            } else {
                SET_HUE(p_led_indicator->current_fade_value, GET_HUE(p_led_indicator->last_fade_value) - p_led_indicator->fade_value_count);
            }
            SET_INDEX(p_led_indicator->current_fade_value, GET_INDEX(p_blink_step->value));

            uint32_t r,g,b,irgb_value;
            led_indicator_strips_hsv2rgb(p_led_indicator->current_fade_value, &r, &g, &b);
            irgb_value = (r << 16) | (g << 8) | b;
            SET_INDEX(irgb_value, GET_INDEX(p_blink_step->value));
            p_led_indicator->hal_indicator_set_rgb(p_led_indicator->hardware_data, irgb_value);

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
                p_led_indicator->current_fade_value = hsv_value;
                p_led_indicator->last_fade_value = hsv_value;
                p_led_indicator->p_blink_steps[active_blink] += 1;
            }

            break;
        }

        case LED_BLINK_HSV: {
            if (!p_led_indicator->hal_indicator_set_hsv) {
                ESP_LOGW(TAG, "LED_BLINK_HSV Skip: no hal_indicator_set_hsv function");
                break;
            }
            p_led_indicator->hal_indicator_set_hsv(p_led_indicator->hardware_data, p_blink_step->value);
            p_led_indicator->current_fade_value = p_blink_step->value;
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
            int16_t diff_value = GET_HUE(p_blink_step->value) - GET_HUE(p_led_indicator->last_fade_value);

            if (p_blink_step->hold_time_ms == 0) {
                p_led_indicator->hal_indicator_set_hsv(p_led_indicator->hardware_data, p_blink_step->value);
                p_led_indicator->current_fade_value = p_blink_step->value;
                p_led_indicator->last_fade_value = p_led_indicator->current_fade_value;
                p_led_indicator->p_blink_steps[active_blink] += 1;
                break;
            }

            if (diff_value >= 0) {
                SET_HUE(p_led_indicator->current_fade_value, p_led_indicator->fade_value_count + GET_HUE(p_led_indicator->last_fade_value));
            } else {
                SET_HUE(p_led_indicator->current_fade_value, GET_HUE(p_led_indicator->last_fade_value) - p_led_indicator->fade_value_count);
            }

            SET_INDEX(p_led_indicator->current_fade_value, GET_INDEX(p_blink_step->value));
            p_led_indicator->hal_indicator_set_hsv(p_led_indicator->hardware_data, p_led_indicator->current_fade_value);

            if (diff_value == 0) {
                ticks = p_blink_step->hold_time_ms;
                p_led_indicator->fade_value_count += 1;
            } else if (p_blink_step->hold_time_ms > BRIGHTNESS_TICKS * abs(diff_value)) {
                ticks = p_blink_step->hold_time_ms /  abs(diff_value) ;
                p_led_indicator->fade_value_count += 1;
            } else {
                p_led_indicator->fade_value_count += abs(diff_value) * ticks / p_blink_step->hold_time_ms;
            }

            ESP_LOGD(TAG, "hsv ring ticks value: %d", ticks);
            leave = true;
            timer_restart = true;
            timer_period_ms = ticks;

            if (p_led_indicator->fade_value_count > abs(diff_value)) {
                p_led_indicator->fade_value_count = BRIGHTNESS_MIN;
                p_led_indicator->current_fade_value = p_blink_step->value;
                p_led_indicator->last_fade_value = p_blink_step->value;
                p_led_indicator->p_blink_steps[active_blink] += 1;
            }

            break;
        }

        case LED_BLINK_BRIGHTNESS: {
            if (!p_led_indicator->hal_indicator_set_brightness) {
                ESP_LOGW(TAG, "LED_BLINK_BRIGHTNESS Skip: no hal_indicator_set_brightness function");
                break;
            }

            uint16_t brightness_value = GET_BRIGHTNESS(p_blink_step->value);
            brightness_value = led_indicator_get_gamma_value(brightness_value);
            SET_BRIGHTNESS(p_led_indicator->current_fade_value, brightness_value);
            SET_INDEX(p_led_indicator->current_fade_value, GET_INDEX(p_blink_step->value));
            p_led_indicator->hal_indicator_set_brightness(p_led_indicator->hardware_data, p_led_indicator->current_fade_value);
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

            uint32_t brightness_value = GET_BRIGHTNESS(p_blink_step->value);
            uint16_t ticks = BRIGHTNESS_TICKS;
            int16_t diff_value = GET_BRIGHTNESS(p_blink_step->value) - GET_BRIGHTNESS(p_led_indicator->last_fade_value);
            SET_INDEX(p_led_indicator->current_fade_value, GET_INDEX(p_blink_step->value));
            // diff_brightness > 0: Fade increase
            // diff_brightness < 0: Fade decrease
            if (p_blink_step->hold_time_ms == 0) {
                brightness_value =  led_indicator_get_gamma_value(brightness_value);
                SET_BRIGHTNESS(p_led_indicator->current_fade_value, brightness_value);
                p_led_indicator->hal_indicator_set_brightness(p_led_indicator->hardware_data, p_led_indicator->current_fade_value);
                p_led_indicator->last_fade_value = p_led_indicator->current_fade_value;
                p_led_indicator->p_blink_steps[active_blink] += 1;
                break;
            }

            if (diff_value >= 0) {
                brightness_value = p_led_indicator->fade_value_count + GET_BRIGHTNESS(p_led_indicator->last_fade_value);
            } else {
                brightness_value = GET_BRIGHTNESS(p_led_indicator->last_fade_value) - p_led_indicator->fade_value_count;
            }
            brightness_value =  led_indicator_get_gamma_value(brightness_value);
            SET_BRIGHTNESS(p_led_indicator->current_fade_value, brightness_value);
            p_led_indicator->hal_indicator_set_brightness(p_led_indicator->hardware_data, p_led_indicator->current_fade_value);

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
                SET_BRIGHTNESS(p_led_indicator->current_fade_value, GET_BRIGHTNESS(p_blink_step->value));
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

static _led_indicator_t *_led_indicator_create_com(_led_indicator_com_config_t *cfg)
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
    p_led_indicator->is_active_level_high = cfg->is_active_level_high;
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

led_indicator_handle_t led_indicator_create(const led_indicator_config_t *config)
{
    esp_err_t ret = ESP_OK;
    bool if_blink_default_list = false;
    ESP_LOGI(TAG, "LED Indicator Version: %d.%d.%d", LED_INDICATOR_VER_MAJOR, LED_INDICATOR_VER_MINOR, LED_INDICATOR_VER_PATCH);
    LED_INDICATOR_CHECK(config != NULL, "invalid config pointer", return NULL);
    // LED_INDICATOR_CHECK(config->blink_lists != NULL, "LED indicator blink lists can't be NULL", return NULL);
    _led_indicator_com_config_t com_cfg = {0};
    _led_indicator_t *p_led_indicator = NULL;
    switch (config->mode) {
    case LED_GPIO_MODE: {
        const led_indicator_gpio_config_t *cfg = config->led_indicator_gpio_config;
        ret = led_indicator_gpio_init((void *)cfg->gpio_num);
        LED_INDICATOR_CHECK(ESP_OK == ret, "GPIO mode init failed", return NULL);

        com_cfg.is_active_level_high = cfg->is_active_level_high;
        com_cfg.hardware_data = (void *)cfg->gpio_num;
        com_cfg.hal_indicator_set_on_off = led_indicator_gpio_set_on_off;
        com_cfg.hal_indicator_deinit = led_indicator_gpio_deinit;
        com_cfg.hal_indicator_set_brightness =  NULL;
        com_cfg.duty_resolution = LED_DUTY_1_BIT;
        if (config->blink_lists == NULL) {
            ESP_LOGI(TAG, "blink_lists is null, use default blink list");
            com_cfg.blink_lists = default_led_indicator_blink_lists;
            com_cfg.blink_list_num = DEFAULT_BLINK_LIST_NUM;
            if_blink_default_list = true;
        } else {
            com_cfg.blink_lists = config->blink_lists;
            com_cfg.blink_list_num = config->blink_list_num;
        }

        p_led_indicator = _led_indicator_create_com(&com_cfg);
        break;
    }
    case LED_LEDC_MODE: {
        const led_indicator_ledc_config_t *cfg = config->led_indicator_ledc_config;
        ret = led_indicator_ledc_init((void *)cfg);
        LED_INDICATOR_CHECK(ESP_OK == ret, "LEDC mode init failed", return NULL);
        com_cfg.is_active_level_high = cfg->is_active_level_high;
        com_cfg.hardware_data = (void *)cfg->channel;
        com_cfg.hal_indicator_set_on_off = led_indicator_ledc_set_on_off;
        com_cfg.hal_indicator_deinit = led_indicator_ledc_deinit;
        com_cfg.hal_indicator_set_brightness = led_indicator_ledc_set_brightness;
        com_cfg.duty_resolution = LED_DUTY_8_BIT;
        if (config->blink_lists == NULL) {
            ESP_LOGI(TAG, "blink_lists is null, use default blink list");
            com_cfg.blink_lists = default_led_indicator_blink_lists;
            com_cfg.blink_list_num = DEFAULT_BLINK_LIST_NUM;
            if_blink_default_list = true;
        } else {
            com_cfg.blink_lists = config->blink_lists;
            com_cfg.blink_list_num = config->blink_list_num;
        }

        p_led_indicator = _led_indicator_create_com(&com_cfg);
        break;
    }
    case LED_STRIPS_MODE: {
        const led_indicator_strips_config_t *cfg = config->led_indicator_strips_config;
        void *hardware_data = NULL;
        ret = led_indicator_strips_init((void *)cfg, &hardware_data);
        LED_INDICATOR_CHECK(ESP_OK == ret, "LED rgb init failed", return NULL);
        com_cfg.is_active_level_high = cfg->is_active_level_high;
        com_cfg.hardware_data = hardware_data;
        com_cfg.hal_indicator_set_on_off = led_indicator_strips_set_on_off;
        com_cfg.hal_indicator_deinit = led_indicator_strips_deinit;
        com_cfg.hal_indicator_set_brightness = led_indicator_strips_set_brightness;
        com_cfg.hal_indicator_set_rgb = led_indicator_strips_set_rgb;
        com_cfg.hal_indicator_set_hsv = led_indicator_strips_set_hsv;
        com_cfg.duty_resolution = LED_DUTY_8_BIT;
        if (config->blink_lists == NULL) {
            ESP_LOGI(TAG, "blink_lists is null, use default blink list");
            com_cfg.blink_lists = default_led_indicator_blink_lists;
            com_cfg.blink_list_num = DEFAULT_BLINK_LIST_NUM;
            if_blink_default_list = true;
        } else {
            com_cfg.blink_lists = config->blink_lists;
            com_cfg.blink_list_num = config->blink_list_num;
        }

        p_led_indicator = _led_indicator_create_com(&com_cfg);
        break;
    }
    case LED_CUSTOM_MODE: {
        const led_indicator_custom_config_t *cfg = config->led_indicator_custom_config;
        LED_INDICATOR_CHECK_WARNING(cfg->hal_indicator_init != NULL, "LED indicator does not have hal_indicator_init ", goto without_init);
        ret = cfg->hal_indicator_init(cfg->hardware_data);
        LED_INDICATOR_CHECK(ret == ESP_OK, "LED indicator init failed", return NULL);
without_init:
        com_cfg.is_active_level_high = cfg->is_active_level_high;
        com_cfg.hardware_data = cfg->hardware_data;
        com_cfg.hal_indicator_set_on_off = cfg->hal_indicator_set_on_off;
        com_cfg.hal_indicator_deinit = cfg->hal_indicator_deinit;
        com_cfg.hal_indicator_set_brightness = cfg->hal_indicator_set_brightness;
        com_cfg.hal_indicator_set_rgb = cfg->hal_indicator_set_rgb;
        com_cfg.hal_indicator_set_hsv = cfg->hal_indicator_set_hsv;
        com_cfg.duty_resolution = cfg->duty_resolution;
        com_cfg.blink_lists = config->blink_lists;
        com_cfg.blink_list_num = config->blink_list_num;
        if (config->blink_lists == NULL) {
            ESP_LOGI(TAG, "blink_lists is null, use default blink list");
            com_cfg.blink_lists = default_led_indicator_blink_lists;
            com_cfg.blink_list_num = DEFAULT_BLINK_LIST_NUM;
            if_blink_default_list = true;
        } else {
            com_cfg.blink_lists = config->blink_lists;
            com_cfg.blink_list_num = config->blink_list_num;
        }

        p_led_indicator = _led_indicator_create_com(&com_cfg);
        break;
    }
    default:
        ESP_LOGE(TAG, "Unsupported indicator mode");
        break;
    }

    LED_INDICATOR_CHECK(NULL != p_led_indicator, "LED indicator create failed", return NULL);
    p_led_indicator->mode = config->mode;
    _led_indicator_add_node(p_led_indicator);
    ESP_LOGI(TAG, "Indicator create successfully. type:%s, hardware_data:%p, is_active_level_high:%d, blink_lists:%s", led_indicator_mode_str[p_led_indicator->mode], p_led_indicator->hardware_data, p_led_indicator->is_active_level_high, if_blink_default_list ? "default" : "custom");
    return (led_indicator_handle_t)p_led_indicator;
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

led_indicator_handle_t led_indicator_get_handle(void *hardware_data)
{
    _led_indicator_slist_t *node;
    SLIST_FOREACH(node, &s_led_indicator_slist_head, next) {
        if (node->p_led_indicator->hardware_data == hardware_data) {
            return (led_indicator_handle_t)(node->p_led_indicator);
        }
    }
    return NULL;
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
    p_led_indicator->p_blink_steps[blink_type] = LED_BLINK_STOP;
    p_led_indicator->preempt_blink = NULL_PREEMPT_BLINK;
    _blink_list_switch(p_led_indicator); //stop and switch to next blink steps
    xSemaphoreGive(p_led_indicator->mutex);

    return ESP_OK;
}

uint8_t led_indicator_get_brightness(led_indicator_handle_t handle)
{
    LED_INDICATOR_CHECK(handle != NULL, "invalid p_handle", return 0);
    _led_indicator_t *p_led_indicator = (_led_indicator_t *)handle;
    xSemaphoreTake(p_led_indicator->mutex, portMAX_DELAY);
    uint8_t fade_value = p_led_indicator->current_fade_value & 0xFF;
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
    uint32_t hardware_level = on_off ? p_led_indicator->is_active_level_high : !p_led_indicator->is_active_level_high;
    p_led_indicator->hal_indicator_set_on_off(p_led_indicator->hardware_data, hardware_level);
    SET_BRIGHTNESS(p_led_indicator->current_fade_value, on_off ? LED_STATE_ON : LED_STATE_OFF);
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
    p_led_indicator->hal_indicator_set_brightness(p_led_indicator->hardware_data, brightness);
    SET_BRIGHTNESS(p_led_indicator->current_fade_value, GET_BRIGHTNESS(brightness));
    SET_INDEX(p_led_indicator->current_fade_value, GET_INDEX(brightness));
    p_led_indicator->last_fade_value = p_led_indicator->current_fade_value;
    xSemaphoreGive(p_led_indicator->mutex);
    return ESP_OK;
}

uint32_t led_indicator_get_hsv(led_indicator_handle_t handle)
{
    LED_INDICATOR_CHECK(handle != NULL, "invalid p_handle", return 0);
    _led_indicator_t *p_led_indicator = (_led_indicator_t *)handle;
    xSemaphoreTake(p_led_indicator->mutex, portMAX_DELAY);
    uint32_t hsv_value = p_led_indicator->current_fade_value & 0x1FFFFFFF;
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
    p_led_indicator->hal_indicator_set_hsv(p_led_indicator->hardware_data, ihsv_value);
    p_led_indicator->current_fade_value = ihsv_value;
    p_led_indicator->last_fade_value = ihsv_value;
    xSemaphoreGive(p_led_indicator->mutex);
    return ESP_OK;
}

uint32_t led_indicator_get_rgb(led_indicator_handle_t handle)
{
    LED_INDICATOR_CHECK(handle != NULL, "invalid p_handle", return 0);
    _led_indicator_t *p_led_indicator = (_led_indicator_t *)handle;
    xSemaphoreTake(p_led_indicator->mutex, portMAX_DELAY);
    uint32_t ihsv_value = p_led_indicator->current_fade_value;
    xSemaphoreGive(p_led_indicator->mutex);
    
    uint32_t r,g,b,rgb_value;
    led_indicator_strips_hsv2rgb(ihsv_value, &r, &g, &b);
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
    uint32_t hsv = led_indicator_strips_rgb2hsv(irgb_value);
    xSemaphoreTake(p_led_indicator->mutex, portMAX_DELAY);
    p_led_indicator->hal_indicator_set_rgb(p_led_indicator->hardware_data, irgb_value);
    p_led_indicator->current_fade_value = hsv;
    SET_INDEX(p_led_indicator->current_fade_value, GET_INDEX(irgb_value));
    p_led_indicator->last_fade_value = p_led_indicator->current_fade_value;
    xSemaphoreGive(p_led_indicator->mutex);
    return ESP_OK;
}
