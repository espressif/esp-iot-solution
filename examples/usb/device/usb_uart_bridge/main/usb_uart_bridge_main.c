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

#include <sys/param.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/ringbuf.h"
#include "driver/gpio.h"
#include "driver/uart.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_system.h"
#include "assert.h"

#include "tinyusb.h"
#include "tusb.h"
#include "tusb_cdc_acm.h"
#include "board.h"

static const char *TAG = "USB2UART";

#define BOARD_UART_PORT UART_NUM_1
#define BOARD_UART_TXD_PIN BOARD_IO_FREE_45
#define BOARD_UART_RXD_PIN BOARD_IO_FREE_46

#ifdef CONFIG_UART_AUTO_DOWNLOAD
#define BOARD_AUTODLD_EN_PIN BOARD_IO_FREE_3
#define BOARD_AUTODLD_IO0_PIN BOARD_IO_FREE_26
static bool s_autodld_trigger = false;
static bool s_reset_trigger = false;
#endif

#define USB_RX_BUF_SIZE 256
#define USB_TX_BUF_SIZE 1024
#define UART_RX_BUF_SIZE 256
#define UART_TX_BUF_SIZE 1024
volatile bool s_reset_to_flash = false;
volatile bool s_wait_reset = false;
volatile bool s_in_boot = false;

#define CONFIG_BAUD_RATE(b) (b)
#define CONFIG_STOP_BITS(s) (((s)==2)?UART_STOP_BITS_2:(((s)==1)?UART_STOP_BITS_1_5:UART_STOP_BITS_1))
#define CONFIG_PARITY(p) (((p)==2)?UART_PARITY_EVEN:(((p)==1)?UART_PARITY_ODD:UART_PARITY_DISABLE))
#define CONFIG_DATA_BITS(b) (((b)==5)?UART_DATA_5_BITS:(((b)==6)?UART_DATA_6_BITS:(((b)==7)?UART_DATA_7_BITS:UART_DATA_8_BITS)))

#define STR_STOP_BITS(s) (((s)==2)?"UART_STOP_BITS_2":(((s)==1)?"UART_STOP_BITS_1_5":"UART_STOP_BITS_1"))
#define STR_PARITY(p) (((p)==2)?"UART_PARITY_EVEN":(((p)==1)?"UART_PARITY_ODD":"UART_PARITY_DISABLE"))
#define STR_DATA_BITS(b) (((b)==5)?"UART_DATA_5_BITS":(((b)==6)?"UART_DATA_6_BITS":(((b)==7)?"UART_DATA_7_BITS":"UART_DATA_8_BITS")))

/*
* uint32_t bit_rate;
* uint8_t  stop_bits; ///< 0: 1 stop bit - 1: 1.5 stop bits - 2: 2 stop bits
* uint8_t  parity;    ///< 0: None - 1: Odd - 2: Even - 3: Mark - 4: Space
* uint8_t  data_bits; ///< can be 5, 6, 7, 8 or 16
* */
static uint32_t s_baud_rate_active = 115200;
static uint8_t s_stop_bits_active = 0;
static uint8_t s_parity_active = 0;
static uint8_t s_data_bits_active = 8;

RingbufHandle_t s_usb_tx_ringbuf;

static void board_uart_init(void)
{
    uart_config_t uart_config = {
        .baud_rate = CONFIG_BAUD_RATE(s_baud_rate_active),
        .data_bits = CONFIG_DATA_BITS(s_data_bits_active),
        .parity = CONFIG_PARITY(s_parity_active),
        .stop_bits = CONFIG_STOP_BITS(s_stop_bits_active),
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_APB,
    };

    ESP_LOGI(TAG, "init uart%d: %d %s %s %s", BOARD_UART_PORT, s_baud_rate_active, STR_DATA_BITS(s_data_bits_active), STR_PARITY(s_parity_active), STR_STOP_BITS(s_stop_bits_active));

    // We won't use a buffer for sending data.
    uart_driver_install(BOARD_UART_PORT, UART_RX_BUF_SIZE, UART_TX_BUF_SIZE, 0, NULL, 0);
    uart_param_config(BOARD_UART_PORT, &uart_config);
    uart_set_pin(BOARD_UART_PORT, BOARD_UART_TXD_PIN, BOARD_UART_RXD_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
}

#ifdef CONFIG_UART_AUTO_DOWNLOAD
static bool board_autodl_gpio_init(void)
{
    gpio_config_t io_conf = {0};
    //disable interrupt
    io_conf.intr_type = GPIO_INTR_DISABLE;
    //set as output mode
    io_conf.mode = GPIO_MODE_OUTPUT;
    //bit mask of the pins that you want to set
    io_conf.pin_bit_mask = ((1ULL << BOARD_AUTODLD_EN_PIN) | (1ULL << BOARD_AUTODLD_IO0_PIN));
    //disable pull-down mode
    io_conf.pull_down_en = 0;
    //disable pull-up mode
    io_conf.pull_up_en = GPIO_PULLUP_ENABLE;
    //configure GPIO with the given settings
    esp_err_t ret = gpio_config(&io_conf);

    if (ret != ESP_OK) {
        return false;
    }

    return true;
}
#endif

// Invoked when device is mounted
void tud_mount_cb(void)
{
    ESP_LOGW(__func__, "");
}

// Invoked when device is unmounted
void tud_umount_cb(void)
{
    ESP_LOGW(__func__, "");
}

// Invoked when usb bus is suspended
// remote_wakeup_en : if host allows us to perform remote wakeup
// USB Specs: Within 7ms, device must draw an average current less than 2.5 mA from bus
void tud_suspend_cb(bool remote_wakeup_en)
{
    ESP_LOGW(__func__, "remote_wakeup_en = %d", remote_wakeup_en);
}

// Invoked when usb bus is resumed
void tud_resume_cb(void)
{
    ESP_LOGW(__func__, "");
}

void tinyusb_cdc_rx_callback(int itf, cdcacm_event_t *event)
{
    /* initialization */
    size_t rx_size = 0;
    uint8_t rx_buf[USB_RX_BUF_SIZE];
    /* read from usb */
    esp_err_t ret = tinyusb_cdcacm_read(itf, rx_buf, USB_RX_BUF_SIZE, &rx_size);

    if (ret == ESP_OK) {//TODO:may time out
        size_t xfer_size = uart_write_bytes(BOARD_UART_PORT, rx_buf, rx_size);
        assert(xfer_size == rx_size);
        ESP_LOGD(TAG, "uart write data (%d bytes): %s", rx_size, rx_buf);
    } else {
        ESP_LOGE(TAG, "usb read error");
    }
}

void tinyusb_cdc_line_state_changed_callback(int itf, cdcacm_event_t *event)
{
    int dtr = event->line_state_changed_data.dtr;
    int rts = event->line_state_changed_data.rts;
    ESP_LOGI(TAG, "Line state changed! dtr:%d, rts:%d ", dtr, rts);

#ifdef CONFIG_UART_AUTO_DOWNLOAD
    bool _io0 = true;
    bool _en = true;
    static bool _io0_last = true;
    static bool _en_last = true;
    if (dtr == 1 && rts == 0) { //esptool ~100ms
        _io0 = true;
        _en = false;
    }

    if (dtr == 0 && rts == 1) {
        _io0 = false;
        _en = true;
    }

    if (dtr == rts) {
        ESP_LOGW(TAG, "...");
    } else {
        if(s_autodld_trigger || s_reset_trigger) return;
        gpio_set_level(BOARD_AUTODLD_EN_PIN, _en);
        if(!_io0_last && _en_last && _io0 && !_en) {
            ets_delay_us(50000);
            ESP_LOGW(TAG, "boot reset target chip");
            gpio_set_level(BOARD_AUTODLD_EN_PIN, true);
            ets_delay_us(10000);
            s_autodld_trigger = true;
        } else if(_io0_last && !_en_last && !_io0 && _en) {
            gpio_set_level(BOARD_AUTODLD_IO0_PIN, true);
            ets_delay_us(50000);
            ESP_LOGW(TAG, "normal reset target chip");
            ets_delay_us(10000);
            _io0_last = _io0;
            _en_last = _en;
            s_reset_trigger = true;
            ESP_LOGW(TAG, "strapin! -> io0:%d, en:%d ", _io0, _en);
            return;
        }
        gpio_set_level(BOARD_AUTODLD_IO0_PIN, _io0);
        _io0_last = _io0;
        _en_last = _en;
        ESP_LOGW(TAG, "strapin! -> io0:%d, en:%d ", _io0, _en);
    }
#endif

}

void tinyusb_cdc_line_coding_changed_callback(int itf, cdcacm_event_t *event)
{
    uint32_t bit_rate = event->line_coding_changed_data.p_line_coding->bit_rate;
    uint8_t stop_bits = event->line_coding_changed_data.p_line_coding->stop_bits;
    uint8_t parity = event->line_coding_changed_data.p_line_coding->parity;
    uint8_t data_bits = event->line_coding_changed_data.p_line_coding->data_bits;
    ESP_LOGV(TAG, "host require bit_rate=%u stop_bits=%u parity=%u data_bits=%u", bit_rate, stop_bits, parity, data_bits);

    if (s_baud_rate_active != bit_rate) {
        if (ESP_OK == uart_set_baudrate(BOARD_UART_PORT, CONFIG_BAUD_RATE(bit_rate))) {
            s_baud_rate_active = bit_rate;
            ESP_LOGW(TAG, "set bit_rate=%d", bit_rate);
        }
    }

    if (s_stop_bits_active != stop_bits) {
        if (ESP_OK == uart_set_stop_bits(BOARD_UART_PORT, CONFIG_STOP_BITS(stop_bits))) {
            s_stop_bits_active = stop_bits;
            ESP_LOGW(TAG, "set stop_bits=%s", STR_STOP_BITS(stop_bits));
        }
    }

    if (s_parity_active != parity) {
        if (ESP_OK == uart_set_parity(BOARD_UART_PORT, CONFIG_PARITY(parity))) {
            s_parity_active = parity;
            ESP_LOGW(TAG, "set parity=%s", STR_PARITY(parity));
        }
    }

    if (s_data_bits_active != data_bits) {
        if (ESP_OK == uart_set_word_length(BOARD_UART_PORT, CONFIG_DATA_BITS(data_bits))) {
            s_data_bits_active = data_bits;
            ESP_LOGW(TAG, "set data_bits=%s", STR_DATA_BITS(data_bits));
        }
    }
}

static esp_err_t _usb_tx_ringbuf_read(uint8_t *out_buf, size_t req_bytes, size_t *read_bytes)
{
    uint8_t *buf = xRingbufferReceiveUpTo(s_usb_tx_ringbuf, read_bytes, 0, req_bytes);

    if (buf) {
        memcpy(out_buf, buf, *read_bytes);
        vRingbufferReturnItem(s_usb_tx_ringbuf, (void *)(buf));
        return ESP_OK;
    } else {
        return ESP_ERR_NO_MEM;
    }
}

esp_err_t usb_tx_ringbuf_read(uint8_t *out_buf, size_t out_buf_sz, size_t *rx_data_size)
{

    size_t read_sz;

    esp_err_t res = _usb_tx_ringbuf_read(out_buf, out_buf_sz, &read_sz);

    if (res != ESP_OK) {
        return res;
    }

    *rx_data_size = read_sz;

    /* Buffer's data can be wrapped, at that situations we should make another retrievement */
    if (_usb_tx_ringbuf_read(out_buf + read_sz, out_buf_sz - read_sz, &read_sz) == ESP_OK) {
        *rx_data_size += read_sz;
    }

    return ESP_OK;
}

static void usb_tx_task(void *arg)
{
    uint8_t *data = (uint8_t *)heap_caps_malloc(USB_TX_BUF_SIZE, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    size_t rx_data_size = 0;

    while (1) {
        ulTaskNotifyTake(pdFALSE, 1);
        if (ESP_OK == usb_tx_ringbuf_read(data, USB_TX_BUF_SIZE, &rx_data_size)) {
            if(rx_data_size == 0) continue;
            size_t ret = tinyusb_cdcacm_write_queue(0, data, rx_data_size);
            ESP_LOGV(TAG, "usb tx data size = %d ret=%u", rx_data_size,ret);
            tud_cdc_n_write_flush(0);
        }
    }

    free(data);
}

static void uart_read_task(void *arg)
{
    TaskHandle_t usb_tx_handle = (TaskHandle_t )arg;
    uint8_t *data = (uint8_t *)heap_caps_malloc(UART_RX_BUF_SIZE, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);

    while (1) {
        const int rxBytes = uart_read_bytes(BOARD_UART_PORT, data, UART_RX_BUF_SIZE, 1);
        if (rxBytes > 0) {
            int res = xRingbufferSend(s_usb_tx_ringbuf,
                                      data,
                                      rxBytes, 0);
            if (res != pdTRUE) {
                ESP_LOGW(TAG, "The unread buffer is too small, the data has been lost");
            }
            xTaskNotifyGive(usb_tx_handle);
        }
    }

    free(data);
}

void app_main(void)
{
    iot_board_init();

#ifdef CONFIG_BOARD_ESP32S3_USB_OTG_EV
    iot_board_usb_set_mode(USB_DEVICE_MODE);
    iot_board_usb_device_set_power(false, false);
#endif

    board_uart_init();
#ifdef CONFIG_UART_AUTO_DOWNLOAD
    board_autodl_gpio_init();
#endif
    s_usb_tx_ringbuf = xRingbufferCreate(USB_TX_BUF_SIZE, RINGBUF_TYPE_BYTEBUF);

    if (s_usb_tx_ringbuf == NULL) {
        ESP_LOGE(TAG, "Creation buffer error");
        assert(0);
    }

    tinyusb_config_t tusb_cfg = {
        .descriptor = NULL,
        .string_descriptor = NULL,
        .external_phy = false // In the most cases you need to use a `false` value
    };

    ESP_ERROR_CHECK(tinyusb_driver_install(&tusb_cfg));

    tinyusb_config_cdcacm_t amc_cfg = {
        .usb_dev = TINYUSB_USBDEV_0,
        .cdc_port = TINYUSB_CDC_ACM_0,
        .rx_unread_buf_sz = USB_RX_BUF_SIZE,
        .callback_rx = &tinyusb_cdc_rx_callback, // the first way to register a callback
        .callback_rx_wanted_char = NULL,
        .callback_line_state_changed = &tinyusb_cdc_line_state_changed_callback,
        .callback_line_coding_changed = &tinyusb_cdc_line_coding_changed_callback
    };

    ESP_ERROR_CHECK(tusb_cdc_acm_init(&amc_cfg));

    TaskHandle_t usb_tx_handle = NULL;
    xTaskCreate(usb_tx_task, "usb_tx", 4096, NULL, 4, &usb_tx_handle);
    vTaskDelay(500 / portTICK_RATE_MS);
    xTaskCreate(uart_read_task, "uart_rx", 4096, (void *)usb_tx_handle, 4, NULL);

    ESP_LOGI(TAG, "USB initialization DONE");

    while (1) { //TODO:reset to boot need refact
#ifdef CONFIG_UART_AUTO_DOWNLOAD
        static size_t trigger_reset = 0;
        const size_t threshold_reset = 15; //150ms at least

        if ((s_reset_trigger || s_autodld_trigger) && ++trigger_reset > threshold_reset)
        {
            /* code */
            trigger_reset = 0;
            s_reset_trigger = 0;
            s_autodld_trigger = 0;
            gpio_set_level(BOARD_AUTODLD_EN_PIN, true);
            gpio_set_level(BOARD_AUTODLD_IO0_PIN, true);
            ESP_LOGI(TAG, "strapin!2 -> io0:%d, en:%d \n", true, true);
        }
#endif
        vTaskDelay(10 / portTICK_RATE_MS);
    }
}
