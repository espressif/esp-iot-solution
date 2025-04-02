/*
 * SPDX-FileCopyrightText: 2015-2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include "esp_system.h"
#include "esp_log.h"
#include "esp_err.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "esp_mac.h"
#include "lwip/dns.h"
#include "lwip/inet.h"
#include "ping/ping_sock.h"  // Add this header for ping functionality

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"

#include "usb/usb_host.h"
#include "usb/cdc_acm_host.h"
#include "usb_host_eth.h"  // Include our new header

#define EXAMPLE_USB_HOST_PRIORITY   (20)
#define USE_CH397A

#ifdef USE_CH397A
#ifndef CONFIG_USB_HOST_ENABLE_ENUM_FILTER_CALLBACK
#error "CONFIG_USB_HOST_ENABLE_ENUM_FILTER_CALLBACK must be enabled for CH397A"
#endif
#define ENABLE_ENUM_FILTER_CALLBACK 1
#define EXAMPLE_USB_DEVICE_VID      (0x1a86)
#define EXAMPLE_USB_DEVICE_PID      (0x5397) // 0x303A:0x4001 (TinyUSB CDC device)
#else
#define EXAMPLE_USB_DEVICE_VID      (0x303A)
#define EXAMPLE_USB_DEVICE_PID      (0x4001) // 0x303A:0x4001 (TinyUSB CDC device)
#endif

static const char *TAG = "USB-CDC";
static SemaphoreHandle_t device_disconnected_sem;

// Ping related variables
static TaskHandle_t s_ping_task_hdl = NULL;
static bool s_stop_ping_test = false;

#if ENABLE_ENUM_FILTER_CALLBACK
static bool set_config_cb(const usb_device_desc_t *dev_desc, uint8_t *bConfigurationValue)
{
    // If the USB device has more than one configuration, set the second configuration
    if (dev_desc->bNumConfigurations > 1) {
        *bConfigurationValue = 2;
    } else {
        *bConfigurationValue = 1;
    }
    printf("USB device configuration value set to %d\n", *bConfigurationValue);
    // Return true to enumerate the USB device
    return true;
}
#endif // ENABLE_ENUM_FILTER_CALLBACK

/**
 * @brief Ping callback function called when ping operation completes
 */
static void cmd_ping_on_ping_success(esp_ping_handle_t hdl, void *args)
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

    ESP_LOGI(TAG, "Ping success - %lu bytes from %s icmp_seq=%d ttl=%d time=%lu ms",
             recv_len, ipaddr_ntoa(&target_addr), seqno, ttl, elapsed_time);
}

/**
 * @brief Ping callback function called when ping operation fails
 */
static void cmd_ping_on_ping_timeout(esp_ping_handle_t hdl, void *args)
{
    uint16_t seqno;
    ip_addr_t target_addr;
    esp_ping_get_profile(hdl, ESP_PING_PROF_SEQNO, &seqno, sizeof(seqno));
    esp_ping_get_profile(hdl, ESP_PING_PROF_IPADDR, &target_addr, sizeof(target_addr));
    ESP_LOGW(TAG, "Ping timeout - From %s icmp_seq=%d", ipaddr_ntoa(&target_addr), seqno);
}

/**
 * @brief Ping callback function called when ping operation ends
 */
static void cmd_ping_on_ping_end(esp_ping_handle_t hdl, void *args)
{
    uint32_t transmitted;
    uint32_t received;
    uint32_t total_time_ms;

    esp_ping_get_profile(hdl, ESP_PING_PROF_REQUEST, &transmitted, sizeof(transmitted));
    esp_ping_get_profile(hdl, ESP_PING_PROF_REPLY, &received, sizeof(received));
    esp_ping_get_profile(hdl, ESP_PING_PROF_DURATION, &total_time_ms, sizeof(total_time_ms));

    ESP_LOGI(TAG, "Ping statistics: %"PRIu32" packets transmitted, %"PRIu32" received, %.1f%% packet loss, time %"PRIu32"ms",
             transmitted, received, (transmitted - received) * 100.0 / transmitted, total_time_ms);

    // Delete the ping session to free allocated resources
    esp_ping_delete_session(hdl);
}

/**
 * @brief Ping test task that periodically pings a known IP address
 */
static void ping_test_task(void *arg)
{
    esp_ping_config_t ping_config = ESP_PING_DEFAULT_CONFIG();

    // Set ping target to Google's DNS server
    ip_addr_t target_addr;
    ipaddr_aton("8.8.8.8", &target_addr);
    ping_config.target_addr = target_addr;

    // Set ping parameters
    ping_config.count = 0;  // Ping in infinite loop
    ping_config.interval_ms = 3000;  // 5 seconds between pings
    ping_config.timeout_ms = 2000;  // 1 second timeout
    ping_config.data_size = 64;  // 64 bytes of data

    esp_ping_callbacks_t cbs = {
        .on_ping_success = cmd_ping_on_ping_success,
        .on_ping_timeout = cmd_ping_on_ping_timeout,
        .on_ping_end = cmd_ping_on_ping_end,
        .cb_args = NULL
    };

    esp_ping_handle_t ping_handle = NULL;
    ESP_LOGI(TAG, "Starting continuous ping test to 8.8.8.8");

    // Create a ping session
    esp_ping_new_session(&ping_config, &cbs, &ping_handle);

    // Start ping
    esp_ping_start(ping_handle);

    // Wait until we're told to stop
    while (!s_stop_ping_test) {
        vTaskDelay(pdMS_TO_TICKS(100));

        // If device disconnected, break the loop
        if (!usb_host_eth_is_connected()) {
            ESP_LOGI(TAG, "Device disconnected, stopping ping test");
            break;
        }
    }

    // Cleanup
    if (ping_handle) {
        esp_ping_stop(ping_handle);
        esp_ping_delete_session(ping_handle);
    }

    // Reset flag and task handle before exit
    s_stop_ping_test = false;
    s_ping_task_hdl = NULL;
    vTaskDelete(NULL);
}

/**
 * @brief USB Host library handling task
 *
 * @param arg Unused
 */
static void usb_lib_task(void *arg)
{
    while (1) {
        // Start handling system events
        uint32_t event_flags;
        usb_host_lib_handle_events(portMAX_DELAY, &event_flags);
        if (event_flags & USB_HOST_LIB_EVENT_FLAGS_NO_CLIENTS) {
            ESP_ERROR_CHECK(usb_host_device_free_all());
        }
        if (event_flags & USB_HOST_LIB_EVENT_FLAGS_ALL_FREE) {
            ESP_LOGI(TAG, "USB: All devices freed");
            // Continue handling USB events to allow device reconnection
        }
    }
}

/**
 * @brief Main application
 *
 * Here we open a USB CDC ECM device and set up a network interface
 */
void app_main(void)
{
    device_disconnected_sem = xSemaphoreCreateBinary();
    assert(device_disconnected_sem);

    // Install USB Host driver. Should only be called once in entire application
    ESP_LOGI(TAG, "Installing USB Host");
    const usb_host_config_t host_config = {
        .skip_phy_setup = false,
        .intr_flags = ESP_INTR_FLAG_LEVEL1,
#if ENABLE_ENUM_FILTER_CALLBACK
        .enum_filter_cb = set_config_cb,
#endif
    };
    ESP_ERROR_CHECK(usb_host_install(&host_config));

    // Create a task that will handle USB library events
    BaseType_t task_created = xTaskCreate(usb_lib_task, "usb_lib", 4096, xTaskGetCurrentTaskHandle(), EXAMPLE_USB_HOST_PRIORITY, NULL);
    assert(task_created == pdTRUE);

    ESP_LOGI(TAG, "Installing CDC-ACM driver");
    ESP_ERROR_CHECK(cdc_acm_host_install(NULL));

    // Initialize USB ECM network interface
    usb_host_eth_config_t eth_config = USB_HOST_ETH_DEFAULT_CONFIG();
    eth_config.vid = EXAMPLE_USB_DEVICE_VID;
    eth_config.pid = EXAMPLE_USB_DEVICE_PID;
    ESP_ERROR_CHECK(usb_host_eth_init(&eth_config));

    while (true) {
        // Connect to USB ECM device
        ESP_LOGI(TAG, "Trying to connect to USB ECM device");
        esp_err_t err = usb_host_eth_connect(device_disconnected_sem);

        if (err == ESP_ERR_NOT_FOUND) {
            ESP_LOGI(TAG, "Device not found, waiting for connection...");
            vTaskDelay(pdMS_TO_TICKS(1000));
            continue;
        } else if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to connect to device: %s", esp_err_to_name(err));
            vTaskDelay(pdMS_TO_TICKS(1000));
            continue;
        }

        // Start ping test after successful connection
        ESP_LOGI(TAG, "Starting ping test task");
        s_stop_ping_test = false;
        BaseType_t ping_task_created = xTaskCreate(
                                           ping_test_task,                  // Task function
                                           "ping_test",                     // Task name
                                           4096,                            // Stack size (bytes)
                                           NULL,                            // Task parameters
                                           EXAMPLE_USB_HOST_PRIORITY - 2,   // Task priority
                                           &s_ping_task_hdl                 // Task handle
                                       );

        if (ping_task_created != pdTRUE) {
            ESP_LOGE(TAG, "Failed to create ping test task");
            // Continue with the device connection even if ping task fails
        } else {
            ESP_LOGI(TAG, "Ping test task started successfully");
        }

        // Block until device is disconnected
        xSemaphoreTake(device_disconnected_sem, portMAX_DELAY);
        ESP_LOGI(TAG, "Device disconnected, cleaning up");

        // Stop the ping task if it's running
        if (s_ping_task_hdl != NULL) {
            ESP_LOGI(TAG, "Stopping ping test task");
            s_stop_ping_test = true;
            // Give some time for the task to clean up before restarting the loop
            vTaskDelay(pdMS_TO_TICKS(500));
        }
    }
}
