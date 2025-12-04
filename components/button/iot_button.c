/* SPDX-FileCopyrightText: 2022-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"
#include "driver/gpio.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "esp_check.h"
#include "iot_button.h"
#include "sdkconfig.h"
#include "button_interface.h"

static const char *TAG = "button";
static portMUX_TYPE s_button_lock = portMUX_INITIALIZER_UNLOCKED;
#define BUTTON_ENTER_CRITICAL()           portENTER_CRITICAL(&s_button_lock)
#define BUTTON_EXIT_CRITICAL()            portEXIT_CRITICAL(&s_button_lock)

#define BTN_CHECK(a, str, ret_val)                                \
    if (!(a)) {                                                   \
        ESP_LOGE(TAG, "%s(%d): %s", __FUNCTION__, __LINE__, str); \
        return (ret_val);                                         \
    }

static const char *button_event_str[] = {
    "BUTTON_PRESS_DOWN",
    "BUTTON_PRESS_UP",
    "BUTTON_PRESS_REPEAT",
    "BUTTON_PRESS_REPEAT_DONE",
    "BUTTON_SINGLE_CLICK",
    "BUTTON_DOUBLE_CLICK",
    "BUTTON_MULTIPLE_CLICK",
    "BUTTON_LONG_PRESS_START",
    "BUTTON_LONG_PRESS_HOLD",
    "BUTTON_LONG_PRESS_UP",
    "BUTTON_PRESS_END",
    "BUTTON_EVENT_MAX",
    "BUTTON_NONE_PRESS",
};

enum {
    PRESS_DOWN_CHECK = 0,
    PRESS_UP_CHECK,
    PRESS_REPEAT_DOWN_CHECK,
    PRESS_REPEAT_UP_CHECK,
    PRESS_LONG_PRESS_UP_CHECK,
};

/**
 * @brief Structs to store callback info
 *
 */
typedef struct {
    button_cb_t cb;
    void *usr_data;
    button_event_args_t event_args;
} button_cb_info_t;

/**
 * @brief Structs to record individual key parameters
 *
 */
typedef struct button_dev_t {
    uint32_t              ticks;                    /*!< Count for the current button state. */
    uint32_t              long_press_ticks;         /*!< Trigger ticks for long press,  */
    uint32_t              short_press_ticks;        /*!< Trigger ticks for repeat press */
    uint32_t              long_press_hold_cnt;      /*!< Record long press hold count */
    uint8_t               repeat;
    uint8_t               state: 3;
    uint8_t               debounce_cnt: 4;          /*!< Max 15 */
    uint8_t               button_level: 1;
    button_event_t        event;
    button_driver_t       *driver;
    button_cb_info_t      *cb_info[BUTTON_EVENT_MAX];
    size_t                size[BUTTON_EVENT_MAX];
    int                   count[2];
    struct button_dev_t   *next;
} button_dev_t;

//button handle list head.
static button_dev_t *g_head_handle = NULL;
static esp_timer_handle_t g_button_timer_handle = NULL;
static bool g_is_timer_running = false;
static button_power_save_config_t power_save_usr_cfg = {0};

#define TICKS_INTERVAL    CONFIG_BUTTON_PERIOD_TIME_MS
#define DEBOUNCE_TICKS    CONFIG_BUTTON_DEBOUNCE_TICKS //MAX 8
#define SHORT_TICKS       (CONFIG_BUTTON_SHORT_PRESS_TIME_MS /TICKS_INTERVAL)
#define LONG_TICKS        (CONFIG_BUTTON_LONG_PRESS_TIME_MS /TICKS_INTERVAL)
#define SERIAL_TICKS      (CONFIG_BUTTON_LONG_PRESS_HOLD_SERIAL_TIME_MS /TICKS_INTERVAL)
#define TOLERANCE         (CONFIG_BUTTON_PERIOD_TIME_MS*4)

#define CALL_EVENT_CB(ev)                                                   \
    if (btn->cb_info[ev]) {                                                 \
        for (int i = 0; i < btn->size[ev]; i++) {                           \
            btn->cb_info[ev][i].cb(btn, btn->cb_info[ev][i].usr_data);      \
        }                                                                   \
    }                                                                       \

#define TIME_TO_TICKS(time, congfig_time)  (0 == (time))?congfig_time:(((time) / TICKS_INTERVAL))?((time) / TICKS_INTERVAL):1

/**
  * @brief  Button driver core function, driver state machine.
  */
static void button_handler(button_dev_t *btn)
{
    uint8_t read_gpio_level = btn->driver->get_key_level(btn->driver);

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
    case PRESS_DOWN_CHECK:
        if (btn->button_level == BUTTON_ACTIVE) {
            btn->event = (uint8_t)BUTTON_PRESS_DOWN;
            CALL_EVENT_CB(BUTTON_PRESS_DOWN);
            btn->ticks = 0;
            btn->repeat = 1;
            btn->state = PRESS_UP_CHECK;
        } else {
            btn->event = (uint8_t)BUTTON_NONE_PRESS;
        }
        break;

    case PRESS_UP_CHECK:
        if (btn->button_level != BUTTON_ACTIVE) {
            btn->event = (uint8_t)BUTTON_PRESS_UP;
            CALL_EVENT_CB(BUTTON_PRESS_UP);
            btn->ticks = 0;
            btn->state = PRESS_REPEAT_DOWN_CHECK;

        } else if (btn->ticks >= btn->long_press_ticks) {
            btn->event = (uint8_t)BUTTON_LONG_PRESS_START;
            btn->state = PRESS_LONG_PRESS_UP_CHECK;
            /** Calling callbacks for BUTTON_LONG_PRESS_START */
            uint32_t pressed_time = iot_button_get_pressed_time(btn);
            int32_t diff = pressed_time - btn->long_press_ticks * TICKS_INTERVAL;
            if (btn->cb_info[btn->event] && btn->count[0] == 0) {
                if (abs(diff) <= TOLERANCE && btn->cb_info[btn->event][btn->count[0]].event_args.long_press.press_time == (btn->long_press_ticks * TICKS_INTERVAL)) {
                    do {
                        btn->cb_info[btn->event][btn->count[0]].cb(btn, btn->cb_info[btn->event][btn->count[0]].usr_data);
                        btn->count[0]++;
                        if (btn->count[0] >= btn->size[btn->event]) {
                            break;
                        }
                    } while (btn->cb_info[btn->event][btn->count[0]].event_args.long_press.press_time == btn->long_press_ticks * TICKS_INTERVAL);
                }
            }
        }
        break;

    case PRESS_REPEAT_DOWN_CHECK:
        if (btn->button_level == BUTTON_ACTIVE) {
            btn->event = (uint8_t)BUTTON_PRESS_DOWN;
            CALL_EVENT_CB(BUTTON_PRESS_DOWN);
            btn->event = (uint8_t)BUTTON_PRESS_REPEAT;
            btn->repeat++;
            CALL_EVENT_CB(BUTTON_PRESS_REPEAT); // repeat hit
            btn->ticks = 0;
            btn->state = PRESS_REPEAT_UP_CHECK;
        } else if (btn->ticks > btn->short_press_ticks) {
            if (btn->repeat == 1) {
                btn->event = (uint8_t)BUTTON_SINGLE_CLICK;
                CALL_EVENT_CB(BUTTON_SINGLE_CLICK);
            } else if (btn->repeat == 2) {
                btn->event = (uint8_t)BUTTON_DOUBLE_CLICK;
                CALL_EVENT_CB(BUTTON_DOUBLE_CLICK); // repeat hit
            }

            btn->event = (uint8_t)BUTTON_MULTIPLE_CLICK;

            /** Calling the callbacks for MULTIPLE BUTTON CLICKS */
            for (int i = 0; i < btn->size[btn->event]; i++) {
                if (btn->repeat == btn->cb_info[btn->event][i].event_args.multiple_clicks.clicks) {
                    btn->cb_info[btn->event][i].cb(btn, btn->cb_info[btn->event][i].usr_data);
                }
            }

            btn->event = (uint8_t)BUTTON_PRESS_REPEAT_DONE;
            CALL_EVENT_CB(BUTTON_PRESS_REPEAT_DONE); // repeat hit
            btn->repeat = 0;
            btn->state = 0;
            btn->event = (uint8_t)BUTTON_PRESS_END;
            CALL_EVENT_CB(BUTTON_PRESS_END);
        }
        break;

    case 3:
        if (btn->button_level != BUTTON_ACTIVE) {
            btn->event = (uint8_t)BUTTON_PRESS_UP;
            CALL_EVENT_CB(BUTTON_PRESS_UP);
            if (btn->ticks < btn->short_press_ticks) {
                btn->ticks = 0;
                btn->state = PRESS_REPEAT_DOWN_CHECK; //repeat press
            } else {
                btn->state = PRESS_DOWN_CHECK;
                btn->event = (uint8_t)BUTTON_PRESS_END;
                CALL_EVENT_CB(BUTTON_PRESS_END);
            }
        }
        break;

    case PRESS_LONG_PRESS_UP_CHECK:
        if (btn->button_level == BUTTON_ACTIVE) {
            //continue hold trigger
            if (btn->ticks >= (btn->long_press_hold_cnt + 1) * SERIAL_TICKS + btn->long_press_ticks) {
                btn->event = (uint8_t)BUTTON_LONG_PRESS_HOLD;
                btn->long_press_hold_cnt++;
                CALL_EVENT_CB(BUTTON_LONG_PRESS_HOLD);
            }

            /** Calling callbacks for BUTTON_LONG_PRESS_START based on press_time */
            uint32_t pressed_time = iot_button_get_pressed_time(btn);
            if (btn->cb_info[BUTTON_LONG_PRESS_START]) {
                button_cb_info_t *cb_info = btn->cb_info[BUTTON_LONG_PRESS_START];
                uint16_t time = cb_info[btn->count[0]].event_args.long_press.press_time;
                if (btn->long_press_ticks * TICKS_INTERVAL > time) {
                    for (int i = btn->count[0] + 1; i < btn->size[BUTTON_LONG_PRESS_START]; i++) {
                        time = cb_info[i].event_args.long_press.press_time;
                        if (btn->long_press_ticks * TICKS_INTERVAL <= time) {
                            btn->count[0] = i;
                            break;
                        }
                    }
                }
                if (btn->count[0] < btn->size[BUTTON_LONG_PRESS_START] && abs((int)pressed_time - (int)time) <= TOLERANCE) {
                    btn->event = (uint8_t)BUTTON_LONG_PRESS_START;
                    do {
                        cb_info[btn->count[0]].cb(btn, cb_info[btn->count[0]].usr_data);
                        btn->count[0]++;
                        if (btn->count[0] >= btn->size[BUTTON_LONG_PRESS_START]) {
                            break;
                        }
                    } while (time == cb_info[btn->count[0]].event_args.long_press.press_time);
                }
            }

            /** Updating counter for BUTTON_LONG_PRESS_UP press_time */
            if (btn->cb_info[BUTTON_LONG_PRESS_UP]) {
                button_cb_info_t *cb_info = btn->cb_info[BUTTON_LONG_PRESS_UP];
                uint16_t time = cb_info[btn->count[1] + 1].event_args.long_press.press_time;
                if (btn->long_press_ticks * TICKS_INTERVAL > time) {
                    for (int i = btn->count[1] + 1; i < btn->size[BUTTON_LONG_PRESS_UP]; i++) {
                        time = cb_info[i].event_args.long_press.press_time;
                        if (btn->long_press_ticks * TICKS_INTERVAL <= time) {
                            btn->count[1] = i;
                            break;
                        }
                    }
                }
                if (btn->count[1] + 1 < btn->size[BUTTON_LONG_PRESS_UP] && abs((int)pressed_time - (int)time) <= TOLERANCE) {
                    do {
                        btn->count[1]++;
                        if (btn->count[1] + 1 >= btn->size[BUTTON_LONG_PRESS_UP]) {
                            break;
                        }
                    } while (time == cb_info[btn->count[1] + 1].event_args.long_press.press_time);
                }
            }
        } else { //releasd

            btn->event = BUTTON_LONG_PRESS_UP;

            /** calling callbacks for BUTTON_LONG_PRESS_UP press_time */
            if (btn->cb_info[btn->event] && btn->count[1] >= 0) {
                button_cb_info_t *cb_info = btn->cb_info[btn->event];
                do {
                    cb_info[btn->count[1]].cb(btn, cb_info[btn->count[1]].usr_data);
                    if (!btn->count[1]) {
                        break;
                    }
                    btn->count[1]--;
                } while (cb_info[btn->count[1]].event_args.long_press.press_time == cb_info[btn->count[1] + 1].event_args.long_press.press_time);

                /** Reset the counter */
                btn->count[1] = -1;
            }
            /** Reset counter */
            if (btn->cb_info[BUTTON_LONG_PRESS_START]) {
                btn->count[0] = 0;
            }

            btn->event = (uint8_t)BUTTON_PRESS_UP;
            CALL_EVENT_CB(BUTTON_PRESS_UP);
            btn->state = PRESS_DOWN_CHECK; //reset
            btn->long_press_hold_cnt = 0;
            btn->event = (uint8_t)BUTTON_PRESS_END;
            CALL_EVENT_CB(BUTTON_PRESS_END);
        }
        break;
    }
}

static void button_cb(void *args)
{
    button_dev_t *target;
    /*!< When all buttons enter the BUTTON_NONE_PRESS state, the system enters low-power mode */
    bool enter_power_save_flag = true;
    for (target = g_head_handle; target; target = target->next) {
        button_handler(target);
        if (!(target->driver->enable_power_save && target->debounce_cnt == 0 && target->event == BUTTON_NONE_PRESS)) {
            enter_power_save_flag = false;
        }
    }
    if (enter_power_save_flag) {
        /*!< Stop esp timer for power save */
        if (g_is_timer_running) {
            esp_timer_stop(g_button_timer_handle);
            g_is_timer_running = false;
        }
        for (target = g_head_handle; target; target = target->next) {
            if (target->driver->enable_power_save && target->driver->enter_power_save) {
                target->driver->enter_power_save(target->driver);
            }
        }
        /*!< Notify the user that the Button has entered power save mode by calling this callback function. */
        if (power_save_usr_cfg.enter_power_save_cb) {
            power_save_usr_cfg.enter_power_save_cb(power_save_usr_cfg.usr_data);
        }
    }
}

esp_err_t iot_button_register_cb(button_handle_t btn_handle, button_event_t event, button_event_args_t *event_args, button_cb_t cb, void *usr_data)
{
    ESP_RETURN_ON_FALSE(NULL != btn_handle, ESP_ERR_INVALID_ARG, TAG, "Pointer of handle is invalid");
    button_dev_t *btn = (button_dev_t *) btn_handle;
    ESP_RETURN_ON_FALSE(event < BUTTON_EVENT_MAX, ESP_ERR_INVALID_ARG, TAG, "event is invalid");
    ESP_RETURN_ON_FALSE(NULL != cb, ESP_ERR_INVALID_ARG, TAG, "Pointer of cb is invalid");
    ESP_RETURN_ON_FALSE(event != BUTTON_MULTIPLE_CLICK || event_args, ESP_ERR_INVALID_ARG, TAG, "event is invalid");

    if (event_args) {
        ESP_RETURN_ON_FALSE(!(event == BUTTON_LONG_PRESS_START || event == BUTTON_LONG_PRESS_UP) || event_args->long_press.press_time > btn->short_press_ticks * TICKS_INTERVAL, ESP_ERR_INVALID_ARG, TAG, "event_args is invalid");
        ESP_RETURN_ON_FALSE(event != BUTTON_MULTIPLE_CLICK || event_args->multiple_clicks.clicks, ESP_ERR_INVALID_ARG, TAG, "event_args is invalid");
    }

    if (!btn->cb_info[event]) {
        btn->cb_info[event] = calloc(1, sizeof(button_cb_info_t));
        BTN_CHECK(NULL != btn->cb_info[event], "calloc cb_info failed", ESP_ERR_NO_MEM);
        if (event == BUTTON_LONG_PRESS_START) {
            btn->count[0] = 0;
        } else if (event == BUTTON_LONG_PRESS_UP) {
            btn->count[1] = -1;
        }
    } else {
        button_cb_info_t *p = realloc(btn->cb_info[event], sizeof(button_cb_info_t) * (btn->size[event] + 1));
        BTN_CHECK(NULL != p, "realloc cb_info failed", ESP_ERR_NO_MEM);
        btn->cb_info[event] = p;
    }

    btn->cb_info[event][btn->size[event]].cb = cb;
    btn->cb_info[event][btn->size[event]].usr_data = usr_data;
    btn->size[event]++;

    /** Inserting the event_args in sorted manner */
    if (event == BUTTON_LONG_PRESS_START || event == BUTTON_LONG_PRESS_UP) {
        uint16_t press_time = btn->long_press_ticks * TICKS_INTERVAL;
        if (event_args) {
            press_time = event_args->long_press.press_time;
        }
        BTN_CHECK(press_time / TICKS_INTERVAL > btn->short_press_ticks, "press_time event_args is less than short_press_ticks", ESP_ERR_INVALID_ARG);
        if (btn->size[event] >= 2) {
            for (int i = btn->size[event] - 2; i >= 0; i--) {
                if (btn->cb_info[event][i].event_args.long_press.press_time > press_time) {
                    btn->cb_info[event][i + 1] = btn->cb_info[event][i];

                    btn->cb_info[event][i].event_args.long_press.press_time = press_time;
                    btn->cb_info[event][i].cb = cb;
                    btn->cb_info[event][i].usr_data = usr_data;
                } else {
                    btn->cb_info[event][i + 1].event_args.long_press.press_time = press_time;
                    btn->cb_info[event][i + 1].cb = cb;
                    btn->cb_info[event][i + 1].usr_data = usr_data;
                    break;
                }
            }
        } else {
            btn->cb_info[event][btn->size[event] - 1].event_args.long_press.press_time = press_time;
        }

        int32_t press_ticks = press_time / TICKS_INTERVAL;
        if (btn->short_press_ticks < press_ticks && press_ticks < btn->long_press_ticks) {
            iot_button_set_param(btn, BUTTON_LONG_PRESS_TIME_MS, (void*)(intptr_t)press_time);
        }
    }

    if (event == BUTTON_MULTIPLE_CLICK) {
        uint16_t clicks = btn->long_press_ticks * TICKS_INTERVAL;
        if (event_args) {
            clicks = event_args->multiple_clicks.clicks;
        }
        if (btn->size[event] >= 2) {
            for (int i = btn->size[event] - 2; i >= 0; i--) {
                if (btn->cb_info[event][i].event_args.multiple_clicks.clicks > clicks) {
                    btn->cb_info[event][i + 1] = btn->cb_info[event][i];

                    btn->cb_info[event][i].event_args.multiple_clicks.clicks = clicks;
                    btn->cb_info[event][i].cb = cb;
                    btn->cb_info[event][i].usr_data = usr_data;
                } else {
                    btn->cb_info[event][i + 1].event_args.multiple_clicks.clicks = clicks;
                    btn->cb_info[event][i + 1].cb = cb;
                    btn->cb_info[event][i + 1].usr_data = usr_data;
                    break;
                }
            }
        } else {
            btn->cb_info[event][btn->size[event] - 1].event_args.multiple_clicks.clicks = clicks;
        }
    }
    return ESP_OK;
}

esp_err_t iot_button_unregister_cb(button_handle_t btn_handle, button_event_t event, button_event_args_t *event_args)
{
    ESP_RETURN_ON_FALSE(NULL != btn_handle, ESP_ERR_INVALID_ARG, TAG, "Pointer of handle is invalid");
    ESP_RETURN_ON_FALSE(event < BUTTON_EVENT_MAX, ESP_ERR_INVALID_ARG, TAG, "event is invalid");
    button_dev_t *btn = (button_dev_t *) btn_handle;
    ESP_RETURN_ON_FALSE(btn->cb_info[event], ESP_ERR_INVALID_STATE, TAG, "No callbacks registered for the event");

    int check = -1;

    if ((event == BUTTON_LONG_PRESS_START || event == BUTTON_LONG_PRESS_UP) && event_args) {
        if (event_args->long_press.press_time != 0) {
            goto unregister_event;
        }
    }

    if (event == BUTTON_MULTIPLE_CLICK && event_args) {
        if (event_args->multiple_clicks.clicks != 0) {
            goto unregister_event;
        }
    }

    if (btn->cb_info[event]) {
        free(btn->cb_info[event]);

        /** Reset the counter */
        if (event == BUTTON_LONG_PRESS_START) {
            btn->count[0] = 0;
        } else if (event == BUTTON_LONG_PRESS_UP) {
            btn->count[1] = -1;
        }

    }

    btn->cb_info[event] = NULL;
    btn->size[event] = 0;
    return ESP_OK;

unregister_event:

    for (int i = 0; i < btn->size[event]; i++) {
        if ((event == BUTTON_LONG_PRESS_START || event == BUTTON_LONG_PRESS_UP) && event_args->long_press.press_time) {
            if (event_args->long_press.press_time != btn->cb_info[event][i].event_args.long_press.press_time) {
                continue;
            }
        }

        if (event == BUTTON_MULTIPLE_CLICK && event_args->multiple_clicks.clicks) {
            if (event_args->multiple_clicks.clicks != btn->cb_info[event][i].event_args.multiple_clicks.clicks) {
                continue;
            }
        }
        check = i;
        for (int j = i; j <= btn->size[event] - 1; j++) {
            btn->cb_info[event][j] = btn->cb_info[event][j + 1];
        }

        if (btn->size[event] != 1) {
            button_cb_info_t *p = realloc(btn->cb_info[event], sizeof(button_cb_info_t) * (btn->size[event] - 1));
            BTN_CHECK(NULL != p, "realloc cb_info failed", ESP_ERR_NO_MEM);
            btn->cb_info[event] = p;
            btn->size[event]--;
        } else {
            free(btn->cb_info[event]);
            btn->cb_info[event] = NULL;
            btn->size[event] = 0;
        }
        break;
    }

    ESP_RETURN_ON_FALSE(check != -1, ESP_ERR_NOT_FOUND, TAG, "No such callback registered for the event");
    return ESP_OK;
}

size_t iot_button_count_cb(button_handle_t btn_handle)
{
    BTN_CHECK(NULL != btn_handle, "Pointer of handle is invalid", ESP_ERR_INVALID_ARG);
    button_dev_t *btn = (button_dev_t *) btn_handle;
    size_t ret = 0;
    for (size_t i = 0; i < BUTTON_EVENT_MAX; i++) {
        if (btn->cb_info[i]) {
            ret += btn->size[i];
        }
    }
    return ret;
}

size_t iot_button_count_event_cb(button_handle_t btn_handle, button_event_t event)
{
    BTN_CHECK(NULL != btn_handle, "Pointer of handle is invalid", ESP_ERR_INVALID_ARG);
    button_dev_t *btn = (button_dev_t *) btn_handle;
    return btn->size[event];
}

button_event_t iot_button_get_event(button_handle_t btn_handle)
{
    BTN_CHECK(NULL != btn_handle, "Pointer of handle is invalid", BUTTON_NONE_PRESS);
    button_dev_t *btn = (button_dev_t *) btn_handle;
    return btn->event;
}

const char *iot_button_get_event_str(button_event_t event)
{
    BTN_CHECK(event <= BUTTON_NONE_PRESS && event >= BUTTON_PRESS_DOWN, "event value is invalid", "invalid event");
    return button_event_str[event];
}

esp_err_t iot_button_print_event(button_handle_t btn_handle)
{
    BTN_CHECK(NULL != btn_handle, "Pointer of handle is invalid", ESP_FAIL);
    button_dev_t *btn = (button_dev_t *) btn_handle;
    ESP_LOGI(TAG, "%s", button_event_str[btn->event]);
    return ESP_OK;
}

uint8_t iot_button_get_repeat(button_handle_t btn_handle)
{
    BTN_CHECK(NULL != btn_handle, "Pointer of handle is invalid", 0);
    button_dev_t *btn = (button_dev_t *) btn_handle;
    return btn->repeat;
}

uint32_t iot_button_get_pressed_time(button_handle_t btn_handle)
{
    BTN_CHECK(NULL != btn_handle, "Pointer of handle is invalid", 0);
    button_dev_t *btn = (button_dev_t *) btn_handle;
    return (btn->ticks * TICKS_INTERVAL);
}

uint32_t iot_button_get_ticks_time(button_handle_t btn_handle)
{
    return iot_button_get_pressed_time(btn_handle);
}

uint16_t iot_button_get_long_press_hold_cnt(button_handle_t btn_handle)
{
    BTN_CHECK(NULL != btn_handle, "Pointer of handle is invalid", 0);
    button_dev_t *btn = (button_dev_t *) btn_handle;
    return btn->long_press_hold_cnt;
}

esp_err_t iot_button_set_param(button_handle_t btn_handle, button_param_t param, void *value)
{
    BTN_CHECK(NULL != btn_handle, "Pointer of handle is invalid", ESP_ERR_INVALID_ARG);
    button_dev_t *btn = (button_dev_t *) btn_handle;
    BUTTON_ENTER_CRITICAL();
    switch (param) {
    case BUTTON_LONG_PRESS_TIME_MS:
        btn->long_press_ticks = (int32_t)value / TICKS_INTERVAL;
        break;
    case BUTTON_SHORT_PRESS_TIME_MS:
        btn->short_press_ticks = (int32_t)value / TICKS_INTERVAL;
        break;
    default:
        break;
    }
    BUTTON_EXIT_CRITICAL();
    return ESP_OK;
}

uint8_t iot_button_get_key_level(button_handle_t btn_handle)
{
    BTN_CHECK(NULL != btn_handle, "Pointer of handle is invalid", 0);
    button_dev_t *btn = (button_dev_t *)btn_handle;
    uint8_t level = btn->driver->get_key_level(btn->driver);
    return level;
}

esp_err_t iot_button_resume(void)
{
    if (!g_button_timer_handle) {
        return ESP_ERR_INVALID_STATE;
    }
    if (!g_is_timer_running) {
        esp_timer_start_periodic(g_button_timer_handle, TICKS_INTERVAL * 1000U);
        g_is_timer_running = true;
    }
    return ESP_OK;
}

esp_err_t iot_button_stop(void)
{
    BTN_CHECK(g_button_timer_handle, "Button timer handle is invalid", ESP_ERR_INVALID_STATE);
    BTN_CHECK(g_is_timer_running, "Button timer is not running", ESP_ERR_INVALID_STATE);

    esp_err_t err = esp_timer_stop(g_button_timer_handle);
    BTN_CHECK(ESP_OK == err, "Button timer stop failed", ESP_FAIL);
    g_is_timer_running = false;
    return ESP_OK;
}

esp_err_t iot_button_register_power_save_cb(const button_power_save_config_t *config)
{
    BTN_CHECK(g_head_handle, "No button registered", ESP_ERR_INVALID_STATE);
    BTN_CHECK(config->enter_power_save_cb, "Enter power save callback is invalid", ESP_ERR_INVALID_ARG);

    power_save_usr_cfg.enter_power_save_cb = config->enter_power_save_cb;
    power_save_usr_cfg.usr_data = config->usr_data;
    return ESP_OK;
}

esp_err_t iot_button_create(const button_config_t *config, const button_driver_t *driver, button_handle_t *ret_button)
{
    if (!g_head_handle) {
        ESP_LOGI(TAG, "IoT Button Version: %d.%d.%d", BUTTON_VER_MAJOR, BUTTON_VER_MINOR, BUTTON_VER_PATCH);
    }
    ESP_RETURN_ON_FALSE(driver && config && ret_button, ESP_ERR_INVALID_ARG, TAG, "Invalid argument");
    button_dev_t *btn = (button_dev_t *) calloc(1, sizeof(button_dev_t));
    ESP_RETURN_ON_FALSE(btn, ESP_ERR_NO_MEM, TAG, "Button memory alloc failed");

    btn->driver = (button_driver_t *)driver;
    btn->long_press_ticks = TIME_TO_TICKS(config->long_press_time, LONG_TICKS);
    btn->short_press_ticks = TIME_TO_TICKS(config->short_press_time, SHORT_TICKS);
    btn->event = BUTTON_NONE_PRESS;
    btn->button_level = BUTTON_INACTIVE;

    btn->next = g_head_handle;
    g_head_handle = btn;

    if (!g_button_timer_handle) {
        esp_timer_create_args_t button_timer = {0};
        button_timer.arg = NULL;
        button_timer.callback = button_cb;
        button_timer.dispatch_method = ESP_TIMER_TASK;
        button_timer.name = "button_timer";
        esp_timer_create(&button_timer, &g_button_timer_handle);
    }

    if (!driver->enable_power_save && !g_is_timer_running) {
        esp_timer_start_periodic(g_button_timer_handle, TICKS_INTERVAL * 1000U);
        g_is_timer_running = true;
    }

    *ret_button = (button_handle_t)btn;
    return ESP_OK;
}

esp_err_t iot_button_delete(button_handle_t btn_handle)
{
    esp_err_t ret = ESP_OK;
    ESP_RETURN_ON_FALSE(NULL != btn_handle, ESP_ERR_INVALID_ARG, TAG, "Pointer of handle is invalid");
    button_dev_t *btn = (button_dev_t *)btn_handle;

    for (int i = 0; i < BUTTON_EVENT_MAX; i++) {
        if (btn->cb_info[i]) {
            free(btn->cb_info[i]);
        }
    }

    ret = btn->driver->del(btn->driver);
    ESP_RETURN_ON_FALSE(ESP_OK == ret, ret, TAG, "Failed to delete button driver");

    button_dev_t **curr;
    for (curr = &g_head_handle; *curr;) {
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
        g_button_timer_handle = NULL;
        g_is_timer_running = false;
    }
    return ESP_OK;
}
