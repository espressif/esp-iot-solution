/*
 * This file is subject to the terms of the GFX License. If a copy of
 * the license was not distributed with this file, you can obtain one at:
 *
 *              http://ugfx.org/license.html
 */

/* uGFX Config Include */
#include "sdkconfig.h"
#include "esp_log.h"

/* RTOS Includes */
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "freertos/semphr.h"

/* TouchPad Include */
#include "iot_touchpad.h"

#define TOUCHPAD_QUEUE_LENGTH   10
#define TOUCHPAD_WAITQUEUE_TIME 20 // millisecond

#define TOUCHPAD1 CONFIG_UGFX_TOGGLE_TOUCHPAD_NUM1
#define TOUCHPAD2 CONFIG_UGFX_TOGGLE_TOUCHPAD_NUM2

#define TOUCHPAD_THRES_PERCENT 0.0378

static tp_handle_t tp_dev0, tp_dev1;
static QueueHandle_t touch_queue = NULL;

static void touchpad_cb(void *arg)
{
    touch_pad_t tp_num = iot_tp_num_get((tp_handle_t)arg);
    xQueueSend(touch_queue, &tp_num, portMAX_DELAY);
}

static void touchpad_init()
{
    tp_dev0 = iot_tp_create(TOUCHPAD1, TOUCHPAD_THRES_PERCENT);
    tp_dev1 = iot_tp_create(TOUCHPAD2, TOUCHPAD_THRES_PERCENT);

    iot_tp_add_cb(tp_dev0, TOUCHPAD_CB_TAP, touchpad_cb, tp_dev0);
    iot_tp_add_cb(tp_dev1, TOUCHPAD_CB_TAP, touchpad_cb, tp_dev1);

    if (touch_queue == NULL) {
        touch_queue = xQueueCreate(TOUCHPAD_QUEUE_LENGTH, sizeof(touch_pad_t));
    }
}

static unsigned get_touchpad_num()
{
    touch_pad_t tp_num;
    if (xQueueReceive(touch_queue, &tp_num, TOUCHPAD_WAITQUEUE_TIME / portTICK_PERIOD_MS) == pdTRUE) {
        return tp_num;
    } else {
        return TOUCH_PAD_MAX;
    }
}

#ifdef CONFIG_UGFX_GUI_ENABLE

/* uGFX Includes */
#include "gos_freertos_priv.h"
#include "gfx.h"

#if (GFX_USE_GINPUT && GINPUT_NEED_TOGGLE) /*|| defined(__DOXYGEN__)*/

#include "src/ginput/ginput_driver_toggle.h"

GINPUT_TOGGLE_DECLARE_STRUCTURE();

void ginput_lld_toggle_init(const GToggleConfig *ptc)
{
    touchpad_init();
}

unsigned ginput_lld_toggle_getbits(const GToggleConfig *ptc)
{
    static unsigned bit = 0;
    switch (get_touchpad_num()) {
    case TOUCHPAD1:
        bit = 1; // number 1 toggle device touch
        break;

    case TOUCHPAD2:
        bit = 2; // number 2 toggle device touch
        break;

    case TOUCH_PAD_MAX:
        bit = 0; // no toggle device touch
        break;

    default:
        bit = 0; // no toggle device touch
        break;
    }
    return bit;
}

#endif /* GFX_USE_GINPUT && GINPUT_NEED_TOGGLE */

#endif /* CONFIG_UGFX_GUI_ENABLE */

#ifdef CONFIG_LVGL_GUI_ENABLE

/* lvgl include */
#include "lvgl_indev_config.h"

/*Function pointer to read data. Return 'true' if there is still data to be read (buffered)*/
static bool ex_tp_read(lv_indev_data_t *data)
{
    static uint32_t key = 0;
    data->state = LV_INDEV_STATE_REL;

    switch (get_touchpad_num()) {
    case TOUCHPAD1:
        data->state = LV_INDEV_STATE_PR;
        key = LV_GROUP_KEY_ENTER;
        break;

    case TOUCHPAD2:
        data->state = LV_INDEV_STATE_PR;
        key = LV_GROUP_KEY_NEXT;
        break;

    case TOUCH_PAD_MAX:
        data->state = LV_INDEV_STATE_REL;
        break;

    default:
        data->state = LV_INDEV_STATE_REL;
        break;
    }
    // please be sure that your touch driver every time return old (last clcked) value.
    data->key = key;

    // Return 'true' if there is still data to be read (buffered)*/
    return false;
}

/* Input device interface，Initialize your device */
lv_indev_drv_t lvgl_indev_init()
{
    touchpad_init();

    lv_indev_drv_t indev_drv;      /*Descriptor of an input device driver*/
    lv_indev_drv_init(&indev_drv); /*Basic initialization*/

    indev_drv.type = LV_INDEV_TYPE_KEYPAD; /*The touchpad is keypad type device*/
    indev_drv.read = ex_tp_read;           /*Library ready your keypad via this function*/

    lv_indev_drv_register(&indev_drv); /*Finally register the driver*/

    return indev_drv;
}

#endif /* CONFIG_LVGL_GUI_ENABLE */