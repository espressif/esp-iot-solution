/*
 * SPDX-FileCopyrightText: 2016-2021 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "iot_knob.h"

static const char *TAG = "Knob";

#define KNOB_CHECK(a, str, ret_val)                          \
    if (!(a))                                                     \
    {                                                             \
        ESP_LOGE(TAG, "%s(%d): %s", __FUNCTION__, __LINE__, str); \
        return (ret_val);                                         \
    }

#define KNOB_CHECK_GOTO(a, str, label) if(!(a)) { \
        ESP_LOGE(TAG,"%s:%d (%s):%s", __FILE__, __LINE__, __FUNCTION__, str); \
        goto label; \
    }

#define CALL_EVENT_CB(ev)   if(knob->cb[ev])knob->cb[ev](knob, knob->usr_data)

typedef enum {
    KNOB_READY = 0,                     /*!< Knob state: ready*/
    KNOB_PHASE_A,                       /*!< Knob state: phase A arrives first */
    KNOB_PHASE_B,                       /*!< Knob state: phase B arrives first */
} knob_state_t;

typedef struct Knob {
    bool          encoder_a_change;                            /*<! true means Encoder A phase Inverted*/
    bool          encoder_b_change;                            /*<! true means Encoder B phase Inverted*/
    uint8_t       default_direction;                           /*!< 0:positive increase   1:negative increase */
    knob_state_t  state;                                       /*!< knob state machine status */
    uint8_t       debounce_a_cnt;                              /*!< Encoder A phase debounce count */
    uint8_t       debounce_b_cnt;                              /*!< Encoder B phase debounce count */
    uint8_t       encoder_a_level;                             /*!< Encoder A phase current level */
    uint8_t       encoder_b_level;                             /*!< Encoder B phase current Level */
    knob_event_t  event;                                       /*!< Current event */
    uint16_t      ticks;                                       /*!< Timer interrupt count */
    int32_t       count_value;                                 /*!< Knob count */
    uint8_t       (*hal_knob_level)(void *hardware_data);      /*!< Get current level */
    void          *encoder_a;                                  /*!< Encoder A phase gpio number */
    void          *encoder_b;                                  /*!< Encoder B phase gpio number */
    void          *usr_data[KNOB_EVENT_MAX];                   /*!< User data for event */
    knob_cb_t     cb[KNOB_EVENT_MAX];                          /*!< Event callback */
    struct Knob   *next;                                       /*!< Next pointer */
} knob_dev_t;

static knob_dev_t *s_head_handle = NULL;
static esp_timer_handle_t s_knob_timer_handle;
static bool s_is_timer_running = false;

#define TICKS_INTERVAL    CONFIG_KNOB_PERIOD_TIME_MS
#define DEBOUNCE_TICKS    CONFIG_KNOB_DEBOUNCE_TICKS
#define HIGH_LIMIT        CONFIG_KNOB_HIGH_LIMIT
#define LOW_LIMIT         CONFIG_KNOB_LOW_LIMIT

static void knob_handler(knob_dev_t *knob)
{
    uint8_t pha_value = knob->hal_knob_level(knob->encoder_a);
    uint8_t phb_value = knob->hal_knob_level(knob->encoder_b);

    if ((knob->state) > 0) {
        knob->ticks++;
    }

    if (pha_value != knob->encoder_a_level) {
        if (++(knob->debounce_a_cnt) >= DEBOUNCE_TICKS) {
            knob->encoder_a_change = true;
            knob->encoder_a_level = pha_value;
            knob->debounce_a_cnt = 0;
        }
    } else {
        knob->debounce_a_cnt = 0;
    }

    if (phb_value != knob->encoder_b_level) {
        if (++(knob->debounce_b_cnt) >= DEBOUNCE_TICKS) {
            knob->encoder_b_change = true;
            knob->encoder_b_level = phb_value;
            knob->debounce_b_cnt = 0;
        }
    } else {
        knob->debounce_b_cnt = 0;
    }

    switch (knob->state) {
    case KNOB_READY:
        if (knob->encoder_a_change) {
            knob->encoder_a_change = false;
            knob->ticks = 0;
            knob->state = KNOB_PHASE_A;
        } else if ( knob->encoder_b_change) {
            knob->encoder_b_change = false;
            knob->ticks = 0;
            knob->state = KNOB_PHASE_B;
        }
        break;

    case KNOB_PHASE_A:
        if (knob->encoder_b_change) {
            knob->encoder_b_change = false;
            if (knob->default_direction) {
                knob->count_value--;
                knob->event = KNOB_LEFT;
                CALL_EVENT_CB(KNOB_LEFT);
                if (knob->count_value <= LOW_LIMIT) {
                    knob->event = KNOB_L_LIM;
                    CALL_EVENT_CB(KNOB_L_LIM);
                    knob->count_value = 0;
                } else if (knob->count_value == 0) {
                    knob->event = KNOB_ZERO;
                    CALL_EVENT_CB(KNOB_ZERO);
                }
            } else {
                knob->count_value++;
                knob->event = KNOB_RIGHT;
                CALL_EVENT_CB(KNOB_RIGHT);
                if (knob->count_value >= HIGH_LIMIT) {
                    knob->event = KNOB_H_LIM;
                    CALL_EVENT_CB(KNOB_H_LIM);
                    knob->count_value = 0;
                } else if (knob->count_value == 0) {
                    knob->event = KNOB_ZERO;
                    CALL_EVENT_CB(KNOB_ZERO);
                }
            }
            knob->ticks = 0;
            knob->state = KNOB_READY;
        } else if (knob->encoder_a_change) {
            knob->encoder_a_change = false;
            knob->ticks = 0;
            knob->state = KNOB_READY;
        }
        break;

    case KNOB_PHASE_B:
        if (knob->encoder_a_change) {
            knob->encoder_a_change = false;
            if (knob->default_direction) {
                knob->count_value++;
                knob->event = KNOB_RIGHT;
                CALL_EVENT_CB(KNOB_RIGHT);
                if (knob->count_value >= HIGH_LIMIT) {
                    knob->event = KNOB_H_LIM;
                    CALL_EVENT_CB(KNOB_H_LIM);
                    knob->count_value = 0;
                } else if (knob->count_value == 0) {
                    knob->event = KNOB_ZERO;
                    CALL_EVENT_CB(KNOB_ZERO);
                }
            } else {
                knob->count_value--;
                knob->event = KNOB_LEFT;
                CALL_EVENT_CB(KNOB_LEFT);
                if (knob->count_value <= LOW_LIMIT) {
                    knob->event = KNOB_L_LIM;
                    CALL_EVENT_CB(KNOB_L_LIM);
                    knob->count_value = 0;
                } else if (knob->count_value == 0) {
                    knob->event = KNOB_ZERO;
                    CALL_EVENT_CB(KNOB_ZERO);
                }
            }
            knob->ticks = 0;
            knob->state = KNOB_READY;
        } else if (knob->encoder_b_change) {
            knob->encoder_b_change = false;
            knob->ticks = 0;
            knob->state = KNOB_READY;
        }
        break;
    }
}

static esp_err_t _knob_gpio_init(uint8_t gpio_num)
{
    gpio_config_t gpio_cfg = {
        .pin_bit_mask = (1ULL << gpio_num),
        .mode = GPIO_MODE_INPUT,
        .intr_type = GPIO_INTR_ANYEDGE,
        .pull_up_en = 1,
    };
    esp_err_t ret = gpio_config(&gpio_cfg);

    return ret;
}

static esp_err_t _knob_gpio_deinit(int gpio_num)
{
    /** both disable pullup and pulldown */
    gpio_config_t gpio_conf = {
        .intr_type = GPIO_INTR_DISABLE,
        .mode = GPIO_MODE_INPUT,
        .pin_bit_mask = (1ULL << gpio_num),
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .pull_up_en = GPIO_PULLUP_DISABLE,
    };
    gpio_config(&gpio_conf);
    return ESP_OK;
}

static uint8_t _knob_gpio_get_key_level(void *gpio_num)
{
    return (uint8_t)gpio_get_level((uint32_t)gpio_num);
}

static void knob_cb(void *args)
{
    knob_dev_t *target;
    for (target = s_head_handle; target; target = target->next) {
        knob_handler(target);
    }
}

knob_handle_t iot_knob_create(const knob_config_t *config)
{
    KNOB_CHECK(NULL != config, "config pointer can't be NULL!", NULL)
    KNOB_CHECK(config->gpio_encoder_a != config->gpio_encoder_b, "encoder A can't be the same as encoder B", NULL);
    esp_err_t ret = ESP_OK;
    ret = _knob_gpio_init(config->gpio_encoder_a);
    KNOB_CHECK(ESP_OK == ret, "encoder A gpio init failed", NULL);
    ret = _knob_gpio_init(config->gpio_encoder_b);
    KNOB_CHECK_GOTO(ESP_OK == ret, "encoder B gpio init failed", _encoder_a_deinit);

    knob_dev_t *knob = (knob_dev_t *) calloc(1, sizeof(knob_dev_t));
    KNOB_CHECK_GOTO(NULL != knob, "alloc knob failed", _encoder_b_deinit);
    knob->default_direction = config->default_direction;
    knob->hal_knob_level = _knob_gpio_get_key_level;
    knob->encoder_a = (void *)(long)config->gpio_encoder_a;
    knob->encoder_b = (void *)(long)config->gpio_encoder_b;

    knob->encoder_a_level = knob->hal_knob_level(knob->encoder_a);
    knob->encoder_b_level = knob->hal_knob_level(knob->encoder_b);

    knob->next = s_head_handle;
    s_head_handle = knob;

    if (false == s_is_timer_running) {
        esp_timer_create_args_t knob_timer;
        knob_timer.arg = NULL;
        knob_timer.callback = knob_cb;
        knob_timer.dispatch_method = ESP_TIMER_TASK;
        knob_timer.name = "knob_timer";
        esp_timer_create(&knob_timer, &s_knob_timer_handle);
        esp_timer_start_periodic(s_knob_timer_handle, TICKS_INTERVAL * 1000U);
        s_is_timer_running = true;
    }

    ESP_LOGI(TAG, "Iot Knob Config Succeed, encoder A:%d, encoder B:%d, direction:%d, Version: %d.%d.%d",config->gpio_encoder_a, config->gpio_encoder_b, config->default_direction, KNOB_VER_MAJOR, KNOB_VER_MINOR, KNOB_VER_PATCH);
    return (knob_handle_t)knob;

_encoder_b_deinit:
    _knob_gpio_deinit(config->gpio_encoder_b);
_encoder_a_deinit:
    _knob_gpio_deinit(config->gpio_encoder_a);
    return NULL;
}

esp_err_t iot_knob_delete(knob_handle_t knob_handle)
{
    esp_err_t ret = ESP_OK;
    KNOB_CHECK(NULL != knob_handle, "Pointer of handle is invalid", ESP_ERR_INVALID_ARG);
    knob_dev_t *knob = (knob_dev_t *)knob_handle;
    ret = _knob_gpio_deinit((int)(knob->usr_data));
    KNOB_CHECK(ESP_OK == ret, "knob deinit failed", ESP_FAIL);
    knob_dev_t **curr;
    for (curr = &s_head_handle; *curr; ) {
        knob_dev_t *entry = *curr;
        if (entry == knob) {
            *curr = entry->next;
            free(entry);
        } else {
            curr = &entry->next;
        }
    }

    uint16_t number = 0;
    knob_dev_t *target = s_head_handle;
    while (target) {
        target = target->next;
        number++;
    }
    ESP_LOGD(TAG, "remain knob number=%d", number);

    if (0 == number && s_is_timer_running) { /**<  if all knob is deleted, stop the timer */
        esp_timer_stop(s_knob_timer_handle);
        esp_timer_delete(s_knob_timer_handle);
        s_is_timer_running = false;
    }

    return ESP_OK;
}

esp_err_t iot_knob_register_cb(knob_handle_t knob_handle, knob_event_t event, knob_cb_t cb, void *usr_data)
{
    KNOB_CHECK(NULL != knob_handle, "Pointer of handle is invalid", ESP_ERR_INVALID_ARG);
    KNOB_CHECK(event < KNOB_EVENT_MAX, "event is invalid", ESP_ERR_INVALID_ARG);
    knob_dev_t *knob = (knob_dev_t *) knob_handle;
    knob->cb[event] = cb;
    knob->usr_data[event] = usr_data;
    return ESP_OK;
}

esp_err_t iot_knob_unregister_cb(knob_handle_t knob_handle, knob_event_t event)
{
    KNOB_CHECK(NULL != knob_handle, "Pointer of handle is invalid", ESP_ERR_INVALID_ARG);
    KNOB_CHECK(event < KNOB_EVENT_MAX, "event is invalid", ESP_ERR_INVALID_ARG);
    knob_dev_t *knob = (knob_dev_t *) knob_handle;
    knob->cb[event] = NULL;
    return ESP_OK;
}

knob_event_t iot_knob_get_event(knob_handle_t knob_handle)
{
    KNOB_CHECK(NULL != knob_handle, "Pointer of handle is invalid", ESP_ERR_INVALID_ARG);
    knob_dev_t *knob = (knob_dev_t *) knob_handle;
    return knob->event;
}

int32_t iot_knob_get_count_value(knob_handle_t knob_handle)
{
    KNOB_CHECK(NULL != knob_handle, "Pointer of handle is invalid", ESP_ERR_INVALID_ARG);
    knob_dev_t *knob = (knob_dev_t *) knob_handle;
    return knob->count_value;
}

esp_err_t iot_knob_clear_count_value(knob_handle_t knob_handle)
{
    KNOB_CHECK(NULL != knob_handle, "Pointer of handle is invalid", ESP_ERR_INVALID_ARG);
    knob_dev_t *knob = (knob_dev_t *) knob_handle;
    knob->count_value = 0;
    return ESP_OK;
}