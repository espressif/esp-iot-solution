/* SPDX-FileCopyrightText: 2022-2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "iot_button.h"
#include "esp_timer.h"
#include "sdkconfig.h"

static const char *TAG = "button";

#define BTN_CHECK(a, str, ret_val)                                \
    if (!(a)) {                                                   \
        ESP_LOGE(TAG, "%s(%d): %s", __FUNCTION__, __LINE__, str); \
        return (ret_val);                                         \
    }

/**
 * @brief Structs to record individual key parameters
 *
 */
typedef struct Button {
    uint16_t        ticks;
    uint16_t        long_press_ticks;     /*! Trigger ticks for long press*/
    uint16_t        short_press_ticks;    /*! Trigger ticks for repeat press*/
    uint16_t        long_press_hold_cnt;  /*! Record long press hold count*/
    uint8_t         repeat;
    uint8_t         state: 3;
    uint8_t         debounce_cnt: 3;
    uint8_t         active_level: 1;
    uint8_t         button_level: 1;
    button_event_t  event;
    uint8_t         (*hal_button_Level)(void *hardware_data);
    esp_err_t       (*hal_button_deinit)(void *hardware_data);
    void            *hardware_data;
    void            *usr_data[BUTTON_EVENT_MAX];
    button_type_t   type;
    button_cb_t     cb[BUTTON_EVENT_MAX];
    struct Button   *next;
} button_dev_t;

//button handle list head.
static button_dev_t *g_head_handle = NULL;
static esp_timer_handle_t g_button_timer_handle;
static bool g_is_timer_running = false;

#define TICKS_INTERVAL    CONFIG_BUTTON_PERIOD_TIME_MS
#define DEBOUNCE_TICKS    CONFIG_BUTTON_DEBOUNCE_TICKS //MAX 8
#define SHORT_TICKS       (CONFIG_BUTTON_SHORT_PRESS_TIME_MS /TICKS_INTERVAL)
#define LONG_TICKS        (CONFIG_BUTTON_LONG_PRESS_TIME_MS /TICKS_INTERVAL)
#define SERIAL_TICKS      (CONFIG_BUTTON_SERIAL_TIME_MS /TICKS_INTERVAL)

#define CALL_EVENT_CB(ev)   if(btn->cb[ev])btn->cb[ev](btn, btn->usr_data[ev])

#define TIME_TO_TICKS(time, congfig_time)  (0 == (time))?congfig_time:(((time) / TICKS_INTERVAL))?((time) / TICKS_INTERVAL):1

/**
  * @brief  Button driver core function, driver state machine.
  */
static void button_handler(button_dev_t *btn)
{
    uint8_t read_gpio_level = btn->hal_button_Level(btn->hardware_data);

    /** ticks counter working.. */
    if ((btn->state) > 0) {
        btn->ticks++;
    }

    /**< button debounce handle */
    if (read_gpio_level != btn->button_level) {
        if (++(btn->debounce_cnt) >= DEBOUNCE_TICKS) {
            btn->button_level = read_gpio_level;
            btn->debounce_cnt = 0;
        }
    } else {
        btn->debounce_cnt = 0;
    }

    /** State machine */
    switch (btn->state) {
    case 0:
        if (btn->button_level == btn->active_level) {
            btn->event = (uint8_t)BUTTON_PRESS_DOWN;
            CALL_EVENT_CB(BUTTON_PRESS_DOWN);
            btn->ticks = 0;
            btn->repeat = 1;
            btn->state = 1;
        } else {
            btn->event = (uint8_t)BUTTON_NONE_PRESS;
        }
        break;

    case 1:
        if (btn->button_level != btn->active_level) {
            btn->event = (uint8_t)BUTTON_PRESS_UP;
            CALL_EVENT_CB(BUTTON_PRESS_UP);
            btn->ticks = 0;
            btn->state = 2;

        } else if (btn->ticks > btn->long_press_ticks) {
            btn->event = (uint8_t)BUTTON_LONG_PRESS_START;
            CALL_EVENT_CB(BUTTON_LONG_PRESS_START);
            btn->state = 5;
        }
        break;

    case 2:
        if (btn->button_level == btn->active_level) {
            btn->event = (uint8_t)BUTTON_PRESS_DOWN;
            CALL_EVENT_CB(BUTTON_PRESS_DOWN);
            btn->repeat++;
            CALL_EVENT_CB(BUTTON_PRESS_REPEAT); // repeat hit
            btn->ticks = 0;
            btn->state = 3;
        } else if (btn->ticks > btn->short_press_ticks) {
            if (btn->repeat == 1) {
                btn->event = (uint8_t)BUTTON_SINGLE_CLICK;
                CALL_EVENT_CB(BUTTON_SINGLE_CLICK);
            } else if (btn->repeat == 2) {
                btn->event = (uint8_t)BUTTON_DOUBLE_CLICK;
                CALL_EVENT_CB(BUTTON_DOUBLE_CLICK); // repeat hit
            }
            btn->event = (uint8_t)BUTTON_PRESS_REPEAT_DONE;
            CALL_EVENT_CB(BUTTON_PRESS_REPEAT_DONE); // repeat hit
            btn->state = 0;
        }
        break;

    case 3:
        if (btn->button_level != btn->active_level) {
            btn->event = (uint8_t)BUTTON_PRESS_UP;
            CALL_EVENT_CB(BUTTON_PRESS_UP);
            if (btn->ticks < SHORT_TICKS) {
                btn->ticks = 0;
                btn->state = 2; //repeat press
            } else {
                btn->state = 0;
            }
        }
        break;

    case 5:
        if (btn->button_level == btn->active_level) {
            //continue hold trigger
            if (btn->ticks >= (btn->long_press_hold_cnt + 1) * SERIAL_TICKS) {
                btn->event = (uint8_t)BUTTON_LONG_PRESS_HOLD;
                btn->long_press_hold_cnt++;
                CALL_EVENT_CB(BUTTON_LONG_PRESS_HOLD);
            }
        } else { //releasd
            btn->event = (uint8_t)BUTTON_PRESS_UP;
            CALL_EVENT_CB(BUTTON_PRESS_UP);
            btn->state = 0; //reset
            btn->long_press_hold_cnt = 0;
        }
        break;
    }
}

static void button_cb(void *args)
{
    button_dev_t *target;
    for (target = g_head_handle; target; target = target->next) {
        button_handler(target);
    }
}

static button_dev_t *button_create_com(uint8_t active_level, uint8_t (*hal_get_key_state)(void *hardware_data), void *hardware_data, uint16_t long_press_ticks, uint16_t short_press_ticks)
{
    BTN_CHECK(NULL != hal_get_key_state, "Function pointer is invalid", NULL);

    button_dev_t *btn = (button_dev_t *) calloc(1, sizeof(button_dev_t));
    BTN_CHECK(NULL != btn, "Button memory alloc failed", NULL);
    btn->hardware_data = hardware_data;
    btn->event = BUTTON_NONE_PRESS;
    btn->active_level = active_level;
    btn->hal_button_Level = hal_get_key_state;
    btn->button_level = !active_level;
    btn->long_press_ticks = long_press_ticks;
    btn->short_press_ticks = short_press_ticks;

    /** Add handle to list */
    btn->next = g_head_handle;
    g_head_handle = btn;

    if (false == g_is_timer_running) {
        esp_timer_create_args_t button_timer;
        button_timer.arg = NULL;
        button_timer.callback = button_cb;
        button_timer.dispatch_method = ESP_TIMER_TASK;
        button_timer.name = "button_timer";
        esp_timer_create(&button_timer, &g_button_timer_handle);
        esp_timer_start_periodic(g_button_timer_handle, TICKS_INTERVAL * 1000U);
        g_is_timer_running = true;
    }

    return btn;
}

static esp_err_t button_delete_com(button_dev_t *btn)
{
    BTN_CHECK(NULL != btn, "Pointer of handle is invalid", ESP_ERR_INVALID_ARG);

    button_dev_t **curr;
    for (curr = &g_head_handle; *curr; ) {
        button_dev_t *entry = *curr;
        if (entry == btn) {
            *curr = entry->next;
            free(entry);
        } else {
            curr = &entry->next;
        }
    }

    /* count button number */
    uint16_t number = 0;
    button_dev_t *target = g_head_handle;
    while (target) {
        target = target->next;
        number++;
    }
    ESP_LOGD(TAG, "remain btn number=%d", number);

    if (0 == number && g_is_timer_running) { /**<  if all button is deleted, stop the timer */
        esp_timer_stop(g_button_timer_handle);
        esp_timer_delete(g_button_timer_handle);
        g_is_timer_running = false;
    }
    return ESP_OK;
}

button_handle_t iot_button_create(const button_config_t *config)
{
    ESP_LOGI(TAG, "IoT Button Version: %d.%d.%d", BUTTON_VER_MAJOR, BUTTON_VER_MINOR, BUTTON_VER_PATCH);
    BTN_CHECK(config, "Invalid button config", NULL);

    esp_err_t ret = ESP_OK;
    button_dev_t *btn = NULL;
    uint16_t long_press_time = 0;
    uint16_t short_press_time = 0;
    long_press_time = TIME_TO_TICKS(config->long_press_time, LONG_TICKS);
    short_press_time = TIME_TO_TICKS(config->short_press_time, SHORT_TICKS);
    switch (config->type) {
    case BUTTON_TYPE_GPIO: {
        const button_gpio_config_t *cfg = &(config->gpio_button_config);
        ret = button_gpio_init(cfg);
        BTN_CHECK(ESP_OK == ret, "gpio button init failed", NULL);
        btn = button_create_com(cfg->active_level, button_gpio_get_key_level, (void *)cfg->gpio_num, long_press_time, short_press_time);
    } break;
    case BUTTON_TYPE_ADC: {
        const button_adc_config_t *cfg = &(config->adc_button_config);
        ret = button_adc_init(cfg);
        BTN_CHECK(ESP_OK == ret, "adc button init failed", NULL);
        btn = button_create_com(1, button_adc_get_key_level, (void *)ADC_BUTTON_COMBINE(cfg->adc_channel, cfg->button_index), long_press_time, short_press_time);
    } break;
    case BUTTON_TYPE_CUSTOM: {
        if (config->custom_button_config.button_custom_init) {
            ret = config->custom_button_config.button_custom_init(config->custom_button_config.priv);
            BTN_CHECK(ESP_OK == ret, "custom button init failed", NULL);
        }

        btn = button_create_com(config->custom_button_config.active_level,
                                config->custom_button_config.button_custom_get_key_value,
                                config->custom_button_config.priv,
                                long_press_time, short_press_time);
        if (btn) {
            btn->hal_button_deinit = config->custom_button_config.button_custom_deinit;
        }
    } break;

    default:
        ESP_LOGE(TAG, "Unsupported button type");
        break;
    }
    BTN_CHECK(NULL != btn, "button create failed", NULL);
    btn->type = config->type;
    return (button_handle_t)btn;
}

esp_err_t iot_button_delete(button_handle_t btn_handle)
{
    esp_err_t ret = ESP_OK;
    BTN_CHECK(NULL != btn_handle, "Pointer of handle is invalid", ESP_ERR_INVALID_ARG);
    button_dev_t *btn = (button_dev_t *)btn_handle;
    switch (btn->type) {
    case BUTTON_TYPE_GPIO:
        ret = button_gpio_deinit((int)(btn->hardware_data));
        break;
    case BUTTON_TYPE_ADC:
        ret = button_adc_deinit(ADC_BUTTON_SPLIT_CHANNEL(btn->hardware_data), ADC_BUTTON_SPLIT_INDEX(btn->hardware_data));
        break;
    case BUTTON_TYPE_CUSTOM:
        if (btn->hal_button_deinit) {
            ret = btn->hal_button_deinit(btn->hardware_data);
        }

        if (btn->hardware_data) {
            free(btn->hardware_data);
            btn->hardware_data = NULL;
        }
        break;
    default:
        break;
    }
    BTN_CHECK(ESP_OK == ret, "button deinit failed", ESP_FAIL);
    button_delete_com(btn);
    return ESP_OK;
}

esp_err_t iot_button_register_cb(button_handle_t btn_handle, button_event_t event, button_cb_t cb, void *usr_data)
{
    BTN_CHECK(NULL != btn_handle, "Pointer of handle is invalid", ESP_ERR_INVALID_ARG);
    BTN_CHECK(event < BUTTON_EVENT_MAX, "event is invalid", ESP_ERR_INVALID_ARG);
    button_dev_t *btn = (button_dev_t *) btn_handle;
    BTN_CHECK(NULL == btn->cb[event], "Callback is already registered", ESP_ERR_INVALID_STATE);
    btn->cb[event] = cb;
    btn->usr_data[event] = usr_data;
    return ESP_OK;
}

esp_err_t iot_button_unregister_cb(button_handle_t btn_handle, button_event_t event)
{
    BTN_CHECK(NULL != btn_handle, "Pointer of handle is invalid", ESP_ERR_INVALID_ARG);
    BTN_CHECK(event < BUTTON_EVENT_MAX, "event is invalid", ESP_ERR_INVALID_ARG);
    button_dev_t *btn = (button_dev_t *) btn_handle;
    BTN_CHECK(NULL != btn->cb[event], "Callback is not registered yet", ESP_ERR_INVALID_STATE);
    btn->cb[event] = NULL;
    btn->usr_data[event] = NULL;
    return ESP_OK;
}

size_t iot_button_count_cb(button_handle_t btn_handle)
{
    BTN_CHECK(NULL != btn_handle, "Pointer of handle is invalid", ESP_ERR_INVALID_ARG);
    button_dev_t *btn = (button_dev_t *) btn_handle;
    size_t ret = 0;
    for (size_t i = 0; i < BUTTON_EVENT_MAX; i++) {
        if (btn->cb[i]) {
            ret++;
        }
    }
    return ret;
}

button_event_t iot_button_get_event(button_handle_t btn_handle)
{
    BTN_CHECK(NULL != btn_handle, "Pointer of handle is invalid", BUTTON_NONE_PRESS);
    button_dev_t *btn = (button_dev_t *) btn_handle;
    return btn->event;
}

uint8_t iot_button_get_repeat(button_handle_t btn_handle)
{
    BTN_CHECK(NULL != btn_handle, "Pointer of handle is invalid", 0);
    button_dev_t *btn = (button_dev_t *) btn_handle;
    return btn->repeat;
}

uint16_t iot_button_get_ticks_time(button_handle_t btn_handle)
{
    BTN_CHECK(NULL != btn_handle, "Pointer of handle is invalid", 0);
    button_dev_t *btn = (button_dev_t *) btn_handle;
    return (btn->ticks * TICKS_INTERVAL);
}

uint16_t iot_button_get_long_press_hold_cnt(button_handle_t btn_handle)
{
    BTN_CHECK(NULL != btn_handle, "Pointer of handle is invalid", 0);
    button_dev_t *btn = (button_dev_t *) btn_handle;
    return btn->long_press_hold_cnt;
}