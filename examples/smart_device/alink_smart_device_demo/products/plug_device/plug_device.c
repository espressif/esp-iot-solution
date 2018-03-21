/* Smart device Example

   For other examples please check:
   https://github.com/espressif/esp-iot-solution/tree/master/examples

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
 */

#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include <string.h>
#include "esp_log.h"
#include "iot_power_meter.h"
#include "driver/pcnt.h"
#include "iot_button.h"
#include "iot_relay.h"
#include "iot_touchpad.h"
#include "iot_led.h"
#include "plug_device.h"
#include "iot_param.h"
#include "plug_config.h"

#define TAG "plug"
#define IOT_CHECK(tag, a, ret)  if(!(a)) {                                             \
        ESP_LOGE(tag,"%s:%d (%s)", __FILE__, __LINE__, __FUNCTION__);      \
        return (ret);                                                                   \
        }

typedef struct {
    plug_status_t unit_sta[PLUG_UNIT_NUM];
} save_param_t;

typedef struct {
    plug_status_t* state_ptr;
    button_handle_t btn_handle;
    relay_handle_t relay_handle;
    led_handle_t led_handle;
    tp_handle_t tp_handle;
} plug_unit_t;

typedef struct {
    plug_unit_t* units[PLUG_UNIT_NUM];
    pm_handle_t pm_handle;
    led_handle_t net_led;
    button_handle_t main_btn;
    save_param_t save_param;
    SemaphoreHandle_t xSemWriteInfo;
} plug_dev_t;

static const plug_uint_def_t DEF_PLUG_UNIT[PLUG_UNIT_NUM] = {
        {
        .button_io           = SMART_PLUG_UNIT0_BUTTON_IO,
        .button_active_level = SMART_PLUG_UNIT0_BUTTON_ACTIVE_LEVEL,
        .led_io              = SMART_PLUG_UNIT0_LED_IO,
        .led_off_level       = SMART_PLUG_UNIT0_LED_OFF_LEVEL,
        .relay_off_level     = SMART_PLUG_UNIT0_RELAY_OFF_LEVEL,
        .relay_ctrl_mode     = SMART_PLUG_UNIT0_RELAY_CTRL_MODE,
        .relay_io_mode       = SMART_PLUG_UNIT0_RELAY_IO_MODE,
        .relay_ctl_io        = SMART_PLUG_UNIT0_RELAY_CTL_IO,
        .relay_clk_io        = SMART_PLUG_UNIT0_RELAY_CLK_IO,
        },
#if PLUG_UNIT_NUM > 1
        {
        .button_io           = SMART_PLUG_UNIT1_BUTTON_IO,
        .button_active_level = SMART_PLUG_UNIT1_BUTTON_ACTIVE_LEVEL,
        .led_io              = SMART_PLUG_UNIT1_LED_IO,
        .led_off_level       = SMART_PLUG_UNIT1_LED_OFF_LEVEL,
        .relay_off_level     = SMART_PLUG_UNIT1_RELAY_OFF_LEVEL,
        .relay_ctrl_mode     = SMART_PLUG_UNIT1_RELAY_CTRL_MODE,
        .relay_io_mode       = SMART_PLUG_UNIT1_RELAY_IO_MODE,
        .relay_ctl_io        = SMART_PLUG_UNIT1_RELAY_CTL_IO,
        .relay_clk_io        = SMART_PLUG_UNIT1_RELAY_CLK_IO,
        },
#endif
#if PLUG_UNIT_NUM > 2
        {
        .button_io           = SMART_PLUG_UNIT2_BUTTON_IO,
        .button_active_level = SMART_PLUG_UNIT2_BUTTON_ACTIVE_LEVEL,
        .led_io              = SMART_PLUG_UNIT2_LED_IO,
        .led_off_level       = SMART_PLUG_UNIT2_LED_OFF_LEVEL,
        .relay_off_level     = SMART_PLUG_UNIT2_RELAY_OFF_LEVEL,
        .relay_ctrl_mode     = SMART_PLUG_UNIT2_RELAY_CTRL_MODE,
        .relay_io_mode       = SMART_PLUG_UNIT2_RELAY_IO_MODE,
        .relay_ctl_io        = SMART_PLUG_UNIT2_RELAY_CTL_IO,
        .relay_clk_io        = SMART_PLUG_UNIT2_RELAY_CLK_IO,
        },
#endif
#if PLUG_UNIT_NUM > 3
        {
        .button_io           = SMART_PLUG_UNIT3_BUTTON_IO,
        .button_active_level = SMART_PLUG_UNIT3_BUTTON_ACTIVE_LEVEL,
        .led_io              = SMART_PLUG_UNIT3_LED_IO,
        .led_off_level       = SMART_PLUG_UNIT3_LED_OFF_LEVEL,
        .relay_off_level     = SMART_PLUG_UNIT3_RELAY_OFF_LEVEL,
        .relay_ctrl_mode     = SMART_PLUG_UNIT3_RELAY_CTRL_MODE,
        .relay_io_mode       = SMART_PLUG_UNIT3_RELAY_IO_MODE,
        .relay_ctl_io        = SMART_PLUG_UNIT3_RELAY_CTL_IO,
        .relay_clk_io        = SMART_PLUG_UNIT3_RELAY_CLK_IO,
        },
#endif
};

static plug_dev_t* g_plug;
static plug_unit_t* plug_unit_create(plug_status_t* state_ptr, button_handle_t btn, relay_handle_t relay,
        led_handle_t led, tp_handle_t touchpad)
{
    plug_unit_t* plug_unit = (plug_unit_t*) calloc(1, sizeof(plug_unit_t));
    plug_unit->state_ptr     = state_ptr;
    plug_unit->btn_handle    = btn;
    plug_unit->relay_handle  = relay;
    plug_unit->led_handle    = led;
    plug_unit->tp_handle     = touchpad;
    return plug_unit;
}

static void plug_unit_set(plug_unit_t* plug_unit, plug_status_t state)
{
    if (state == PLUG_OFF) {
        *(plug_unit->state_ptr) = PLUG_OFF;
        if (plug_unit->relay_handle != NULL) {
            iot_relay_state_write(plug_unit->relay_handle, RELAY_STATUS_OPEN);
        }
        if (plug_unit->led_handle != NULL) {
            iot_led_state_write(plug_unit->led_handle, LED_OFF);
        }
    } else {
        *(plug_unit->state_ptr) = PLUG_ON;
        if (plug_unit->relay_handle != NULL) {
            iot_relay_state_write(plug_unit->relay_handle, RELAY_STATUS_CLOSE);
        }
        if (plug_unit->led_handle != NULL) {
            iot_led_state_write(plug_unit->led_handle, LED_ON);
        }
    }
    if (g_plug != NULL) {
        iot_param_save(PLUG_NAME_SPACE, PLUG_PARAM_KEY, &(g_plug->save_param), sizeof(g_plug->save_param));
    }
}

static void button_tap_cb(void* arg)
{
    plug_unit_t* unit_ptr = (plug_unit_t*) arg;
    ESP_EARLY_LOGI(TAG, "a button pressed");
    plug_status_t state = *(unit_ptr->state_ptr);
    if (state == PLUG_OFF) {
        plug_unit_set(unit_ptr, PLUG_ON);
    }
    else {
        plug_unit_set(unit_ptr, PLUG_OFF);
    }
    if (g_plug->xSemWriteInfo != NULL) {
        xSemaphoreGive(g_plug->xSemWriteInfo);
    }
}

static void main_button_cb(void* arg)
{
    plug_dev_t* plug_dev = (plug_dev_t*) arg;
    for (int i = 0; i < PLUG_UNIT_NUM; i++) {
        if (plug_dev->units[i] != NULL) {
            plug_unit_set(plug_dev->units[i], PLUG_OFF);
        }
    }
    if (g_plug->xSemWriteInfo != NULL) {
        xSemaphoreGive(g_plug->xSemWriteInfo);
    }
}

void powermeter_task(void* arg)
{
    plug_dev_t* plug_dev = (plug_dev_t*) arg;
    while(1) {
        vTaskDelay(5000 / portTICK_RATE_MS);
        ESP_LOGI(TAG, "value of power:%d", iot_powermeter_read(plug_dev->pm_handle, PM_POWER));
        ESP_LOGI(TAG, "value of voltage:%d", iot_powermeter_read(plug_dev->pm_handle, PM_VOLTAGE));
        ESP_LOGI(TAG, "value of current:%d", iot_powermeter_read(plug_dev->pm_handle, PM_CURRENT));
    }
    vTaskDelete(NULL);
}

esp_err_t plug_powermeter_read(plug_handle_t plug_handle, uint32_t *power, uint32_t *current, uint32_t *voltage)
{
    IOT_CHECK(TAG, plug_handle != NULL, ESP_FAIL);
    plug_dev_t* plug_dev = (plug_dev_t*) plug_handle;
    *power = iot_powermeter_read(plug_dev->pm_handle, PM_POWER);
    *current = iot_powermeter_read(plug_dev->pm_handle, PM_CURRENT);
    *voltage = iot_powermeter_read(plug_dev->pm_handle, PM_VOLTAGE);
    return ESP_OK;
}

esp_err_t plug_state_write(plug_handle_t plug_handle, plug_status_t state, uint8_t unit_id)
{
    IOT_CHECK(TAG, plug_handle != NULL, ESP_FAIL);
    plug_dev_t* plug_dev = (plug_dev_t*) plug_handle;
    IOT_CHECK(TAG, unit_id < PLUG_UNIT_NUM, ESP_FAIL);
    plug_unit_set(plug_dev->units[unit_id], state);
    return ESP_OK;
}

esp_err_t plug_state_read(plug_handle_t plug_handle, plug_status_t* state_ptr, uint8_t unit_id)
{
    IOT_CHECK(TAG, plug_handle != NULL, ESP_FAIL);
    plug_dev_t* plug_dev = (plug_dev_t*) plug_handle;
    IOT_CHECK(TAG, unit_id < PLUG_UNIT_NUM, ESP_FAIL);
    *state_ptr = *(plug_dev->units[unit_id]->state_ptr);
    return ESP_OK;
}

esp_err_t plug_net_status_write(plug_handle_t plug_handle, plug_net_status_t net_status)
{
    IOT_CHECK(TAG, plug_handle != NULL, ESP_FAIL);
    plug_dev_t* plug_dev = (plug_dev_t*) plug_handle;
    switch (net_status) {
        case PLUG_STA_DISCONNECTED:
            iot_led_state_write(plug_dev->net_led, LED_QUICK_BLINK);
            break;
        case PLUG_CLOUD_DISCONNECTED:
            iot_led_state_write(plug_dev->net_led, LED_SLOW_BLINK);
            break;
        case PLUG_CLOUD_CONNECTED:
            iot_led_state_write(plug_dev->net_led, LED_ON);
            break;
        default:
            break;
    }
    return ESP_OK;
}

plug_handle_t plug_init(SemaphoreHandle_t xSemWriteInfo)
{
    button_handle_t btn_handle;
    relay_handle_t relay_handle;
    led_handle_t led_handle;
    plug_dev_t* plug_dev = (plug_dev_t*) calloc(1, sizeof(plug_dev_t));
    g_plug = plug_dev;
    memset(plug_dev, 0, sizeof(plug_dev_t));
    iot_param_load(PLUG_NAME_SPACE, PLUG_PARAM_KEY, &plug_dev->save_param);
    plug_dev->xSemWriteInfo = xSemWriteInfo;

    /* create net led */
    plug_dev->net_led = iot_led_create(NET_LED_NUM, LED_DARK_LEVEL);
    iot_led_state_write(plug_dev->net_led, LED_QUICK_BLINK);

#if PLUG_POWER_METER_ENABLE
    /* create a power meter object */
    pm_config_t pm_conf = {
        .power_io_num      = PM_CF_IO_NUM,
        .power_pcnt_unit   = PM_CF_PCNT_UNIT_NUM,
        .power_ref_param   = PM_POWER_PARAM,
        .voltage_io_num    = PM_CFI_IO_NUM,
        .voltage_pcnt_unit = PM_CFI_PCNT_UNIT_NUM,
        .voltage_ref_param = PM_VOLTAGE_PARAM,
        .current_io_num    = PM_CFI_IO_NUM,
        .current_pcnt_unit = PM_CFI_PCNT_UNIT_NUM,
        .current_ref_param = PM_CURRENT_PARAM,
        .sel_io_num        = PM_MODE_SEL_PIN,
        .sel_level         = PM_MODE_SEL_PIN_LEVEL,
        .pm_mode           = PM_SINGLE_VOLTAGE
    };
    plug_dev->pm_handle = iot_powermeter_create(pm_conf);
    xTaskCreate(powermeter_task, "powermeter_task", 2048, plug_dev, 5, NULL);
#endif
    relay_io_t relay_io;
    /* create a button object as the main button */
    plug_dev->main_btn = iot_button_create(BUTTON_IO_NUM_MAIN, BUTTON_ACTIVE_LEVEL);
    iot_button_set_evt_cb(plug_dev->main_btn, BUTTON_CB_TAP, main_button_cb, plug_dev);

    /* create units */
    for (int i = 0; i < PLUG_UNIT_NUM; i++) {
        btn_handle = iot_button_create(DEF_PLUG_UNIT[i].button_io, DEF_PLUG_UNIT[i].button_active_level);

        if (DEF_PLUG_UNIT[i].relay_ctrl_mode == RELAY_GPIO_CONTROL) {
            relay_io.single_io.ctl_io_num = DEF_PLUG_UNIT[i].relay_ctl_io;
        } else {
            relay_io.flip_io.d_io_num = DEF_PLUG_UNIT[i].relay_ctl_io;
            relay_io.flip_io.cp_io_num = DEF_PLUG_UNIT[i].relay_clk_io;
        }
        relay_handle = iot_relay_create(relay_io, DEF_PLUG_UNIT[i].relay_off_level, DEF_PLUG_UNIT[i].relay_ctrl_mode, DEF_PLUG_UNIT[i].relay_io_mode);
        led_handle = iot_led_create(DEF_PLUG_UNIT[i].led_io, DEF_PLUG_UNIT[i].led_off_level);
        plug_dev->units[i] = plug_unit_create(&plug_dev->save_param.unit_sta[i], btn_handle, relay_handle,
                led_handle, NULL);
        iot_button_set_evt_cb(btn_handle, BUTTON_CB_TAP, button_tap_cb, plug_dev->units[i]);
        plug_unit_set(plug_dev->units[i], plug_dev->save_param.unit_sta[i]);
    }

    return plug_dev;
}
