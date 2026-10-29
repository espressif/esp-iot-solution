/*
 * SPDX-FileCopyrightText: 2024-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "sdkconfig.h"
#include "btn_progress.h"
#include "hid_transport.h"
#include "bsp/keycodes.h"
#include "bsp/keymap.h"
#include "usb_descriptors.h"
#include "rgb_matrix.h"
#include "pixel.h"
#include "settings.h"
#include "esp_log.h"
#include "esp_system.h"
#include "bsp/esp-bsp.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

static const char *TAG = "btn_progress";

static btn_report_type_t report_type = TINYUSB_HID_REPORT;
static light_type_t light_type = RGB_MATRIX;
static SemaphoreHandle_t s_switch_lock;

#define HSV_MAX 18
static uint8_t hsv_index = 0;
static uint8_t hsv_color[HSV_MAX][3] = {
    {132, 102, 255}, // HSV_AZURE
    {170, 255, 255}, // HSV_BLUE
    {64,  255, 255}, // HSV_CHARTREUSE
    {11,  176, 255}, // HSV_CORAL
    {128, 255, 255}, // HSV_CYAN
    {36,  255, 255}, // HSV_GOLD
    {30,  218, 218}, // HSV_GOLDENROD
    {85,  255, 255}, // HSV_GREEN
    {213, 255, 255}, // HSV_MAGENTA
    {21,  255, 255}, // HSV_ORANGE
    {234, 128, 255}, // HSV_PINK
    {191, 255, 255}, // HSV_PURPLE
    {0,   255, 255}, // HSV_RED
    {106, 255, 255}, // HSV_SPRINGGREEN
    {128, 255, 128}, // HSV_TEAL
    {123, 90,  112}, // HSV_TURQUOISE
    {0,   0,   255}, // HSV_WHITE
    {43,  255, 255}, // HSV_YELLOW
};

static SemaphoreHandle_t get_switch_lock(void)
{
    if (!s_switch_lock) {
        s_switch_lock = xSemaphoreCreateMutex();
    }
    return s_switch_lock;
}

esp_err_t keyboard_switch_report_type(btn_report_type_t new_type)
{
    if (new_type >= BTN_REPORT_TYPE_MAX) {
        return ESP_ERR_INVALID_ARG;
    }

    SemaphoreHandle_t lock = get_switch_lock();
    if (lock) {
        xSemaphoreTake(lock, portMAX_DELAY);
    }

    if (new_type == report_type) {
        if (lock) {
            xSemaphoreGive(lock);
        }
        return ESP_OK;
    }

    ESP_LOGI(TAG, "switch report type %d -> %d", report_type, new_type);

    report_type = new_type;

    sys_param_t *sys_param = settings_get_parameter();
    sys_param->report_type = new_type;
    if (new_type == BLE_HID_REPORT) {
        light_type = RGB_MATRIX;
        sys_param->light_type = RGB_MATRIX;
        btn_progress_set_light_type(RGB_MATRIX);
    }

    esp_err_t ret = settings_write_parameter_to_nvs();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "save report type failed: %s", esp_err_to_name(ret));
        if (lock) {
            xSemaphoreGive(lock);
        }
        return ret;
    }

    bsp_ws2812_clear();
    bsp_ws2812_enable(false);
    ESP_LOGI(TAG, "restart to apply report type %d", new_type);
    if (lock) {
        xSemaphoreGive(lock);
    }
    vTaskDelay(pdMS_TO_TICKS(200));
    esp_restart();

    return ESP_OK;
}

static void _report(hid_report_t report)
{
    hid_transport_keyboard_report(report);
}

void btn_progress(keyboard_btn_report_t kbd_report)
{
    static uint8_t layer = 0;
    uint8_t mo_action_layer = layer;
    uint8_t keycode[120] = {0};
    uint8_t keynum = 0;
    uint8_t modify = 0;
    hid_report_t kbd_hid_report = {0};
    hid_report_t consumer_report = {0};
    bool if_consumer_report = false;
    bool release_consumer_report = false;
    sys_param_t *sys_param = settings_get_parameter();
    /*!< Report with key pressed */
    for (int i = 0; i < kbd_report.key_pressed_num; i++) {
        uint8_t row = kbd_report.key_data[i].output_index;
        uint8_t col = kbd_report.key_data[i].input_index;
        uint16_t kc = keymaps[mo_action_layer][row][col];

        switch (kc) {
        case QK_MOMENTARY ... QK_MOMENTARY_MAX:
            // Momentary action_layer
            mo_action_layer = QK_MOMENTARY_GET_LAYER(kc);
            ESP_LOGI(TAG, "Momentary action_layer: %d", mo_action_layer);
            continue;
            break;

        case HID_KEY_CONTROL_LEFT ... HID_KEY_GUI_RIGHT:
            // Modifier key
            modify |= 1 << (kc - HID_KEY_CONTROL_LEFT);
            continue;
            break;

        case QK_OUTPUT_USB:
#if CONFIG_KEYBOARD_KEY_OUTPUT_SWITCH_ENABLE
            keyboard_switch_report_type(TINYUSB_HID_REPORT);
#endif
            break;

        case QK_OUTPUT_BLUETOOTH:
#if CONFIG_KEYBOARD_KEY_OUTPUT_SWITCH_ENABLE
            keyboard_switch_report_type(BLE_HID_REPORT);
#endif
            break;

        /*!< Change to win11 light or local light */
        case KC_NUM_LOCK:
            if (report_type == TINYUSB_HID_REPORT) {
                light_type = (light_type == RGB_MATRIX) ? LAMP_ARRAY_MATRIX : RGB_MATRIX;
                sys_param->light_type = light_type;
                settings_write_parameter_to_nvs();
            } else if (report_type == BLE_HID_REPORT) {
                light_type = RGB_MATRIX;
                sys_param->light_type = light_type;
                settings_write_parameter_to_nvs();
            }
            break;

        case KC_KB_MUTE:
            consumer_report.consumer_report.keycode = HID_USAGE_CONSUMER_MUTE;
            if_consumer_report = true;
            break;
        case KC_KB_VOLUME_UP:
            consumer_report.consumer_report.keycode = HID_USAGE_CONSUMER_VOLUME_INCREMENT;
            if_consumer_report = true;
            break;
        case KC_KB_VOLUME_DOWN:
            consumer_report.consumer_report.keycode = HID_USAGE_CONSUMER_VOLUME_DECREMENT;
            if_consumer_report = true;
            break;

        case QK_BACKLIGHT_UP:
            rgb_matrix_increase_val();
            break;

        case QK_BACKLIGHT_DOWN:
            rgb_matrix_decrease_val();
            break;

        case RGB_SPD:
            rgb_matrix_decrease_speed();
            break;

        case RGB_SPI:
            rgb_matrix_increase_speed();
            break;

        case QK_BACKLIGHT_TOGGLE:
            rgb_matrix_toggle();
            if (!rgb_matrix_is_enabled()) {
                bsp_ws2812_clear();
            }
            bsp_ws2812_enable(rgb_matrix_is_enabled());
            break;

        case RGB_MODE_FORWARD: {
            uint16_t index = (rgb_matrix_get_mode() + 1) % RGB_MATRIX_EFFECT_MAX;
            rgb_matrix_mode(index);
            break;
        }

        case RGB_MODE_REVERSE: {
            uint16_t index = rgb_matrix_get_mode() - 1;
            if (index < 1) {
                index = RGB_MATRIX_EFFECT_MAX - 1;
            }
            rgb_matrix_mode(index);
            break;
        }

        case RGB_TOG:
            rgb_matrix_sethsv(hsv_color[hsv_index][0], hsv_color[hsv_index][1], hsv_color[hsv_index][2]);
            hsv_index = (hsv_index + 1) % HSV_MAX;
            break;

        default:
            if (kc != HID_KEY_NONE) {
                keycode[keynum++] = kc;
            }
            break;
        }
    }

    /*!< Find if consumer key release */
    for (int i = 0; i < kbd_report.key_release_num; i++) {
        uint8_t row = kbd_report.key_release_data[i].output_index;
        uint8_t col = kbd_report.key_release_data[i].input_index;
        uint16_t kc = keymaps[mo_action_layer][row][col];
        switch (kc) {
        case KC_KB_MUTE:
            release_consumer_report = true;
            break;

        case KC_KB_VOLUME_UP:
            release_consumer_report = true;
            break;

        case KC_KB_VOLUME_DOWN:
            release_consumer_report = true;
            break;
        }
    }

    if (keynum <= 6) {
        kbd_hid_report.report_id = REPORT_ID_KEYBOARD;
        kbd_hid_report.keyboard_report.modifier = modify;
        for (int i = 0; i < keynum; i++) {
            kbd_hid_report.keyboard_report.keycode[i] = keycode[i];
        }
    } else {
        kbd_hid_report.report_id = REPORT_ID_FULL_KEY_KEYBOARD;
        kbd_hid_report.keyboard_full_key_report.modifier = modify;
        for (int i = 0; i < keynum; i++) {
            // USAGE ID for keyboard starts from 4
            uint8_t key = keycode[i] - 3;
            uint8_t byteIndex = (key - 1) / 8;
            uint8_t bitIndex = (key - 1) % 8;
            kbd_hid_report.keyboard_full_key_report.keycode[byteIndex] |= (1 << bitIndex);
        }
    }

    _report(kbd_hid_report);

    if (if_consumer_report && !release_consumer_report) {
        consumer_report.report_id = REPORT_ID_CONSUMER;
        _report(consumer_report);
    } else if (release_consumer_report) {
        consumer_report.report_id = REPORT_ID_CONSUMER;
        consumer_report.consumer_report.keycode = 0;
        _report(consumer_report);
    }
}

void light_progress(void)
{
    if (light_type == LAMP_ARRAY_MATRIX) {
        NeopixelUpdateEffect();
    } else {
        rgb_matrix_task();
    }
}

void btn_progress_set_report_type(btn_report_type_t type)
{
    report_type = type;
}

void btn_progress_set_light_type(light_type_t type)
{
    light_type = type;
}
