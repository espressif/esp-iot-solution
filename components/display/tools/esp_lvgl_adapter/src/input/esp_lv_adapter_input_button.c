/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file esp_lv_adapter_input_button.c
 * @brief Navigation buttons input device implementation for LVGL adapter
 */

#include <stdlib.h>
#include <stdbool.h>
#include "esp_err.h"
#include "esp_log.h"
#include "esp_check.h"
#include "esp_lv_adapter_input.h"
#include "adapter_internal.h"
#include "button_compat.h"

/* Tag for logging */
static const char *TAG = "esp_lvgl:button";

/**
 * @brief Button index enumeration
 */
typedef enum {
    ESP_LV_ADAPTER_BTN_PREV,      /*!< Previous/left button */
    ESP_LV_ADAPTER_BTN_NEXT,      /*!< Next/right button */
    ESP_LV_ADAPTER_BTN_ENTER,     /*!< Enter/confirm button */
    ESP_LV_ADAPTER_BTN_COUNT      /*!< Total number of buttons */
} esp_lv_adapter_btn_index_t;

/*****************************************************************************
 *                         LVGL v9 Implementation                            *
 *****************************************************************************/

#if LVGL_VERSION_MAJOR >= 9

/**
 * @brief Navigation buttons context for LVGL v9
 */
typedef struct {
    button_handle_t btn[ESP_LV_ADAPTER_BTN_COUNT];    /*!< Button handles array */
    lv_indev_t *indev;                          /*!< LVGL input device */
    bool btn_prev;                              /*!< Previous button state */
    bool btn_next;                              /*!< Next button state */
    bool btn_enter;                             /*!< Enter button state */
} esp_lv_adapter_button_ctx_t;

/* Forward declarations */
static void lvgl_button_read(lv_indev_t *indev_drv, lv_indev_data_t *data);
static void lvgl_button_down(void *arg, void *arg2);
static void lvgl_button_up(void *arg, void *arg2);

lv_indev_t *esp_lv_adapter_register_navigation_buttons(const esp_lv_adapter_nav_buttons_config_t *config)
{
    if (!config || !config->disp) {
        return NULL;
    }

    /* Validate button configurations/handles */
    if (!button_compat_is_valid(config->button_prev) ||
            !button_compat_is_valid(config->button_next) ||
            !button_compat_is_valid(config->button_enter)) {
        return NULL;
    }

    esp_lv_adapter_button_ctx_t *buttons_ctx = calloc(1, sizeof(esp_lv_adapter_button_ctx_t));
    if (!buttons_ctx) {
        ESP_LOGE(TAG, "alloc ctx");
        return NULL;
    }

    /* Create button handles using compatibility layer */
    esp_lv_adapter_button_config_or_handle_t btn_configs[ESP_LV_ADAPTER_BTN_COUNT] = {
        config->button_prev,
        config->button_next,
        config->button_enter
    };

    for (int i = 0; i < ESP_LV_ADAPTER_BTN_COUNT; i++) {
        if (button_compat_create(btn_configs[i], &buttons_ctx->btn[i]) != ESP_OK) {
            for (int j = 0; j < i; j++) {
                button_compat_delete(buttons_ctx->btn[j]);
            }
            free(buttons_ctx);
            return NULL;
        }
    }

    /* Register button callbacks using compatibility layer */
    for (int i = 0; i < ESP_LV_ADAPTER_BTN_COUNT; i++) {
        if (buttons_ctx->btn[i]) {
            esp_err_t err = button_compat_register_cb(buttons_ctx->btn[i], BUTTON_PRESS_DOWN, lvgl_button_down, buttons_ctx);
            if (err != ESP_OK) {
                for (int j = 0; j < ESP_LV_ADAPTER_BTN_COUNT; j++) {
                    button_compat_delete(buttons_ctx->btn[j]);
                }
                free(buttons_ctx);
                return NULL;
            }
            err = button_compat_register_cb(buttons_ctx->btn[i], BUTTON_PRESS_UP, lvgl_button_up, buttons_ctx);
            if (err != ESP_OK) {
                for (int j = 0; j < ESP_LV_ADAPTER_BTN_COUNT; j++) {
                    button_compat_delete(buttons_ctx->btn[j]);
                }
                free(buttons_ctx);
                return NULL;
            }
        }
    }

    if (esp_lv_adapter_lock((uint32_t) -1) != ESP_OK) {
        for (int i = 0; i < ESP_LV_ADAPTER_BTN_COUNT; i++) {
            button_compat_delete(buttons_ctx->btn[i]);
        }
        free(buttons_ctx);
        return NULL;
    }

    lv_indev_t *indev = lv_indev_create();
    if (!indev) {
        esp_lv_adapter_unlock();
        for (int i = 0; i < ESP_LV_ADAPTER_BTN_COUNT; i++) {
            button_compat_delete(buttons_ctx->btn[i]);
        }
        free(buttons_ctx);
        return NULL;
    }

    lv_indev_set_type(indev, LV_INDEV_TYPE_ENCODER);
    lv_indev_set_mode(indev, LV_INDEV_MODE_EVENT);
    lv_indev_set_read_cb(indev, lvgl_button_read);
    lv_indev_set_disp(indev, config->disp);
    lv_indev_set_driver_data(indev, buttons_ctx);
    buttons_ctx->indev = indev;

    esp_lv_adapter_unlock();

    esp_err_t ret = esp_lv_adapter_register_input_device(indev, ESP_LV_ADAPTER_INPUT_TYPE_BUTTON, buttons_ctx);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to register input device for management: %d", ret);
    }

    return indev;
}

esp_err_t esp_lv_adapter_unregister_navigation_buttons(lv_indev_t *buttons)
{
    ESP_RETURN_ON_FALSE(buttons, ESP_ERR_INVALID_ARG, TAG, "Navigation input handle cannot be NULL");

    esp_lv_adapter_unregister_input_device(buttons);

    esp_lv_adapter_button_ctx_t *buttons_ctx = (esp_lv_adapter_button_ctx_t *)lv_indev_get_driver_data(buttons);

    esp_err_t ret = esp_lv_adapter_lock((uint32_t) -1);
    ESP_RETURN_ON_ERROR(ret, TAG, "Failed to acquire LVGL lock (ret=%d)", ret);

    lv_indev_delete(buttons);
    esp_lv_adapter_unlock();

    if (buttons_ctx) {
        for (int i = 0; i < ESP_LV_ADAPTER_BTN_COUNT; i++) {
            button_compat_delete(buttons_ctx->btn[i]);
        }
        free(buttons_ctx);
    }

    return ESP_OK;
}

static void lvgl_button_read(lv_indev_t *indev_drv, lv_indev_data_t *data)
{
    static uint32_t last_key;
    esp_lv_adapter_button_ctx_t *ctx = (esp_lv_adapter_button_ctx_t *)lv_indev_get_driver_data(indev_drv);
    if (!ctx) {
        data->state = LV_INDEV_STATE_RELEASED;
        return;
    }

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

static void lvgl_button_down(void *arg, void *arg2)
{
    button_handle_t button = (button_handle_t)arg;
    esp_lv_adapter_button_ctx_t *ctx = (esp_lv_adapter_button_ctx_t *)arg2;
    if (!ctx) {
        return;
    }

    if (button == ctx->btn[ESP_LV_ADAPTER_BTN_PREV]) {
        ctx->btn_prev = true;
    }
    if (button == ctx->btn[ESP_LV_ADAPTER_BTN_NEXT]) {
        ctx->btn_next = true;
    }
    if (button == ctx->btn[ESP_LV_ADAPTER_BTN_ENTER]) {
        ctx->btn_enter = true;
    }
}

static void lvgl_button_up(void *arg, void *arg2)
{
    button_handle_t button = (button_handle_t)arg;
    esp_lv_adapter_button_ctx_t *ctx = (esp_lv_adapter_button_ctx_t *)arg2;
    if (!ctx) {
        return;
    }

    if (button == ctx->btn[ESP_LV_ADAPTER_BTN_PREV]) {
        ctx->btn_prev = false;
    }
    if (button == ctx->btn[ESP_LV_ADAPTER_BTN_NEXT]) {
        ctx->btn_next = false;
    }
    if (button == ctx->btn[ESP_LV_ADAPTER_BTN_ENTER]) {
        ctx->btn_enter = false;
    }
}

#else

typedef struct {
    button_handle_t btn[ESP_LV_ADAPTER_BTN_COUNT];
    lv_indev_drv_t indev_drv;
    bool btn_prev;
    bool btn_next;
    bool btn_enter;
} esp_lv_adapter_button_ctx_t;

static void lvgl_button_read(lv_indev_drv_t *indev_drv, lv_indev_data_t *data);
static void lvgl_button_down(void *arg, void *arg2);
static void lvgl_button_up(void *arg, void *arg2);

lv_indev_t *esp_lv_adapter_register_navigation_buttons(const esp_lv_adapter_nav_buttons_config_t *config)
{
    if (!config || !config->disp) {
        return NULL;
    }

    if (!button_compat_is_valid(config->button_prev) ||
            !button_compat_is_valid(config->button_next) ||
            !button_compat_is_valid(config->button_enter)) {
        return NULL;
    }

    esp_lv_adapter_button_ctx_t *buttons_ctx = calloc(1, sizeof(esp_lv_adapter_button_ctx_t));
    if (!buttons_ctx) {
        ESP_LOGE(TAG, "alloc ctx");
        return NULL;
    }

    esp_lv_adapter_button_config_or_handle_t btn_configs[ESP_LV_ADAPTER_BTN_COUNT] = {
        config->button_prev,
        config->button_next,
        config->button_enter
    };

    for (int i = 0; i < ESP_LV_ADAPTER_BTN_COUNT; i++) {
        if (button_compat_create(btn_configs[i], &buttons_ctx->btn[i]) != ESP_OK) {
            for (int j = 0; j < i; j++) {
                button_compat_delete(buttons_ctx->btn[j]);
            }
            free(buttons_ctx);
            return NULL;
        }
    }

    for (int i = 0; i < ESP_LV_ADAPTER_BTN_COUNT; i++) {
        if (buttons_ctx->btn[i]) {
            if (button_compat_register_cb(buttons_ctx->btn[i], BUTTON_PRESS_DOWN, lvgl_button_down, buttons_ctx) != ESP_OK) {
                for (int j = 0; j < ESP_LV_ADAPTER_BTN_COUNT; j++) {
                    button_compat_delete(buttons_ctx->btn[j]);
                }
                free(buttons_ctx);
                return NULL;
            }
            if (button_compat_register_cb(buttons_ctx->btn[i], BUTTON_PRESS_UP, lvgl_button_up, buttons_ctx) != ESP_OK) {
                for (int j = 0; j < ESP_LV_ADAPTER_BTN_COUNT; j++) {
                    button_compat_delete(buttons_ctx->btn[j]);
                }
                free(buttons_ctx);
                return NULL;
            }
        }
    }

    if (esp_lv_adapter_lock((uint32_t) -1) != ESP_OK) {
        for (int i = 0; i < ESP_LV_ADAPTER_BTN_COUNT; i++) {
            button_compat_delete(buttons_ctx->btn[i]);
        }
        free(buttons_ctx);
        return NULL;
    }

    lv_indev_drv_init(&buttons_ctx->indev_drv);
    buttons_ctx->indev_drv.type = LV_INDEV_TYPE_ENCODER;
    buttons_ctx->indev_drv.disp = config->disp;
    buttons_ctx->indev_drv.read_cb = lvgl_button_read;
    buttons_ctx->indev_drv.user_data = buttons_ctx;
    buttons_ctx->indev_drv.long_press_repeat_time = 300;

    lv_indev_t *indev = lv_indev_drv_register(&buttons_ctx->indev_drv);
    if (!indev) {
        esp_lv_adapter_unlock();
        for (int i = 0; i < ESP_LV_ADAPTER_BTN_COUNT; i++) {
            button_compat_delete(buttons_ctx->btn[i]);
        }
        free(buttons_ctx);
        return NULL;
    }

    esp_lv_adapter_unlock();

    esp_err_t ret = esp_lv_adapter_register_input_device(indev, ESP_LV_ADAPTER_INPUT_TYPE_BUTTON, buttons_ctx);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to register input device for management: %d", ret);
    }

    return indev;
}

esp_err_t esp_lv_adapter_unregister_navigation_buttons(lv_indev_t *buttons)
{
    ESP_RETURN_ON_FALSE(buttons, ESP_ERR_INVALID_ARG, TAG, "Navigation input handle cannot be NULL");

    lv_indev_drv_t *indev_drv = buttons->driver;
    ESP_RETURN_ON_FALSE(indev_drv, ESP_ERR_INVALID_STATE, TAG, "Navigation driver not initialized");

    esp_lv_adapter_button_ctx_t *buttons_ctx = (esp_lv_adapter_button_ctx_t *)indev_drv->user_data;

    esp_lv_adapter_unregister_input_device(buttons);

    esp_err_t ret = esp_lv_adapter_lock((uint32_t) -1);
    ESP_RETURN_ON_ERROR(ret, TAG, "Failed to acquire LVGL lock (ret=%d)", ret);

    lv_indev_delete(buttons);
    esp_lv_adapter_unlock();

    if (buttons_ctx) {
        for (int i = 0; i < ESP_LV_ADAPTER_BTN_COUNT; i++) {
            button_compat_delete(buttons_ctx->btn[i]);
        }
        free(buttons_ctx);
    }

    return ESP_OK;
}

static void lvgl_button_read(lv_indev_drv_t *indev_drv, lv_indev_data_t *data)
{
    static uint32_t last_key;
    esp_lv_adapter_button_ctx_t *ctx = (esp_lv_adapter_button_ctx_t *)indev_drv->user_data;
    if (!ctx) {
        data->state = LV_INDEV_STATE_RELEASED;
        return;
    }

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

static void lvgl_button_down(void *arg, void *arg2)
{
    button_handle_t button = (button_handle_t)arg;
    esp_lv_adapter_button_ctx_t *ctx = (esp_lv_adapter_button_ctx_t *)arg2;
    if (!ctx) {
        return;
    }

    if (button == ctx->btn[ESP_LV_ADAPTER_BTN_PREV]) {
        ctx->btn_prev = true;
    }
    if (button == ctx->btn[ESP_LV_ADAPTER_BTN_NEXT]) {
        ctx->btn_next = true;
    }
    if (button == ctx->btn[ESP_LV_ADAPTER_BTN_ENTER]) {
        ctx->btn_enter = true;
    }
}

static void lvgl_button_up(void *arg, void *arg2)
{
    button_handle_t button = (button_handle_t)arg;
    esp_lv_adapter_button_ctx_t *ctx = (esp_lv_adapter_button_ctx_t *)arg2;
    if (!ctx) {
        return;
    }

    if (button == ctx->btn[ESP_LV_ADAPTER_BTN_PREV]) {
        ctx->btn_prev = false;
    }
    if (button == ctx->btn[ESP_LV_ADAPTER_BTN_NEXT]) {
        ctx->btn_next = false;
    }
    if (button == ctx->btn[ESP_LV_ADAPTER_BTN_ENTER]) {
        ctx->btn_enter = false;
    }
}

#endif /* LVGL_VERSION_MAJOR >= 9 */
