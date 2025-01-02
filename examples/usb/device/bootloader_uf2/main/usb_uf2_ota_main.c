/* SPDX-FileCopyrightText: 2022-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_tinyuf2.h"
#include "nvs.h"
#include "nvs_flash.h"
#if CONFIG_BOOTLOADER_UF2_LED_INDICATOR_ENABLE
#include "driver/gpio.h"
#endif

static const char *TAG = "uf2_ota_example";

#if CONFIG_BOOTLOADER_UF2_LED_INDICATOR_ENABLE
static void led_task(void *args)
{
#define LED CONFIG_BOOTLOADER_UF2_LED_INDICATOR_GPIO_NUM
    gpio_set_direction(LED, GPIO_MODE_OUTPUT);
    while (1) {
        gpio_set_level(LED, 1);
        vTaskDelay(1000 / portTICK_PERIOD_MS);
        gpio_set_level(LED, 0);
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}
#endif

static void uf2_update_complete_cb()
{
    TaskHandle_t main_task_hdl = xTaskGetHandle("main");
    xTaskNotifyGive(main_task_hdl);
}

void app_main(void)
{
    /* install UF2 OTA */
    ESP_LOGI(TAG, "Entering UF2 Bootloader mode");

    // Initialize NVS
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        // NVS partition was truncated and needs to be erased
        // Retry nvs_flash_init
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);

#if CONFIG_BOOTLOADER_UF2_LED_INDICATOR_ENABLE
    xTaskCreate(led_task, "led_task", 2048, NULL, 5, NULL);
#endif

    tinyuf2_ota_config_t config = DEFAULT_TINYUF2_OTA_CONFIG();
    config.subtype = ESP_PARTITION_SUBTYPE_APP_FACTORY;
    config.complete_cb = uf2_update_complete_cb;
    /* disable auto restart, manual restart later */
    config.if_restart = false;

    tinyuf2_nvs_config_t nvs_config = DEFAULT_TINYUF2_NVS_CONFIG();
    nvs_config.part_name = CONFIG_BOOTLOADER_UF2_NVS_PART_NAME;
    nvs_config.namespace_name = CONFIG_BOOTLOADER_UF2_NVS_NAMESPACE_NAME;

    esp_tinyuf2_install(&config, &nvs_config);

    /* Waiting for UF2 ota completed */
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    ESP_LOGI(TAG, "Firmware update complete");

    // uninstall UF2 OTA, restore to normal USB device
    esp_tinyuf2_uninstall();
    ESP_LOGI(TAG, "Restarting now");
    /* restart to new app */
    esp_restart();
}
