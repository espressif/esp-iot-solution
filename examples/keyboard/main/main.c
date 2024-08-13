/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "ble_hid.h"
#include "bsp/esp-bsp.h"
#include "bsp/keycodes.h"
#include "btn_progress.h"
#include "esp_log.h"
#include "esp_pm.h"
#include "keyboard_button.h"
#include "led_strip.h"
#include "nvs_flash.h"
#include "rgb_matrix.h"
#include "settings.h"
#include "tinyusb_hid.h"
#include "esp_timer.h"

static keyboard_btn_handle_t kbd_handle = NULL;
static TaskHandle_t light_progress_task_handle = NULL;
uint64_t last_time = 0;
sys_param_t* sys_param;

static void keyboard_cb(keyboard_btn_handle_t kbd_handle, keyboard_btn_report_t kbd_report, void *user_data)
{
    if (sys_param->report_type == BLE_HID_REPORT) {
        if (eTaskGetState(light_progress_task_handle) == eSuspended) {
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
        if (sys_param->report_type == BLE_HID_REPORT) {
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

    bsp_keyboard_init(&kbd_handle, NULL);

    /*!< Init LED and clear WS2812's status */
    led_strip_handle_t led_strip = NULL;
    bsp_ws2812_init(&led_strip);
    if (led_strip) {
        led_strip_clear(led_strip);
    }

    bsp_lamp_array_init(KC_NUM_LOCK);
    bsp_rgb_matrix_init();
    settings_read_parameter_from_nvs();
    sys_param = settings_get_parameter();
    btn_progress_set_report_type(sys_param->report_type);
    btn_progress_set_light_type(sys_param->light_type);
    bsp_ws2812_enable(true);
    if (sys_param->report_type == TINYUSB_HID_REPORT) {
        tinyusb_hid_init();
    } else if (sys_param->report_type == BLE_HID_REPORT) {
        /*!< BLE temporarily does not support lighting effects on Windows 11 and is forced to be set to RGB Matrix */
        btn_progress_set_light_type(RGB_MATRIX);
        ble_hid_init();
        esp_pm_config_t pm_config = {
            .max_freq_mhz = CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ,
            .min_freq_mhz = 160,
#if CONFIG_FREERTOS_USE_TICKLESS_IDLE
            .light_sleep_enable = true
#endif
        };
        ESP_ERROR_CHECK(esp_pm_configure(&pm_config));
    }

    keyboard_btn_cb_config_t cb_cfg = {
        .event = KBD_EVENT_PRESSED,
        .callback = keyboard_cb,
    };
    keyboard_button_register_cb(kbd_handle, cb_cfg, NULL);

    xTaskCreate(light_progress_task, "light_progress_task", 4096, NULL, 5, &light_progress_task_handle);
}
