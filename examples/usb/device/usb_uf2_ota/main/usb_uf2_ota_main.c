/* SPDX-FileCopyrightText: 2022-2023 Espressif Systems (Shanghai) CO LTD
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

static const char *TAG = "uf2_ota_example";

static void uf2_update_complete_cb()
{
    TaskHandle_t main_task_hdl = xTaskGetHandle("main");
    xTaskNotifyGive(main_task_hdl);
}

void app_main(void)
{
    /* install UF2 OTA */
    tinyuf2_ota_config_t config = DEFAULT_TINYUF2_OTA_CONFIG();
    config.complete_cb = uf2_update_complete_cb;
    /* disable auto restart, manual restart later */
    config.if_restart = false;

    esp_tinyuf2_install(&config, NULL);

    /* Waiting for UF2 ota completed */
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    ESP_LOGI(TAG, "Firmware update complete");

    for (int i = 5; i >= 0; i--) {
        ESP_LOGI(TAG, "Restarting in %d seconds...", i);
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }

    ESP_LOGI(TAG, "Restarting now");
    /* restart to new app */
    esp_restart();
}
