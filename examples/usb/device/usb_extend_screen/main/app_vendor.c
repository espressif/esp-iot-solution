/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "esp_log.h"
#include "app_usb.h"
#include "app_lcd.h"
#include "esp_timer.h"
#include "usb_frame.h"

static const char *TAG = "app_vendor";
static frame_t *current_frame = NULL;

//--------------------------------------------------------------------+
// Vendor callbacks
//--------------------------------------------------------------------+

#define CONFIG_USB_VENDOR_RX_BUFSIZE  VENDOR_BUF_SIZE

// -- Display Packets
#define UDISP_TYPE_RGB565  0
#define UDISP_TYPE_RGB888  1
#define UDISP_TYPE_YUV420  2
#define UDISP_TYPE_JPG     3
#define UDISP_TYPE_END     0xff

typedef struct {
    uint16_t crc16;
    uint8_t  type;
    uint8_t cmd;
    uint16_t x;
    uint16_t y;
    uint16_t width;
    uint16_t height;
    uint32_t frame_id: 10;
    uint32_t payload_total: 22; //padding 32bit align
} __attribute__((packed)) udisp_frame_header_t;

void transfer_task(void *pvParameter)
{
    frame_allocate(6, CONFIG_USB_EXTEND_SCREEN_FRAME_LIMIT_B);
    frame_t *usr_frame = NULL;
    while (1) {
        usr_frame = frame_get_filled();
        app_lcd_draw(usr_frame->data,  usr_frame->info.total, usr_frame->info.width, usr_frame->info.height);
        frame_return_empty(usr_frame);
    }
}

static bool buffer_skip(frame_info_t *frame_info, uint32_t len)
{
    if (frame_info->received + len >= frame_info->total) {
        return true;
    }
    frame_info->received += len;
    return false;
}

static bool buffer_fill(frame_t *frame, uint8_t *buf, uint32_t len)
{
    if (0 == len) {
        return false;
    }

    if (frame_add_data(frame, buf, len) != ESP_OK) {
    }
    frame->info.received += len;

    if (frame->info.received >= frame->info.total) {
        frame_send_filled(frame);
        return true;
    }
    return false;
}

void tud_vendor_rx_cb(uint8_t itf, uint8_t const* buffer, uint16_t bufsize)
{
    static uint8_t rx_buf[CONFIG_USB_VENDOR_RX_BUFSIZE];
    static bool skip_frame = false;
    static frame_info_t skip_frame_info = {0};

    while (tud_vendor_n_available(itf)) {
        int read_res = tud_vendor_n_read(itf, rx_buf, CONFIG_USB_VENDOR_RX_BUFSIZE);
        if (read_res > 0) {
            if (!current_frame && !skip_frame) {
                udisp_frame_header_t *pblt = (udisp_frame_header_t *)rx_buf;
                switch (pblt->type) {
                case UDISP_TYPE_RGB565:
                case UDISP_TYPE_RGB888:
                case UDISP_TYPE_YUV420:
                case UDISP_TYPE_JPG: {
                    if (pblt->x != 0 || pblt->y != 0 || pblt->width != EXAMPLE_LCD_H_RES || pblt->height != EXAMPLE_LCD_V_RES) {
                        break;
                    }

                    static int fps_count = 0;
                    static int64_t start_time = 0;
                    fps_count++;
                    if (fps_count == 50) {
                        int64_t end_time = esp_timer_get_time();
                        ESP_LOGI(TAG, "Input fps: %f", 1000000.0 / ((end_time - start_time) / 50.0));
                        start_time = end_time;
                        fps_count = 0;
                    }

                    current_frame = frame_get_empty();
                    if (current_frame) {
                        current_frame->info.width = pblt->width;
                        current_frame->info.height = pblt->height;
                        current_frame->info.total = pblt->payload_total;
                        current_frame->info.received = 0;
                        ESP_LOGD(TAG, "read_res: %d, rx bblt x:%d y:%d w:%d h:%d total:%"PRIu32" (%d)", read_res, pblt->x, pblt->y, pblt->width, pblt->height, current_frame->info.total, (pblt->width) * (pblt->height) * 2);
                        if (buffer_fill(current_frame, &rx_buf[sizeof(udisp_frame_header_t)], read_res - sizeof(udisp_frame_header_t))) {
                            current_frame = NULL;
                        }
                    } else {
                        memset(&skip_frame_info, 0, sizeof(skip_frame_info));
                        skip_frame_info.total = pblt->payload_total;
                        skip_frame = true;
                        buffer_skip(&skip_frame_info, read_res - sizeof(udisp_frame_header_t));
                        ESP_LOGE(TAG, "Get frame is null");
                    }
                    break;
                }
                case UDISP_TYPE_END:
                    break;
                default:
                    ESP_LOGE(TAG, "error cmd");
                    break;
                }
            } else if (skip_frame) {
                if (buffer_skip(&skip_frame_info, read_res)) {
                    current_frame = NULL;
                    skip_frame = false;
                }
            } else {
                if (buffer_fill(current_frame, rx_buf, read_res)) {
                    current_frame = NULL;
                }
            }
        }
    }
}

esp_err_t app_vendor_init(void)
{
    xTaskCreatePinnedToCore(transfer_task, "transfer_task", 4096, NULL, CONFIG_VENDOR_TASK_PRIORITY, NULL, 0);
    return ESP_OK;
}
