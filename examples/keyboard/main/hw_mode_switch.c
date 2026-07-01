/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "hw_mode_switch.h"

#include "sdkconfig.h"
#include "driver/gpio.h"
#include "esp_check.h"
#include "esp_event.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"

#if CONFIG_KEYBOARD_HW_MODE_SWITCH_ENABLE

static const char *TAG = "hw_mode_switch";

ESP_EVENT_DEFINE_BASE(HW_MODE_SWITCH_EVENT);

enum {
    HW_MODE_SWITCH_EVENT_CHANGED = 0,
};

#define HW_MODE_SWITCH_DEBOUNCE_MS 20
#define HW_MODE_SWITCH_SAMPLE_NUM  5

static hw_mode_switch_cb_t s_mode_changed_cb;
static void *s_mode_changed_ctx;
static TimerHandle_t s_debounce_timer;
static TaskHandle_t s_debounce_task;
static btn_report_type_t s_last_report_type = BTN_REPORT_TYPE_MAX;

static btn_report_type_t hw_mode_switch_decode(int sw_l, int sw_r)
{
    if (sw_l == 0 && sw_r == 1) {
        return BLE_HID_REPORT;
    } else if (sw_l == 1 && sw_r == 1) {
        return TINYUSB_HID_REPORT;
    }

    ESP_LOGW(TAG, "invalid switch state SW_L=%d SW_R=%d, fallback to USB", sw_l, sw_r);
    return TINYUSB_HID_REPORT;
}

btn_report_type_t hw_mode_switch_get_report_type(void)
{
    int sw_l_sum = 0;
    int sw_r_sum = 0;

    for (int i = 0; i < HW_MODE_SWITCH_SAMPLE_NUM; i++) {
        sw_l_sum += gpio_get_level(CONFIG_KEYBOARD_HW_MODE_GPIO_SW_L) ? 1 : 0;
        sw_r_sum += gpio_get_level(CONFIG_KEYBOARD_HW_MODE_GPIO_SW_R) ? 1 : 0;
        vTaskDelay(pdMS_TO_TICKS(2));
    }

    return hw_mode_switch_decode(sw_l_sum > (HW_MODE_SWITCH_SAMPLE_NUM / 2),
                                 sw_r_sum > (HW_MODE_SWITCH_SAMPLE_NUM / 2));
}

static void hw_mode_switch_event_handler(void *handler_arg, esp_event_base_t base, int32_t id, void *event_data)
{
    (void)handler_arg;
    (void)base;
    (void)id;

    btn_report_type_t new_mode = *(btn_report_type_t *)event_data;
    if (s_mode_changed_cb) {
        s_mode_changed_cb(new_mode, s_mode_changed_ctx);
    }
}

static void hw_mode_switch_worker_task(void *arg)
{
    (void)arg;

    while (true) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

        btn_report_type_t new_mode = hw_mode_switch_get_report_type();
        if (new_mode != s_last_report_type) {
            s_last_report_type = new_mode;
            esp_err_t ret = esp_event_post(HW_MODE_SWITCH_EVENT, HW_MODE_SWITCH_EVENT_CHANGED,
                                           &new_mode, sizeof(new_mode), pdMS_TO_TICKS(100));
            if (ret != ESP_OK) {
                ESP_LOGW(TAG, "post switch event failed: %s", esp_err_to_name(ret));
            }
        }

        gpio_intr_enable(CONFIG_KEYBOARD_HW_MODE_GPIO_SW_L);
        gpio_intr_enable(CONFIG_KEYBOARD_HW_MODE_GPIO_SW_R);
    }
}

static void hw_mode_switch_debounce_cb(TimerHandle_t timer)
{
    (void)timer;

    if (s_debounce_task) {
        xTaskNotifyGive(s_debounce_task);
    } else {
        gpio_intr_enable(CONFIG_KEYBOARD_HW_MODE_GPIO_SW_L);
        gpio_intr_enable(CONFIG_KEYBOARD_HW_MODE_GPIO_SW_R);
    }
}

static void IRAM_ATTR hw_mode_switch_isr_handler(void *arg)
{
    (void)arg;
    BaseType_t high_task_woken = pdFALSE;

    gpio_intr_disable(CONFIG_KEYBOARD_HW_MODE_GPIO_SW_L);
    gpio_intr_disable(CONFIG_KEYBOARD_HW_MODE_GPIO_SW_R);
    if (xTimerResetFromISR(s_debounce_timer, &high_task_woken) != pdPASS) {
        gpio_intr_enable(CONFIG_KEYBOARD_HW_MODE_GPIO_SW_L);
        gpio_intr_enable(CONFIG_KEYBOARD_HW_MODE_GPIO_SW_R);
    }

    if (high_task_woken == pdTRUE) {
        portYIELD_FROM_ISR();
    }
}

esp_err_t hw_mode_switch_gpio_init(void)
{
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << CONFIG_KEYBOARD_HW_MODE_GPIO_SW_L) | (1ULL << CONFIG_KEYBOARD_HW_MODE_GPIO_SW_R),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = CONFIG_KEYBOARD_HW_MODE_GPIO_INTERNAL_PULLUP ? GPIO_PULLUP_ENABLE : GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_ANYEDGE,
    };
    return gpio_config(&io_conf);
}

static esp_err_t hw_mode_switch_register_gpio_isr(gpio_num_t gpio_num)
{
    return gpio_isr_handler_add(gpio_num, hw_mode_switch_isr_handler, NULL);
}

esp_err_t hw_mode_switch_init(hw_mode_switch_cb_t on_mode_changed, void *ctx)
{
    esp_err_t ret = esp_event_loop_create_default();
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        return ret;
    }

    s_mode_changed_cb = on_mode_changed;
    s_mode_changed_ctx = ctx;

    if (!s_debounce_task) {
        BaseType_t task_ok = xTaskCreate(hw_mode_switch_worker_task, "hw_mode_sw_work", 3072, NULL, 5, &s_debounce_task);
        ESP_RETURN_ON_FALSE(task_ok == pdPASS, ESP_ERR_NO_MEM, TAG, "create debounce worker task failed");
    }

    if (!s_debounce_timer) {
        s_debounce_timer = xTimerCreate("hw_mode_sw", pdMS_TO_TICKS(HW_MODE_SWITCH_DEBOUNCE_MS),
                                        pdFALSE, NULL, hw_mode_switch_debounce_cb);
        ESP_RETURN_ON_FALSE(s_debounce_timer, ESP_ERR_NO_MEM, TAG, "create debounce timer failed");
    }

    ESP_RETURN_ON_ERROR(hw_mode_switch_gpio_init(), TAG, "config switch GPIO failed");

    ESP_RETURN_ON_ERROR(hw_mode_switch_register_gpio_isr(CONFIG_KEYBOARD_HW_MODE_GPIO_SW_L),
                        TAG, "add SW_L ISR failed");
    ESP_RETURN_ON_ERROR(hw_mode_switch_register_gpio_isr(CONFIG_KEYBOARD_HW_MODE_GPIO_SW_R),
                        TAG, "add SW_R ISR failed");
    ESP_RETURN_ON_ERROR(esp_event_handler_instance_register(HW_MODE_SWITCH_EVENT, HW_MODE_SWITCH_EVENT_CHANGED,
                                                            hw_mode_switch_event_handler, NULL, NULL),
                        TAG, "register switch event failed");

    s_last_report_type = hw_mode_switch_get_report_type();
    ESP_LOGI(TAG, "initial switch mode=%d", s_last_report_type);
    return ESP_OK;
}

#else

esp_err_t hw_mode_switch_gpio_init(void)
{
    return ESP_OK;
}

esp_err_t hw_mode_switch_init(hw_mode_switch_cb_t on_mode_changed, void *ctx)
{
    (void)on_mode_changed;
    (void)ctx;
    return ESP_OK;
}

btn_report_type_t hw_mode_switch_get_report_type(void)
{
    return TINYUSB_HID_REPORT;
}

#endif
