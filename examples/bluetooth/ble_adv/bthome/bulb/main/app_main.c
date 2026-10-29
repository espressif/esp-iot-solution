/*
 * SPDX-FileCopyrightText: 2024-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "esp_sleep.h"
#include "bthome_v2.h"
#include "ble_hci.h"
#include "led_strip.h"
#include "nvs_flash.h"

// GPIO assignment from kconfig
#define LED_STRIP_BLINK_GPIO  CONFIG_EXAMPLE_LED_STRIP_BLINK_GPIO
// Numbers of the LED in the strip from kconfig
#define LED_STRIP_LED_NUMBERS CONFIG_EXAMPLE_LED_STRIP_LED_NUMBERS
// RMT resolution from kconfig (led strip needs a high resolution)
#define LED_STRIP_RMT_RES_HZ  CONFIG_EXAMPLE_LED_STRIP_RMT_RES_HZ

static const char *TAG = "bt_home_bulb";

static const uint8_t encrypt_key[] = {0x23, 0x1d, 0x39, 0xc1, 0xd7, 0xcc, 0x1a, 0xb1, 0xae, 0xe2, 0x24, 0xcd, 0x09, 0x6d, 0xb9, 0x32};
static const uint8_t peer_mac[] = { 0x54, 0x48, 0xE6, 0x8F, 0x80, 0xA5};

static QueueHandle_t s_scan_result_queue;

static led_strip_handle_t configure_led(void)
{
    // LED strip general initialization, according to your led board design
    led_strip_config_t strip_config = {
        .strip_gpio_num = LED_STRIP_BLINK_GPIO,   // The GPIO that connected to the LED strip's data line
        .max_leds = LED_STRIP_LED_NUMBERS,        // The number of LEDs in the strip,
        .led_pixel_format = LED_PIXEL_FORMAT_GRB, // Pixel format of your LED strip
        .led_model = LED_MODEL_WS2812,            // LED strip model
        .flags.invert_out = false,                // whether to invert the output signal
    };

    // LED strip backend configuration: RMT
    led_strip_rmt_config_t rmt_config = {
#if ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(5, 0, 0)
        .rmt_channel = 0,
#else
        .clk_src = RMT_CLK_SRC_DEFAULT,        // different clock source can lead to different power consumption
        .resolution_hz = LED_STRIP_RMT_RES_HZ, // RMT counter clock frequency
        .flags.with_dma = false,               // DMA feature is available on ESP target like ESP32-S3
#endif
    };

    // LED Strip object handle
    led_strip_handle_t led_strip;
    ESP_ERROR_CHECK(led_strip_new_rmt_device(&strip_config, &rmt_config, &led_strip));
    ESP_LOGI(TAG, "Created LED strip object with RMT backend");
    return led_strip;
}

static void ble_hci_scan_cb(ble_hci_scan_result_t *scan_result, uint16_t result_len)
{
    ble_hci_scan_result_t result;
    for (int i = 0; i < result_len; i++) {
        memcpy(&result, scan_result, sizeof(ble_hci_scan_result_t));
        if (xQueueSend(s_scan_result_queue, &result, 0) != pdPASS) {
            ESP_LOGW(TAG, "Failed to send s_scan_result_queue");
        }
    }
}

static void configure_ble_scan(void)
{
    s_scan_result_queue = xQueueCreate(10, sizeof(ble_hci_scan_result_t));
    ble_hci_init();
    ble_hci_reset();
    ble_hci_enable_meta_event();

    ble_hci_scan_param_t scan_param = {
        .scan_type = BLE_SCAN_TYPE_PASSIVE,
        .scan_interval = 0x50,
        .scan_window = 0x50,
        .own_addr_type = BLE_ADDR_TYPE_PUBLIC,
        .filter_policy = ADV_FILTER_ALLOW_SCAN_WLST_CON_ANY,
    };

    ble_hci_set_scan_param(&scan_param);
    ble_hci_set_register_scan_callback(&ble_hci_scan_cb);
    ble_hci_addr_t peer_mac_addr = {0};
    memcpy((uint8_t *)peer_mac_addr, peer_mac, BLE_HCI_ADDR_LEN);
    ble_hci_add_to_accept_list(peer_mac_addr, BLE_ADDR_TYPE_RANDOM);
    ble_hci_set_scan_enable(true, true);
}

static void settings_store(bthome_handle_t handle, const char *key, const uint8_t *data, uint8_t len)
{
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open("storage", NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error opening NVS handle: %s", esp_err_to_name(err));
        return;
    }

    err = nvs_set_blob(nvs_handle, key, data, len);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to write blob to NVS: %s", esp_err_to_name(err));
    } else {
        err = nvs_commit(nvs_handle);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to commit to NVS: %s", esp_err_to_name(err));
        }
    }

    nvs_close(nvs_handle);
}

static void settings_load(bthome_handle_t handle, const char *key, uint8_t *data, uint8_t len)
{
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open("storage", NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Error opening NVS handle: %s", esp_err_to_name(err));
        memset(data, 0, len);
        return;
    }

    size_t required_size = len;
    err = nvs_get_blob(nvs_handle, key, data, &required_size);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Failed to read blob from NVS: %s", esp_err_to_name(err));
        memset(data, 0, len);
    }

    nvs_close(nvs_handle);
}

void app_main(void)
{
    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    bool led_onoff = 0;
    uint8_t lightness = 127;
    ble_hci_scan_result_t result;

    configure_ble_scan();

    bthome_handle_t bthome_recv;
    ESP_ERROR_CHECK(bthome_create(&bthome_recv));
    bthome_callbacks_t callbacks = {
        .store = settings_store,
        .load = settings_load,
    };
    ESP_ERROR_CHECK(bthome_register_callbacks(bthome_recv, &callbacks));
    ESP_ERROR_CHECK(bthome_set_encrypt_key(bthome_recv, encrypt_key));
    ESP_ERROR_CHECK(bthome_set_peer_mac_addr(bthome_recv, peer_mac));

    // Configure LED
    led_strip_handle_t led_strip = configure_led();

    while (1) {
        if (xQueueReceive(s_scan_result_queue, &result, portMAX_DELAY) == pdPASS) {
            bthome_reports_t *reports = bthome_parse_adv_data(bthome_recv, result.ble_adv, result.adv_data_len);
            if (reports == NULL) {
                ESP_LOGW(TAG, "Parse BTHome reports failed");
                continue;
            }

            for (int i = 0; i < reports->num_reports; i++) {
                if (reports->report[i].id == BTHOME_EVENT_ID_BUTTON) {
                    if (reports->report[i].data[0] == 0x01) {
                        ESP_LOGI(TAG, "Button Clicked");
                        led_onoff = !led_onoff;
                    }
                }

                if (reports->report[i].id == BTHOME_EVENT_ID_DIMMER) {
                    if (reports->report[i].data[1] > 127) {
                        reports->report[i].data[1] = 127;
                    }
                    if (reports->report[i].data[0] == 0x01) {
                        lightness = 127 + reports->report[i].data[1];
                    } else if (reports->report[i].data[0] == 0x02) {
                        lightness = 128 - reports->report[i].data[1];
                    }
                }

                if (led_onoff) {
                    ESP_ERROR_CHECK(led_strip_set_pixel(led_strip, 0, lightness, lightness, lightness));
                } else {
                    ESP_ERROR_CHECK(led_strip_set_pixel(led_strip, 0, 0, 0, 0));
                }
                ESP_ERROR_CHECK(led_strip_refresh(led_strip));
            }
            bthome_free_reports(reports);
        }
    }
}
