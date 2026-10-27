/*
 * SPDX-FileCopyrightText: 2025-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "unity.h"
#include "unity_test_utils_memory.h"
#include "esp_check.h"
#include "esp_intr_alloc.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "ping/ping_sock.h"
#include "usb/usb_host.h"
#include "iot_eth.h"
#include "iot_eth_netif_glue.h"
#include "iot_usbh_rndis.h"

#define TAG "rndis_test"

// Maximum allowed loss in bytes, as expected by Unity's memory helpers.
size_t TEST_MEMORY_LEAK_THRESHOLD = 0;

#define UPDATE_LEAK_THRESHOLD(first_val, repeat_val) \
static bool is_first = true; \
if (is_first) { \
    is_first = false; \
    TEST_MEMORY_LEAK_THRESHOLD = first_val; \
} else { \
    TEST_MEMORY_LEAK_THRESHOLD = repeat_val; \
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
    UPDATE_LEAK_THRESHOLD(40, 0);

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

enum {
    TEST_LINK_UP = BIT0,
    TEST_LINK_DOWN = BIT1,
    TEST_GOT_IP = BIT2,
    TEST_ETH_STOP = BIT3,
    TEST_PING_DONE = BIT4,
};

typedef struct {
    EventGroupHandle_t events;
    iot_eth_handle_t eth;
    esp_netif_t *netif;
} reconnect_test_t;

static void reconnect_event_handler(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    reconnect_test_t *test = arg;
    if (base == IOT_ETH_EVENT && *(iot_eth_handle_t *)data == test->eth) {
        if (id == IOT_ETH_EVENT_CONNECTED) {
            xEventGroupSetBits(test->events, TEST_LINK_UP);
        } else if (id == IOT_ETH_EVENT_DISCONNECTED) {
            xEventGroupClearBits(test->events, TEST_LINK_UP | TEST_GOT_IP);
            xEventGroupSetBits(test->events, TEST_LINK_DOWN);
        } else if (id == IOT_ETH_EVENT_STOP) {
            xEventGroupSetBits(test->events, TEST_ETH_STOP);
        }
    } else if (base == IP_EVENT && ((ip_event_got_ip_t *)data)->esp_netif == test->netif) {
        xEventGroupSetBits(test->events, TEST_GOT_IP);
    }
}

static esp_err_t wait_for_reconnect_event(EventGroupHandle_t events, EventBits_t expected, uint32_t timeout_ms)
{
    EventBits_t bits = xEventGroupWaitBits(events, expected, pdTRUE, pdTRUE, pdMS_TO_TICKS(timeout_ms));
    ESP_RETURN_ON_FALSE((bits & expected) == expected, ESP_ERR_TIMEOUT, TAG, "Timeout waiting for events 0x%lx (got 0x%lx)", (unsigned long)expected, (unsigned long)bits);
    return ESP_OK;
}

static void reconnect_ping_end(esp_ping_handle_t ping, void *arg)
{
    xEventGroupSetBits((EventGroupHandle_t)arg, TEST_PING_DONE);
}

static esp_err_t ping_gateway(reconnect_test_t *test)
{
    esp_netif_ip_info_t ip;
    ESP_RETURN_ON_ERROR(esp_netif_get_ip_info(test->netif, &ip), TAG, "Get IP failed");
    ESP_RETURN_ON_FALSE(ip.ip.addr != 0 && ip.gw.addr != 0, ESP_ERR_INVALID_STATE, TAG, "Missing IP or gateway");
    esp_ping_config_t config = ESP_PING_DEFAULT_CONFIG();
    config.count = 3;
    config.interval_ms = 200;
    config.timeout_ms = 1000;
    config.interface = esp_netif_get_netif_impl_index(test->netif);
    IP_ADDR4(&config.target_addr, esp_ip4_addr1(&ip.gw), esp_ip4_addr2(&ip.gw), esp_ip4_addr3(&ip.gw), esp_ip4_addr4(&ip.gw));
    esp_ping_callbacks_t callbacks = {.on_ping_end = reconnect_ping_end, .cb_args = test->events};
    esp_ping_handle_t ping = NULL;
    ESP_RETURN_ON_ERROR(esp_ping_new_session(&config, &callbacks, &ping), TAG, "Create ping failed");
    xEventGroupClearBits(test->events, TEST_PING_DONE);
    esp_err_t ret = esp_ping_start(ping);
    if (ret == ESP_OK) {
        ret = wait_for_reconnect_event(test->events, TEST_PING_DONE, 10000);
        if (ret != ESP_OK) {
            // Join the callback before releasing its event group.
            esp_ping_stop(ping);
            xEventGroupWaitBits(test->events, TEST_PING_DONE, pdTRUE, pdTRUE, portMAX_DELAY);
        }
    }
    uint32_t replies = 0;
    esp_ping_get_profile(ping, ESP_PING_PROF_REPLY, &replies, sizeof(replies));
    esp_ping_delete_session(ping);
    ESP_LOGI(TAG, "Gateway " IPSTR ": %lu/3 ping replies", IP2STR(&ip.gw), (unsigned long)replies);
    return ret == ESP_OK && replies == 0 ? ESP_FAIL : ret;
}

TEST_CASE("usbh rndis reconnects after root port power cycles", "[iot_usbh_rndis][root_port_power][auto]")
{
    // Allow first-use allocations for interrupts, lwIP TLS and timekeeping locks.
    // Subsequent runs allow the 20-byte IP-lost timeout node until expiry.
    UPDATE_LEAK_THRESHOLD(400, 20);

    const usb_host_config_t host_config = {
        .root_port_unpowered = true,
        .intr_flags = ESP_INTR_FLAG_LEVEL1,
    };
    TEST_ESP_OK(usb_host_install(&host_config));
    install();

    reconnect_test_t test = {.events = xEventGroupCreate()};
    TEST_ASSERT_NOT_NULL(test.events);
    static const usb_device_match_id_t dev_match_id[] = {
        {
            .match_flags = USB_DEVICE_ID_MATCH_VENDOR | USB_DEVICE_ID_MATCH_PRODUCT,
            .idVendor = USB_DEVICE_VENDOR_ANY,
            .idProduct = USB_DEVICE_PRODUCT_ANY,
        },
        {0}, // Null-terminated.
    };
    iot_usbh_rndis_config_t rndis_cfg = {.match_id_list = dev_match_id};
    iot_eth_driver_t *driver = NULL;
    TEST_ESP_OK(iot_eth_new_usb_rndis(&rndis_cfg, &driver));
    iot_eth_config_t eth_cfg = {.driver = driver};
    TEST_ESP_OK(iot_eth_install(&eth_cfg, &test.eth));
    esp_netif_config_t netif_cfg = ESP_NETIF_DEFAULT_ETH();
    test.netif = esp_netif_new(&netif_cfg);
    TEST_ASSERT_NOT_NULL(test.netif);
    iot_eth_netif_glue_handle_t glue = iot_eth_new_netif_glue(test.eth);
    TEST_ASSERT_NOT_NULL(glue);
    TEST_ESP_OK(esp_netif_attach(test.netif, glue));

    esp_err_t ret = ESP_OK;
    bool started = false;
    bool powered = false;
    esp_event_handler_instance_t eth_handler = NULL;
    esp_event_handler_instance_t ip_handler = NULL;
    ESP_GOTO_ON_ERROR(esp_event_handler_instance_register(IOT_ETH_EVENT, ESP_EVENT_ANY_ID, reconnect_event_handler, &test, &eth_handler), cleanup, TAG, "Register ETH events failed");
    ESP_GOTO_ON_ERROR(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_ETH_GOT_IP, reconnect_event_handler, &test, &ip_handler), cleanup, TAG, "Register IP events failed");
    ESP_GOTO_ON_ERROR(iot_eth_start(test.eth), cleanup, TAG, "Start Ethernet failed");
    started = true;
    ESP_GOTO_ON_ERROR(usb_host_lib_set_root_port_power(true), cleanup, TAG, "Initial root port power on failed");
    powered = true;
    ESP_GOTO_ON_ERROR(wait_for_reconnect_event(test.events, TEST_LINK_UP | TEST_GOT_IP, 60000), cleanup, TAG, "Initial connection failed");
    ESP_GOTO_ON_ERROR(ping_gateway(&test), cleanup, TAG, "Initial ping failed");

    for (int cycle = 1; cycle <= 5; cycle++) {
        ESP_LOGI(TAG, "Reconnect cycle %d/5: root port off", cycle);
        xEventGroupClearBits(test.events, TEST_LINK_DOWN);
        ESP_GOTO_ON_ERROR(usb_host_lib_set_root_port_power(false), cleanup, TAG, "Root port power off failed");
        powered = false;
        ESP_GOTO_ON_ERROR(wait_for_reconnect_event(test.events, TEST_LINK_DOWN, 10000), cleanup, TAG, "Disconnect not reported");
        vTaskDelay(pdMS_TO_TICKS(3000));

        // Require fresh link and DHCP events after every reconnection.
        xEventGroupClearBits(test.events, TEST_LINK_UP | TEST_GOT_IP);
        ESP_LOGI(TAG, "Reconnect cycle %d/5: root port on", cycle);
        ESP_GOTO_ON_ERROR(usb_host_lib_set_root_port_power(true), cleanup, TAG, "Root port power on failed");
        powered = true;
        ESP_GOTO_ON_ERROR(wait_for_reconnect_event(test.events, TEST_LINK_UP | TEST_GOT_IP, 60000), cleanup, TAG, "Reconnect failed");
        ESP_GOTO_ON_ERROR(ping_gateway(&test), cleanup, TAG, "Ping after reconnect failed");
        ESP_LOGI(TAG, "Reconnect cycle %d/5 passed", cycle);
    }

cleanup:
    // Disconnect USB before freeing the driver referenced by CDC callbacks.
    if (powered) {
        ESP_ERROR_CHECK(usb_host_lib_set_root_port_power(false));
    }
    usb_host_lib_info_t info = {0};
    for (int retry = 0; retry < 100; retry++) {
        ESP_ERROR_CHECK(usb_host_lib_info(&info));
        if (info.num_devices == 0) {
            break;
        }
        vTaskDelay(pdMS_TO_TICKS(50));
    }
    ESP_ERROR_CHECK(info.num_devices == 0 ? ESP_OK : ESP_ERR_TIMEOUT);
    if (started) {
        ESP_ERROR_CHECK(iot_eth_stop(test.eth));
        ESP_ERROR_CHECK(wait_for_reconnect_event(test.events, TEST_ETH_STOP, 10000));
    }
    // Unregister callbacks before their stack context and event group disappear.
    if (ip_handler) {
        ESP_ERROR_CHECK(esp_event_handler_instance_unregister(IP_EVENT, IP_EVENT_ETH_GOT_IP, ip_handler));
    }
    if (eth_handler) {
        ESP_ERROR_CHECK(esp_event_handler_instance_unregister(IOT_ETH_EVENT, ESP_EVENT_ANY_ID, eth_handler));
    }
    ESP_ERROR_CHECK(iot_eth_del_netif_glue(glue));
    ESP_ERROR_CHECK(iot_eth_uninstall(test.eth));
    esp_netif_destroy(test.netif);
    vEventGroupDelete(test.events);
    uninstall();
    TEST_ESP_OK(ret);
}

void setUp(void)
{
    TEST_MEMORY_LEAK_THRESHOLD = 0;
    unity_utils_record_free_mem();
}

void tearDown(void)
{
    unity_utils_evaluate_leaks_direct(TEST_MEMORY_LEAK_THRESHOLD);
}

void app_main(void)
{
    printf("IOT USBH RNDIS TEST \n");
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    unity_run_menu();
}
