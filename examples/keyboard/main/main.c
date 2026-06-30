/*
 * SPDX-FileCopyrightText: 2024-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <stdio.h>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "bsp/esp-bsp.h"
#include "bsp/keycodes.h"
#include "btn_progress.h"
#include "esp_log.h"
#include "keyboard_button.h"
#include "nvs_flash.h"
#include "settings.h"
#include "hid_transport.h"
#include "hw_mode_switch.h"
#include "led_strip.h"
#include "rgb_matrix.h"
#include "esp_timer.h"
#if CONFIG_KEYBOARD_BATTERY_ADC_ENABLE
#include "battery_adc.h"
#endif

static keyboard_btn_handle_t kbd_handle = NULL;
static TaskHandle_t light_progress_task_handle = NULL;
uint64_t last_time = 0;

#if CONFIG_KEYBOARD_HW_MODE_SWITCH_ENABLE
static void hw_mode_changed_cb(btn_report_type_t new_mode, void *ctx)
{
    (void)ctx;
    ESP_ERROR_CHECK_WITHOUT_ABORT(keyboard_switch_report_type(new_mode));
}
#endif

static void keyboard_cb(keyboard_btn_handle_t kbd_handle, keyboard_btn_report_t kbd_report, void *user_data)
{
    if (hid_transport_needs_ble_style_pm()) {
        if (light_progress_task_handle &&
                eTaskGetState(light_progress_task_handle) == eSuspended) {
            if (rgb_matrix_is_enabled()) {
                bsp_ws2812_enable(true);
                rgb_matrix_set_suspend_state(false);
                vTaskResume(light_progress_task_handle);
            }
        }
        last_time = esp_timer_get_time();
    }

    btn_progress(kbd_report);

    /*!< Lighting with key pressed */
    if (kbd_report.key_change_num > 0) {
        for (int i = 1; i <= kbd_report.key_change_num; i++) {
            process_rgb_matrix(kbd_report.key_data[kbd_report.key_pressed_num - i].output_index, kbd_report.key_data[kbd_report.key_pressed_num - i].input_index, true);
        }
    }
}

static void light_progress_task(void *pvParameters)
{
    while (1) {
        if (bsp_ws2812_is_enable()) {
            light_progress();
        }
        vTaskDelay(10 / portTICK_PERIOD_MS);
        if (hid_transport_needs_ble_style_pm()) {
            if (esp_timer_get_time() - last_time > CONFIG_LIGHT_SLEEP_TIMEOUT_MS * 1000) {
                last_time = esp_timer_get_time();
                rgb_matrix_set_suspend_state(true);
                bsp_ws2812_enable(false);
                vTaskSuspend(NULL);
            }
        }
    }
}

void app_main(void)
{
    esp_err_t ret;
    ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    /*!< Init LED and clear WS2812's status */
    led_strip_handle_t led_strip = NULL;
    bsp_ws2812_init(&led_strip);
    if (led_strip) {
        led_strip_clear(led_strip);
    }

    bsp_lamp_array_init(KC_NUM_LOCK);
    bsp_rgb_matrix_init();

    settings_read_parameter_from_nvs();
    sys_param_t *sys_param = settings_get_parameter();

#if CONFIG_KEYBOARD_HW_MODE_SWITCH_ENABLE
    ESP_ERROR_CHECK(hw_mode_switch_gpio_init());
    btn_report_type_t hw_mode = hw_mode_switch_get_report_type();
    if (sys_param->report_type != hw_mode) {
        sys_param->report_type = hw_mode;
        ESP_ERROR_CHECK(settings_write_parameter_to_nvs());
    }
#endif

    btn_progress_set_report_type(sys_param->report_type);
    btn_progress_set_light_type(sys_param->light_type);

    rgb_matrix_enable_noeeprom();
    rgb_matrix_mode_noeeprom(RGB_MATRIX_TYPING_HEATMAP);
    rgb_matrix_set_speed_noeeprom(80);
    rgb_matrix_sethsv_noeeprom(0, 255, 100);

    bsp_ws2812_enable(true);
    if (sys_param->report_type == BLE_HID_REPORT) {
        /*!< BLE temporarily does not support lighting effects on Windows 11 and is forced to be set to RGB Matrix */
        btn_progress_set_light_type(RGB_MATRIX);
    }

    /*!< Init BLE/USB transport before keyboard scan */
    ESP_ERROR_CHECK(hid_transport_init());

    bsp_keyboard_init(&kbd_handle, NULL);

#if CONFIG_KEYBOARD_HW_MODE_SWITCH_ENABLE
    ESP_ERROR_CHECK(hw_mode_switch_init(hw_mode_changed_cb, NULL));
#endif

    keyboard_btn_cb_config_t cb_cfg = {
        .event = KBD_EVENT_PRESSED,
        .callback = keyboard_cb,
    };
    keyboard_button_register_cb(kbd_handle, cb_cfg, NULL);

    xTaskCreate(light_progress_task, "light_progress_task", 4096, NULL, 5, &light_progress_task_handle);
#if CONFIG_KEYBOARD_BATTERY_ADC_ENABLE
    battery_adc_init();
#endif
}
