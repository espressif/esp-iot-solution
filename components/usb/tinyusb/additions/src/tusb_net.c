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

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "lwip/netif.h"
#include "esp_private/wifi.h"

#include "tusb_net.h"

extern bool s_wifi_is_connected;
static SemaphoreHandle_t Net_Semphore;

bool tud_network_wait_xmit(uint32_t ms)
{
    if (xSemaphoreTake(Net_Semphore, ms/portTICK_PERIOD_MS) == pdTRUE) {
        xSemaphoreGive(Net_Semphore);
        return true;
    }
    return false;
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

void tusb_net_init(void)
{
    vSemaphoreCreateBinary(Net_Semphore);
}

//--------------------------------------------------------------------+
// tinyusb callbacks
//--------------------------------------------------------------------+

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

void tud_network_idle_status_change_cb(bool enable)
{
    if (enable == true) {
        xSemaphoreGive(Net_Semphore);
    } else {
        xSemaphoreTake(Net_Semphore, 0);
    }
}