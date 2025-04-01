/*
 * SPDX-FileCopyrightText: 2023-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "unity.h"
#include "iot_usbh_cdc.h"
#include "usbh_modem_board.h"
#include "esp_log.h"

#define TEST_MEMORY_LEAK_THRESHOLD (-20000)

TEST_CASE("usbh modem memory leak", "[iot_usbh_modem][read-write][auto]")
{
    /* Initialize default TCP/IP stack */
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    modem_config_t modem_config = MODEM_DEFAULT_CONFIG();
    /* if Not enter ppp, modem will enter command mode after init */
    modem_config.flags |= MODEM_FLAGS_INIT_NOT_ENTER_PPP;
    /* if Not waiting for modem ready, just return after modem init */
    modem_config.flags |= MODEM_FLAGS_INIT_NOT_BLOCK;
    modem_board_init(&modem_config);
    vTaskDelay(1000 / portTICK_PERIOD_MS);
    modem_board_deinit();
    esp_netif_deinit();
    esp_event_loop_delete_default();
    vTaskDelay(1000 / portTICK_PERIOD_MS);
}

static size_t before_free_8bit;
static size_t before_free_32bit;

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
    printf("IOT USBH MODEM TEST \n");
    unity_run_menu();
}
