/*
 * SPDX-FileCopyrightText: 2024-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdint.h>
#include <string.h>
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_log.h"
#include "esp_err.h"
#if CONFIG_IDF_TARGET_ESP32P4
#include "esp_hosted.h"
#include "esp_hosted_misc.h"
#endif
#include "esp_mac.h"
#include "sdkconfig.h"

#include "cmd_wifi.h"

#include "tinyusb.h"
#include "tinyusb_net.h"
#include "tusb_console.h"
#include "tusb_bth.h"

#if CFG_TUD_CDC
#include "tusb_cdc_acm.h"
static uint8_t buf[CONFIG_TINYUSB_CDC_RX_BUFSIZE + 1];
void Command_Parse(char* Cmd);
tinyusb_config_cdcacm_t amc_cfg;
#elif CONFIG_UART_ENABLE
void initialise_uart(void);
#endif

#ifdef CONFIG_HEAP_TRACING
#include "esp_heap_trace.h"
#define NUM_RECORDS 100
static heap_trace_record_t trace_record[NUM_RECORDS]; // This buffer must be in internal RAM
#endif /* CONFIG_HEAP_TRACING */

static const char *TAG = "USB_Dongle";

/*
 * Register commands that can be used with FreeRTOS+CLI through the UDP socket.
 * The commands are defined in CLI-commands.c.
 */
void vRegisterCLICommands(void);

#if CFG_TUD_CDC
void tinyusb_cdc_rx_callback(int itf, cdcacm_event_t *event)
{
    /* initialization */
    size_t rx_size = 0;

    /* read */
    esp_err_t ret = tinyusb_cdcacm_read(0, buf, CONFIG_TINYUSB_CDC_RX_BUFSIZE, &rx_size);
    if (ret == ESP_OK) {
        buf[rx_size] = '\0';
        Command_Parse((char*)buf);
    } else {
        ESP_LOGE(TAG, "itf %d: itf Read error", itf);
    }
}

void tinyusb_cdc_line_state_changed_callback(int itf, cdcacm_event_t *event)
{
    int dtr = event->line_state_changed_data.dtr;
    int rst = event->line_state_changed_data.rts;
    ESP_LOGI(TAG, "Line state changed! itf:%d dtr:%d, rst:%d", itf, dtr, rst);
}
#endif /* CFG_TUD_CDC */

void app_main(void)
{
    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

#if CFG_TUD_NCM || CFG_TUD_ECM_RNDIS
    // Get MAC address BEFORE USB initialization, so it's set when USB enumerates
    // This is critical if USB is connected before power-on
    uint8_t mac_addr[6] = {0};
#if CONFIG_IDF_TARGET_ESP32P4
    esp_hosted_connect_to_slave();
    esp_err_t mac_ret = esp_hosted_iface_mac_addr_get(mac_addr, 6, ESP_MAC_WIFI_STA);
    if (mac_ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get slave STA MAC address: %s, using default", esp_err_to_name(mac_ret));
    } else {
        // Update TinyUSB RNDIS MAC address BEFORE USB initialization
        memcpy(tud_network_mac_address, mac_addr, 6);
        ESP_LOGI(TAG, "Slave STA MAC: %02X:%02X:%02X:%02X:%02X:%02X",
                 mac_addr[0], mac_addr[1], mac_addr[2],
                 mac_addr[3], mac_addr[4], mac_addr[5]);
    }
#else
    esp_read_mac(mac_addr, ESP_MAC_WIFI_STA);
    // Update TinyUSB RNDIS MAC address BEFORE USB initialization
    memcpy(tud_network_mac_address, mac_addr, 6);
    ESP_LOGI(TAG, "USB Network MAC: %02X:%02X:%02X:%02X:%02X:%02X",
             mac_addr[0], mac_addr[1], mac_addr[2],
             mac_addr[3], mac_addr[4], mac_addr[5]);
#endif
#endif /* CFG_TUD_NCM || CFG_TUD_ECM_RNDIS */

    ESP_LOGI(TAG, "USB initialization");

    tinyusb_config_t tusb_cfg = {
        .external_phy = false // In the most cases you need to use a `false` value
    };

    ESP_ERROR_CHECK(tinyusb_driver_install(&tusb_cfg));

#if CFG_TUD_NCM || CFG_TUD_ECM_RNDIS
    tinyusb_net_config_t tusb_net_cfg = {
        .on_recv_callback = wifi_recv_callback,
        .free_tx_buffer = wifi_buffer_free,
    };
    memcpy(tusb_net_cfg.mac_addr, mac_addr, 6);

    tinyusb_net_init(TINYUSB_USBDEV_0, &tusb_net_cfg);
    initialise_wifi();
#endif /* CFG_TUD_NCM || CFG_TUD_ECM_RNDIS */

#if CFG_TUD_BTH
    // init ble controller
    tusb_bth_init();
#endif /* CFG_TUD_BTH */

#if CFG_TUD_CDC
    tinyusb_config_cdcacm_t amc_cfg = {
        .usb_dev = TINYUSB_USBDEV_0,
        .cdc_port = TINYUSB_CDC_ACM_0,
        .rx_unread_buf_sz = 128,
        .callback_rx = &tinyusb_cdc_rx_callback, // the first way to register a callback
        .callback_rx_wanted_char = NULL,
        .callback_line_state_changed = &tinyusb_cdc_line_state_changed_callback,
        .callback_line_coding_changed = NULL
    };

    ESP_ERROR_CHECK(tusb_cdc_acm_init(&amc_cfg));
    esp_tusb_init_console(TINYUSB_CDC_ACM_0);
#elif CONFIG_UART_ENABLE
    initialise_uart();
#endif /* CFG_TUD_CDC */

    ESP_LOGI(TAG, "USB initialization DONE");

#ifdef CONFIG_HEAP_TRACING
    heap_trace_init_standalone(trace_record, NUM_RECORDS);
    heap_trace_start(HEAP_TRACE_LEAKS);
#endif

    /* Register commands with the FreeRTOS+CLI command interpreter. */
    vRegisterCLICommands();
}
