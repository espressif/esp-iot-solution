/**
 * @copyright Copyright 2021 Espressif Systems (Shanghai) Co. Ltd.
 *
 *      Licensed under the Apache License, Version 2.0 (the "License");
 *      you may not use this file except in compliance with the License.
 *      You may obtain a copy of the License at
 *
 *               http://www.apache.org/licenses/LICENSE-2.0
 * 
 *      Unless required by applicable law or agreed to in writing, software
 *      distributed under the License is distributed on an "AS IS" BASIS,
 *      WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *      See the License for the specific language governing permissions and
 *      limitations under the License.
 */

#include <stdint.h>
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "lwip/ip_addr.h"
#include "lwip/netif.h"

#include "esp_log.h"
#include "esp_err.h"
#include "esp_wifi.h"
#include "esp_private/wifi.h"
#include "sdkconfig.h"


#ifdef CONFIG_HEAP_TRACING
#include "esp_heap_trace.h"
#endif

#include "tinyusb.h"
#include "tusb_cdc_acm.h"

#include "cmd_wifi.h"

SemaphoreHandle_t semp;

#ifdef CONFIG_HEAP_TRACING
#define NUM_RECORDS 100
static heap_trace_record_t trace_record[NUM_RECORDS]; // This buffer must be in internal RAM
#endif

static const char *TAG = "USB_Dongle_WiFi";
static uint8_t buf[CONFIG_TINYUSB_CDC_RX_BUFSIZE + 1];
extern bool s_wifi_is_connected;

extern void Command_Parse(char* Cmd);

/*
 * Register commands that can be used with FreeRTOS+CLI through the UDP socket.
 * The commands are defined in CLI-commands.c.
 */
extern void vRegisterCLICommands( void );


bool tud_network_recv_cb(const uint8_t *src, uint16_t size)
{
  if (s_wifi_is_connected) {
    esp_wifi_internal_tx(ESP_IF_WIFI_STA, src, size);
  }
  tud_network_recv_renew();
  return true;
}

uint16_t tud_network_xmit_cb(uint8_t *dst, void *ref, uint16_t arg)
{
  uint16_t len = arg;

  /* traverse the "pbuf chain"; see ./lwip/src/core/pbuf.c for more info */
  memcpy(dst, ref, len);
  return len;
}

void tud_network_init_cb(void)
{
    /* TODO */
}

bool tud_network_wait_xmit(uint32_t ms)
{
  if (xSemaphoreTake(semp, ms/portTICK_PERIOD_MS) == pdTRUE) {
    xSemaphoreGive(semp);
    return true;
  }
  return false;
}

void tud_network_idle_status_change_cb(bool enable)
{
    if (enable == true) {
      xSemaphoreGive(semp);
    } else {
      xSemaphoreTake(semp, 0);
    }
}

esp_err_t pkt_wifi2usb(void *buffer, uint16_t len, void *eb)
{
    if (!tud_ready()) {
      esp_wifi_internal_free_rx_buffer(eb);
      return ERR_USE;
    }
    
    if (tud_network_wait_xmit(100)) {
      /* if the network driver can accept another packet, we make it happen */
      if (tud_network_can_xmit()) {
          tud_network_xmit(buffer, len);
      }
    }

    esp_wifi_internal_free_rx_buffer(eb);
    return ESP_OK;
}

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

void app_main(void)
{
    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK( ret );

    vSemaphoreCreateBinary(semp);

    ESP_LOGI(TAG, "USB initialization");

    tinyusb_config_t tusb_cfg = {
        .external_phy = false // In the most cases you need to use a `false` value
    };

    ESP_ERROR_CHECK(tinyusb_driver_install(&tusb_cfg));

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
    ESP_LOGI(TAG, "USB initialization DONE");

    initialise_wifi();
    
    /* Register commands with the FreeRTOS+CLI command interpreter. */
    vRegisterCLICommands();
}
