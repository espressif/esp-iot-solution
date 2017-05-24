/*
  * ESPRESSIF MIT License
  *
  * Copyright (c) 2017 <ESPRESSIF SYSTEMS (SHANGHAI) PTE LTD>
  *
  * Permission is hereby granted for use on ESPRESSIF SYSTEMS products only, in which case,
  * it is free of charge, to any person obtaining a copy of this software and associated
  * documentation files (the "Software"), to deal in the Software without restriction, including
  * without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense,
  * and/or sell copies of the Software, and to permit persons to whom the Software is furnished
  * to do so, subject to the following conditions:
  *
  * The above copyright notice and this permission notice shall be included in all copies or
  * substantial portions of the Software.
  *
  * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
  * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
  * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
  * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
  * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
  *
  */

#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include <string.h>
#include "esp_log.h"
#include "power_meter.h"
#include "driver/pcnt.h"
#include "button.h"
#include "relay.h"
#include "touchpad.h"
#include "led.h"
#include "socket_device.h"
#include "param.h"
#include "socket_config.h"

#define TAG "socket"
#define IOT_CHECK(tag, a, ret)  if(!(a)) {                                             \
        ESP_LOGE(tag,"%s:%d (%s)", __FILE__, __LINE__, __FUNCTION__);      \
        return (ret);                                                                   \
        }

typedef struct {
    socket_status_t unit_sta[SOCKET_UNIT_NUM];
} save_param_t;

typedef struct {
    socket_status_t* state_ptr;
    button_handle_t btn_handle;
    relay_handle_t relay_handle;
    led_handle_t led_handle;
    touchpad_handle_t tp_handle;
} socket_unit_t;

typedef struct {
    socket_unit_t* units[SOCKET_UNIT_NUM];
    pm_handle_t pm_handle;
    led_handle_t net_led;
    button_handle_t main_btn;
    save_param_t save_param;
    SemaphoreHandle_t xSemWriteInfo;
} socket_dev_t;


static const socket_uint_def_t DEF_SOCKET_UNIT[SOCKET_UNIT_NUM] = {
        {
        .button_io           = SMART_SOCKET_UNIT0_BUTTON_IO,
        .button_active_level = SMART_SOCKET_UNIT0_BUTTON_ACTIVE_LEVEL,
        .led_io              = SMART_SOCKET_UNIT0_LED_IO,
        .led_off_level       = SMART_SOCKET_UNIT0_LED_OFF_LEVEL,
        .relay_off_level     = SMART_SOCKET_UNIT0_RELAY_OFF_LEVEL,
        .relay_ctrl_mode     = SMART_SOCKET_UNIT0_RELAY_CTRL_MODE,
        .relay_io_mode       = SMART_SOCKET_UNIT0_RELAY_IO_MODE,
        .relay_ctl_io        = SMART_SOCKET_UNIT0_RELAY_CTL_IO,
        .relay_clk_io        = SMART_SOCKET_UNIT0_RELAY_CLK_IO,
        },
#if SOCKET_UNIT_NUM > 1
        {
        .button_io           = SMART_SOCKET_UNIT1_BUTTON_IO,
        .button_active_level = SMART_SOCKET_UNIT1_BUTTON_ACTIVE_LEVEL,
        .led_io              = SMART_SOCKET_UNIT1_LED_IO,
        .led_off_level       = SMART_SOCKET_UNIT1_LED_OFF_LEVEL,
        .relay_off_level     = SMART_SOCKET_UNIT1_RELAY_OFF_LEVEL,
        .relay_ctrl_mode     = SMART_SOCKET_UNIT1_RELAY_CTRL_MODE,
        .relay_io_mode       = SMART_SOCKET_UNIT1_RELAY_IO_MODE,
        .relay_ctl_io        = SMART_SOCKET_UNIT1_RELAY_CTL_IO,
        .relay_clk_io        = SMART_SOCKET_UNIT1_RELAY_CLK_IO,
        },
#endif
#if SOCKET_UNIT_NUM > 2
        {
        .button_io           = SMART_SOCKET_UNIT2_BUTTON_IO,
        .button_active_level = SMART_SOCKET_UNIT2_BUTTON_ACTIVE_LEVEL,
        .led_io              = SMART_SOCKET_UNIT2_LED_IO,
        .led_off_level       = SMART_SOCKET_UNIT2_LED_OFF_LEVEL,
        .relay_off_level     = SMART_SOCKET_UNIT2_RELAY_OFF_LEVEL,
        .relay_ctrl_mode     = SMART_SOCKET_UNIT2_RELAY_CTRL_MODE,
        .relay_io_mode       = SMART_SOCKET_UNIT2_RELAY_IO_MODE,
        .relay_ctl_io        = SMART_SOCKET_UNIT2_RELAY_CTL_IO,
        .relay_clk_io        = SMART_SOCKET_UNIT2_RELAY_CLK_IO,
        },
#endif
#if SOCKET_UNIT_NUM > 3
        {
        .button_io           = SMART_SOCKET_UNIT3_BUTTON_IO,
        .button_active_level = SMART_SOCKET_UNIT3_BUTTON_ACTIVE_LEVEL,
        .led_io              = SMART_SOCKET_UNIT3_LED_IO,
        .led_off_level       = SMART_SOCKET_UNIT3_LED_OFF_LEVEL,
        .relay_off_level     = SMART_SOCKET_UNIT3_RELAY_OFF_LEVEL,
        .relay_ctrl_mode     = SMART_SOCKET_UNIT3_RELAY_CTRL_MODE,
        .relay_io_mode       = SMART_SOCKET_UNIT3_RELAY_IO_MODE,
        .relay_ctl_io        = SMART_SOCKET_UNIT3_RELAY_CTL_IO,
        .relay_clk_io        = SMART_SOCKET_UNIT3_RELAY_CLK_IO,
        },
#endif
};

static socket_dev_t* g_socket;
static socket_unit_t* socket_unit_create(socket_status_t* state_ptr, button_handle_t btn, relay_handle_t relay,
        led_handle_t led, touchpad_handle_t touchpad)
{
    socket_unit_t* socket_unit = (socket_unit_t*) calloc(1, sizeof(socket_unit_t));
    socket_unit->state_ptr     = state_ptr;
    socket_unit->btn_handle    = btn;
    socket_unit->relay_handle  = relay;
    socket_unit->led_handle    = led;
    socket_unit->tp_handle     = touchpad;
    return socket_unit;
}

static void socket_unit_set(socket_unit_t* socket_unit, socket_status_t state)
{
    if (state == SOCKET_OFF) {
        *(socket_unit->state_ptr) = SOCKET_OFF;
        if (socket_unit->relay_handle != NULL) {
            relay_state_write(socket_unit->relay_handle, RELAY_STATUS_OPEN);
        }
        if (socket_unit->led_handle != NULL) {
            led_state_write(socket_unit->led_handle, LED_NORMAL_OFF);
        }
    } else {
        *(socket_unit->state_ptr) = SOCKET_ON;
        if (socket_unit->relay_handle != NULL) {
            relay_state_write(socket_unit->relay_handle, RELAY_STATUS_CLOSE);
        }
        if (socket_unit->led_handle != NULL) {
            led_state_write(socket_unit->led_handle, LED_NORMAL_ON);
        }
    }
    if (g_socket != NULL) {
        param_save(SOCKET_NAME_SPACE, SOCKET_PARAM_KEY, &(g_socket->save_param), sizeof(g_socket->save_param));
    }
}

static void button_tap_cb(void* arg)
{
    socket_unit_t* unit_ptr = (socket_unit_t*) arg;
    ESP_EARLY_LOGI(TAG, "a button pressed");
    socket_status_t state = *(unit_ptr->state_ptr);
    if (state == SOCKET_OFF) {
        socket_unit_set(unit_ptr, SOCKET_ON);
    }
    else {
        socket_unit_set(unit_ptr, SOCKET_OFF);
    }
    if (g_socket->xSemWriteInfo != NULL) {
        xSemaphoreGive(g_socket->xSemWriteInfo);
    }
}

static void main_button_cb(void* arg)
{
    socket_dev_t* socket_dev = (socket_dev_t*) arg;
    for (int i = 0; i < SOCKET_UNIT_NUM; i++) {
        if (socket_dev->units[i] != NULL) {
            socket_unit_set(socket_dev->units[i], SOCKET_OFF);
        }
    }
    if (g_socket->xSemWriteInfo != NULL) {
        xSemaphoreGive(g_socket->xSemWriteInfo);
    }
}

void powermeter_task(void* arg)
{
    socket_dev_t* socket_dev = (socket_dev_t*) arg;
    while(1) {
        vTaskDelay(5000 / portTICK_RATE_MS);
        ESP_LOGI(TAG, "value of power:%d", powermeter_read(socket_dev->pm_handle, PM_POWER));
        ESP_LOGI(TAG, "value of voltage:%d", powermeter_read(socket_dev->pm_handle, PM_VOLTAGE));
        ESP_LOGI(TAG, "value of current:%d", powermeter_read(socket_dev->pm_handle, PM_CURRENT));
    }
    vTaskDelete(NULL);
}

esp_err_t socket_state_write(socket_handle_t socket_handle, socket_status_t state, uint8_t unit_id)
{
    IOT_CHECK(TAG, socket_handle != NULL, ESP_FAIL);
    socket_dev_t* socket_dev = (socket_dev_t*) socket_handle;
    IOT_CHECK(TAG, unit_id < SOCKET_UNIT_NUM, ESP_FAIL);
    socket_unit_set(socket_dev->units[unit_id], state);
    return ESP_OK;
}

esp_err_t socket_state_read(socket_handle_t socket_handle, socket_status_t* state_ptr, uint8_t unit_id)
{
    IOT_CHECK(TAG, socket_handle != NULL, ESP_FAIL);
    socket_dev_t* socket_dev = (socket_dev_t*) socket_handle;
    IOT_CHECK(TAG, unit_id < SOCKET_UNIT_NUM, ESP_FAIL);
    *state_ptr = *(socket_dev->units[unit_id]->state_ptr);
    return ESP_OK;
}

esp_err_t socket_net_status_write(socket_handle_t socket_handle, socket_net_status_t net_sta)
{
    IOT_CHECK(TAG, socket_handle != NULL, ESP_FAIL);
    socket_dev_t* socket_dev = (socket_dev_t*) socket_handle;
    switch (net_sta) {
        case SOCKET_STA_DISCONNECTED:
            led_state_write(socket_dev->net_led, LED_QUICK_BLINK);
            break;
        case SOCKET_CONNECTING_CLOUD:
            led_state_write(socket_dev->net_led, LED_SLOW_BLINK);
            break;
        case SOCKET_CLOUD_CONNECTED:
            led_state_write(socket_dev->net_led, LED_NORMAL_ON);
            break;
        default:
            break;
    }
    return ESP_OK;
}

socket_handle_t socket_init(SemaphoreHandle_t xSemWriteInfo)
{
    button_handle_t btn_handle;
    relay_handle_t relay_handle;
    led_handle_t led_handle;
    socket_dev_t* socket_dev = (socket_dev_t*) calloc(1, sizeof(socket_dev_t));
    g_socket = socket_dev;
    memset(socket_dev, 0, sizeof(socket_dev_t));
    param_load(SOCKET_NAME_SPACE, SOCKET_PARAM_KEY, &socket_dev->save_param);

    socket_dev->xSemWriteInfo = xSemWriteInfo;
    /* set led frequency */
    led_setup(5, 1);

    /* create net led */
    socket_dev->net_led = led_create(NET_LED_NUM, LED_DARK_LEVEL);
    led_state_write(socket_dev->net_led, LED_QUICK_BLINK);

#if SOCKET_POWER_METER_ENABLE
    /* create a power meter object */
    pm_config_t pm_conf = {
        .power_io_num      = PM_CF_IO_NUM,
        .power_pcnt_unit   = PCNT_UNIT_0,
        .power_ref_param   = PM_POWER_PARAM,
        .voltage_io_num    = PM_CFI_IO_NUM,
        .voltage_pcnt_unit = PCNT_UNIT_1,
        .voltage_ref_param = PM_VOLTAGE_PARAM,
        .current_io_num    = PM_CFI_IO_NUM,
        .current_pcnt_unit = PCNT_UNIT_1,
        .current_ref_param = PM_CURRENT_PARAM,
        .sel_io_num = 17,
        .sel_level = 0,
        .pm_mode = PM_SINGLE_VOLTAGE
    };
    socket_dev->pm_handle = powermeter_create(pm_conf);
    xTaskCreate(powermeter_task, "powermeter_task", 2048, socket_dev, 5, NULL);
#endif
    relay_io_t relay_io;
    /* create a button object as the main button */
    socket_dev->main_btn = button_dev_init(BUTTON_IO_NUM_MAIN, 0, BUTTON_ACTIVE_LEVEL);
    button_dev_add_tap_cb(BUTTON_TAP_CB, main_button_cb, socket_dev, 50 / portTICK_PERIOD_MS, socket_dev->main_btn);

    /* create units */
    for (int i = 0; i < SOCKET_UNIT_NUM; i++) {
        btn_handle = button_dev_init(DEF_SOCKET_UNIT[i].button_io, 0, DEF_SOCKET_UNIT[i].button_active_level);

        if (DEF_SOCKET_UNIT[i].relay_ctrl_mode == RELAY_GPIO_CONTROL) {
            relay_io.single_io.ctl_io_num = DEF_SOCKET_UNIT[i].relay_ctl_io;
        } else {
            relay_io.flip_io.d_io_num = DEF_SOCKET_UNIT[i].relay_ctl_io;
            relay_io.flip_io.cp_io_num = DEF_SOCKET_UNIT[i].relay_clk_io;
        }
        relay_handle = relay_create(relay_io, DEF_SOCKET_UNIT[i].relay_off_level, DEF_SOCKET_UNIT[i].relay_ctrl_mode, DEF_SOCKET_UNIT[i].relay_io_mode);
        led_handle = led_create(DEF_SOCKET_UNIT[i].led_io, DEF_SOCKET_UNIT[i].led_off_level);
        socket_dev->units[i] = socket_unit_create(&socket_dev->save_param.unit_sta[i], btn_handle, relay_handle,
                led_handle, NULL);
        button_dev_add_tap_cb(BUTTON_TAP_CB, button_tap_cb, socket_dev->units[i], 50 / portTICK_PERIOD_MS, btn_handle);
        socket_unit_set(socket_dev->units[i], socket_dev->save_param.unit_sta[i]);
    }

    return socket_dev;
}
