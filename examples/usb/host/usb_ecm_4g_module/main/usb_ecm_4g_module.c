/*
 * SPDX-FileCopyrightText: 2024-2025 Espressif Systems (Shanghai) CO LTD
*
* SPDX-License-Identifier: Apache-2.0
*/
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_check.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "dhcpserver/dhcpserver_options.h"
#include "ping/ping_sock.h"
#include "iot_usbh_ecm.h"
#include "app_wifi.h"
#include "iot_eth.h"
#include "iot_eth_netif_glue.h"
#include "iot_usbh_cdc.h"

static const char *TAG = "ECM_4G_MODULE";

static void on_ping_success(esp_ping_handle_t hdl, void *args)
{
    uint8_t ttl;
    uint16_t seqno;
    uint32_t elapsed_time, recv_len;
    ip_addr_t target_addr;
    esp_ping_get_profile(hdl, ESP_PING_PROF_SEQNO, &seqno, sizeof(seqno));
    esp_ping_get_profile(hdl, ESP_PING_PROF_TTL, &ttl, sizeof(ttl));
    esp_ping_get_profile(hdl, ESP_PING_PROF_IPADDR, &target_addr, sizeof(target_addr));
    esp_ping_get_profile(hdl, ESP_PING_PROF_SIZE, &recv_len, sizeof(recv_len));
    esp_ping_get_profile(hdl, ESP_PING_PROF_TIMEGAP, &elapsed_time, sizeof(elapsed_time));
    ESP_LOGI(TAG, "%"PRIu32" bytes from %s icmp_seq=%u ttl=%u time=%"PRIu32" ms\n", recv_len, ipaddr_ntoa(&target_addr), seqno, ttl, elapsed_time);
}

static void on_ping_timeout(esp_ping_handle_t hdl, void *args)
{
    uint16_t seqno;
    ip_addr_t target_addr;
    esp_ping_get_profile(hdl, ESP_PING_PROF_SEQNO, &seqno, sizeof(seqno));
    esp_ping_get_profile(hdl, ESP_PING_PROF_IPADDR, &target_addr, sizeof(target_addr));
    ESP_LOGW(TAG, "From %s icmp_seq=%u timeout\n", ipaddr_ntoa(&target_addr), seqno);
    // Users can add logic to handle ping timeout
    // Add Wait or Reset logic
}

static void iot_event_handle(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    switch (event_id) {
    case IOT_ETH_EVENT_START:
        ESP_LOGI(TAG, "IOT_ETH_EVENT_START");
        break;
    case IOT_ETH_EVENT_STOP:
        ESP_LOGI(TAG, "IOT_ETH_EVENT_STOP");
        break;
    case IOT_ETH_EVENT_CONNECTED:
        ESP_LOGI(TAG, "IOT_ETH_EVENT_CONNECTED");
        break;
    case IOT_ETH_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "IOT_ETH_EVENT_DISCONNECTED");
        break;
    default:
        ESP_LOGI(TAG, "IOT_ETH_EVENT_UNKNOWN");
        break;
    }
}

#if CONFIG_USB_HOST_ENABLE_ENUM_FILTER_CALLBACK

static bool usb_host_enum_filter_cb(const usb_device_desc_t *dev_desc, uint8_t *bConfigurationValue)
{
    if (dev_desc->bNumConfigurations > 1) {
        *bConfigurationValue = 2;
    } else {
        *bConfigurationValue = 1;
    }
    ESP_LOGI(TAG, "USB device configuration value set to %d", *bConfigurationValue);
    // Return true to enumerate the USB device
    return true;
}

static void usb_lib_task(void *arg)
{
    // Install USB Host driver. Should only be called once in entire application
    const usb_host_config_t host_config = {
        .skip_phy_setup = false,
        .intr_flags = ESP_INTR_FLAG_LEVEL1,
        .enum_filter_cb = usb_host_enum_filter_cb,
    };
    ESP_ERROR_CHECK(usb_host_install(&host_config));

    //Signalize the usbh_cdc_driver_install, the USB host library has been installed
    xTaskNotifyGive(arg);

    bool has_clients = true;
    bool has_devices = false;
    while (has_clients) {
        uint32_t event_flags;
        ESP_ERROR_CHECK(usb_host_lib_handle_events(portMAX_DELAY, &event_flags));
        if (event_flags & USB_HOST_LIB_EVENT_FLAGS_NO_CLIENTS) {
            ESP_LOGI(TAG, "Get FLAGS_NO_CLIENTS");
            if (ESP_OK == usb_host_device_free_all()) {
                ESP_LOGI(TAG, "All devices marked as free, no need to wait FLAGS_ALL_FREE event");
                has_clients = false;
            } else {
                ESP_LOGI(TAG, "Wait for the FLAGS_ALL_FREE");
                has_devices = true;
            }
        }
        if (has_devices && event_flags & USB_HOST_LIB_EVENT_FLAGS_ALL_FREE) {
            ESP_LOGI(TAG, "Get FLAGS_ALL_FREE");
            has_clients = false;
        }
    }
    ESP_LOGI(TAG, "No more clients and devices, uninstall USB Host library");

    // Clean up USB Host
    vTaskDelay(100); // Short delay to allow clients clean-up
    usb_host_uninstall();
    ESP_LOGD(TAG, "USB Host library is uninstalled");
    vTaskDelete(NULL);
}
#endif

void app_main(void)
{
    /* Initialize default TCP/IP stack */
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    esp_event_handler_register(IOT_ETH_EVENT, ESP_EVENT_ANY_ID, iot_event_handle, NULL);

#if CONFIG_USB_HOST_ENABLE_ENUM_FILTER_CALLBACK
    BaseType_t task_created = xTaskCreatePinnedToCore(usb_lib_task, "usb_lib", 4096, xTaskGetCurrentTaskHandle(), 5, NULL, 0);
    ESP_RETURN_ON_FALSE(task_created == pdPASS,, TAG, "xTaskCreatePinnedToCore failed");
    // Wait unit the USB host library is installed
    uint32_t notify_value = ulTaskNotifyTake(false, pdMS_TO_TICKS(1000));
    if (notify_value == 0) {
        ESP_LOGE(TAG, "USB host library not installed");
        return;
    }
#endif

    // install usbh cdc driver
    usbh_cdc_driver_config_t config = {
        .task_stack_size = 1024 * 4,
        .task_priority = 5,
        .task_coreid = 0,
#if CONFIG_USB_HOST_ENABLE_ENUM_FILTER_CALLBACK
        .skip_init_usb_host_driver = true,
#else
        .skip_init_usb_host_driver = false,
#endif
    };
    ESP_ERROR_CHECK(usbh_cdc_driver_install(&config));

    iot_usbh_ecm_config_t ecm_cfg = {
        .auto_detect = true,
        .auto_detect_timeout = pdMS_TO_TICKS(1000),
    };

    iot_eth_driver_t *ecm_handle = NULL;
    esp_err_t ret = iot_eth_new_usb_ecm(&ecm_cfg, &ecm_handle);
    if (ret != ESP_OK || ecm_handle == NULL) {
        ESP_LOGE(TAG, "Failed to create USB ECM driver");
        return;
    }

    iot_eth_config_t eth_cfg = {
        .driver = ecm_handle,
        .stack_input = NULL,
        .user_data = NULL,
    };

    iot_eth_handle_t eth_handle = NULL;
    ret = iot_eth_install(&eth_cfg, &eth_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to install USB ECM driver");
        return;
    }

    esp_netif_config_t netif_cfg = ESP_NETIF_DEFAULT_ETH();
    esp_netif_t *eth_netif = esp_netif_new(&netif_cfg);

    iot_eth_netif_glue_handle_t glue = iot_eth_new_netif_glue(eth_handle);
    if (glue == NULL) {
        ESP_LOGE(TAG, "Failed to create netif glue");
        return;
    }
    esp_netif_attach(eth_netif, glue);

    while (1) {
        ret = iot_eth_start(eth_handle);
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "Failed to start USB ECM driver, try again...");
            vTaskDelay(1000 / portTICK_PERIOD_MS);
            continue;
        }
        break;
    }

    app_wifi_main();

    ip_addr_t target_addr;
    memset(&target_addr, 0, sizeof(target_addr));
    char *ping_addr_s = NULL;
    ping_addr_s = "8.8.8.8";
    esp_ping_config_t ping_config = ESP_PING_DEFAULT_CONFIG();
    ipaddr_aton(ping_addr_s, &target_addr);
    ping_config.target_addr = target_addr;
    ping_config.timeout_ms = 2000;
    ping_config.task_stack_size = 4096;
    ping_config.count = 1;

    /* set callback functions */
    esp_ping_callbacks_t cbs = {
        .on_ping_success = on_ping_success,
        .on_ping_timeout = on_ping_timeout,
        .on_ping_end = NULL,
        .cb_args = NULL,
    };
    esp_ping_handle_t ping;
    esp_ping_new_session(&ping_config, &cbs, &ping);

    while (1) {
        esp_ping_start(ping);
        vTaskDelay(5000 / portTICK_PERIOD_MS);
    }
}
