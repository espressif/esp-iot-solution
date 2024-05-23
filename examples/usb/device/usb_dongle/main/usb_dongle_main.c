/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdint.h>
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_log.h"
#include "esp_err.h"
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

    esp_read_mac(tusb_net_cfg.mac_addr, ESP_MAC_WIFI_STA);

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
