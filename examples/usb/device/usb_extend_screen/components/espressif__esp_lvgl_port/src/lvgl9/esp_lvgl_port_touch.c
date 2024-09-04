/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "esp_log.h"
#include "esp_err.h"
#include "esp_check.h"
#include "esp_lcd_touch.h"
#include "esp_lvgl_port.h"

static const char *TAG = "LVGL";

/*******************************************************************************
* Types definitions
*******************************************************************************/

typedef struct {
    esp_lcd_touch_handle_t  handle;     /* LCD touch IO handle */
    lv_indev_t              *indev;     /* LVGL input device driver */
} lvgl_port_touch_ctx_t;

/*******************************************************************************
* Function definitions
*******************************************************************************/

static void lvgl_port_touchpad_read(lv_indev_t *indev_drv, lv_indev_data_t *data);
static void lvgl_port_touch_interrupt_callback(esp_lcd_touch_handle_t tp);

/*******************************************************************************
* Public API functions
*******************************************************************************/

lv_indev_t *lvgl_port_add_touch(const lvgl_port_touch_cfg_t *touch_cfg)
{
    esp_err_t ret = ESP_OK;
    lv_indev_t *indev = NULL;
    assert(touch_cfg != NULL);
    assert(touch_cfg->disp != NULL);
    assert(touch_cfg->handle != NULL);

    /* Touch context */
    lvgl_port_touch_ctx_t *touch_ctx = malloc(sizeof(lvgl_port_touch_ctx_t));
    if (touch_ctx == NULL) {
        ESP_LOGE(TAG, "Not enough memory for touch context allocation!");
        return NULL;
    }
    touch_ctx->handle = touch_cfg->handle;

    if (touch_ctx->handle->config.int_gpio_num != GPIO_NUM_NC) {
        /* Register touch interrupt callback */
        ret = esp_lcd_touch_register_interrupt_callback_with_data(touch_ctx->handle, lvgl_port_touch_interrupt_callback, touch_ctx);
        ESP_GOTO_ON_ERROR(ret, err, TAG, "Error in register touch interrupt.");
    }

    lvgl_port_lock(0);
    /* Register a touchpad input device */
    indev = lv_indev_create();
    lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
    /* Event mode can be set only, when touch interrupt enabled */
    if (touch_ctx->handle->config.int_gpio_num != GPIO_NUM_NC) {
        lv_indev_set_mode(indev, LV_INDEV_MODE_EVENT);
    }
    lv_indev_set_read_cb(indev, lvgl_port_touchpad_read);
    lv_indev_set_disp(indev, touch_cfg->disp);
    lv_indev_set_user_data(indev, touch_ctx);
    touch_ctx->indev = indev;
    lvgl_port_unlock();

err:
    if (ret != ESP_OK) {
        if (touch_ctx) {
            free(touch_ctx);
        }
    }

    return indev;
}

esp_err_t lvgl_port_remove_touch(lv_indev_t *touch)
{
    assert(touch);
    lvgl_port_touch_ctx_t *touch_ctx = (lvgl_port_touch_ctx_t *)lv_indev_get_user_data(touch);

    lvgl_port_lock(0);
    /* Remove input device driver */
    lv_indev_delete(touch);
    lvgl_port_unlock();

    if (touch_ctx->handle->config.int_gpio_num != GPIO_NUM_NC) {
        /* Unregister touch interrupt callback */
        esp_lcd_touch_register_interrupt_callback(touch_ctx->handle, NULL);
    }

    if (touch_ctx) {
        free(touch_ctx);
    }

    return ESP_OK;
}

/*******************************************************************************
* Private functions
*******************************************************************************/

static void lvgl_port_touchpad_read(lv_indev_t *indev_drv, lv_indev_data_t *data)
{
    assert(indev_drv);
    lvgl_port_touch_ctx_t *touch_ctx = (lvgl_port_touch_ctx_t *)lv_indev_get_user_data(indev_drv);
    assert(touch_ctx);
    assert(touch_ctx->handle);

    uint16_t touchpad_x[1] = {0};
    uint16_t touchpad_y[1] = {0};
    uint8_t touchpad_cnt = 0;

    /* Read data from touch controller into memory */
    esp_lcd_touch_read_data(touch_ctx->handle);

    /* Read data from touch controller */
    bool touchpad_pressed = esp_lcd_touch_get_coordinates(touch_ctx->handle, touchpad_x, touchpad_y, NULL, NULL, &touchpad_cnt, 1);

    if (touchpad_pressed && touchpad_cnt > 0) {
        data->point.x = touchpad_x[0];
        data->point.y = touchpad_y[0];
        data->state = LV_INDEV_STATE_PRESSED;
    } else {
        data->state = LV_INDEV_STATE_RELEASED;
    }
}

static void IRAM_ATTR lvgl_port_touch_interrupt_callback(esp_lcd_touch_handle_t tp)
{
    lvgl_port_touch_ctx_t *touch_ctx = (lvgl_port_touch_ctx_t *) tp->config.user_data;

    /* Wake LVGL task, if needed */
    lvgl_port_task_wake(LVGL_PORT_EVENT_TOUCH, touch_ctx->indev);
}
