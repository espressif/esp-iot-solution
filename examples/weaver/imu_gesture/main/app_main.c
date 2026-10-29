/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_log.h>
#include <esp_event.h>
#include <nvs_flash.h>

#include <esp_weaver.h>
#include "protocol_examples_common.h"
#include "app_driver.h"
#include "app_imu_gesture.h"

static const char *TAG = "app_main";

static void app_imu_gesture_init(esp_weaver_node_t *node)
{
    esp_weaver_device_t *imu_device = app_imu_device_create();
    if (!imu_device) {
        ESP_LOGE(TAG, "Could not create IMU gesture device, aborting");
        vTaskDelay(5000 / portTICK_PERIOD_MS);
        abort();
    }

    esp_err_t err = esp_weaver_node_add_device(node, imu_device);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to add IMU gesture device to node: %s, aborting", esp_err_to_name(err));
        vTaskDelay(5000 / portTICK_PERIOD_MS);
        abort();
    }
    ESP_LOGI(TAG, "IMU gesture device created and added to node");
}

void app_main(void)
{
    /* Initialize NVS */
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);

    /* Initialize hardware driver */
    app_driver_init();

    /* Initialize the Weaver Model Node */
    esp_weaver_config_t weaver_config = ESP_WEAVER_CONFIG_DEFAULT();
    esp_weaver_node_t *node = esp_weaver_node_init(&weaver_config, CONFIG_EXAMPLE_NODE_NAME, CONFIG_EXAMPLE_NODE_TYPE);
    if (!node) {
        ESP_LOGE(TAG, "Could not initialize weaver node, aborting");
        vTaskDelay(5000 / portTICK_PERIOD_MS);
        abort();
    }
    ESP_LOGI(TAG, "Node ID: %s", esp_weaver_get_node_id());

    app_imu_gesture_init(node);

    /* Initialize TCP/IP stack, event loop, and connect to Wi-Fi */
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    ESP_ERROR_CHECK(example_connect());

    /* Start local control (HTTP server + mDNS) with Kconfig-configured security */
    err = esp_weaver_local_ctrl_start();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start local control: %s", esp_err_to_name(err));
    } else {
        ESP_LOGI(TAG, "Local control started successfully");
        esp_weaver_local_ctrl_set_txt("device_name", CONFIG_EXAMPLE_DEVICE_NAME);
    }
}
