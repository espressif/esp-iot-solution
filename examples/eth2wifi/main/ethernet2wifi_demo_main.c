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

#include "esp_event_loop.h"
#include "esp_event.h"
#include "esp_log.h"

#include "esp_eth.h"

#include "esp_wifi.h"
#include "esp_wifi_internal.h"

#include "nvs_flash.h"

#ifdef CONFIG_PHY_LAN8720
#include "eth_phy/phy_lan8720.h"
#define DEFAULT_ETHERNET_PHY_CONFIG phy_lan8720_default_ethernet_config
#endif
#ifdef CONFIG_PHY_TLK110
#include "eth_phy/phy_tlk110.h"
#define DEFAULT_ETHERNET_PHY_CONFIG phy_tlk110_default_ethernet_config
#endif

static const char* TAG = "eth2wifi_demo";

#define PIN_PHY_POWER CONFIG_PHY_POWER_PIN
#define PIN_SMI_MDC   CONFIG_PHY_SMI_MDC_PIN
#define PIN_SMI_MDIO  CONFIG_PHY_SMI_MDIO_PIN

typedef struct {
    void* buffer;
    uint16_t len;
    void* eb;
} tcpip_adapter_eth_input_t;

static bool mac_is_set = false;
static xQueueHandle eth_queue_handle;
static bool wifi_is_connected = false;
static bool ethernet_is_connected = false;
static uint8_t eth_mac[6];

#ifdef CONFIG_PHY_USE_POWER_PIN
/* This replaces the default PHY power on/off function with one that
   also uses a GPIO for power on/off.

   If this GPIO is not connected on your device (and PHY is always powered), you can use the default PHY-specific power
   on/off function rather than overriding with this one.
 */
static void phy_device_power_enable_via_gpio(bool enable)
{
    assert(DEFAULT_ETHERNET_PHY_CONFIG.phy_power_enable);

    if (!enable) {
        /* Do the PHY-specific power_enable(false) function before powering down */
        DEFAULT_ETHERNET_PHY_CONFIG.phy_power_enable(false);
    }

    gpio_pad_select_gpio(PIN_PHY_POWER);
    gpio_set_direction(PIN_PHY_POWER, GPIO_MODE_OUTPUT);

    if (enable == true) {
        gpio_set_level(PIN_PHY_POWER, 1);
        ESP_LOGD(TAG, "phy_device_power_enable(TRUE)");
    } else {
        gpio_set_level(PIN_PHY_POWER, 0);
        ESP_LOGD(TAG, "power_enable(FALSE)");
    }

    // Allow the power up/down to take effect, min 300us
    vTaskDelay(1);

    if (enable) {
        /* Run the PHY-specific power on operations now the PHY has power */
        DEFAULT_ETHERNET_PHY_CONFIG.phy_power_enable(true);
    }
}
#endif

static void ethernet2wifi_mac_status_set(bool status)
{
    mac_is_set = status;
}

static bool ethernet2wifi_mac_status_get(void)
{
    return mac_is_set;
}

static void eth_gpio_config_rmii(void)
{
    // RMII data pins are fixed:
    // TXD0 = GPIO19
    // TXD1 = GPIO22
    // TX_EN = GPIO21
    // RXD0 = GPIO25
    // RXD1 = GPIO26
    // CLK == GPIO0
    phy_rmii_configure_data_interface_pins();
    // MDC is GPIO 23, MDIO is GPIO 18
    phy_rmii_smi_configure_pins(PIN_SMI_MDC, PIN_SMI_MDIO);
}

static esp_err_t tcpip_adapter_eth_input_sta_output(void* buffer, uint16_t len, void* eb)
{
    tcpip_adapter_eth_input_t msg;

    msg.buffer = buffer;
    msg.len = len;
    msg.eb = eb;

    if (xQueueSend(eth_queue_handle, &msg, portMAX_DELAY) != pdTRUE) {
        return ESP_FAIL;
    }

    return ESP_OK;
}

static void eth_task(void* pvParameter)
{
    tcpip_adapter_eth_input_t msg;

    for (;;) {
        if (xQueueReceive(eth_queue_handle, &msg, (portTickType)portMAX_DELAY) == pdTRUE) {
            if (msg.len > 0) {
                if (!ethernet2wifi_mac_status_get()) {
                    memcpy(eth_mac, (uint8_t*)msg.buffer + 6, sizeof(eth_mac));
                    ESP_ERROR_CHECK(esp_wifi_start());
#ifdef CONFIG_ETH_TO_STATION_MODE
                    esp_wifi_set_mac(WIFI_IF_STA, eth_mac);
                    esp_wifi_connect();
#else
                    esp_wifi_set_mac(WIFI_IF_AP, eth_mac);
#endif
                    ethernet2wifi_mac_status_set(true);
                }

                if (wifi_is_connected) {
#ifdef CONFIG_ETH_TO_STATION_MODE
                    esp_wifi_internal_tx(ESP_IF_WIFI_STA, msg.buffer, msg.len - 4);
#else
                    esp_wifi_internal_tx(ESP_IF_WIFI_AP, msg.buffer, msg.len - 4);
#endif
                }
            }

            esp_eth_free_rx_buf(msg.buffer);
        }
    }
}

static void initialise_wifi(void)
{
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init_internal(&cfg));
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
        },
    };
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_AP, &wifi_config));
#endif
}

static void initialise_ethernet(void)
{
    eth_config_t config = DEFAULT_ETHERNET_PHY_CONFIG;
    
    /* Set the PHY address in the example configuration */
    config.phy_addr = CONFIG_PHY_ADDRESS;
    config.gpio_config = eth_gpio_config_rmii;
    config.tcpip_input = tcpip_adapter_eth_input_sta_output;

#ifdef CONFIG_PHY_USE_POWER_PIN
    /* Replace the default 'power enable' function with an example-specific
       one that toggles a power GPIO. */
    config.phy_power_enable = phy_device_power_enable_via_gpio;
#endif
    esp_eth_init_internal(&config);
    esp_eth_enable();
}

static esp_err_t tcpip_adapter_sta_input_eth_output(void* buffer, uint16_t len, void* eb)
{
    if (ethernet_is_connected) {
        esp_eth_tx(buffer, len);
    }

    esp_wifi_internal_free_rx_buffer(eb);
    return ESP_OK;
}

static esp_err_t tcpip_adapter_ap_input_eth_output(void* buffer, uint16_t len, void* eb)
{
    if (ethernet_is_connected) {
        esp_eth_tx(buffer, len);
    }

    esp_wifi_internal_free_rx_buffer(eb);
    return ESP_OK;
}

static esp_err_t event_handler(void* ctx, system_event_t* event)
{
    switch (event->event_id) {
        case SYSTEM_EVENT_STA_START:
            printf("SYSTEM_EVENT_STA_START\r\n");
            break;

        case SYSTEM_EVENT_STA_CONNECTED:
            printf("SYSTEM_EVENT_STA_CONNECTED\r\n");
            wifi_is_connected = true;

            esp_wifi_internal_reg_rxcb(ESP_IF_WIFI_STA, (wifi_rxcb_t)tcpip_adapter_sta_input_eth_output);
            break;

        case SYSTEM_EVENT_STA_GOT_IP:
            printf("SYSTEM_EVENT_STA_GOT_IP\r\n");
            break;

        case SYSTEM_EVENT_STA_DISCONNECTED:
            printf("SlYSTEM_EVENT_STA_DISCONNECTED\r\n");
            wifi_is_connected = false;
            esp_wifi_internal_reg_rxcb(ESP_IF_WIFI_STA, NULL);
            esp_wifi_connect();
            break;

        case SYSTEM_EVENT_AP_STACONNECTED:
            printf("SYSTEM_EVENT_AP_STACONNECTED\r\n");
            wifi_is_connected = true;

            esp_wifi_internal_reg_rxcb(ESP_IF_WIFI_AP, (wifi_rxcb_t)tcpip_adapter_ap_input_eth_output);
            break;

        case SYSTEM_EVENT_AP_STADISCONNECTED:
            printf("SYSTEM_EVENT_AP_STADISCONNECTED\r\n");
            wifi_is_connected = false;
            esp_wifi_internal_reg_rxcb(ESP_IF_WIFI_AP, NULL);
            break;

        case SYSTEM_EVENT_ETH_CONNECTED:
            printf("SYSTEM_EVENT_ETH_CONNECTED\r\n");
            ethernet_is_connected = true;
            break;

        case SYSTEM_EVENT_ETH_DISCONNECTED:
            printf("SYSTEM_EVENT_ETH_DISCONNECTED\r\n");
            ethernet2wifi_mac_status_set(false);
            ethernet_is_connected = false;
            break;

        default:
            break;
    }

    return ESP_OK;
}

void app_main()
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK( nvs_flash_erase() );
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK( ret );
    
    eth_queue_handle = xQueueCreate(CONFIG_DMA_RX_BUF_NUM, sizeof(tcpip_adapter_eth_input_t));
    xTaskCreate(eth_task, "eth_task", 2048, NULL, (tskIDLE_PRIORITY + 2), NULL);
    
    esp_event_loop_init(event_handler, NULL);

    initialise_ethernet();

    initialise_wifi();
}
