/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "esp_log.h"
#include "esp_err.h"
#include "esp_check.h"
#include "esp_lvgl_port.h"

static const char *TAG = "LVGL";

/*******************************************************************************
* Types definitions
*******************************************************************************/

typedef enum {
    LVGL_PORT_NAV_BTN_PREV,
    LVGL_PORT_NAV_BTN_NEXT,
    LVGL_PORT_NAV_BTN_ENTER,
    LVGL_PORT_NAV_BTN_CNT,
} lvgl_port_nav_btns_t;

typedef struct {
    button_handle_t btn[LVGL_PORT_NAV_BTN_CNT];     /* Button handlers */
    lv_indev_t      *indev;  /* LVGL input device driver */
    bool btn_prev; /* Button prev state */
    bool btn_next; /* Button next state */
    bool btn_enter; /* Button enter state */
} lvgl_port_nav_btns_ctx_t;

/*******************************************************************************
* Function definitions
*******************************************************************************/

static void lvgl_port_navigation_buttons_read(lv_indev_t *indev_drv, lv_indev_data_t *data);
static void lvgl_port_btn_down_handler(void *arg, void *arg2);
static void lvgl_port_btn_up_handler(void *arg, void *arg2);

/*******************************************************************************
* Public API functions
*******************************************************************************/

lv_indev_t *lvgl_port_add_navigation_buttons(const lvgl_port_nav_btns_cfg_t *buttons_cfg)
{
    lv_indev_t *indev;
    esp_err_t ret = ESP_OK;
    assert(buttons_cfg != NULL);
    assert(buttons_cfg->disp != NULL);

    /* Touch context */
    lvgl_port_nav_btns_ctx_t *buttons_ctx = malloc(sizeof(lvgl_port_nav_btns_ctx_t));
    if (buttons_ctx == NULL) {
        ESP_LOGE(TAG, "Not enough memory for buttons context allocation!");
        return NULL;
    }

    /* Previous button */
    if (buttons_cfg->button_prev != NULL) {
        buttons_ctx->btn[LVGL_PORT_NAV_BTN_PREV] = iot_button_create(buttons_cfg->button_prev);
        ESP_GOTO_ON_FALSE(buttons_ctx->btn[LVGL_PORT_NAV_BTN_PREV], ESP_ERR_NO_MEM, err, TAG, "Not enough memory for button create!");
    }

    /* Next button */
    if (buttons_cfg->button_next != NULL) {
        buttons_ctx->btn[LVGL_PORT_NAV_BTN_NEXT] = iot_button_create(buttons_cfg->button_next);
        ESP_GOTO_ON_FALSE(buttons_ctx->btn[LVGL_PORT_NAV_BTN_NEXT], ESP_ERR_NO_MEM, err, TAG, "Not enough memory for button create!");
    }

    /* Enter button */
    if (buttons_cfg->button_enter != NULL) {
        buttons_ctx->btn[LVGL_PORT_NAV_BTN_ENTER] = iot_button_create(buttons_cfg->button_enter);
        ESP_GOTO_ON_FALSE(buttons_ctx->btn[LVGL_PORT_NAV_BTN_ENTER], ESP_ERR_NO_MEM, err, TAG, "Not enough memory for button create!");
    }

    /* Button handlers */
    for (int i = 0; i < LVGL_PORT_NAV_BTN_CNT; i++) {
        ESP_ERROR_CHECK(iot_button_register_cb(buttons_ctx->btn[i], BUTTON_PRESS_DOWN, lvgl_port_btn_down_handler, buttons_ctx));
        ESP_ERROR_CHECK(iot_button_register_cb(buttons_ctx->btn[i], BUTTON_PRESS_UP, lvgl_port_btn_up_handler, buttons_ctx));
    }

    buttons_ctx->btn_prev = false;
    buttons_ctx->btn_next = false;
    buttons_ctx->btn_enter = false;

    lvgl_port_lock(0);
    /* Register a touchpad input device */
    indev = lv_indev_create();
    lv_indev_set_type(indev, LV_INDEV_TYPE_ENCODER);
    lv_indev_set_mode(indev, LV_INDEV_MODE_EVENT);
    lv_indev_set_read_cb(indev, lvgl_port_navigation_buttons_read);
    lv_indev_set_disp(indev, buttons_cfg->disp);
    lv_indev_set_user_data(indev, buttons_ctx);
    //buttons_ctx->indev->long_press_repeat_time = 300;
    buttons_ctx->indev = indev;
    lvgl_port_unlock();

    return indev;

err:
    if (ret != ESP_OK) {
        for (int i = 0; i < LVGL_PORT_NAV_BTN_CNT; i++) {
            if (buttons_ctx->btn[i] != NULL) {
                iot_button_delete(buttons_ctx->btn[i]);
            }
        }

        if (buttons_ctx != NULL) {
            free(buttons_ctx);
        }
    }

    return NULL;
}

esp_err_t lvgl_port_remove_navigation_buttons(lv_indev_t *buttons)
{
    assert(buttons);
    lvgl_port_nav_btns_ctx_t *buttons_ctx = (lvgl_port_nav_btns_ctx_t *)lv_indev_get_user_data(buttons);

    lvgl_port_lock(0);
    /* Remove input device driver */
    lv_indev_delete(buttons);
    lvgl_port_unlock();

    if (buttons_ctx) {
        free(buttons_ctx);
    }

    return ESP_OK;
}

/*******************************************************************************
* Private functions
*******************************************************************************/

static void lvgl_port_navigation_buttons_read(lv_indev_t *indev_drv, lv_indev_data_t *data)
{
    static uint32_t last_key = 0;

    assert(indev_drv);
    lvgl_port_nav_btns_ctx_t *ctx = (lvgl_port_nav_btns_ctx_t *)lv_indev_get_user_data(indev_drv);
    assert(ctx);

    /* Buttons */
    if (ctx->btn_prev) {
        data->key = LV_KEY_LEFT;
        last_key = LV_KEY_LEFT;
        data->state = LV_INDEV_STATE_PRESSED;
    } else if (ctx->btn_next) {
        data->key = LV_KEY_RIGHT;
        last_key = LV_KEY_RIGHT;
        data->state = LV_INDEV_STATE_PRESSED;
    } else if (ctx->btn_enter) {
        data->key = LV_KEY_ENTER;
        last_key = LV_KEY_ENTER;
        data->state = LV_INDEV_STATE_PRESSED;
    } else {
        data->key = last_key;
        data->state = LV_INDEV_STATE_RELEASED;
    }
}

static void lvgl_port_btn_down_handler(void *arg, void *arg2)
{
    lvgl_port_nav_btns_ctx_t *ctx = (lvgl_port_nav_btns_ctx_t *) arg2;
    button_handle_t button = (button_handle_t)arg;
    if (ctx && button) {
        /* PREV */
        if (button == ctx->btn[LVGL_PORT_NAV_BTN_PREV]) {
            ctx->btn_prev = true;
        }
        /* NEXT */
        if (button == ctx->btn[LVGL_PORT_NAV_BTN_NEXT]) {
            ctx->btn_next = true;
        }
        /* ENTER */
        if (button == ctx->btn[LVGL_PORT_NAV_BTN_ENTER]) {
            ctx->btn_enter = true;
        }
    }

    /* Wake LVGL task, if needed */
    lvgl_port_task_wake(LVGL_PORT_EVENT_TOUCH, ctx->indev);
}

static void lvgl_port_btn_up_handler(void *arg, void *arg2)
{
    lvgl_port_nav_btns_ctx_t *ctx = (lvgl_port_nav_btns_ctx_t *) arg2;
    button_handle_t button = (button_handle_t)arg;
    if (ctx && button) {
        /* PREV */
        if (button == ctx->btn[LVGL_PORT_NAV_BTN_PREV]) {
            ctx->btn_prev = false;
        }
        /* NEXT */
        if (button == ctx->btn[LVGL_PORT_NAV_BTN_NEXT]) {
            ctx->btn_next = false;
        }
        /* ENTER */
        if (button == ctx->btn[LVGL_PORT_NAV_BTN_ENTER]) {
            ctx->btn_enter = false;
        }
    }

    /* Wake LVGL task, if needed */
    lvgl_port_task_wake(LVGL_PORT_EVENT_TOUCH, ctx->indev);
}
