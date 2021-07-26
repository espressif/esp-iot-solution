// Copyright 2019-2021 Espressif Systems (Shanghai) PTE LTD
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at

//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "string.h"
#include "unity.h"
#include "test_utils.h"
#include "esp_usbh_cdc.h"

#define IN_RINGBUF_SIZE (1024 * 16)
#define OUT_RINGBUF_SIZE (1024 * 16)

static usb_desc_ep_t bulk_out_ep_desc = {
    .bLength = sizeof(usb_desc_ep_t),
    .bDescriptorType = USB_B_DESCRIPTOR_TYPE_ENDPOINT,
    .bEndpointAddress = 0x01,       //EP 1 OUT
    .bmAttributes = USB_BM_ATTRIBUTES_XFER_BULK,
    .wMaxPacketSize = 64,           //MPS of 64 bytes
    .bInterval = 0,
};

static usb_desc_ep_t bulk_in_ep_desc = {
    .bLength = sizeof(usb_desc_ep_t),
    .bDescriptorType = USB_B_DESCRIPTOR_TYPE_ENDPOINT,
    .bEndpointAddress = 0x81,       //EP 2 IN
    .bmAttributes = USB_BM_ATTRIBUTES_XFER_BULK,
    .wMaxPacketSize = 64,           //MPS of 64 bytes
    .bInterval = 0,
};

static void uart_event_task_entry(void *param)
{
    //ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    size_t data_len = 0;
    uint8_t buf[256];
    while (1) {
        usbh_cdc_get_buffered_data_len(&data_len);
        if (data_len == 0 || data_len > 256) {
            vTaskDelay(1);
            continue;
        }
        usbh_cdc_read_bytes(buf, data_len, 10);
        ESP_LOGI(TAG, "RCV %d: %.*s", data_len, data_len, buf);
    }
}

TEST_CASE("usb cdc R/W", "[esp_usbh_cdc]")
{
    static usbh_cdc_config_t config = {
        .bulk_in_ep = &bulk_in_ep_desc,
        .bulk_out_ep = &bulk_out_ep_desc,
        .rx_buffer_size = IN_RINGBUF_SIZE,
        .tx_buffer_size = OUT_RINGBUF_SIZE,
    };

    esp_err_t ret = usbh_cdc_driver_install(&config);
    assert(ret == ESP_OK);

    ret = xTaskCreate(uart_event_task_entry,             //Task Entry
                                 "uart_event",              //Task Name
                                 2048,           //Task Stack Size(Bytes)
                                 NULL,                           //Task Parameter
                                 2,             //Task Priority
                                 NULL   //Task Handler
                                );
    uint8_t buff[] = "AT\r\n";

    while (1)
    {
        usbh_cdc_write_bytes(buff, 4);
        vTaskDelay(100);
    }
}