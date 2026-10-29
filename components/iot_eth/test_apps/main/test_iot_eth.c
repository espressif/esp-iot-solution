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
#include "test_main.h"
#include "esp_netif.h"
#include "iot_eth.h"
#include "iot_eth_types.h"
#include "iot_eth_netif_glue.h"

#define TAG "iot_eth_test"

typedef struct {
    iot_eth_driver_t base;
    iot_eth_mediator_t *mediator;
} net_ctx_t;

ssize_t TEST_MEMORY_LEAK_THRESHOLD = 0;
esp_netif_t *eth_netif;

TEST_CASE("iot eth glue memory leak", "[iot-eth][auto]")
{
    UPDATE_LEAK_THRESHOLD(-40);

    iot_eth_handle_t eth_handle = NULL;
    iot_eth_netif_glue_handle_t glue = iot_eth_new_netif_glue(eth_handle);
    TEST_ASSERT(glue != NULL);

    iot_eth_del_netif_glue(glue);
}

static esp_err_t drv_init(iot_eth_driver_t *driver)
{
    ESP_LOGI(TAG, "network interface init success");
    return ESP_OK;
}

static esp_err_t drv_set_mediator(iot_eth_driver_t *driver, iot_eth_mediator_t *mediator)
{
    net_ctx_t *net = __containerof(driver, net_ctx_t, base);
    net->mediator = mediator;
    ESP_LOGI(TAG, "network interface set mediator success");
    return ESP_OK;
}

static esp_err_t usbh_rndis_transmit(iot_eth_driver_t *h, uint8_t *buffer, size_t buflen)
{
    ESP_LOGI(TAG, "network interface transmit success");
    return ESP_OK;
}

static esp_err_t drv_get_addr(iot_eth_driver_t *driver, uint8_t *mac)
{
    ESP_LOGI(TAG, "network interface get mac success");
    return ESP_OK;
}

static esp_err_t drv_deinit(iot_eth_driver_t *driver)
{
    ESP_LOGI(TAG, "network interface deinit success");
    free(driver);
    return ESP_OK;
}

static iot_eth_driver_t * new_rndis_eth_driver()
{
    net_ctx_t *net = calloc(1, sizeof(net_ctx_t));
    TEST_ASSERT(net != NULL);
    net->base.name = "test drv";
    net->base.init = drv_init;
    net->base.deinit = drv_deinit;
    net->base.set_mediator = drv_set_mediator;
    net->base.get_addr = drv_get_addr;
    net->base.transmit = usbh_rndis_transmit;
    return &net->base;
}

static void _test_leak(esp_netif_t *eth_netif)
{
    iot_eth_driver_t *rndis_eth_driver = new_rndis_eth_driver();
    TEST_ASSERT(rndis_eth_driver != NULL);

    iot_eth_config_t eth_cfg = {
        .driver = rndis_eth_driver,
        .stack_input = NULL,
    };
    iot_eth_handle_t eth_handle = NULL;
    TEST_ESP_OK(iot_eth_install(&eth_cfg, &eth_handle));

    iot_eth_netif_glue_handle_t glue = iot_eth_new_netif_glue(eth_handle);
    TEST_ASSERT(glue != NULL);
    esp_netif_attach(eth_netif, glue);
    iot_eth_start(eth_handle);
    ESP_LOGI(TAG, "net started");

    net_ctx_t *net = __containerof(rndis_eth_driver, net_ctx_t, base);
    iot_eth_link_t link = IOT_ETH_LINK_UP;
    net->mediator->on_stage_changed(net->mediator, IOT_ETH_STAGE_LINK, &link);
    vTaskDelay(500 / portTICK_PERIOD_MS);
    link = IOT_ETH_LINK_DOWN;
    net->mediator->on_stage_changed(net->mediator, IOT_ETH_STAGE_LINK, &link);

    ESP_LOGI(TAG, "net stop");
    iot_eth_stop(eth_handle);
    iot_eth_del_netif_glue(glue);
    iot_eth_uninstall(eth_handle);
}

TEST_CASE("iot eth memory leak", "[iot-eth][auto]")
{
    UPDATE_LEAK_THRESHOLD(-40);
    _test_leak(eth_netif);
    vTaskDelay(500 / portTICK_PERIOD_MS);
    _test_leak(eth_netif);
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
    esp_reent_cleanup();    //clean up some of the newlib's lazy allocations
    size_t after_free_8bit = heap_caps_get_free_size(MALLOC_CAP_8BIT);
    size_t after_free_32bit = heap_caps_get_free_size(MALLOC_CAP_32BIT);
    check_leak(before_free_8bit, after_free_8bit, "8BIT");
    check_leak(before_free_32bit, after_free_32bit, "32BIT");
}

void app_main(void)
{
    printf("IOT ETH TEST \n");
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_config_t netif_cfg = ESP_NETIF_DEFAULT_ETH();
    eth_netif = esp_netif_new(&netif_cfg);
    TEST_ASSERT(eth_netif != NULL);
    unity_run_menu();
}
