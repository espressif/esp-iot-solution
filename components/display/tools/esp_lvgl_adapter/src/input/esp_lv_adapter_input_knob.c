/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file esp_lv_adapter_input_knob.c
 * @brief Rotary encoder (knob) input device implementation for LVGL adapter
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
static const char *TAG = "esp_lvgl:knob";

/**
 * @brief Encoder (knob) context structure
 */
typedef struct {
    knob_handle_t knob_handle;      /*!< Knob/encoder handle */
    button_handle_t btn_handle;     /*!< Button handle (optional) */
#if LVGL_VERSION_MAJOR >= 9
    lv_indev_t *indev;              /*!< LVGL v9 input device */
#else
    lv_indev_drv_t indev_drv;       /*!< LVGL v8 input device driver */
#endif
    bool btn_enter;                 /*!< Button press state */
    int32_t diff;                   /*!< Encoder position difference */
} esp_lv_adapter_encoder_ctx_t;

/*****************************************************************************
 *                         LVGL v9 Implementation                            *
 *****************************************************************************/

#if LVGL_VERSION_MAJOR >= 9

static void lvgl_encoder_read(lv_indev_t *indev_drv, lv_indev_data_t *data);
static void lvgl_encoder_btn_down(void *button_handle, void *user_data);
static void lvgl_encoder_btn_up(void *button_handle, void *user_data);
static void lvgl_encoder_left(void *arg, void *arg2);
static void lvgl_encoder_right(void *arg, void *arg2);
static int32_t lvgl_encoder_diff(knob_handle_t knob, knob_event_t event);

lv_indev_t *esp_lv_adapter_register_encoder(const esp_lv_adapter_encoder_config_t *config)
{
    if (!config || !config->disp || !config->encoder_a_b) {
        return NULL;
    }

    esp_lv_adapter_encoder_ctx_t *encoder_ctx = calloc(1, sizeof(esp_lv_adapter_encoder_ctx_t));
    if (!encoder_ctx) {
        ESP_LOGE(TAG, "alloc ctx");
        return NULL;
    }

    encoder_ctx->knob_handle = iot_knob_create(config->encoder_a_b);
    if (!encoder_ctx->knob_handle) {
        free(encoder_ctx);
        return NULL;
    }

    if (iot_knob_register_cb(encoder_ctx->knob_handle, KNOB_LEFT, lvgl_encoder_left, encoder_ctx) != ESP_OK) {
        iot_knob_delete(encoder_ctx->knob_handle);
        free(encoder_ctx);
        return NULL;
    }
    if (iot_knob_register_cb(encoder_ctx->knob_handle, KNOB_RIGHT, lvgl_encoder_right, encoder_ctx) != ESP_OK) {
        iot_knob_delete(encoder_ctx->knob_handle);
        free(encoder_ctx);
        return NULL;
    }

    if (button_compat_is_valid(config->encoder_enter)) {
        if (button_compat_create(config->encoder_enter, &encoder_ctx->btn_handle) != ESP_OK) {
            iot_knob_delete(encoder_ctx->knob_handle);
            free(encoder_ctx);
            return NULL;
        }

        if (button_compat_register_cb(encoder_ctx->btn_handle, BUTTON_PRESS_DOWN, lvgl_encoder_btn_down, encoder_ctx) != ESP_OK) {
            button_compat_delete(encoder_ctx->btn_handle);
            iot_knob_delete(encoder_ctx->knob_handle);
            free(encoder_ctx);
            return NULL;
        }
        if (button_compat_register_cb(encoder_ctx->btn_handle, BUTTON_PRESS_UP, lvgl_encoder_btn_up, encoder_ctx) != ESP_OK) {
            button_compat_delete(encoder_ctx->btn_handle);
            iot_knob_delete(encoder_ctx->knob_handle);
            free(encoder_ctx);
            return NULL;
        }
    }

    if (esp_lv_adapter_lock((uint32_t) -1) != ESP_OK) {
        button_compat_delete(encoder_ctx->btn_handle);
        iot_knob_delete(encoder_ctx->knob_handle);
        free(encoder_ctx);
        return NULL;
    }

    lv_indev_t *indev = lv_indev_create();
    if (!indev) {
        esp_lv_adapter_unlock();
        button_compat_delete(encoder_ctx->btn_handle);
        iot_knob_delete(encoder_ctx->knob_handle);
        free(encoder_ctx);
        return NULL;
    }

    lv_indev_set_type(indev, LV_INDEV_TYPE_ENCODER);
#ifdef LV_INDEV_MODE_STREAM
    lv_indev_set_mode(indev, LV_INDEV_MODE_STREAM);
#else
    lv_indev_set_mode(indev, LV_INDEV_MODE_TIMER);
#endif
    lv_indev_set_read_cb(indev, lvgl_encoder_read);
    lv_indev_set_disp(indev, config->disp);
    lv_indev_set_driver_data(indev, encoder_ctx);
    encoder_ctx->indev = indev;

    esp_lv_adapter_unlock();

    esp_err_t ret = esp_lv_adapter_register_input_device(indev, ESP_LV_ADAPTER_INPUT_TYPE_ENCODER, encoder_ctx);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to register input device for management: %d", ret);
    }

    return indev;
}

esp_err_t esp_lv_adapter_unregister_encoder(lv_indev_t *encoder)
{
    ESP_RETURN_ON_FALSE(encoder, ESP_ERR_INVALID_ARG, TAG, "Encoder input handle cannot be NULL");

    esp_lv_adapter_unregister_input_device(encoder);

    esp_lv_adapter_encoder_ctx_t *encoder_ctx = (esp_lv_adapter_encoder_ctx_t *)lv_indev_get_driver_data(encoder);

    esp_err_t ret = esp_lv_adapter_lock((uint32_t) -1);
    ESP_RETURN_ON_ERROR(ret, TAG, "Failed to acquire LVGL lock (ret=%d)", ret);

    lv_indev_delete(encoder);
    esp_lv_adapter_unlock();

    if (encoder_ctx) {
        if (encoder_ctx->knob_handle) {
            iot_knob_delete(encoder_ctx->knob_handle);
        }
        button_compat_delete(encoder_ctx->btn_handle);
        free(encoder_ctx);
    }

    return ESP_OK;
}

static void lvgl_encoder_read(lv_indev_t *indev_drv, lv_indev_data_t *data)
{
    esp_lv_adapter_encoder_ctx_t *ctx = (esp_lv_adapter_encoder_ctx_t *)lv_indev_get_driver_data(indev_drv);
    if (!ctx) {
        data->enc_diff = 0;
        data->state = LV_INDEV_STATE_RELEASED;
        return;
    }

    data->enc_diff = ctx->diff;
    data->state = ctx->btn_enter ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
    ctx->diff = 0;
}

static void lvgl_encoder_btn_down(void *button_handle, void *user_data)
{
    esp_lv_adapter_encoder_ctx_t *ctx = (esp_lv_adapter_encoder_ctx_t *)user_data;
    button_handle_t button = (button_handle_t)button_handle;
    if (!ctx) {
        return;
    }

    if (button == ctx->btn_handle) {
        ctx->btn_enter = true;
    }
}

static void lvgl_encoder_btn_up(void *button_handle, void *user_data)
{
    esp_lv_adapter_encoder_ctx_t *ctx = (esp_lv_adapter_encoder_ctx_t *)user_data;
    button_handle_t button = (button_handle_t)button_handle;
    if (!ctx) {
        return;
    }

    if (button == ctx->btn_handle) {
        ctx->btn_enter = false;
    }
}

static void lvgl_encoder_left(void *arg, void *arg2)
{
    esp_lv_adapter_encoder_ctx_t *ctx = (esp_lv_adapter_encoder_ctx_t *)arg2;
    knob_handle_t knob = (knob_handle_t)arg;
    if (!ctx || !knob) {
        return;
    }

    if (knob == ctx->knob_handle) {
        int32_t diff = lvgl_encoder_diff(knob, KNOB_LEFT);
        ctx->diff = (ctx->diff > 0) ? diff : ctx->diff + diff;
    }
}

static void lvgl_encoder_right(void *arg, void *arg2)
{
    esp_lv_adapter_encoder_ctx_t *ctx = (esp_lv_adapter_encoder_ctx_t *)arg2;
    knob_handle_t knob = (knob_handle_t)arg;
    if (!ctx || !knob) {
        return;
    }

    if (knob == ctx->knob_handle) {
        int32_t diff = lvgl_encoder_diff(knob, KNOB_RIGHT);
        ctx->diff = (ctx->diff < 0) ? diff : ctx->diff + diff;
    }
}

static int32_t lvgl_encoder_diff(knob_handle_t knob, knob_event_t event)
{
    static int32_t last_v;
    int32_t diff = 0;
    int32_t invd = iot_knob_get_count_value(knob);

    if (last_v ^ invd) {
        diff = (int32_t)((uint32_t)invd - (uint32_t)last_v);
        if (event == KNOB_RIGHT && invd < last_v) {
            diff += CONFIG_KNOB_HIGH_LIMIT;
        } else if (event == KNOB_LEFT && invd > last_v) {
            diff += CONFIG_KNOB_LOW_LIMIT;
        }
        last_v = invd;
    }

    return diff;
}

#else

static void lvgl_encoder_read(lv_indev_drv_t *indev_drv, lv_indev_data_t *data);
static void lvgl_encoder_btn_down(void *button_handle, void *user_data);
static void lvgl_encoder_btn_up(void *button_handle, void *user_data);
static void lvgl_encoder_left(void *arg, void *arg2);
static void lvgl_encoder_right(void *arg, void *arg2);
static int32_t lvgl_encoder_diff(knob_handle_t knob, knob_event_t event);

lv_indev_t *esp_lv_adapter_register_encoder(const esp_lv_adapter_encoder_config_t *config)
{
    if (!config || !config->disp || !config->encoder_a_b) {
        return NULL;
    }

    esp_lv_adapter_encoder_ctx_t *encoder_ctx = calloc(1, sizeof(esp_lv_adapter_encoder_ctx_t));
    if (!encoder_ctx) {
        ESP_LOGE(TAG, "alloc ctx");
        return NULL;
    }

    encoder_ctx->knob_handle = iot_knob_create(config->encoder_a_b);
    if (!encoder_ctx->knob_handle) {
        free(encoder_ctx);
        return NULL;
    }

    if (iot_knob_register_cb(encoder_ctx->knob_handle, KNOB_LEFT, lvgl_encoder_left, encoder_ctx) != ESP_OK) {
        iot_knob_delete(encoder_ctx->knob_handle);
        free(encoder_ctx);
        return NULL;
    }
    if (iot_knob_register_cb(encoder_ctx->knob_handle, KNOB_RIGHT, lvgl_encoder_right, encoder_ctx) != ESP_OK) {
        iot_knob_delete(encoder_ctx->knob_handle);
        free(encoder_ctx);
        return NULL;
    }

    if (button_compat_is_valid(config->encoder_enter)) {
        if (button_compat_create(config->encoder_enter, &encoder_ctx->btn_handle) != ESP_OK) {
            iot_knob_delete(encoder_ctx->knob_handle);
            free(encoder_ctx);
            return NULL;
        }

        if (button_compat_register_cb(encoder_ctx->btn_handle, BUTTON_PRESS_DOWN, lvgl_encoder_btn_down, encoder_ctx) != ESP_OK) {
            button_compat_delete(encoder_ctx->btn_handle);
            iot_knob_delete(encoder_ctx->knob_handle);
            free(encoder_ctx);
            return NULL;
        }
        if (button_compat_register_cb(encoder_ctx->btn_handle, BUTTON_PRESS_UP, lvgl_encoder_btn_up, encoder_ctx) != ESP_OK) {
            button_compat_delete(encoder_ctx->btn_handle);
            iot_knob_delete(encoder_ctx->knob_handle);
            free(encoder_ctx);
            return NULL;
        }
    }

    if (esp_lv_adapter_lock((uint32_t) -1) != ESP_OK) {
        button_compat_delete(encoder_ctx->btn_handle);
        iot_knob_delete(encoder_ctx->knob_handle);
        free(encoder_ctx);
        return NULL;
    }

    lv_indev_drv_init(&encoder_ctx->indev_drv);
    encoder_ctx->indev_drv.type = LV_INDEV_TYPE_ENCODER;
    encoder_ctx->indev_drv.disp = config->disp;
    encoder_ctx->indev_drv.read_cb = lvgl_encoder_read;
    encoder_ctx->indev_drv.user_data = encoder_ctx;

    lv_indev_t *indev = lv_indev_drv_register(&encoder_ctx->indev_drv);
    if (!indev) {
        esp_lv_adapter_unlock();
        button_compat_delete(encoder_ctx->btn_handle);
        iot_knob_delete(encoder_ctx->knob_handle);
        free(encoder_ctx);
        return NULL;
    }

    esp_lv_adapter_unlock();

    esp_err_t ret = esp_lv_adapter_register_input_device(indev, ESP_LV_ADAPTER_INPUT_TYPE_ENCODER, encoder_ctx);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to register input device for management: %d", ret);
    }

    return indev;
}

esp_err_t esp_lv_adapter_unregister_encoder(lv_indev_t *encoder)
{
    ESP_RETURN_ON_FALSE(encoder, ESP_ERR_INVALID_ARG, TAG, "Encoder input handle cannot be NULL");

    lv_indev_drv_t *indev_drv = encoder->driver;
    ESP_RETURN_ON_FALSE(indev_drv, ESP_ERR_INVALID_STATE, TAG, "Encoder driver not initialized");

    esp_lv_adapter_encoder_ctx_t *encoder_ctx = (esp_lv_adapter_encoder_ctx_t *)indev_drv->user_data;

    esp_lv_adapter_unregister_input_device(encoder);

    esp_err_t ret = esp_lv_adapter_lock((uint32_t) -1);
    ESP_RETURN_ON_ERROR(ret, TAG, "Failed to acquire LVGL lock (ret=%d)", ret);

    lv_indev_delete(encoder);
    esp_lv_adapter_unlock();

    if (encoder_ctx) {
        if (encoder_ctx->knob_handle) {
            iot_knob_delete(encoder_ctx->knob_handle);
        }
        button_compat_delete(encoder_ctx->btn_handle);
        free(encoder_ctx);
    }

    return ESP_OK;
}

static void lvgl_encoder_read(lv_indev_drv_t *indev_drv, lv_indev_data_t *data)
{
    esp_lv_adapter_encoder_ctx_t *ctx = (esp_lv_adapter_encoder_ctx_t *)indev_drv->user_data;
    if (!ctx) {
        data->enc_diff = 0;
        data->state = LV_INDEV_STATE_RELEASED;
        return;
    }

    data->enc_diff = ctx->diff;
    data->state = ctx->btn_enter ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
    ctx->diff = 0;
}

static void lvgl_encoder_btn_down(void *button_handle, void *user_data)
{
    esp_lv_adapter_encoder_ctx_t *ctx = (esp_lv_adapter_encoder_ctx_t *)user_data;
    button_handle_t button = (button_handle_t)button_handle;
    if (!ctx) {
        return;
    }

    if (button == ctx->btn_handle) {
        ctx->btn_enter = true;
    }
}

static void lvgl_encoder_btn_up(void *button_handle, void *user_data)
{
    esp_lv_adapter_encoder_ctx_t *ctx = (esp_lv_adapter_encoder_ctx_t *)user_data;
    button_handle_t button = (button_handle_t)button_handle;
    if (!ctx) {
        return;
    }

    if (button == ctx->btn_handle) {
        ctx->btn_enter = false;
    }
}

static void lvgl_encoder_left(void *arg, void *arg2)
{
    esp_lv_adapter_encoder_ctx_t *ctx = (esp_lv_adapter_encoder_ctx_t *)arg2;
    knob_handle_t knob = (knob_handle_t)arg;
    if (!ctx || !knob) {
        return;
    }

    if (knob == ctx->knob_handle) {
        int32_t diff = lvgl_encoder_diff(knob, KNOB_LEFT);
        ctx->diff = (ctx->diff > 0) ? diff : ctx->diff + diff;
    }
}

static void lvgl_encoder_right(void *arg, void *arg2)
{
    esp_lv_adapter_encoder_ctx_t *ctx = (esp_lv_adapter_encoder_ctx_t *)arg2;
    knob_handle_t knob = (knob_handle_t)arg;
    if (!ctx || !knob) {
        return;
    }

    if (knob == ctx->knob_handle) {
        int32_t diff = lvgl_encoder_diff(knob, KNOB_RIGHT);
        ctx->diff = (ctx->diff < 0) ? diff : ctx->diff + diff;
    }
}

static int32_t lvgl_encoder_diff(knob_handle_t knob, knob_event_t event)
{
    static int32_t last_v;
    int32_t diff = 0;
    int32_t invd = iot_knob_get_count_value(knob);

    if (last_v ^ invd) {
        diff = (int32_t)((uint32_t)invd - (uint32_t)last_v);
        if (event == KNOB_RIGHT && invd < last_v) {
            diff += CONFIG_KNOB_HIGH_LIMIT;
        } else if (event == KNOB_LEFT && invd > last_v) {
            diff += CONFIG_KNOB_LOW_LIMIT;
        }
        last_v = invd;
    }

    return diff;
}

#endif /* LVGL_VERSION_MAJOR >= 9 */
