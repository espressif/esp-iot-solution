/* SPDX-FileCopyrightText: 2022-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "stdio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_idf_version.h"
#include "esp_log.h"
#include "unity.h"
#include "keyboard_button.h"
#include "sdkconfig.h"

static const char *TAG = "KEYBOARD TEST";

#define TEST_MEMORY_LEAK_THRESHOLD (-400)

static size_t before_free_8bit;
static size_t before_free_32bit;

static void keyboard_cb(keyboard_btn_handle_t kbd_handle, keyboard_btn_report_t kbd_report, void *user_data)
{
    if (kbd_report.key_pressed_num == 0) {
        ESP_LOGI(TAG, "All keys released\n\n");
        return;
    }
    printf("pressed: ");
    for (int i = 0; i < kbd_report.key_pressed_num; i++) {
        printf("(%d,%d) ", kbd_report.key_data[i].output_index, kbd_report.key_data[i].input_index);
    }
    printf("\n\n");
}

static void keyboard_combination_cb1(keyboard_btn_handle_t kbd_handle, keyboard_btn_report_t kbd_report, void *user_data)
{
    printf("combination 1: ");
    for (int i = 0; i < kbd_report.key_pressed_num; i++) {
        printf("(%d,%d) ", kbd_report.key_data[i].output_index, kbd_report.key_data[i].input_index);
    }
    printf("\n\n");
}

static keyboard_btn_handle_t _kbd_init(void)
{
    keyboard_btn_config_t cfg = {
        .output_gpios = (int[])
        {
            40, 39, 38, 45, 48, 47
        },
        .output_gpio_num = 6,
        .input_gpios = (int[])
        {
            21, 14, 13, 12, 11, 10, 9, 4, 5, 6, 7, 15, 16, 17, 18
        },
        .input_gpio_num = 15,
        .active_level = 1,
        .debounce_ticks = 2,
        .ticks_interval = 500,
        .enable_power_save = true,
    };
    keyboard_btn_handle_t kbd_handle = NULL;
    keyboard_button_create(&cfg, &kbd_handle);
    return kbd_handle;
}

TEST_CASE("keyboard test", "[keyboard]")
{
    keyboard_btn_handle_t kbd_handle =  _kbd_init();
    TEST_ASSERT_NOT_NULL(kbd_handle);
    keyboard_btn_cb_config_t cb_cfg = {
        .event = KBD_EVENT_PRESSED,
        .callback = keyboard_cb,
    };
    keyboard_button_register_cb(kbd_handle, cb_cfg, NULL);

    cb_cfg.event = KBD_EVENT_COMBINATION;
    cb_cfg.callback = keyboard_combination_cb1;
    cb_cfg.event_data.combination.key_num = 2;
    cb_cfg.event_data.combination.key_data = (keyboard_btn_data_t[]) {
        {5, 1},
        {1, 1},
    };
    keyboard_button_register_cb(kbd_handle, cb_cfg, NULL);

    while (1) {
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}

TEST_CASE("keyboard test memory leak", "[keyboard][memory leak]")
{
    keyboard_btn_handle_t kbd_handle =  _kbd_init();
    TEST_ASSERT_NOT_NULL(kbd_handle);
    keyboard_btn_cb_config_t cb_cfg = {
        .event = KBD_EVENT_PRESSED,
        .callback = keyboard_cb,
    };
    keyboard_button_register_cb(kbd_handle, cb_cfg, NULL);

    cb_cfg.event = KBD_EVENT_COMBINATION;
    cb_cfg.callback = keyboard_combination_cb1;
    cb_cfg.event_data.combination.key_num = 2;
    cb_cfg.event_data.combination.key_data = (keyboard_btn_data_t[]) {
        {5, 1},
        {1, 1},
    };
    keyboard_button_register_cb(kbd_handle, cb_cfg, NULL);

    vTaskDelay(1000 / portTICK_PERIOD_MS);

    keyboard_button_delete(kbd_handle);
}

static void check_leak(size_t before_free, size_t after_free, const char *type)
{
    ssize_t delta = after_free - before_free;
    printf("MALLOC_CAP_%s: Before %u bytes free, After %u bytes free (delta %d)\n", type, before_free, after_free, delta);
    TEST_ASSERT_MESSAGE(delta >= TEST_MEMORY_LEAK_THRESHOLD, "memory leak");
}

void setUp(void)
{
    before_free_8bit = heap_caps_get_free_size(MALLOC_CAP_8BIT);
    before_free_32bit = heap_caps_get_free_size(MALLOC_CAP_32BIT);
}

void tearDown(void)
{
    size_t after_free_8bit = heap_caps_get_free_size(MALLOC_CAP_8BIT);
    size_t after_free_32bit = heap_caps_get_free_size(MALLOC_CAP_32BIT);
    check_leak(before_free_8bit, after_free_8bit, "8BIT");
    check_leak(before_free_32bit, after_free_32bit, "32BIT");
}

void app_main(void)
{
    /*
     * _____ ____  ____    _  __          ____                      _   _____ _____ ____ _____
     *| ____/ ___||  _ \  | |/ /___ _   _| __ )  ___   __ _ _ __ __| | |_   _| ____/ ___|_   _|
     *|  _| \___ \| |_) | | ' // _ \ | | |  _ \ / _ \ / _` | '__/ _` |   | | |  _| \___ \ | |
     *| |___ ___) |  __/  | . \  __/ |_| | |_) | (_) | (_| | | | (_| |   | | | |___ ___) || |
     *|_____|____/|_|     |_|\_\___|\__, |____/ \___/ \__,_|_|  \__,_|   |_| |_____|____/ |_|
     *                              |___/
     */
    printf("  _____ ____  ____    _  __          ____                      _   _____ _____ ____ _____ \n");
    printf(" | ____/ ___||  _ \\  | |/ /___ _   _| __ )  ___   __ _ _ __ __| | |_   _| ____/ ___|_   _|\n");
    printf(" |  _| \\___ \\| |_) | | ' // _ \\ | | |  _ \\ / _ \\ / _` | '__/ _` |   | | |  _| \\___ \\ | |  \n");
    printf(" | |___ ___) |  __/  | . \\  __/ |_| | |_) | (_) | (_| | | | (_| |   | | | |___ ___) || |  \n");
    printf(" |_____|____/|_|     |_|\\_\\___|\\__, |____/ \\___/ \\__,_|_|  \\__,_|   |_| |_____|____/ |_|  \n");
    printf("                               |___/                                                       \n");
    unity_run_menu();
}
