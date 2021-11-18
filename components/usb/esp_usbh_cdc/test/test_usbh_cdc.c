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
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "unity.h"
#include "test_utils.h"
#include "esp_usbh_cdc.h"
#include "esp_log.h"

#define IN_RINGBUF_SIZE (1024 * 1)
#define OUT_RINGBUF_SIZE (1024 * 1)
#define READ_TASK_KILL_BIT BIT1

#define TAG "host_cdc"
EventGroupHandle_t s_event_group_hdl = NULL;

static usb_ep_desc_t bulk_out_ep_desc = {
    .bLength = sizeof(usb_ep_desc_t),
    .bDescriptorType = USB_B_DESCRIPTOR_TYPE_ENDPOINT,
    .bEndpointAddress = 0x01,       //EP 1 OUT
    .bmAttributes = USB_BM_ATTRIBUTES_XFER_BULK,
    .wMaxPacketSize = 64,           //MPS of 64 bytes
    .bInterval = 0,
};

static usb_ep_desc_t bulk_in_ep_desc = {
    .bLength = sizeof(usb_ep_desc_t),
    .bDescriptorType = USB_B_DESCRIPTOR_TYPE_ENDPOINT,
    .bEndpointAddress = 0x81,       //EP 2 IN
    .bmAttributes = USB_BM_ATTRIBUTES_XFER_BULK,
    .wMaxPacketSize = 64,           //MPS of 64 bytes
    .bInterval = 0,
};

static void usb_read_task(void *param)
{
    size_t data_len = 0;
    uint8_t buf[256];
    while (!(xEventGroupGetBits(s_event_group_hdl) & READ_TASK_KILL_BIT)) {
        usbh_cdc_get_buffered_data_len(&data_len);
        if (data_len == 0 || data_len > 256) {
            vTaskDelay(1);
            continue;
        }
        usbh_cdc_read_bytes(buf, data_len, 10);
        ESP_LOGI(TAG, "RCV len=%d: %.*s", data_len, data_len, buf);
    }
    xEventGroupClearBits(s_event_group_hdl, READ_TASK_KILL_BIT);
    ESP_LOGW(TAG, "CDC read task deleted");
    vTaskDelete(NULL);
}

TEST_CASE("usb cdc R/W", "[esp_usbh_cdc]")
{
    s_event_group_hdl = xEventGroupCreate();
    TEST_ASSERT(s_event_group_hdl);
    static usbh_cdc_config_t config = {
        .bulk_in_ep = &bulk_in_ep_desc,
        .bulk_out_ep = &bulk_out_ep_desc,
        .rx_buffer_size = IN_RINGBUF_SIZE,
        .tx_buffer_size = OUT_RINGBUF_SIZE,
    };

    TEST_ASSERT_EQUAL(ESP_OK, usbh_cdc_driver_install(&config));
    /* Waitting for USB device connected */
    TEST_ASSERT_EQUAL(ESP_OK, usbh_cdc_wait_connect(portMAX_DELAY));
    xTaskCreate(usb_read_task, "usb_read", 4096, NULL, 2, NULL);
    uint8_t buff[] = "AT\r\n";
    uint8_t loop_num = 40;
    while (--loop_num) {
        int len = usbh_cdc_write_bytes(buff, 4);
        ESP_LOGI(TAG, "Send len=%d: %s", len, buff);
        vTaskDelay(pdMS_TO_TICKS(500));
    }
    xEventGroupSetBits(s_event_group_hdl, READ_TASK_KILL_BIT);
    ESP_LOGW(TAG, "Waiting CDC read task delete");
    while (xEventGroupGetBits(s_event_group_hdl) & READ_TASK_KILL_BIT) {
        vTaskDelay(10 / portTICK_PERIOD_MS);
    }
    vEventGroupDelete(s_event_group_hdl);
    TEST_ASSERT_EQUAL(ESP_OK, usbh_cdc_driver_delete());
}