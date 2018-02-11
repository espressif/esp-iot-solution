/*
 * ESPRESSIF MIT License
 *
 * Copyright (c) 2018 <ESPRESSIF SYSTEMS (SHANGHAI) PTE LTD>
 *
 * Permission is hereby granted for use on ESPRESSIF SYSTEMS products only, in which case,
 * it is free of charge, to any person obtaining a copy of this software and associated
 * documentation files (the "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the Software is furnished
 * to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or
 * substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 */
#include "xpt2046_obj.h"

void iot_xpt2046_init(xpt_conf_t * xpt_conf, spi_device_handle_t * spi)
{
    spi_device_interface_config_t devcfg = {
        .clock_speed_hz = xpt_conf->clk_freq,     //Clock out frequency
        .mode = 0,                                //SPI mode 0
        .spics_io_num = xpt_conf->pin_num_cs,     //CS pin
        .queue_size = 7,                          //We want to be able to queue 7 transactions at a time
    };

    spi_bus_add_device((spi_host_device_t) xpt_conf->spi_host, &devcfg, spi);
}

uint16_t iot_xpt2046_readdata(spi_device_handle_t spi, const uint8_t command, int len)
{
    esp_err_t ret;
    uint8_t datas[3] = {0};
    datas[0] = command;
    if (len == 0) {
        return 0;    //no need to send anything
    }

    spi_transaction_t t = {
        .length = len * 8 * 3,              // Len is in bytes, transaction length is in bits.
        .tx_buffer = &datas,                // Data
        .flags = SPI_TRANS_USE_RXDATA,
    };
    ret = spi_device_transmit(spi, &t); //Transmit!
    assert(ret == ESP_OK);              // Should have had no issues.

    return (t.rx_data[1] << 8 | t.rx_data[2]) >> 3;
}
