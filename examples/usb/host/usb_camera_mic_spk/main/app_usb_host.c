/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "app_usb_host.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "usb/usb_host.h"

static const char *TAG = "app_usb_host";

static void app_usb_board_init(void)
{
#ifdef CONFIG_ESP32_S3_USB_OTG
    const gpio_config_t io_config = {
        .pin_bit_mask = BIT64(GPIO_NUM_18),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&io_config));
    ESP_ERROR_CHECK(gpio_set_level(GPIO_NUM_18, 1));

    const gpio_config_t power_io_config = {
        .pin_bit_mask = BIT64(GPIO_NUM_17) | BIT64(GPIO_NUM_12) | BIT64(GPIO_NUM_13),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&power_io_config));

    ESP_ERROR_CHECK(gpio_set_level(GPIO_NUM_17, 1));
    ESP_ERROR_CHECK(gpio_set_level(GPIO_NUM_12, 0));
    ESP_ERROR_CHECK(gpio_set_level(GPIO_NUM_13, 0));
    vTaskDelay(pdMS_TO_TICKS(10));
    ESP_ERROR_CHECK(gpio_set_level(GPIO_NUM_12, 1));
#endif
}

static void usb_host_task(void *arg)
{
    const usb_host_config_t host_config = {
        .skip_phy_setup = false,
        .intr_flags = ESP_INTR_FLAG_LOWMED,
#if CONFIG_IDF_TARGET_ESP32S2 || CONFIG_IDF_TARGET_ESP32S3
        //default: (RX: 104, NPTX: 64, PTX: 32), fifo_size = 200
        .fifo_settings_custom = {
            .nptx_fifo_lines = 56,
            .ptx_fifo_lines = 72,
            .rx_fifo_lines = 72,
        }
#endif
    };
    esp_err_t ret = usb_host_install(&host_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "USB host install failed: %s", esp_err_to_name(ret));
        vTaskDelete(NULL);
        return;
    }

    ESP_LOGI(TAG, "USB Host installed");
    xTaskNotifyGive((TaskHandle_t)arg);

    while (1) {
        uint32_t event_flags = 0;
        ret = usb_host_lib_handle_events(portMAX_DELAY, &event_flags);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "USB host handle events failed: %s", esp_err_to_name(ret));
            continue;
        }
        if (event_flags & USB_HOST_LIB_EVENT_FLAGS_NO_CLIENTS) {
            ESP_LOGI(TAG, "No USB host clients, free all devices");
            usb_host_device_free_all();
        }
    }
}

esp_err_t app_usb_host_start(void)
{
    app_usb_board_init();

    TaskHandle_t task_handle = NULL;
    BaseType_t ok = xTaskCreatePinnedToCore(usb_host_task, "usb_host", 4096, xTaskGetCurrentTaskHandle(), 5, &task_handle, 0);
    if (ok != pdPASS) {
        ESP_LOGE(TAG, "Create USB host task failed");
        return ESP_ERR_NO_MEM;
    }
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    return ESP_OK;
}
