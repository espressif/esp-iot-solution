/*
 * SPDX-FileCopyrightText: 2023-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>

// IDF includes
#include "esp_err.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_bt.h"
#include "esp_bt_main.h"
#include "esp_gatt_common_api.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/event_groups.h"
#include "esp_idf_version.h"

// Example includes
#include "controller.h"
#include "gap_gatts.h"
#include "hidd.h"

#define HID_DEMO_TAG "HID_REMOTE_CONTROL"

#define TEAR_DOWN_BIT_MASK 0b0011
// Refer to HID report reference defined in hidd.c
// bit 0 - Button A, bit 1 - Button B, bit 2 - Button C, bit 3 - Button D
// 0b0011 represent pressing Button A and Button B simultaneously
// Pressing the button combination will trigger tear down

bool is_connected = false;
QueueHandle_t input_queue = NULL;
static uint8_t s_battery_level = 0;
const char *DEVICE_NAME = "ESP32 Remote";

/**
 * @brief   Print the user input report sent through BLE HID
*/
void print_user_input_report(uint8_t joystick_x, uint8_t joystick_y,
                             uint8_t hat_switch, uint8_t buttons, uint8_t throttle)
{
    ESP_LOGI(HID_DEMO_TAG, " ");
    ESP_LOGI(HID_DEMO_TAG, "----- Sending user input -----");

    ESP_LOGI(HID_DEMO_TAG, "X: 0x%x (%d), Y: 0x%x (%d), SW: %d, B: %d, Thr: %d",
             joystick_x, joystick_x, joystick_y, joystick_y,
             hat_switch, buttons, throttle);

    if (buttons) {
        ESP_LOGI(HID_DEMO_TAG, "Button pressed: %s %s %s %s",
                 (buttons & 0x01) ? "A" : "-",
                 (buttons & 0x02) ? "B" : "-",
                 (buttons & 0x04) ? "C" : "-",
                 (buttons & 0x08) ? "D" : "-");
    }

    ESP_LOGI(HID_DEMO_TAG, " ----- End of user input ----- \n");
}

/**
 * @brief   Main task to send user input to the ESP to the central device
 *
 * @details This task will wait for user input from the console, joystick, or button (using RTOS queue)
 *          The function does not return unless Tear Down button sequence is pressed (and tear down config enabled in menuconfig)
*/
void joystick_task()
{
    esp_err_t ret = ESP_OK;
    input_event_t input_event = {0};
    uint8_t x_axis = 0;
    uint8_t y_axis = 0;
    uint8_t hat_switch = 0; // unused in this example
    uint8_t button_in = 0;
    uint8_t throttle = 0;   // unused in this example
    while (true) {
        DELAY(HID_LATENCY);

        if (!is_connected) {
            ESP_LOGI(HID_DEMO_TAG, "Not connected, do not send user input yet");
            DELAY(3000);
            continue;
        }

        // HID report values to set is dependent on the HID report map (refer to hidd.c)
        // For this examples, the values in the HID report to send are
        // x_axis : 8 bit, 0 - 255
        // y_axis : 8 bit, 0 - 255
        // hat switches : 4 bit
        // buttons 1 to 4: 1 bit each, 0 - 1
        // throttle: 8 bit

        // Send report if there are any inputs (refer to controller.c)
        if (xQueueReceive(input_queue, &input_event, 100) == pdTRUE) {
            switch (input_event.input_source) {
            case (INPUT_SOURCE_BUTTON):
                button_in = input_event.data_button;
                joystick_adc_read(&x_axis, &y_axis);
                ESP_LOGI(HID_DEMO_TAG, "Button input received");
                break;
            case (INPUT_SOURCE_CONSOLE):
                button_in = button_read();
                char_to_joystick_input(input_event.data_console, &x_axis, &y_axis);
                ESP_LOGI(HID_DEMO_TAG, "Console input received");
                break;
            case (INPUT_SOURCE_JOYSTICK):
                button_in = button_read();
                x_axis = input_event.data_joystick_x;
                y_axis = input_event.data_joystick_y;
                ESP_LOGI(HID_DEMO_TAG, "Joystick input received");
                break;
            default:
                ESP_LOGE(HID_DEMO_TAG, "Unknown input source, source %d", input_event.input_source);
                break;
            }

            set_hid_report_values(x_axis, y_axis, button_in, hat_switch, throttle);
            print_user_input_report(x_axis, y_axis, hat_switch, button_in, throttle);
            ret = send_user_input();
        }

        // Alternatively, to simply poll user input can do:
        // joystick_adc_read(&x_axis, &y_axis);
        // button_read(&button_in);

        if (ret != ESP_OK) {
            ESP_LOGE(HID_DEMO_TAG, "Error sending user input, code = %d", ret);
        }

#ifdef CONFIG_EXAMPLE_ENABLE_TEAR_DOWN_DEMO
        if (button_in == TEAR_DOWN_BIT_MASK) {
            ESP_LOGI(HID_DEMO_TAG, "Tear down button sequence pressed, tear down connection and gpio");
            break;
        }
#endif
    }
}

static void battery_timer_cb(TimerHandle_t xTimer)
{
    s_battery_level = (s_battery_level + 1) % 100;
    ESP_LOGI(HID_DEMO_TAG, "Change battery level to %d", s_battery_level);
    if (is_connected) {
        esp_err_t ret = set_hid_battery_level(s_battery_level);
        if (ret != ESP_OK) {
            ESP_LOGE(HID_DEMO_TAG, "Failed to set battery level");
        }
    }
}

/**
 * @brief Initialize bluetooth resources
 * @return ESP_OK on success; any other value indicates error
*/
esp_err_t ble_config(void)
{
    esp_err_t ret;
    ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }

    ESP_ERROR_CHECK(esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT));

    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    ret = esp_bt_controller_init(&bt_cfg);
    if (ret) {
        ESP_LOGE(HID_DEMO_TAG, "%s initialize controller failed\n", __func__);
        return ESP_FAIL;
    }

    ret = esp_bt_controller_enable(ESP_BT_MODE_BLE);
    if (ret) {
        ESP_LOGE(HID_DEMO_TAG, "%s enable controller failed\n", __func__);
        return ESP_FAIL;
    }

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 2, 0)
    esp_bluedroid_config_t cfg = BT_BLUEDROID_INIT_CONFIG_DEFAULT();
    ret = esp_bluedroid_init_with_cfg(&cfg);
#else
    ret = esp_bluedroid_init();
#endif
    if (ret) {
        ESP_LOGE(HID_DEMO_TAG, "%s init bluedroid failed\n", __func__);
        return ESP_FAIL;
    }

    ret = esp_bluedroid_enable();
    if (ret) {
        ESP_LOGE(HID_DEMO_TAG, "%s init bluedroid failed\n", __func__);
        return ESP_FAIL;
    }

    return ESP_OK;
}

/**
 * @brief Tear down and free resources used
*/
esp_err_t tear_down(void)
{
    ESP_ERROR_CHECK(joystick_deinit());
    ESP_ERROR_CHECK(button_deinit());
    ESP_ERROR_CHECK(gap_gatts_deinit());

    ESP_ERROR_CHECK(esp_bluedroid_disable());
    ESP_ERROR_CHECK(esp_bluedroid_deinit());
    ESP_ERROR_CHECK(esp_bt_controller_disable());
    ESP_ERROR_CHECK(esp_bt_controller_mem_release(ESP_BT_MODE_BLE));
    ESP_ERROR_CHECK(nvs_flash_erase());

    return ESP_OK;
}

void app_main(void)
{
    // Resource initialisation
    esp_err_t ret;
#ifdef CONFIG_JOYSTICK_INPUT_MODE_ADC
    ret = joystick_init();
    if (ret != ESP_OK) {
        ESP_LOGE(HID_DEMO_TAG, "%s config joystick failed\n", __func__);
        return;
    }
#endif
    ret = button_init();
    if (ret != ESP_OK) {
        ESP_LOGE(HID_DEMO_TAG, "%s config button failed\n", __func__);
        return;
    }
    ret = ble_config();
    if (ret != ESP_OK) {
        ESP_LOGE(HID_DEMO_TAG, "%s config ble failed\n", __func__);
        return;
    }
    ESP_LOGI(HID_DEMO_TAG, "BLE configured");

    // GAP and GATTS initialisation
    esp_ble_gap_register_callback(gap_event_callback);
    esp_ble_gatts_register_callback(gatts_event_callback);
    ESP_LOGI(HID_DEMO_TAG, "GAP and GATTS Callbacks registered");
    gap_set_security();
    ESP_LOGI(HID_DEMO_TAG, "Security set");
    esp_ble_gatts_app_register(ESP_GATT_UUID_HID_SVC); // Trigger ESP_GATTS_REG_EVT (see gap_gatts.c and hidd.c)
    ret = esp_ble_gatt_set_local_mtu(500);
    if (ret) {
        ESP_LOGE(HID_DEMO_TAG, "set local  MTU failed, error code = %x", ret);
    }

    // Create tasks and queue to handle input events
    input_queue = xQueueCreate(10, sizeof(input_event_t));

#ifdef CONFIG_JOYSTICK_INPUT_MODE_CONSOLE
    print_console_read_help();
    xTaskCreate(joystick_console_read, "console_read_joystick", 2048, NULL, tskIDLE_PRIORITY, NULL);
#else //CONFIG_JOYSTICK_INPUT_MODE_ADC
    xTaskCreate(joystick_ext_read, "ext_hardware_joystick", 2048, NULL, tskIDLE_PRIORITY, NULL);
#endif
    // Create a timer to update battery level periodically (add 1 each time as an example)
    TimerHandle_t  battery_timer = xTimerCreate(NULL, pdMS_TO_TICKS(5000), true, NULL, battery_timer_cb);
    if (xTimerStart(battery_timer, 0) != pdPASS) {
        ESP_LOGE(HID_DEMO_TAG, "Failed to start battery timer");
    }
    // Main joystick task
    joystick_task();

#ifdef CONFIG_EXAMPLE_ENABLE_TEAR_DOWN_DEMO
    // Tear down, after returning from joystick_task()
    ESP_ERROR_CHECK(tear_down());
    ESP_LOGI(HID_DEMO_TAG, "End of joystick demo, restarting in 5 seconds");
    DELAY(5000);
    esp_restart();
#endif
}
