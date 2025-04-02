/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include <stdlib.h>
#include <string.h>
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_mac.h"
#include "esp_event.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "usb/cdc_acm_host.h"
#include "usb_host_eth.h"

static const char *TAG = "USB-ETH";

#define USB_ECM_TX_QUEUE_SIZE 10
#define USB_ECM_TX_TASK_STACK_SIZE 3072
#define USB_ECM_TASK_PRIORITY 20

// Structure for TX packets
typedef struct {
    void *buffer;
    uint32_t length;
} usb_ecm_tx_packet_t;

// Static variables
static esp_netif_t *s_eth_netif = NULL;
static cdc_acm_dev_hdl_t s_cdc_dev = NULL;
static QueueHandle_t s_tx_queue = NULL;
static TaskHandle_t s_tx_task_hdl = NULL;
static bool s_network_started = false;
static bool s_got_ip = false;
static SemaphoreHandle_t s_disconnected_sem = NULL;
static usb_host_eth_config_t s_config = USB_HOST_ETH_DEFAULT_CONFIG();

/**
 * @brief Free function for received network packets
 *
 * This function is called by the TCP/IP stack when it's done with a received buffer
 *
 * @param[in] h        Netif handle
 * @param[in] buffer   Buffer to free
 */
static void usb_ecm_eth_free_rx_buffer(void *h, void *buffer)
{
    if (buffer) {
        free(buffer);
    }
}

/**
 * @brief Data received callback
 *
 * @param[in] data     Pointer to received data
 * @param[in] data_len Length of received data in bytes
 * @param[in] arg      Argument we passed to the device open function
 * @return
 *   true:  We have processed the received data
 *   false: We expect more data
 */
static bool usb_ecm_data_received_cb(const uint8_t *data, size_t data_len, void *arg)
{
    ESP_LOGD(TAG, "Data received, len=%d", data_len);

    if (s_eth_netif) {
        // Allocate a new buffer that lwIP can safely free
        void *buffer = malloc(data_len);
        if (buffer == NULL) {
            ESP_LOGE(TAG, "Failed to allocate memory for RX packet");
            return true;
        }

        // Copy data to the new buffer
        memcpy(buffer, data, data_len);

        // Forward received packet to ESP-NETIF
        esp_err_t ret = esp_netif_receive(s_eth_netif, buffer, data_len, NULL);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to receive packet to TCP/IP stack: %s", esp_err_to_name(ret));
            free(buffer); // Free the buffer if esp_netif_receive fails
        }
    }

    return true;
}

/**
 * @brief Device event callback
 *
 * Handles device events and network connection status
 *
 * @param[in] event    Device event type and data
 * @param[in] user_ctx Argument we passed to the device open function
 */
static void usb_ecm_event_cb(const cdc_acm_host_dev_event_data_t *event, void *user_ctx)
{
    switch (event->type) {
    case CDC_ACM_HOST_ERROR:
        ESP_LOGE(TAG, "CDC-ACM error has occurred, err_no = %i", event->data.error);
        break;
    case CDC_ACM_HOST_DEVICE_DISCONNECTED:
        ESP_LOGI(TAG, "Device suddenly disconnected");
        if (s_cdc_dev) {
            s_cdc_dev = NULL;
            if (s_eth_netif) {
                // Only call stop if currently started
                if (s_network_started) {
                    esp_netif_action_disconnected(s_eth_netif, NULL, 0, NULL);
                    esp_netif_action_stop(s_eth_netif, NULL, 0, NULL);
                    s_network_started = false;
                    s_got_ip = false;
                }
            }
            ESP_ERROR_CHECK(cdc_acm_host_close(event->data.cdc_hdl));

            // Notify the application about disconnection
            if (s_disconnected_sem) {
                xSemaphoreGive(s_disconnected_sem);
            }
        }
        break;
    case CDC_ACM_HOST_NETWORK_CONNECTION:
        if (s_eth_netif) {
            // Update link state based on network connection status
            // Only call start/stop actions if state has changed
            if (event->data.network_connected && !s_network_started) {
                esp_netif_action_start(s_eth_netif, NULL, 0, NULL);
                esp_netif_action_connected(s_eth_netif, NULL, 0, NULL);
                ESP_LOGI(TAG, "Network %s", event->data.network_connected ? "connected" : "disconnected");
                s_network_started = true;

                // Call user connection callback if set
                if (s_config.on_connection_cb) {
                    s_config.on_connection_cb(true);
                }
            } else if (!event->data.network_connected && s_network_started) {
                esp_netif_action_disconnected(s_eth_netif, NULL, 0, NULL);
                esp_netif_action_stop(s_eth_netif, NULL, 0, NULL);
                s_network_started = false;
                s_got_ip = false;
                ESP_LOGI(TAG, "Network %s", event->data.network_connected ? "connected" : "disconnected");

                // Call user connection callback if set
                if (s_config.on_connection_cb) {
                    s_config.on_connection_cb(false);
                }
            }
        }
        break;
    default:
        ESP_LOGW(TAG, "Unsupported CDC event: %i", event->type);
        break;
    }
}

// USB ECM transmit task
static void usb_ecm_tx_task(void *arg)
{
    usb_ecm_tx_packet_t tx_packet;

    while (1) {
        if (xQueueReceive(s_tx_queue, &tx_packet, portMAX_DELAY) == pdTRUE) {
            if (!s_cdc_dev) {
                // Device disconnected, free buffer and continue
                free(tx_packet.buffer);
                continue;
            }

            // Send packet through USB ECM interface
            ESP_LOGD(TAG, "Transmitting packet, len=%lu", tx_packet.length);
            esp_err_t ret = cdc_acm_host_data_tx_blocking(s_cdc_dev, tx_packet.buffer,
                                                          tx_packet.length, s_config.connection_timeout_ms);
            if (ret != ESP_OK) {
                ESP_LOGE(TAG, "Failed to transmit packet: %s", esp_err_to_name(ret));
            }

            // Free the buffer after transmitting
            free(tx_packet.buffer);
        }
    }
}

// USB ECM Ethernet driver transmit function
static esp_err_t usb_ecm_eth_transmit(void *h, void *buffer, size_t length)
{
    if (!s_cdc_dev) {
        ESP_LOGW(TAG, "USB ECM device not connected");
        return ESP_ERR_INVALID_STATE;
    }

    // Allocate buffer for the packet data (will be freed after transmission)
    void *tx_buffer = malloc(length);
    if (!tx_buffer) {
        ESP_LOGE(TAG, "No memory for tx buffer");
        return ESP_ERR_NO_MEM;
    }

    // Copy data to the transmission buffer
    memcpy(tx_buffer, buffer, length);

    // Create packet structure
    usb_ecm_tx_packet_t tx_packet = {
        .buffer = tx_buffer,
        .length = length
    };

    // Send packet to transmit queue
    if (xQueueSend(s_tx_queue, &tx_packet, pdMS_TO_TICKS(100)) != pdTRUE) {
        ESP_LOGE(TAG, "TX queue full, dropping packet");
        free(tx_buffer);
        return ESP_ERR_TIMEOUT;
    }

    return ESP_OK;
}

// Event handler for IP events
static void got_ip_event_handler(void *arg, esp_event_base_t event_base,
                                 int32_t event_id, void *event_data)
{
    ip_event_got_ip_t *event = (ip_event_got_ip_t *) event_data;
    const esp_netif_ip_info_t *ip_info = &event->ip_info;
    s_got_ip = true;
    ESP_LOGI(TAG, "Ethernet Got IP Address");
    ESP_LOGI(TAG, "~~~~~~~~~~~");
    ESP_LOGI(TAG, "ETHIP:" IPSTR, IP2STR(&ip_info->ip));
    ESP_LOGI(TAG, "ETHMASK:" IPSTR, IP2STR(&ip_info->netmask));
    ESP_LOGI(TAG, "ETHGW:" IPSTR, IP2STR(&ip_info->gw));
    ESP_LOGI(TAG, "~~~~~~~~~~~");
}

// Public API implementations

esp_err_t usb_host_eth_init(const usb_host_eth_config_t *config)
{
    if (config == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    // Save configuration
    memcpy(&s_config, config, sizeof(usb_host_eth_config_t));

    // Init TCP/IP network interface (should be called only once in application)
    esp_err_t ret = esp_netif_init();
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) { // Already initialized is OK
        ESP_LOGE(TAG, "Failed to initialize TCP/IP stack");
        return ret;
    }

    // Create default event loop that runs in background if not already created
    ret = esp_event_loop_create_default();
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) { // Already initialized is OK
        ESP_LOGE(TAG, "Failed to create event loop");
        return ret;
    }

    // Register event handlers
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_ETH_GOT_IP, &got_ip_event_handler, NULL));

    // Create network interface for USB ECM with default config
    esp_netif_config_t netif_cfg = ESP_NETIF_DEFAULT_ETH();
    s_eth_netif = esp_netif_new(&netif_cfg);
    if (!s_eth_netif) {
        ESP_LOGE(TAG, "Failed to create netif");
        return ESP_ERR_NO_MEM;
    }

    // Configure driver for the netif
    esp_netif_driver_ifconfig_t driver_ifconfig = {
        .handle = s_eth_netif,
        .transmit = usb_ecm_eth_transmit,
        .driver_free_rx_buffer = usb_ecm_eth_free_rx_buffer,
    };

    // Set the driver configuration for the netif
    ret = esp_netif_set_driver_config(s_eth_netif, &driver_ifconfig);
    if (ret != ESP_OK) {
        esp_netif_destroy(s_eth_netif);
        s_eth_netif = NULL;
        return ret;
    }

    // Generate a MAC address for the interface
    uint8_t mac_addr[6];
    esp_read_mac(mac_addr, ESP_MAC_ETH);
    mac_addr[5] ^= 0x01; // Make it unique from the default Ethernet MAC
    ESP_ERROR_CHECK(esp_netif_set_mac(s_eth_netif, mac_addr));

    ESP_LOGI(TAG, "USB ECM MAC: %02x:%02x:%02x:%02x:%02x:%02x",
             mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);

    // Create TX queue
    s_tx_queue = xQueueCreate(USB_ECM_TX_QUEUE_SIZE, sizeof(usb_ecm_tx_packet_t));
    if (!s_tx_queue) {
        ESP_LOGE(TAG, "Failed to create TX queue");
        esp_netif_destroy(s_eth_netif);
        s_eth_netif = NULL;
        return ESP_ERR_NO_MEM;
    }

    // Create USB ECM transmit task
    BaseType_t task_created = xTaskCreate(usb_ecm_tx_task, "usb_ecm_tx", USB_ECM_TX_TASK_STACK_SIZE, NULL,
                                          USB_ECM_TASK_PRIORITY - 1, &s_tx_task_hdl);
    if (task_created != pdTRUE) {
        vQueueDelete(s_tx_queue);
        s_tx_queue = NULL;
        esp_netif_destroy(s_eth_netif);
        s_eth_netif = NULL;
        ESP_LOGE(TAG, "Failed to create TX task");
        return ESP_ERR_NO_MEM;
    }

    ESP_LOGI(TAG, "USB ECM network interface initialized");
    return ESP_OK;
}

esp_err_t usb_host_eth_deinit(void)
{
    if (!s_eth_netif) {
        return ESP_ERR_INVALID_STATE;
    }

    // Disconnect if connected
    if (s_cdc_dev) {
        usb_host_eth_disconnect();
    }

    // Delete TX task
    if (s_tx_task_hdl) {
        vTaskDelete(s_tx_task_hdl);
        s_tx_task_hdl = NULL;
    }

    // Delete TX queue
    if (s_tx_queue) {
        vQueueDelete(s_tx_queue);
        s_tx_queue = NULL;
    }

    // Unregister event handler
    esp_event_handler_unregister(IP_EVENT, IP_EVENT_ETH_GOT_IP, got_ip_event_handler);

    // Destroy netif
    esp_netif_destroy(s_eth_netif);
    s_eth_netif = NULL;

    ESP_LOGI(TAG, "USB ECM network interface deinitialized");
    return ESP_OK;
}

esp_err_t usb_host_eth_connect(SemaphoreHandle_t disconnected_sem)
{
    if (!s_eth_netif) {
        return ESP_ERR_INVALID_STATE;
    }

    // Store the semaphore handle
    s_disconnected_sem = disconnected_sem;

    // Reset network state flag when reconnecting
    s_network_started = false;

    // Open USB ECM device
    ESP_LOGI(TAG, "Opening CDC ACM device 0x%04X:0x%04X...", s_config.vid, s_config.pid);

    const cdc_acm_host_device_config_t dev_config = {
        .connection_timeout_ms = s_config.connection_timeout_ms,
        .out_buffer_size = s_config.out_buffer_size,
        .in_buffer_size = s_config.in_buffer_size,
        .user_arg = s_eth_netif,
        .event_cb = usb_ecm_event_cb,
        .data_cb = usb_ecm_data_received_cb
    };

    esp_err_t err = cdc_acm_host_open(s_config.vid, s_config.pid, 0, &dev_config, &s_cdc_dev);
    if (err == ESP_ERR_NOT_FOUND) {
        ESP_LOGI(TAG, "Device not found");
        return ESP_ERR_NOT_FOUND;
    } else if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open device: %s", esp_err_to_name(err));
        return err;
    }

    cdc_acm_host_desc_print(s_cdc_dev);

    // Setting ETHERNET packet filter to enable network interface
    ESP_LOGI(TAG, "Setting ETHERNET packet filter");
    uint8_t bmRequestType = USB_BM_REQUEST_TYPE_DIR_OUT | USB_BM_REQUEST_TYPE_TYPE_CLASS | USB_BM_REQUEST_TYPE_RECIP_INTERFACE;
    uint8_t bRequest = 0x43; // SET_ETHERNET_PACKET_FILTER
    uint16_t wValue = 0x0F; // Enable all packets - broadcast, multicast, directed, and promiscuous
    uint16_t wIndex = 0; // Interface number
    uint16_t wLength = 0; // No data
    err = cdc_acm_host_send_custom_request(s_cdc_dev, bmRequestType, bRequest, wValue, wIndex, wLength, NULL);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set ETHERNET packet filter: %s", esp_err_to_name(err));
        cdc_acm_host_close(s_cdc_dev);
        s_cdc_dev = NULL;
        return ESP_FAIL;
    }

    // Set interface alternate setting to enable network connection
    ESP_LOGI(TAG, "Setting interface alternate setting to 1");
    bmRequestType = USB_BM_REQUEST_TYPE_DIR_OUT | USB_BM_REQUEST_TYPE_TYPE_STANDARD | USB_BM_REQUEST_TYPE_RECIP_INTERFACE;
    bRequest = USB_B_REQUEST_SET_INTERFACE;
    wValue = 1; // Alternate setting
    wIndex = 1; // Interface number
    wLength = 0; // No data
    err = cdc_acm_host_send_custom_request(s_cdc_dev, bmRequestType, bRequest, wValue, wIndex, wLength, NULL);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set interface alternate setting: %s", esp_err_to_name(err));
        cdc_acm_host_close(s_cdc_dev);
        s_cdc_dev = NULL;
        return ESP_FAIL;
    }

    // wait until network connection is established
    while (!s_network_started || !s_got_ip) {
        // Wait for network connection
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    ESP_LOGI(TAG, "USB ECM connected and running!");
    return ESP_OK;
}

esp_err_t usb_host_eth_disconnect(void)
{
    if (!s_cdc_dev) {
        return ESP_ERR_INVALID_STATE;
    }

    // Close the device
    ESP_LOGI(TAG, "Disconnecting USB ECM device");
    esp_err_t err = cdc_acm_host_close(s_cdc_dev);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to close device: %s", esp_err_to_name(err));
    }

    // Reset network state
    if (s_eth_netif && s_network_started) {
        esp_netif_action_disconnected(s_eth_netif, NULL, 0, NULL);
        esp_netif_action_stop(s_eth_netif, NULL, 0, NULL);
        s_network_started = false;
    }

    s_cdc_dev = NULL;
    return ESP_OK;
}

esp_netif_t *usb_host_eth_get_netif(void)
{
    return s_eth_netif;
}

bool usb_host_eth_is_connected(void)
{
    return (s_cdc_dev != NULL);
}
