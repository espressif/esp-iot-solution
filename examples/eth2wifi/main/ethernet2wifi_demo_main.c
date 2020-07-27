/* Ethernet to WiFi data forwarding Example

   For other examples please check:
   https://github.com/espressif/esp-iot-solution/tree/master/examples

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
 */
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_event.h"
#include "esp_event_loop.h"
#include "esp_log.h"

#include "esp_eth.h"

#include "esp_private/wifi.h"
#include "esp_wifi.h"

#include "nvs_flash.h"

static const char* TAG = "eth2wifi_demo";

#define FLOW_CONTROL_QUEUE_TIMEOUT_MS (100)
#define FLOW_CONTROL_QUEUE_LENGTH (40)
#define FLOW_CONTROL_WIFI_SEND_TIMEOUT_MS (100)

typedef struct {
    void* packet;
    uint16_t length;
} flow_control_msg_t;

static bool mac_is_set = false;
static xQueueHandle eth_queue_handle;
static bool wifi_is_connected = false;
static bool ethernet_is_connected = false;
static uint8_t eth_mac[6];
static esp_eth_handle_t s_eth_handle = NULL;

static void ethernet2wifi_mac_status_set(bool status)
{
    mac_is_set = status;
}

static bool ethernet2wifi_mac_status_get(void)
{
    return mac_is_set;
}

static esp_err_t pkt_eth2wifi(esp_eth_handle_t handle, uint8_t* buffer, uint32_t len)
{
    esp_err_t ret = ESP_OK;
    flow_control_msg_t msg = {
        .packet = buffer,
        .length = len,
    };

    if (xQueueSend(eth_queue_handle, &msg, pdMS_TO_TICKS(FLOW_CONTROL_QUEUE_TIMEOUT_MS)) != pdTRUE) {
        ESP_LOGE(TAG, "Send flow control message failed or timeout");
        free(buffer);
        ret = ESP_FAIL;
    }

    return ret;
}

static void eth_task(void* pvParameter)
{
    flow_control_msg_t msg;
    int res = 0;
    uint32_t timeout = 0;
    for (;;) {
        if (xQueueReceive(eth_queue_handle, &msg, (portTickType)portMAX_DELAY) == pdTRUE) {
            timeout = 0;
            if (msg.length) {
                do {
                    if (!ethernet2wifi_mac_status_get()) {
                        memcpy(eth_mac, (uint8_t*)msg.packet + 6, sizeof(eth_mac));
                        ESP_ERROR_CHECK(esp_wifi_start());
#ifdef CONFIG_ETH_TO_STATION_MODE
                        esp_wifi_set_mac(WIFI_IF_STA, eth_mac);
                        esp_wifi_connect();
#else
                        esp_wifi_set_mac(WIFI_IF_AP,eth_mac);
#endif
                        ethernet2wifi_mac_status_set(true);
                    }
                    vTaskDelay(pdMS_TO_TICKS(timeout));
                    timeout += 2;
                    if (wifi_is_connected) {
#ifdef CONFIG_ETH_TO_STATION_MODE
                        res = esp_wifi_internal_tx(ESP_IF_WIFI_STA, msg.packet, msg.length);
#else
                        res = esp_wifi_internal_tx(ESP_IF_WIFI_AP, msg.packet, msg.length);
#endif
                    }
                } while (res && timeout < FLOW_CONTROL_WIFI_SEND_TIMEOUT_MS);
                if (res != ESP_OK) {
                    ESP_LOGE(TAG, "WiFi send packet failed: %d", res);
                }
            }

            free(msg.packet);
        }
    }
    vTaskDelete(NULL);
}

static void initialise_wifi(void)
{
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(tcpip_adapter_clear_default_wifi_handlers());
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));

#ifdef CONFIG_ETH_TO_STATION_MODE
    wifi_config_t wifi_config = {
        .sta = {
            .ssid = CONFIG_DEMO_SSID,
            .password = CONFIG_DEMO_PASSWORD,
        },
    };
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config));
#else
    wifi_config_t wifi_config = {
        .ap = {
            .ssid = CONFIG_DEMO_SSID,
            .password = CONFIG_DEMO_PASSWORD,
            .ssid_len = strlen(CONFIG_DEMO_SSID),
            .max_connection = 1,
            .authmode = WIFI_AUTH_WPA2_PSK,
        },
    };
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_AP, &wifi_config));
#endif
}

static void initialise_ethernet(void)
{
    eth_mac_config_t mac_config = ETH_MAC_DEFAULT_CONFIG();
    eth_phy_config_t phy_config = ETH_PHY_DEFAULT_CONFIG();

    /* Set the PHY address in the example configuration */
    phy_config.phy_addr = CONFIG_PHY_ADDRESS;
    phy_config.reset_gpio_num = CONFIG_PHY_POWER_PIN;

    mac_config.smi_mdc_gpio_num = CONFIG_PHY_SMI_MDC_PIN;
    mac_config.smi_mdio_gpio_num = CONFIG_PHY_SMI_MDIO_PIN;
    esp_eth_mac_t* mac = esp_eth_mac_new_esp32(&mac_config);
    assert(mac);

#if CONFIG_PHY_IP101
    esp_eth_phy_t* phy = esp_eth_phy_new_ip101(&phy_config);
#elif CONFIG_PHY_LAN8720
    esp_eth_phy_t* phy = esp_eth_phy_new_lan8720(&phy_config);
#elif CONFIG_PHY_RTL8201
    esp_eth_phy_t* phy = esp_eth_phy_new_rtl8201(&phy_config);
#elif CONFIG_PHY_DP83848
    esp_eth_phy_t* phy = esp_eth_phy_new_dp83848(&phy_config);
#endif
    assert(phy);

    esp_eth_config_t config = ETH_DEFAULT_CONFIG(mac, phy);
    config.stack_input = pkt_eth2wifi;
    ESP_ERROR_CHECK(esp_eth_driver_install(&config, &s_eth_handle));
    esp_eth_ioctl(s_eth_handle, ETH_CMD_S_PROMISCUOUS, (void*)true);
    ESP_ERROR_CHECK(esp_eth_start(s_eth_handle));
}

static esp_err_t tcpip_adapter_sta_input_eth_output(void* buffer, uint16_t len, void* eb)
{
    if (ethernet_is_connected) {
        if (esp_eth_transmit(s_eth_handle, buffer, len) != ESP_OK) {
            ESP_LOGE(TAG, "Ethernet send packet failed");
        }
        //ESP_LOG_BUFFER_HEX("output", buffer,len);
    }

    esp_wifi_internal_free_rx_buffer(eb);
    return ESP_OK;
}

static esp_err_t tcpip_adapter_ap_input_eth_output(void* buffer, uint16_t len, void* eb)
{
    if (ethernet_is_connected) {
        if (esp_eth_transmit(s_eth_handle, buffer, len) != ESP_OK) {
            ESP_LOGE(TAG, "Ethernet send packet failed");
        }
    }

    esp_wifi_internal_free_rx_buffer(eb);
    return ESP_OK;
}

static void event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT) {
        switch (event_id) {
        case WIFI_EVENT_STA_START:
            printf("WIFI_EVENT_STA_START\n");
            break;
        case WIFI_EVENT_STA_CONNECTED:
            printf("WIFI_EVENT_STA_CONNECTED\n");
            wifi_is_connected = true;
            esp_wifi_internal_reg_rxcb(ESP_IF_WIFI_STA, (wifi_rxcb_t)tcpip_adapter_sta_input_eth_output);
            break;
        case WIFI_EVENT_STA_DISCONNECTED:
            printf("WIFI_EVENT_STA_DISCONNECTED\n");
            wifi_is_connected = false;
            esp_wifi_internal_reg_rxcb(ESP_IF_WIFI_STA, NULL);
            esp_wifi_connect();
            break;
        case WIFI_EVENT_AP_STACONNECTED:
            printf("WIFI_EVENT_AP_STACONNECTED\n");
            wifi_is_connected = true;
            esp_wifi_internal_reg_rxcb(ESP_IF_WIFI_AP, (wifi_rxcb_t)tcpip_adapter_ap_input_eth_output);
            break;
        case WIFI_EVENT_AP_STADISCONNECTED:
            printf("WIFI_EVENT_AP_STADISCONNECTED\n");
            wifi_is_connected = false;
            esp_wifi_internal_reg_rxcb(ESP_IF_WIFI_AP, NULL);
            break;
        default:
            break;
        }
    } else if (event_base == ETH_EVENT) {
        switch (event_id) {
        case ETHERNET_EVENT_CONNECTED:
            printf("ETHERNET_EVENT_CONNECTED\n");
            ethernet_is_connected = true;
            esp_eth_ioctl(s_eth_handle, ETH_CMD_G_MAC_ADDR, eth_mac);
            //#ifdef CONFIG_ETH_TO_STATION_MODE
            //            esp_wifi_set_mac(WIFI_IF_STA, eth_mac);
            //            ESP_LOGI(TAG, "mac: " MACSTR, MAC2STR(eth_mac));
            //#else
            //            esp_wifi_set_mac(WIFI_IF_AP, eth_mac);
            //#endif
            //            ESP_ERROR_CHECK(esp_wifi_start());
            //#ifdef CONFIG_ETH_TO_STATION_MODE
            //            ESP_ERROR_CHECK(esp_wifi_connect());
            //#endif
            break;
        case ETHERNET_EVENT_DISCONNECTED:
            printf("ETHERNET_EVENT_DISCONNECTED\n");
            ethernet2wifi_mac_status_set(false);
            ethernet_is_connected = false;
            ESP_ERROR_CHECK(esp_wifi_stop());
            break;
        default:
            break;
        }
    } else if (event_base == IP_EVENT) {
        switch (event_id) {
        case IP_EVENT_STA_GOT_IP:
            printf("IP_EVENT_STA_GOT_IP\n");
            break;
        default:
            break;
        }
    } else {
        ESP_LOGE(TAG, "Unknow event %s", event_base);
    }
}

void app_main()
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    tcpip_adapter_init();

    eth_queue_handle = xQueueCreate(FLOW_CONTROL_QUEUE_LENGTH, sizeof(flow_control_msg_t));
    xTaskCreate(eth_task, "eth_task", 2048, NULL, (tskIDLE_PRIORITY + 2), NULL);

    ESP_ERROR_CHECK(esp_event_loop_create_default());
    ESP_ERROR_CHECK(esp_event_handler_register(ETH_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL));

    initialise_ethernet();

    initialise_wifi();
}
