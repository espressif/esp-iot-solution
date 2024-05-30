/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "data_back.h"
#include "sdkconfig.h"
#include "tinyusb.h"

#ifndef CONFIG_UART_ENABLE
#define CONFIG_UART_ENABLE 0
#endif

#if CONFIG_UART_ENABLE
#include "driver/uart.h"
#define UART_NUM    (CONFIG_UART_PORT_NUM)
#elif CFG_TUD_CDCACM
#include "tusb_cdc_acm.h"
#define ITF_NUM_CDC   0
#endif

void esp_data_back(void* data_buf, size_t length, bool flush)
{
#if CONFIG_UART_ENABLE
    uart_write_bytes(UART_NUM, (char*)data_buf, length);
#elif CFG_TUD_CDCACM
    tinyusb_cdcacm_write_queue(ITF_NUM_CDC, (uint8_t*)data_buf, length);
    if (flush) {
        tinyusb_cdcacm_write_flush(ITF_NUM_CDC, 0);
    }
#endif /* CONFIG_UART_ENABLE */
}
