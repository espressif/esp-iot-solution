/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "unity.h"
#include "iot_usbh_cdc.h"
#include "esp_log.h"

#define IN_RINGBUF_SIZE (256 * 1)
#define OUT_RINGBUF_SIZE (256 * 1)
#define READ_TASK_KILL_BIT BIT1

#define TAG "cdc_test"
EventGroupHandle_t s_event_group_hdl = NULL;
#define TEST_ITF_NUM   2
#define TEST_EP_IN0  0x82
#define TEST_EP_OUT0 0x02
#define TEST_EP_IN1  0x84
#define TEST_EP_OUT1 0x04

#define TEST_MEMORY_LEAK_THRESHOLD (-400)

static usb_ep_desc_t bulk_out_ep_desc = {
    .bLength = sizeof(usb_ep_desc_t),
    .bDescriptorType = USB_B_DESCRIPTOR_TYPE_ENDPOINT,
    .bEndpointAddress = TEST_EP_OUT0,       //EP 1 OUT
    .bmAttributes = USB_BM_ATTRIBUTES_XFER_BULK,
    .wMaxPacketSize = 64,           //MPS of 64 bytes
    .bInterval = 0,
};

static usb_ep_desc_t bulk_in_ep_desc = {
    .bLength = sizeof(usb_ep_desc_t),
    .bDescriptorType = USB_B_DESCRIPTOR_TYPE_ENDPOINT,
    .bEndpointAddress = TEST_EP_IN0,       //EP 2 IN
    .bmAttributes = USB_BM_ATTRIBUTES_XFER_BULK,
    .wMaxPacketSize = 64,           //MPS of 64 bytes
    .bInterval = 0,
};

static void usb_read_task(void *param)
{
    size_t data_len = 0;
    uint8_t buf[IN_RINGBUF_SIZE];
    while (!(xEventGroupGetBits(s_event_group_hdl) & READ_TASK_KILL_BIT)) {
        for (size_t i = 0; i < TEST_ITF_NUM; i++) {
            if (usbh_cdc_get_itf_state(i) == false) {
                continue;
            }
            usbh_cdc_itf_get_buffered_data_len(i, &data_len);
            if (data_len > 0) {
                usbh_cdc_itf_read_bytes(i, buf, data_len, 10);
                ESP_LOGI(TAG, "Itf %d, rcv len=%d: %.*s", i, data_len, data_len, buf);
            } else {
                vTaskDelay(1);
            }
        }
    }
    xEventGroupClearBits(s_event_group_hdl, READ_TASK_KILL_BIT);
    ESP_LOGW(TAG, "CDC read task deleted");
    vTaskDelete(NULL);
}

TEST_CASE("usb dual cdc R/W", "[iot_usbh_cdc][read-write]")
{
    for (size_t i = 0; i < 5; i++) {
        s_event_group_hdl = xEventGroupCreate();
        TEST_ASSERT(s_event_group_hdl);
        static usbh_cdc_config_t config = {
            .bulk_in_ep = &bulk_in_ep_desc,
            .bulk_out_ep = &bulk_out_ep_desc,
            .rx_buffer_size = IN_RINGBUF_SIZE,
            .tx_buffer_size = OUT_RINGBUF_SIZE,
        };
        config.itf_num = 2;
        /* test config with only ep addr */
        config.bulk_in_ep_addrs[1] = TEST_EP_IN1;
        config.bulk_out_ep_addrs[1] = TEST_EP_OUT1;
        config.rx_buffer_sizes[1] = IN_RINGBUF_SIZE;
        config.tx_buffer_sizes[1] = OUT_RINGBUF_SIZE;
        TEST_ASSERT_EQUAL(ESP_OK, usbh_cdc_driver_install(&config));
        /* Waiting for USB device connected */
        TEST_ASSERT_EQUAL(ESP_OK, usbh_cdc_wait_connect(portMAX_DELAY));
        xTaskCreate(usb_read_task, "usb_read", 4096, NULL, 2, NULL);
        uint8_t buff[] = "AT\r\n";
        uint8_t loop_num = 20;
        while (--loop_num) {
            int len = usbh_cdc_itf_write_bytes(0, buff, sizeof(buff));
            ESP_LOGI(TAG, "Send itf0 len=%d: %s", len, buff);
            len = usbh_cdc_itf_write_bytes(1, buff, sizeof(buff));
            ESP_LOGI(TAG, "Send itf1 len=%d: %s", len, buff);
            vTaskDelay(pdMS_TO_TICKS(50));
        }
        xEventGroupSetBits(s_event_group_hdl, READ_TASK_KILL_BIT);
        ESP_LOGW(TAG, "Waiting CDC read task delete");
        while (xEventGroupGetBits(s_event_group_hdl) & READ_TASK_KILL_BIT) {
            vTaskDelay(10 / portTICK_PERIOD_MS);
        }
        vEventGroupDelete(s_event_group_hdl);
        TEST_ASSERT_EQUAL(ESP_OK, usbh_cdc_driver_delete());
    }
    vTaskDelay(1000 / portTICK_PERIOD_MS);
}

static size_t before_free_8bit;
static size_t before_free_32bit;

static void check_leak(size_t before_free, size_t after_free, const char *type)
{
    ssize_t delta = after_free - before_free;
    printf("MALLOC_CAP_%s: Before %u bytes free, After %u bytes free (delta %d)\n", type, before_free, after_free, delta);
    TEST_ASSERT_MESSAGE(delta >= TEST_MEMORY_LEAK_THRESHOLD, "memory leak");
}

void setUp(void)
{
    before_free_8bit = heap_caps_get_free_size(MALLOC_CAP_8BIT);
    before_free_32bit = heap_caps_get_free_size(MALLOC_CAP_32BIT);
}

void tearDown(void)
{
    size_t after_free_8bit = heap_caps_get_free_size(MALLOC_CAP_8BIT);
    size_t after_free_32bit = heap_caps_get_free_size(MALLOC_CAP_32BIT);
    check_leak(before_free_8bit, after_free_8bit, "8BIT");
    check_leak(before_free_32bit, after_free_32bit, "32BIT");
}

void app_main(void)
{
    printf("IOT USBH CDC TEST \n");
    unity_run_menu();
}
