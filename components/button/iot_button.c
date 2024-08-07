/* SPDX-FileCopyrightText: 2022-2024 Espressif Systems (Shanghai) CO LTD
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
#if CONFIG_GPIO_BUTTON_SUPPORT_POWER_SAVE
#include "esp_pm.h"
#endif
#include "iot_button.h"
#include "sdkconfig.h"

static const char *TAG = "button";
static portMUX_TYPE s_button_lock = portMUX_INITIALIZER_UNLOCKED;
#define BUTTON_ENTER_CRITICAL()           portENTER_CRITICAL(&s_button_lock)
#define BUTTON_EXIT_CRITICAL()            portEXIT_CRITICAL(&s_button_lock)

#define BTN_CHECK(a, str, ret_val)                                \
    if (!(a)) {                                                   \
        ESP_LOGE(TAG, "%s(%d): %s", __FUNCTION__, __LINE__, str); \
        return (ret_val);                                         \
    }

/**
 * @brief Structs to store callback info
 *
 */
typedef struct {
    button_cb_t cb;
    void *usr_data;
    button_event_data_t event_data;
} button_cb_info_t;

/**
 * @brief Structs to record individual key parameters
 *
 */
typedef struct Button {
    uint16_t             ticks;
    uint16_t             long_press_ticks;     /*! Trigger ticks for long press*/
    uint16_t             short_press_ticks;    /*! Trigger ticks for repeat press*/
    uint16_t             long_press_hold_cnt;  /*! Record long press hold count*/
    uint16_t             long_press_ticks_default;
    uint8_t              repeat;
    uint8_t              state: 3;
    uint8_t              debounce_cnt: 3;
    uint8_t              active_level: 1;
    uint8_t              button_level: 1;
    uint8_t              enable_power_save: 1;
    button_event_t       event;
    uint8_t (*hal_button_Level)(void *hardware_data);
    esp_err_t (*hal_button_deinit)(void *hardware_data);
    void                 *hardware_data;
    button_type_t        type;
    button_cb_info_t     *cb_info[BUTTON_EVENT_MAX];
    size_t               size[BUTTON_EVENT_MAX];
    int                  count[2];
    struct Button        *next;
} button_dev_t;

//button handle list head.
static button_dev_t *g_head_handle = NULL;
static esp_timer_handle_t g_button_timer_handle = NULL;
static bool g_is_timer_running = false;
#if CONFIG_GPIO_BUTTON_SUPPORT_POWER_SAVE
static button_power_save_config_t power_save_usr_cfg = {0};
#endif

#define TICKS_INTERVAL    CONFIG_BUTTON_PERIOD_TIME_MS
#define DEBOUNCE_TICKS    CONFIG_BUTTON_DEBOUNCE_TICKS //MAX 8
#define SHORT_TICKS       (CONFIG_BUTTON_SHORT_PRESS_TIME_MS /TICKS_INTERVAL)
#define LONG_TICKS        (CONFIG_BUTTON_LONG_PRESS_TIME_MS /TICKS_INTERVAL)
#define SERIAL_TICKS      (CONFIG_BUTTON_SERIAL_TIME_MS /TICKS_INTERVAL)
#define TOLERANCE         CONFIG_BUTTON_LONG_PRESS_TOLERANCE_MS

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
            btn->state = 4;
            /** Calling callbacks for BUTTON_LONG_PRESS_START */
            uint16_t ticks_time = iot_button_get_ticks_time(btn);
            if (btn->cb_info[btn->event] && btn->count[0] == 0) {
                if (abs(ticks_time - (btn->long_press_ticks * TICKS_INTERVAL)) <= TOLERANCE && btn->cb_info[btn->event][btn->count[0]].event_data.long_press.press_time == (btn->long_press_ticks * TICKS_INTERVAL)) {
                    do {
                        btn->cb_info[btn->event][btn->count[0]].cb(btn, btn->cb_info[btn->event][btn->count[0]].usr_data);
                        btn->count[0]++;
                        if (btn->count[0] >= btn->size[btn->event]) {
                            break;
                        }
                    } while (btn->cb_info[btn->event][btn->count[0]].event_data.long_press.press_time == btn->long_press_ticks * TICKS_INTERVAL);
                }
            }
        }
        break;

    case 2:
        if (btn->button_level == btn->active_level) {
            btn->event = (uint8_t)BUTTON_PRESS_DOWN;
            CALL_EVENT_CB(BUTTON_PRESS_DOWN);
            btn->event = (uint8_t)BUTTON_PRESS_REPEAT;
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

            btn->event = (uint8_t)BUTTON_MULTIPLE_CLICK;

            /** Calling the callbacks for MULTIPLE BUTTON CLICKS */
            for (int i = 0; i < btn->size[btn->event]; i++) {
                if (btn->repeat == btn->cb_info[btn->event][i].event_data.multiple_clicks.clicks) {
                    do {
                        btn->cb_info[btn->event][i].cb(btn, btn->cb_info[btn->event][i].usr_data);
                        i++;
                        if (i >= btn->size[btn->event]) {
                            break;
                        }
                    } while (btn->cb_info[btn->event][i].event_data.multiple_clicks.clicks == btn->repeat);
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
        if (btn->button_level != btn->active_level) {
            btn->event = (uint8_t)BUTTON_PRESS_UP;
            CALL_EVENT_CB(BUTTON_PRESS_UP);
            if (btn->ticks < btn->short_press_ticks) {
                btn->ticks = 0;
                btn->state = 2; //repeat press
            } else {
                btn->state = 0;
                btn->event = (uint8_t)BUTTON_PRESS_END;
                CALL_EVENT_CB(BUTTON_PRESS_END);
            }
        }
        break;

    case 4:
        if (btn->button_level == btn->active_level) {
            //continue hold trigger
            if (btn->ticks >= (btn->long_press_hold_cnt + 1) * SERIAL_TICKS + btn->long_press_ticks) {
                btn->event = (uint8_t)BUTTON_LONG_PRESS_HOLD;
                btn->long_press_hold_cnt++;
                CALL_EVENT_CB(BUTTON_LONG_PRESS_HOLD);

                /** Calling callbacks for BUTTON_LONG_PRESS_START based on press_time */
                uint16_t ticks_time = iot_button_get_ticks_time(btn);
                if (btn->cb_info[BUTTON_LONG_PRESS_START]) {
                    button_cb_info_t *cb_info = btn->cb_info[BUTTON_LONG_PRESS_START];
                    uint16_t time = cb_info[btn->count[0]].event_data.long_press.press_time;
                    if (btn->long_press_ticks * TICKS_INTERVAL > time) {
                        for (int i = btn->count[0] + 1; i < btn->size[BUTTON_LONG_PRESS_START]; i++) {
                            time = cb_info[i].event_data.long_press.press_time;
                            if (btn->long_press_ticks * TICKS_INTERVAL <= time) {
                                btn->count[0] = i;
                                break;
                            }
                        }
                    }
                    if (btn->count[0] < btn->size[BUTTON_LONG_PRESS_START] && abs(ticks_time - time) <= TOLERANCE) {
                        do {
                            cb_info[btn->count[0]].cb(btn, cb_info[btn->count[0]].usr_data);
                            btn->count[0]++;
                            if (btn->count[0] >= btn->size[BUTTON_LONG_PRESS_START]) {
                                break;
                            }
                        } while (time == cb_info[btn->count[0]].event_data.long_press.press_time);
                    }
                }

                /** Updating counter for BUTTON_LONG_PRESS_UP press_time */
                if (btn->cb_info[BUTTON_LONG_PRESS_UP]) {
                    button_cb_info_t *cb_info = btn->cb_info[BUTTON_LONG_PRESS_UP];
                    uint16_t time = cb_info[btn->count[1] + 1].event_data.long_press.press_time;
                    if (btn->long_press_ticks * TICKS_INTERVAL > time) {
                        for (int i = btn->count[1] + 1; i < btn->size[BUTTON_LONG_PRESS_UP]; i++) {
                            time = cb_info[i].event_data.long_press.press_time;
                            if (btn->long_press_ticks * TICKS_INTERVAL <= time) {
                                btn->count[1] = i;
                                break;
                            }
                        }
                    }
                    if (btn->count[1] + 1 < btn->size[BUTTON_LONG_PRESS_UP] && abs(ticks_time - time) <= TOLERANCE) {
                        do {
                            btn->count[1]++;
                            if (btn->count[1] + 1 >= btn->size[BUTTON_LONG_PRESS_UP]) {
                                break;
                            }
                        } while (time == cb_info[btn->count[1] + 1].event_data.long_press.press_time);
                    }
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
                } while (cb_info[btn->count[1]].event_data.long_press.press_time == cb_info[btn->count[1] + 1].event_data.long_press.press_time);

                /** Reset the counter */
                btn->count[1] = -1;
            }
            /** Reset counter */
            if (btn->cb_info[BUTTON_LONG_PRESS_START]) {
                btn->count[0] = 0;
            }

            btn->event = (uint8_t)BUTTON_PRESS_UP;
            CALL_EVENT_CB(BUTTON_PRESS_UP);
            btn->state = 0; //reset
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
#if CONFIG_GPIO_BUTTON_SUPPORT_POWER_SAVE
    bool enter_power_save_flag = true;
#endif
    for (target = g_head_handle; target; target = target->next) {
        button_handler(target);
#if CONFIG_GPIO_BUTTON_SUPPORT_POWER_SAVE
        if (!(target->enable_power_save && target->debounce_cnt == 0 && target->event == BUTTON_NONE_PRESS)) {
            enter_power_save_flag = false;
        }
#endif
    }
#if CONFIG_GPIO_BUTTON_SUPPORT_POWER_SAVE
    if (enter_power_save_flag) {
        /*!< Stop esp timer for power save */
        if (g_is_timer_running) {
            esp_timer_stop(g_button_timer_handle);
            g_is_timer_running = false;
        }
        for (target = g_head_handle; target; target = target->next) {
            if (target->type == BUTTON_TYPE_GPIO && target->enable_power_save) {
                button_gpio_intr_control((int)(target->hardware_data), true);
                button_gpio_enable_gpio_wakeup((uint32_t)(target->hardware_data), target->active_level, true);
            }
        }
        /*!< Notify the user that the Button has entered power save mode by calling this callback function. */
        if (power_save_usr_cfg.enter_power_save_cb) {
            power_save_usr_cfg.enter_power_save_cb(power_save_usr_cfg.usr_data);
        }
    }
#endif
}

#if CONFIG_GPIO_BUTTON_SUPPORT_POWER_SAVE
static void IRAM_ATTR button_power_save_isr_handler(void* arg)
{
    if (!g_is_timer_running) {
        esp_timer_start_periodic(g_button_timer_handle, TICKS_INTERVAL * 1000U);
        g_is_timer_running = true;
    }
    button_gpio_intr_control((int)arg, false);
    /*!< disable gpio wakeup not need active level*/
    button_gpio_enable_gpio_wakeup((uint32_t)arg, 0, false);
}
#endif

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
    btn->long_press_ticks_default = btn->long_press_ticks;
    btn->short_press_ticks = short_press_ticks;

    /** Add handle to list */
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

    return btn;
}

static esp_err_t button_delete_com(button_dev_t *btn)
{
    BTN_CHECK(NULL != btn, "Pointer of handle is invalid", ESP_ERR_INVALID_ARG);

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
#if CONFIG_GPIO_BUTTON_SUPPORT_POWER_SAVE
        if (cfg->enable_power_save) {
            btn->enable_power_save = cfg->enable_power_save;
            button_gpio_set_intr(cfg->gpio_num, cfg->active_level == 0 ? GPIO_INTR_LOW_LEVEL : GPIO_INTR_HIGH_LEVEL, button_power_save_isr_handler, (void *)cfg->gpio_num);
        }
#endif
    } break;
#if CONFIG_SOC_ADC_SUPPORTED
    case BUTTON_TYPE_ADC: {
        const button_adc_config_t *cfg = &(config->adc_button_config);
        ret = button_adc_init(cfg);
        BTN_CHECK(ESP_OK == ret, "adc button init failed", NULL);
        btn = button_create_com(1, button_adc_get_key_level, (void *)ADC_BUTTON_COMBINE(cfg->adc_channel, cfg->button_index), long_press_time, short_press_time);
    } break;
#endif
    case BUTTON_TYPE_MATRIX: {
        const button_matrix_config_t *cfg = &(config->matrix_button_config);
        ret = button_matrix_init(cfg);
        BTN_CHECK(ESP_OK == ret, "matrix button init failed", NULL);
        btn = button_create_com(1, button_matrix_get_key_level, (void *)MATRIX_BUTTON_COMBINE(cfg->row_gpio_num, cfg->col_gpio_num), long_press_time, short_press_time);
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
    if (!btn->enable_power_save && !g_is_timer_running) {
        esp_timer_start_periodic(g_button_timer_handle, TICKS_INTERVAL * 1000U);
        g_is_timer_running = true;
    }
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
#if CONFIG_SOC_ADC_SUPPORTED
    case BUTTON_TYPE_ADC:
        ret = button_adc_deinit(ADC_BUTTON_SPLIT_CHANNEL(btn->hardware_data), ADC_BUTTON_SPLIT_INDEX(btn->hardware_data));
        break;
#endif
    case BUTTON_TYPE_MATRIX:
        ret = button_matrix_deinit(MATRIX_BUTTON_SPLIT_ROW(btn->hardware_data), MATRIX_BUTTON_SPLIT_COL(btn->hardware_data));
        break;
    case BUTTON_TYPE_CUSTOM:
        if (btn->hal_button_deinit) {
            ret = btn->hal_button_deinit(btn->hardware_data);
        }

        break;
    default:
        break;
    }
    BTN_CHECK(ESP_OK == ret, "button deinit failed", ESP_FAIL);
    for (int i = 0; i < BUTTON_EVENT_MAX; i++) {
        if (btn->cb_info[i]) {
            free(btn->cb_info[i]);
        }
    }
    button_delete_com(btn);
    return ESP_OK;
}

esp_err_t iot_button_register_cb(button_handle_t btn_handle, button_event_t event, button_cb_t cb, void *usr_data)
{
    BTN_CHECK(NULL != btn_handle, "Pointer of handle is invalid", ESP_ERR_INVALID_ARG);
    button_dev_t *btn = (button_dev_t *) btn_handle;
    BTN_CHECK(event != BUTTON_MULTIPLE_CLICK, "event argument is invalid", ESP_ERR_INVALID_ARG);
    button_event_config_t event_cfg = {
        .event = event,
    };

    if ((event == BUTTON_LONG_PRESS_START || event == BUTTON_LONG_PRESS_UP) && !event_cfg.event_data.long_press.press_time) {
        event_cfg.event_data.long_press.press_time = btn->long_press_ticks_default * TICKS_INTERVAL;
    }

    return iot_button_register_event_cb(btn_handle, event_cfg, cb, usr_data);
}

esp_err_t iot_button_register_event_cb(button_handle_t btn_handle, button_event_config_t event_cfg, button_cb_t cb, void *usr_data)
{
    BTN_CHECK(NULL != btn_handle, "Pointer of handle is invalid", ESP_ERR_INVALID_ARG);
    button_dev_t *btn = (button_dev_t *) btn_handle;
    button_event_t event = event_cfg.event;
    BTN_CHECK(event < BUTTON_EVENT_MAX, "event is invalid", ESP_ERR_INVALID_ARG);
    BTN_CHECK(!(event == BUTTON_LONG_PRESS_START || event == BUTTON_LONG_PRESS_UP) || event_cfg.event_data.long_press.press_time > btn->short_press_ticks * TICKS_INTERVAL, "event_data is invalid", ESP_ERR_INVALID_ARG);
    BTN_CHECK(event != BUTTON_MULTIPLE_CLICK || event_cfg.event_data.multiple_clicks.clicks, "event_data is invalid", ESP_ERR_INVALID_ARG);

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

    /** Inserting the event_data in sorted manner */
    if (event == BUTTON_LONG_PRESS_START || event == BUTTON_LONG_PRESS_UP) {
        uint16_t press_time = event_cfg.event_data.long_press.press_time;
        BTN_CHECK(press_time / TICKS_INTERVAL > btn->short_press_ticks, "press_time event_data is less than short_press_ticks", ESP_ERR_INVALID_ARG);
        if (btn->size[event] >= 2) {
            for (int i = btn->size[event] - 2; i >= 0; i--) {
                if (btn->cb_info[event][i].event_data.long_press.press_time > press_time) {
                    btn->cb_info[event][i + 1] = btn->cb_info[event][i];

                    btn->cb_info[event][i].event_data.long_press.press_time = press_time;
                    btn->cb_info[event][i].cb = cb;
                    btn->cb_info[event][i].usr_data = usr_data;
                } else {
                    btn->cb_info[event][i + 1].event_data.long_press.press_time = press_time;
                    btn->cb_info[event][i + 1].cb = cb;
                    btn->cb_info[event][i + 1].usr_data = usr_data;
                    break;
                }
            }
        } else {
            btn->cb_info[event][btn->size[event] - 1].event_data.long_press.press_time = press_time;
        }

        int32_t press_ticks = press_time / TICKS_INTERVAL;
        if (btn->short_press_ticks < press_ticks && press_ticks < btn->long_press_ticks) {
            iot_button_set_param(btn, BUTTON_LONG_PRESS_TIME_MS, (void*)(intptr_t)press_time);
        }
    }

    if (event == BUTTON_MULTIPLE_CLICK) {
        if (btn->size[event] >= 2) {
            for (int i = btn->size[event] - 2; i >= 0; i--) {
                if (btn->cb_info[event][i].event_data.multiple_clicks.clicks > event_cfg.event_data.multiple_clicks.clicks) {
                    btn->cb_info[event][i + 1] = btn->cb_info[event][i];

                    btn->cb_info[event][i].event_data.multiple_clicks.clicks = event_cfg.event_data.multiple_clicks.clicks;
                    btn->cb_info[event][i].cb = cb;
                    btn->cb_info[event][i].usr_data = usr_data;
                } else {
                    btn->cb_info[event][i + 1].event_data.multiple_clicks.clicks = event_cfg.event_data.multiple_clicks.clicks;
                    btn->cb_info[event][i + 1].cb = cb;
                    btn->cb_info[event][i + 1].usr_data = usr_data;
                    break;
                }
            }
        } else {
            btn->cb_info[event][btn->size[event] - 1].event_data.multiple_clicks.clicks = event_cfg.event_data.multiple_clicks.clicks;
        }
    }

    return ESP_OK;
}

esp_err_t iot_button_unregister_cb(button_handle_t btn_handle, button_event_t event)
{
    BTN_CHECK(NULL != btn_handle, "Pointer of handle is invalid", ESP_ERR_INVALID_ARG);
    BTN_CHECK(event < BUTTON_EVENT_MAX, "event is invalid", ESP_ERR_INVALID_ARG);
    button_dev_t *btn = (button_dev_t *) btn_handle;
    BTN_CHECK(NULL != btn->cb_info[event], "No callbacks registered for the event", ESP_ERR_INVALID_STATE);

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
}

esp_err_t iot_button_unregister_event(button_handle_t btn_handle, button_event_config_t event_cfg, button_cb_t cb)
{
    BTN_CHECK(NULL != btn_handle, "Pointer of handle is invalid", ESP_ERR_INVALID_ARG);
    button_event_t event = event_cfg.event;
    BTN_CHECK(event < BUTTON_EVENT_MAX, "event is invalid", ESP_ERR_INVALID_ARG);
    BTN_CHECK(NULL != cb, "Pointer to function callback is invalid", ESP_ERR_INVALID_ARG);
    button_dev_t *btn = (button_dev_t *) btn_handle;

    int check = -1;

    for (int i = 0; i < btn->size[event]; i++) {
        if (cb == btn->cb_info[event][i].cb) {
            if ((event == BUTTON_LONG_PRESS_START || event == BUTTON_LONG_PRESS_UP) && event_cfg.event_data.long_press.press_time) {
                if (event_cfg.event_data.long_press.press_time != btn->cb_info[event][i].event_data.long_press.press_time) {
                    continue;
                }
            }

            if (event == BUTTON_MULTIPLE_CLICK && event_cfg.event_data.multiple_clicks.clicks) {
                if (event_cfg.event_data.multiple_clicks.clicks != btn->cb_info[event][i].event_data.multiple_clicks.clicks) {
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
    }

    BTN_CHECK(check != -1, "No such callback registered for the event", ESP_ERR_INVALID_STATE);

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

size_t iot_button_count_event(button_handle_t btn_handle, button_event_t event)
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
    uint8_t level = btn->hal_button_Level(btn->hardware_data);
    return (level == btn->active_level) ? 1 : 0;
}

esp_err_t iot_button_resume(void)
{
    BTN_CHECK(g_button_timer_handle, "Button timer handle is invalid", ESP_ERR_INVALID_STATE);
    BTN_CHECK(!g_is_timer_running, "Button timer is already running", ESP_ERR_INVALID_STATE);

    esp_err_t err = esp_timer_start_periodic(g_button_timer_handle, TICKS_INTERVAL * 1000U);
    BTN_CHECK(ESP_OK == err, "Button timer start failed", ESP_FAIL);
    g_is_timer_running = true;
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

#if CONFIG_GPIO_BUTTON_SUPPORT_POWER_SAVE
esp_err_t iot_button_register_power_save_cb(const button_power_save_config_t *config)
{
    BTN_CHECK(g_head_handle, "No button registered", ESP_ERR_INVALID_STATE);
    BTN_CHECK(config->enter_power_save_cb, "Enter power save callback is invalid", ESP_ERR_INVALID_ARG);

    power_save_usr_cfg.enter_power_save_cb = config->enter_power_save_cb;
    power_save_usr_cfg.usr_data = config->usr_data;
    return ESP_OK;
}
#endif
