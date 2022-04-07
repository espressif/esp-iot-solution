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

void esp_data_back(void* data_buf, size_t lenth, bool flush)
{
#if CONFIG_UART_ENABLE
    uart_write_bytes(UART_NUM, (char*)data_buf, lenth);
#elif CFG_TUD_CDCACM
    tinyusb_cdcacm_write_queue(ITF_NUM_CDC, (uint8_t*)data_buf, lenth);
    if (flush) {
        tinyusb_cdcacm_write_flush(ITF_NUM_CDC, 0);
    }
#endif /* CONFIG_UART_ENABLE */
}