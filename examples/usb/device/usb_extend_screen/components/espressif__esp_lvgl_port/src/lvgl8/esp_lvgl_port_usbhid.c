/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include "esp_log.h"
#include "esp_err.h"
#include "esp_check.h"
#include "esp_lvgl_port.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

#include "usb/hid_host.h"
#include "usb/hid_usage_keyboard.h"
#include "usb/hid_usage_mouse.h"

/* LVGL image of cursor */
LV_IMG_DECLARE(img_cursor)

static const char *TAG = "LVGL";

/*******************************************************************************
* Types definitions
*******************************************************************************/

typedef struct {
    QueueHandle_t   queue;      /* USB HID queue */
    TaskHandle_t    task;       /* USB HID task */
    bool            running;    /* USB HID task running */
    struct {
        lv_indev_drv_t  drv;    /* LVGL mouse input device driver */
        uint8_t sensitivity;    /* Mouse sensitivity (cannot be zero) */
        int16_t x;              /* Mouse X coordinate */
        int16_t y;              /* Mouse Y coordinate */
        bool left_button;       /* Mouse left button state */
    } mouse;
    struct {
        lv_indev_drv_t  drv;    /* LVGL keyboard input device driver */
        uint32_t last_key;
        bool     pressed;
    } kb;
} lvgl_port_usb_hid_ctx_t;

typedef struct {
    hid_host_device_handle_t hid_device_handle;
    hid_host_driver_event_t event;
    void *arg;
} lvgl_port_usb_hid_event_t;

/*******************************************************************************
* Local variables
*******************************************************************************/

static lvgl_port_usb_hid_ctx_t lvgl_hid_ctx;

/*******************************************************************************
* Function definitions
*******************************************************************************/

static lvgl_port_usb_hid_ctx_t *lvgl_port_hid_init(void);
static void lvgl_port_usb_hid_task(void *arg);
static void lvgl_port_usb_hid_read_mouse(lv_indev_drv_t *indev_drv, lv_indev_data_t *data);
static void lvgl_port_usb_hid_read_kb(lv_indev_drv_t *indev_drv, lv_indev_data_t *data);
static void lvgl_port_usb_hid_callback(hid_host_device_handle_t hid_device_handle, const hid_host_driver_event_t event, void *arg);

/*******************************************************************************
* Public API functions
*******************************************************************************/

lv_indev_t *lvgl_port_add_usb_hid_mouse_input(const lvgl_port_hid_mouse_cfg_t *mouse_cfg)
{
    lv_indev_t *indev;
    assert(mouse_cfg);
    assert(mouse_cfg->disp);
    assert(mouse_cfg->disp->driver);

    /* Initialize USB HID */
    lvgl_port_usb_hid_ctx_t *hid_ctx = lvgl_port_hid_init();
    if (hid_ctx == NULL) {
        return NULL;
    }

    /* Mouse sensitivity cannot be zero */
    hid_ctx->mouse.sensitivity = (mouse_cfg->sensitivity == 0 ? 1 : mouse_cfg->sensitivity);
    /* Default coordinates to screen center */
    hid_ctx->mouse.x = (mouse_cfg->disp->driver->hor_res * hid_ctx->mouse.sensitivity) / 2;
    hid_ctx->mouse.y = (mouse_cfg->disp->driver->ver_res * hid_ctx->mouse.sensitivity) / 2;

    /* Register a mouse input device */
    lv_indev_drv_init(&hid_ctx->mouse.drv);
    hid_ctx->mouse.drv.type = LV_INDEV_TYPE_POINTER;
    hid_ctx->mouse.drv.disp = mouse_cfg->disp;
    hid_ctx->mouse.drv.read_cb = lvgl_port_usb_hid_read_mouse;
    hid_ctx->mouse.drv.user_data = hid_ctx;
    indev = lv_indev_drv_register(&hid_ctx->mouse.drv);

    /* Set image of cursor */
    lv_obj_t *cursor = mouse_cfg->cursor_img;
    if (cursor == NULL) {
        cursor = lv_img_create(lv_scr_act());
        lv_img_set_src(cursor, &img_cursor);
    }
    lv_indev_set_cursor(indev, cursor);

    return indev;
}

lv_indev_t *lvgl_port_add_usb_hid_keyboard_input(const lvgl_port_hid_keyboard_cfg_t *keyboard_cfg)
{
    lv_indev_t *indev;
    assert(keyboard_cfg);
    assert(keyboard_cfg->disp);

    /* Initialize USB HID */
    lvgl_port_usb_hid_ctx_t *hid_ctx = lvgl_port_hid_init();
    if (hid_ctx == NULL) {
        return NULL;
    }

    /* Register a keyboard input device */
    lv_indev_drv_init(&hid_ctx->kb.drv);
    hid_ctx->kb.drv.type = LV_INDEV_TYPE_KEYPAD;
    hid_ctx->kb.drv.disp = keyboard_cfg->disp;
    hid_ctx->kb.drv.read_cb = lvgl_port_usb_hid_read_kb;
    hid_ctx->kb.drv.user_data = hid_ctx;
    indev = lv_indev_drv_register(&hid_ctx->kb.drv);

    return indev;
}

esp_err_t lvgl_port_remove_usb_hid_input(lv_indev_t *hid)
{
    assert(hid);
    lv_indev_drv_t *indev_drv = hid->driver;
    assert(indev_drv);
    lvgl_port_usb_hid_ctx_t *hid_ctx = (lvgl_port_usb_hid_ctx_t *)indev_drv->user_data;

    /* Remove input device driver */
    lv_indev_delete(hid);

    /* If all hid input devices are removed, stop task and clean all */
    if (lvgl_hid_ctx.mouse.drv.disp == NULL && lvgl_hid_ctx.kb.drv.disp) {
        hid_ctx->running = false;
    }

    return ESP_OK;
}

/*******************************************************************************
* Private functions
*******************************************************************************/

static lvgl_port_usb_hid_ctx_t *lvgl_port_hid_init(void)
{
    esp_err_t ret;

    /* USB HID is already initialized */
    if (lvgl_hid_ctx.task) {
        return &lvgl_hid_ctx;
    }

    /* USB HID host driver config */
    const hid_host_driver_config_t hid_host_driver_config = {
        .create_background_task = true,
        .task_priority = 5,
        .stack_size = 4096,
        .core_id = 0,
        .callback = lvgl_port_usb_hid_callback,
        .callback_arg = &lvgl_hid_ctx,
    };

    ret = hid_host_install(&hid_host_driver_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "USB HID install failed!");
        return NULL;
    }

    lvgl_hid_ctx.queue = xQueueCreate(10, sizeof(lvgl_port_usb_hid_event_t));
    xTaskCreate(&lvgl_port_usb_hid_task, "hid_task", 4 * 1024, &lvgl_hid_ctx, 2, &lvgl_hid_ctx.task);

    return &lvgl_hid_ctx;
}

static char usb_hid_get_keyboard_char(uint8_t key, uint8_t shift)
{
    char ret_key = 0;

    const uint8_t keycode2ascii [57][2] = {
        {0, 0}, /* HID_KEY_NO_PRESS        */
        {0, 0}, /* HID_KEY_ROLLOVER        */
        {0, 0}, /* HID_KEY_POST_FAIL       */
        {0, 0}, /* HID_KEY_ERROR_UNDEFINED */
        {'a', 'A'}, /* HID_KEY_A               */
        {'b', 'B'}, /* HID_KEY_B               */
        {'c', 'C'}, /* HID_KEY_C               */
        {'d', 'D'}, /* HID_KEY_D               */
        {'e', 'E'}, /* HID_KEY_E               */
        {'f', 'F'}, /* HID_KEY_F               */
        {'g', 'G'}, /* HID_KEY_G               */
        {'h', 'H'}, /* HID_KEY_H               */
        {'i', 'I'}, /* HID_KEY_I               */
        {'j', 'J'}, /* HID_KEY_J               */
        {'k', 'K'}, /* HID_KEY_K               */
        {'l', 'L'}, /* HID_KEY_L               */
        {'m', 'M'}, /* HID_KEY_M               */
        {'n', 'N'}, /* HID_KEY_N               */
        {'o', 'O'}, /* HID_KEY_O               */
        {'p', 'P'}, /* HID_KEY_P               */
        {'q', 'Q'}, /* HID_KEY_Q               */
        {'r', 'R'}, /* HID_KEY_R               */
        {'s', 'S'}, /* HID_KEY_S               */
        {'t', 'T'}, /* HID_KEY_T               */
        {'u', 'U'}, /* HID_KEY_U               */
        {'v', 'V'}, /* HID_KEY_V               */
        {'w', 'W'}, /* HID_KEY_W               */
        {'x', 'X'}, /* HID_KEY_X               */
        {'y', 'Y'}, /* HID_KEY_Y               */
        {'z', 'Z'}, /* HID_KEY_Z               */
        {'1', '!'}, /* HID_KEY_1               */
        {'2', '@'}, /* HID_KEY_2               */
        {'3', '#'}, /* HID_KEY_3               */
        {'4', '$'}, /* HID_KEY_4               */
        {'5', '%'}, /* HID_KEY_5               */
        {'6', '^'}, /* HID_KEY_6               */
        {'7', '&'}, /* HID_KEY_7               */
        {'8', '*'}, /* HID_KEY_8               */
        {'9', '('}, /* HID_KEY_9               */
        {'0', ')'}, /* HID_KEY_0               */
        {'\r', '\r'}, /* HID_KEY_ENTER           */
        {0, 0}, /* HID_KEY_ESC             */
        {'\b', 0}, /* HID_KEY_DEL             */
        {0, 0}, /* HID_KEY_TAB             */
        {' ', ' '}, /* HID_KEY_SPACE           */
        {'-', '_'}, /* HID_KEY_MINUS           */
        {'=', '+'}, /* HID_KEY_EQUAL           */
        {'[', '{'}, /* HID_KEY_OPEN_BRACKET    */
        {']', '}'}, /* HID_KEY_CLOSE_BRACKET   */
        {'\\', '|'}, /* HID_KEY_BACK_SLASH      */
        {'\\', '|'}, /* HID_KEY_SHARP           */  // HOTFIX: for NonUS Keyboards repeat HID_KEY_BACK_SLASH
        {';', ':'}, /* HID_KEY_COLON           */
        {'\'', '"'}, /* HID_KEY_QUOTE           */
        {'`', '~'}, /* HID_KEY_TILDE           */
        {',', '<'}, /* HID_KEY_LESS            */
        {'.', '>'}, /* HID_KEY_GREATER         */
        {'/', '?'} /* HID_KEY_SLASH           */
    };

    if (shift > 1) {
        shift = 1;
    }

    if ((key >= HID_KEY_A) && (key <= HID_KEY_SLASH)) {
        ret_key = keycode2ascii[key][shift];
    }

    return ret_key;
}

static void lvgl_port_usb_hid_host_interface_callback(hid_host_device_handle_t hid_device_handle, const hid_host_interface_event_t event, void *arg)
{
    hid_host_dev_params_t dev;
    hid_host_device_get_params(hid_device_handle, &dev);
    lvgl_port_usb_hid_ctx_t *hid_ctx = (lvgl_port_usb_hid_ctx_t *)arg;
    uint8_t data[10];
    unsigned int data_length = 0;

    assert(hid_ctx != NULL);

    switch (event) {
    case HID_HOST_INTERFACE_EVENT_INPUT_REPORT:
        hid_host_device_get_raw_input_report_data(hid_device_handle, data, sizeof(data), &data_length);
        if (dev.proto == HID_PROTOCOL_KEYBOARD) {
            hid_keyboard_input_report_boot_t *keyboard = (hid_keyboard_input_report_boot_t *)data;
            if (data_length < sizeof(hid_keyboard_input_report_boot_t)) {
                return;
            }
            for (int i = 0; i < HID_KEYBOARD_KEY_MAX; i++) {
                if (keyboard->key[i] > HID_KEY_ERROR_UNDEFINED) {
                    char key = 0;

                    /* LVGL special keys */
                    if (keyboard->key[i] == HID_KEY_TAB) {
                        if ((keyboard->modifier.left_shift || keyboard->modifier.right_shift)) {
                            key = LV_KEY_PREV;
                        } else {
                            key = LV_KEY_NEXT;
                        }
                    } else if (keyboard->key[i] == HID_KEY_RIGHT) {
                        key = LV_KEY_RIGHT;
                    } else if (keyboard->key[i] == HID_KEY_LEFT) {
                        key = LV_KEY_LEFT;
                    } else if (keyboard->key[i] == HID_KEY_DOWN) {
                        key = LV_KEY_DOWN;
                    } else if (keyboard->key[i] == HID_KEY_UP) {
                        key = LV_KEY_UP;
                    } else if (keyboard->key[i] == HID_KEY_ENTER || keyboard->key[i] == HID_KEY_KEYPAD_ENTER) {
                        key = LV_KEY_ENTER;
                    } else if (keyboard->key[i] == HID_KEY_DELETE) {
                        key = LV_KEY_DEL;
                    } else if (keyboard->key[i] == HID_KEY_HOME) {
                        key = LV_KEY_HOME;
                    } else if (keyboard->key[i] == HID_KEY_END) {
                        key = LV_KEY_END;
                    } else {
                        /* Get ASCII char */
                        key = usb_hid_get_keyboard_char(keyboard->key[i], (keyboard->modifier.left_shift || keyboard->modifier.right_shift));
                    }

                    if (key == 0) {
                        ESP_LOGI(TAG, "Not recognized key: %c (%d)", keyboard->key[i], keyboard->key[i]);
                    }
                    hid_ctx->kb.last_key = key;
                    hid_ctx->kb.pressed = true;
                }
            }

        } else if (dev.proto == HID_PROTOCOL_MOUSE) {
            hid_mouse_input_report_boot_t *mouse = (hid_mouse_input_report_boot_t *)data;
            if (data_length < sizeof(hid_mouse_input_report_boot_t)) {
                break;
            }
            hid_ctx->mouse.left_button = mouse->buttons.button1;
            hid_ctx->mouse.x += mouse->x_displacement;
            hid_ctx->mouse.y += mouse->y_displacement;
        }
        break;
    case HID_HOST_INTERFACE_EVENT_TRANSFER_ERROR:
        break;
    case HID_HOST_INTERFACE_EVENT_DISCONNECTED:
        hid_host_device_close(hid_device_handle);
        break;
    default:
        break;
    }
}

static void lvgl_port_usb_hid_task(void *arg)
{
    hid_host_dev_params_t dev;
    lvgl_port_usb_hid_ctx_t *ctx = (lvgl_port_usb_hid_ctx_t *)arg;
    hid_host_device_handle_t hid_device_handle = NULL;
    lvgl_port_usb_hid_event_t msg;

    assert(ctx);

    ctx->running = true;

    while (ctx->running) {
        if (xQueueReceive(ctx->queue, &msg, pdMS_TO_TICKS(50))) {
            hid_device_handle = msg.hid_device_handle;
            hid_host_device_get_params(hid_device_handle, &dev);

            switch (msg.event) {
            case HID_HOST_DRIVER_EVENT_CONNECTED:
                /* Handle mouse or keyboard */
                if (dev.proto == HID_PROTOCOL_KEYBOARD || dev.proto == HID_PROTOCOL_MOUSE) {
                    const hid_host_device_config_t dev_config = {
                        .callback = lvgl_port_usb_hid_host_interface_callback,
                        .callback_arg = ctx
                    };

                    ESP_ERROR_CHECK(hid_host_device_open(hid_device_handle, &dev_config));
                    ESP_ERROR_CHECK(hid_class_request_set_idle(hid_device_handle, 0, 0));
                    ESP_ERROR_CHECK(hid_class_request_set_protocol(hid_device_handle, HID_REPORT_PROTOCOL_BOOT));
                    ESP_ERROR_CHECK(hid_host_device_start(hid_device_handle));
                }
                break;
            default:
                break;
            }
        }
    }

    xQueueReset(ctx->queue);
    vQueueDelete(ctx->queue);

    hid_host_uninstall();

    memset(&lvgl_hid_ctx, 0, sizeof(lvgl_port_usb_hid_ctx_t));

    vTaskDelete(NULL);
}

static void lvgl_port_usb_hid_read_mouse(lv_indev_drv_t *indev_drv, lv_indev_data_t *data)
{
    int16_t width = 0;
    int16_t height = 0;
    assert(indev_drv);
    lvgl_port_usb_hid_ctx_t *ctx = (lvgl_port_usb_hid_ctx_t *)indev_drv->user_data;
    assert(ctx);

    if (indev_drv->disp->driver->rotated == LV_DISP_ROT_NONE || indev_drv->disp->driver->rotated == LV_DISP_ROT_180) {
        width = indev_drv->disp->driver->hor_res;
        height = indev_drv->disp->driver->ver_res;
    } else {
        width = indev_drv->disp->driver->ver_res;
        height = indev_drv->disp->driver->hor_res;
    }

    /* Screen borders */
    if (ctx->mouse.x < 0) {
        ctx->mouse.x = 0;
    } else if (ctx->mouse.x > width * ctx->mouse.sensitivity) {
        ctx->mouse.x = width * ctx->mouse.sensitivity;
    }
    if (ctx->mouse.y < 0) {
        ctx->mouse.y = 0;
    } else if (ctx->mouse.y > height * ctx->mouse.sensitivity) {
        ctx->mouse.y = height * ctx->mouse.sensitivity;
    }

    /* Get coordinates by rotation with sensitivity */
    switch (indev_drv->disp->driver->rotated) {
    case LV_DISP_ROT_NONE:
        data->point.x = ctx->mouse.x / ctx->mouse.sensitivity;
        data->point.y = ctx->mouse.y / ctx->mouse.sensitivity;
        break;
    case LV_DISP_ROT_90:
        data->point.y = width - ctx->mouse.x / ctx->mouse.sensitivity;
        data->point.x = ctx->mouse.y / ctx->mouse.sensitivity;
        break;
    case LV_DISP_ROT_180:
        data->point.x = width - ctx->mouse.x / ctx->mouse.sensitivity;
        data->point.y = height - ctx->mouse.y / ctx->mouse.sensitivity;
        break;
    case LV_DISP_ROT_270:
        data->point.y = ctx->mouse.x / ctx->mouse.sensitivity;
        data->point.x = height - ctx->mouse.y / ctx->mouse.sensitivity;
        break;
    }

    if (ctx->mouse.left_button) {
        data->state = LV_INDEV_STATE_PRESSED;
    } else {
        data->state = LV_INDEV_STATE_RELEASED;
    }
}

static void lvgl_port_usb_hid_read_kb(lv_indev_drv_t *indev_drv, lv_indev_data_t *data)
{
    assert(indev_drv);
    lvgl_port_usb_hid_ctx_t *ctx = (lvgl_port_usb_hid_ctx_t *)indev_drv->user_data;
    assert(ctx);

    data->key = ctx->kb.last_key;
    if (ctx->kb.pressed) {
        data->state = LV_INDEV_STATE_PRESSED;
        ctx->kb.pressed = false;
    } else {
        data->state = LV_INDEV_STATE_RELEASED;
        ctx->kb.last_key = 0;
    }
}

static void lvgl_port_usb_hid_callback(hid_host_device_handle_t hid_device_handle, const hid_host_driver_event_t event, void *arg)
{
    lvgl_port_usb_hid_ctx_t *hid_ctx = (lvgl_port_usb_hid_ctx_t *)arg;

    const lvgl_port_usb_hid_event_t msg = {
        .hid_device_handle = hid_device_handle,
        .event = event,
        .arg = arg
    };

    xQueueSend(hid_ctx->queue, &msg, 0);
}
