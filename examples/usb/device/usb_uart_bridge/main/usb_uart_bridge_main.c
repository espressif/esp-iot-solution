/*
 * SPDX-FileCopyrightText: 2022-2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <sys/param.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/ringbuf.h"
#include "driver/gpio.h"
#include "driver/uart.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_system.h"
#include "rom/ets_sys.h"

#include "tinyusb.h"
#include "tusb.h"
#include "tusb_cdc_acm.h"

static const char *TAG = "USB2UART";

#define BOARD_UART_PORT        UART_NUM_1
#define BOARD_UART_TXD_PIN     CONFIG_BOARD_UART_TXD_PIN
#define BOARD_UART_RXD_PIN     CONFIG_BOARD_UART_RXD_PIN
#define UART_RX_BUF_SIZE       CONFIG_UART_RX_BUF_SIZE
#define UART_TX_BUF_SIZE       CONFIG_UART_TX_BUF_SIZE

#ifdef CONFIG_UART_AUTO_DOWNLOAD
#define BOARD_AUTODLD_EN_PIN   CONFIG_BOARD_AUTODLD_EN_PIN
#define BOARD_AUTODLD_BOOT_PIN CONFIG_BOARD_AUTODLD_BOOT_PIN
static bool s_reset_trigger = false;
#endif

#define USB_RX_BUF_SIZE CONFIG_USB_RX_BUF_SIZE
#define USB_TX_BUF_SIZE CONFIG_USB_TX_BUF_SIZE

#define CFG_BAUD_RATE(b) (b)
#define CFG_STOP_BITS(s) (((s)==2)?UART_STOP_BITS_2:(((s)==1)?UART_STOP_BITS_1_5:UART_STOP_BITS_1))
#define CFG_PARITY(p) (((p)==2)?UART_PARITY_EVEN:(((p)==1)?UART_PARITY_ODD:UART_PARITY_DISABLE))
#define CFG_DATA_BITS(b) (((b)==5)?UART_DATA_5_BITS:(((b)==6)?UART_DATA_6_BITS:(((b)==7)?UART_DATA_7_BITS:UART_DATA_8_BITS)))

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
volatile bool s_reset_to_flash = false;
volatile bool s_wait_reset = false;
volatile bool s_in_boot = false;
static RingbufHandle_t s_usb_tx_ringbuf = NULL;
static SemaphoreHandle_t s_usb_tx_requested = NULL;
static SemaphoreHandle_t s_usb_tx_done = NULL;

static void board_uart_init(void)
{
    uart_config_t uart_config = {
        .baud_rate = CFG_BAUD_RATE(s_baud_rate_active),
        .data_bits = CFG_DATA_BITS(s_data_bits_active),
        .parity = CFG_PARITY(s_parity_active),
        .stop_bits = CFG_STOP_BITS(s_stop_bits_active),
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
#if CONFIG_IDF_TARGET_ESP32P4
        .source_clk = UART_SCLK_DEFAULT,
#else
        .source_clk = UART_SCLK_APB,
#endif
    };

    uart_driver_install(BOARD_UART_PORT, UART_RX_BUF_SIZE, UART_TX_BUF_SIZE, 0, NULL, 0);
    uart_param_config(BOARD_UART_PORT, &uart_config);
    uart_set_pin(BOARD_UART_PORT, BOARD_UART_TXD_PIN, BOARD_UART_RXD_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    ESP_LOGI(TAG, "init UART%d: %"PRIu32 " %s %s %s", BOARD_UART_PORT, s_baud_rate_active, STR_DATA_BITS(s_data_bits_active), STR_PARITY(s_parity_active), STR_STOP_BITS(s_stop_bits_active));
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
    io_conf.pin_bit_mask = ((1ULL << BOARD_AUTODLD_EN_PIN) | (1ULL << BOARD_AUTODLD_BOOT_PIN));
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
    ESP_LOGI(TAG, "USB Mounted");
}

// Invoked when device is unmounted
void tud_umount_cb(void)
{
    ESP_LOGI(TAG, "USB Unmounted");
}

// Invoked when usb bus is suspended
// remote_wakeup_en : if host allows us to perform remote wakeup
// USB Specs: Within 7ms, device must draw an average current less than 2.5 mA from bus
void tud_suspend_cb(bool remote_wakeup_en)
{
    ESP_LOGI(TAG, "USB Suspend");
}

// Invoked when usb bus is resumed
void tud_resume_cb(void)
{
    ESP_LOGI(TAG, "USB Resume");
}

void tud_cdc_tx_complete_cb(const uint8_t itf)
{
    if (xSemaphoreTake(s_usb_tx_requested, 0) != pdTRUE) {
        /* Semaphore should have been given before write attempt.
            Sometimes tinyusb can send one more cb even xfer_complete len is zero
        */
        return;
    }

    xSemaphoreGive(s_usb_tx_done);
}

static esp_err_t _wait_for_usb_tx_done(const uint32_t block_time_ms)
{
    if (xSemaphoreTake(s_usb_tx_done, pdMS_TO_TICKS(block_time_ms)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }
    return ESP_OK;
}

static void tinyusb_cdc_rx_callback(int itf, cdcacm_event_t *event)
{
    /* initialization */
    size_t rx_size = 0;
    uint8_t rx_buf[USB_RX_BUF_SIZE] = {0};
    /* read from usb */
    esp_err_t ret = tinyusb_cdcacm_read(itf, rx_buf, USB_RX_BUF_SIZE, &rx_size);

    if (ret == ESP_OK) {
        size_t xfer_size = uart_write_bytes(BOARD_UART_PORT, rx_buf, rx_size);
        if (xfer_size != rx_size) {
            ESP_LOGD(TAG, "uart write lost (%d/%d)", xfer_size, rx_size);
        }
        ESP_LOGD(TAG, "uart write data (%d bytes): %s", rx_size, rx_buf);
    } else {
        ESP_LOGE(TAG, "usb read error");
    }
}

static void tinyusb_cdc_line_state_changed_callback(int itf, cdcacm_event_t *event)
{
    int dtr = event->line_state_changed_data.dtr;
    int rts = event->line_state_changed_data.rts;
    ESP_LOGD(TAG, "Line state changed! dtr:%d, rts:%d ", dtr, rts);

#ifdef CONFIG_UART_AUTO_DOWNLOAD
    bool _io0 = true;
    bool _en = true;
    if (dtr == 1 && rts == 0) { //esptool ~100ms
        _io0 = false;
        _en = true;
    }

    if (dtr == 0 && rts == 1) {
        _io0 = true;
        _en = false;
    }

    s_reset_trigger = false;
    if (dtr & rts) {
        ESP_LOGW(TAG, "...");
        s_reset_trigger = true;
    } else {
        ESP_LOGI(TAG, "DTR = %d, RTS = %d -> BOOT = %d, RST = %d", dtr, rts, _io0, _en);
        gpio_set_level(BOARD_AUTODLD_BOOT_PIN, _io0);
        gpio_set_level(BOARD_AUTODLD_EN_PIN, _en);
    }
#endif

}

static void tinyusb_cdc_line_coding_changed_callback(int itf, cdcacm_event_t *event)
{
    uint32_t bit_rate = event->line_coding_changed_data.p_line_coding->bit_rate;
    uint8_t stop_bits = event->line_coding_changed_data.p_line_coding->stop_bits;
    uint8_t parity = event->line_coding_changed_data.p_line_coding->parity;
    uint8_t data_bits = event->line_coding_changed_data.p_line_coding->data_bits;
    ESP_LOGV(TAG, "host require bit_rate=%" PRIu32 " stop_bits=%u parity=%u data_bits=%u", bit_rate, stop_bits, parity, data_bits);

    if (s_baud_rate_active != bit_rate) {
        if (ESP_OK == uart_set_baudrate(BOARD_UART_PORT, CFG_BAUD_RATE(bit_rate))) {
            s_baud_rate_active = bit_rate;
            ESP_LOGI(TAG, "set bit_rate=%" PRIu32, bit_rate);
        }
    }

    if (s_stop_bits_active != stop_bits) {
        if (ESP_OK == uart_set_stop_bits(BOARD_UART_PORT, CFG_STOP_BITS(stop_bits))) {
            s_stop_bits_active = stop_bits;
            ESP_LOGI(TAG, "set stop_bits=%s", STR_STOP_BITS(stop_bits));
        }
    }

    if (s_parity_active != parity) {
        if (ESP_OK == uart_set_parity(BOARD_UART_PORT, CFG_PARITY(parity))) {
            s_parity_active = parity;
            ESP_LOGI(TAG, "set parity=%s", STR_PARITY(parity));
        }
    }

    if (s_data_bits_active != data_bits) {
        if (ESP_OK == uart_set_word_length(BOARD_UART_PORT, CFG_DATA_BITS(data_bits))) {
            s_data_bits_active = data_bits;
            ESP_LOGI(TAG, "set data_bits=%s", STR_DATA_BITS(data_bits));
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
        return ESP_FAIL;
    }
}

static esp_err_t usb_tx_ringbuf_read(uint8_t *out_buf, size_t out_buf_sz, size_t *rx_data_size)
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
    uint8_t data[USB_TX_BUF_SIZE] = {0};
    size_t rx_data_size = 0;
    while (1) {
        esp_err_t ret = usb_tx_ringbuf_read(data, USB_TX_BUF_SIZE, &rx_data_size);
        if (ESP_OK == ret && rx_data_size != 0) {
            xSemaphoreGive(s_usb_tx_requested);
            size_t ret = tinyusb_cdcacm_write_queue(0, data, rx_data_size);
            ESP_LOGV(TAG, "usb tx data size = %d ret=%u", rx_data_size, ret);
            tud_cdc_n_write_flush(0);
            //We wait for 10 times of the time it takes to send the data
            uint32_t timeout_ms = 10 * (rx_data_size / 64 / 19 + 1);
            if (_wait_for_usb_tx_done(timeout_ms) != ESP_OK) {
                xSemaphoreTake(s_usb_tx_requested, 0);
                ESP_LOGD(TAG, "usb tx timeout");
            }
        } else {
            ulTaskNotifyTake(pdTRUE, 1);
        }
    }
}

static void uart_read_task(void *arg)
{
    TaskHandle_t usb_tx_handle = (TaskHandle_t)arg;
    uint8_t data[UART_RX_BUF_SIZE] = {0};

    while (1) {
        const int rxBytes = uart_read_bytes(BOARD_UART_PORT, data, UART_RX_BUF_SIZE, 1);
        if (rxBytes > 0) {
            int res = xRingbufferSend(s_usb_tx_ringbuf, data, rxBytes, 0);
            if (res != pdTRUE) {
                ESP_LOGW(TAG, "The unread buffer is too small, the data has been lost");
            }
            xTaskNotifyGive(usb_tx_handle);
        }
    }
}

void app_main(void)
{
    board_uart_init();
#ifdef CONFIG_UART_AUTO_DOWNLOAD
    board_autodl_gpio_init();
#endif
    s_usb_tx_ringbuf = xRingbufferCreate(USB_TX_BUF_SIZE, RINGBUF_TYPE_BYTEBUF);
    s_usb_tx_done = xSemaphoreCreateBinary();
    s_usb_tx_requested = xSemaphoreCreateBinary();
    if (s_usb_tx_ringbuf == NULL) {
        ESP_LOGE(TAG, "Creation buffer error");
        assert(0);
    }

    tinyusb_config_t tusb_cfg = {
        .descriptor = NULL,
        .string_descriptor = NULL,
        .external_phy = false
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
    vTaskDelay(pdMS_TO_TICKS(500));
    xTaskCreate(uart_read_task, "uart_rx", 4096, (void *)usb_tx_handle, 4, NULL);

    ESP_LOGI(TAG, "USB initialization DONE");

    while (1) {
#ifdef CONFIG_UART_AUTO_DOWNLOAD
        static size_t trigger_reset = 0;
        const size_t threshold_reset = 10;

        if ((s_reset_trigger) && ++trigger_reset > threshold_reset) {
            trigger_reset = 0;
            s_reset_trigger = false;
            gpio_set_level(BOARD_AUTODLD_BOOT_PIN, true);
            gpio_set_level(BOARD_AUTODLD_EN_PIN, true);
            ESP_LOGI(TAG, "DTR = %d, RTS = %d -> BOOT = %d, RST = %d", 1, 1, 1, 1);
        } else {
            trigger_reset = 0;
        }
#endif
        vTaskDelay(pdMS_TO_TICKS(1));
    }
}
