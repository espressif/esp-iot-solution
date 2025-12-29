/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "unity.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "iot_eth.h"
#include "iot_eth_netif_glue.h"
#include "iot_usbh_rndis.h"

#define TAG "rndis_test"

ssize_t TEST_MEMORY_LEAK_THRESHOLD = 0;

#define UPDATE_LEAK_THRESHOLD(first_val) \
static bool is_first = true; \
if (is_first) { \
    is_first = false; \
    TEST_MEMORY_LEAK_THRESHOLD = first_val; \
} else { \
    TEST_MEMORY_LEAK_THRESHOLD = 0; \
}

static void install()
{
    usbh_cdc_driver_config_t config = {
        .task_stack_size = 1024 * 4,
        .task_priority = 5,
        .task_coreid = 0,
        .skip_init_usb_host_driver = false,
    };

    TEST_ASSERT_EQUAL(ESP_OK, usbh_cdc_driver_install(&config));
}

static void uninstall()
{
    TEST_ASSERT_EQUAL(ESP_OK, usbh_cdc_driver_uninstall());
    vTaskDelay(2000 / portTICK_PERIOD_MS);
}

TEST_CASE("usbh rndis inini device memory leak", "[iot_usbh_rndis][read-write][auto]")
{
    install();
    iot_eth_driver_t *rndis_eth_driver = NULL;
    static const usb_device_match_id_t dev_match_id[] = {
        {
            .match_flags = USB_DEVICE_ID_MATCH_VENDOR | USB_DEVICE_ID_MATCH_PRODUCT,
            .idVendor = USB_DEVICE_VENDOR_ANY,
            .idProduct = USB_DEVICE_PRODUCT_ANY,
        },
        {0},  // Null-terminated
    };
    iot_usbh_rndis_config_t rndis_cfg = {
        .match_id_list = dev_match_id,
    };
    TEST_ESP_OK(iot_eth_new_usb_rndis(&rndis_cfg, &rndis_eth_driver));

    rndis_eth_driver->init(rndis_eth_driver);

    vTaskDelay(10000 / portTICK_PERIOD_MS);
    rndis_eth_driver->deinit(rndis_eth_driver);

    uninstall();
}

TEST_CASE("usbh rndis device memory leak", "[iot_usbh_rndis][read-write][auto]")
{
    UPDATE_LEAK_THRESHOLD(-40);

    iot_eth_driver_t *rndis_eth_driver = NULL;
    // install usbh cdc driver
    install();

    static const usb_device_match_id_t dev_match_id[] = {
        {
            .match_flags = USB_DEVICE_ID_MATCH_VENDOR | USB_DEVICE_ID_MATCH_PRODUCT,
            .idVendor = USB_DEVICE_VENDOR_ANY,
            .idProduct = USB_DEVICE_PRODUCT_ANY,
        },
        {0},  // Null-terminated
    };
    iot_usbh_rndis_config_t rndis_cfg = {
        .match_id_list = dev_match_id,
    };
    TEST_ESP_OK(iot_eth_new_usb_rndis(&rndis_cfg, &rndis_eth_driver));

    iot_eth_config_t eth_cfg = {
        .driver = rndis_eth_driver,
        .stack_input = NULL,
    };
    iot_eth_handle_t eth_handle = NULL;
    TEST_ESP_OK(iot_eth_install(&eth_cfg, &eth_handle));

    esp_netif_config_t netif_cfg = ESP_NETIF_DEFAULT_ETH();
    esp_netif_t *eth_netif = esp_netif_new(&netif_cfg);
    TEST_ASSERT(eth_netif != NULL);
    iot_eth_netif_glue_handle_t glue = iot_eth_new_netif_glue(eth_handle);
    TEST_ASSERT(glue != NULL);
    esp_netif_attach(eth_netif, glue);
    iot_eth_start(eth_handle);
    ESP_LOGI(TAG, "rndis device started");

    vTaskDelay(10000 / portTICK_PERIOD_MS);

    ESP_LOGI(TAG, "rndis device stop");
    iot_eth_stop(eth_handle);
    iot_eth_del_netif_glue(glue);
    iot_eth_uninstall(eth_handle);
    esp_netif_destroy(eth_netif);
    uninstall();
}

static size_t before_free_8bit;
static size_t before_free_32bit;

static void check_leak(size_t before_free, size_t after_free, const char *type)
{
    ssize_t delta = after_free - before_free;
    printf("MALLOC_CAP_%s: Before: %u bytes free, After: %u bytes free (delta:%d)\n", type, before_free, after_free, delta);
    if (!(delta >= TEST_MEMORY_LEAK_THRESHOLD)) {
        ESP_LOGE(TAG, "Memory leak detected, delta: %d bytes, threshold: %d bytes", delta, TEST_MEMORY_LEAK_THRESHOLD);
    }
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
    printf("IOT USBH RNDIS TEST \n");
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    unity_run_menu();
}
