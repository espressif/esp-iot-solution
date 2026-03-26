/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include "unity.h"
#include "unity_test_runner.h"
#include "unity_test_utils_memory.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "nvs_flash.h"


#define LEAKS (1024)

void setUp(void)
{
    unity_utils_record_free_mem();
}

void tearDown(void)
{
    unity_utils_evaluate_leaks_direct(LEAKS);
}

void app_main(void)
{
    /*
     *  ____    _    _     ___   _____          _
     * |  _ \  / \  | |   |_ _| |_   _|___  ___| |_
     * | | | |/ _ \ | |    | |    | |/ _ \/ __| __|
     * | |_| / ___ \| |___ | |    | |  __/\__ \ |_
     * |____/_/   \_\_____|___|   |_|\___||___/\__|
     */
    printf(" ____    _    _     ___   _____          _   \n");
    printf("|  _ \\  / \\  | |   |_ _| |_   _|___  ___| |_ \n");
    printf("| | | |/ _ \\ | |    | |    | |/ _ \\/ __| __|\n");
    printf("| |_| / ___ \\| |___ | |    | |  __/\\__ \\ |_ \n");
    printf("|____/_/   \\_\\_____|___|   |_|\\___||___/\\__|\n");

    esp_err_t nvs_err = nvs_flash_init();
    if (nvs_err == ESP_ERR_NVS_NO_FREE_PAGES ||
        nvs_err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        nvs_err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(nvs_err);

    ESP_ERROR_CHECK(esp_netif_init());

    wifi_init_config_t wifi_cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&wifi_cfg));

    unity_run_menu();
}
