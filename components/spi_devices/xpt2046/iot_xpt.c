// Copyright 2015-2016 Espressif Systems (Shanghai) PTE LTD
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
#include "iot_xpt2046.h"
#include "driver/gpio.h"

void iot_xpt2046_init(xpt_conf_t *xpt_conf, spi_device_handle_t *spi)
{
    if (xpt_conf->init_spi_bus) {
        //Initialize SPI Bus for LCD
        spi_bus_config_t buscfg = {
            .miso_io_num = xpt_conf->pin_num_miso,
            .mosi_io_num = xpt_conf->pin_num_mosi,
            .sclk_io_num = xpt_conf->pin_num_clk,
            .quadwp_io_num = -1,
            .quadhd_io_num = -1,
        };
        spi_bus_initialize(xpt_conf->spi_host, &buscfg, xpt_conf->dma_chan);
    }

    //Initialize non-SPI GPIOs
    gpio_pad_select_gpio(xpt_conf->pin_num_irq);
    gpio_set_direction(xpt_conf->pin_num_irq, GPIO_MODE_INPUT);

    spi_device_interface_config_t devcfg = {
        .clock_speed_hz = xpt_conf->clk_freq, //Clock out frequency
        .mode = 0,                            //SPI mode 0
        .spics_io_num = xpt_conf->pin_num_cs, //CS pin
        .queue_size = 7,                      //We want to be able to queue 7 transactions at a time
    };

    devcfg.flags = SPI_DEVICE_HALFDUPLEX;
    spi_bus_add_device((spi_host_device_t)xpt_conf->spi_host, &devcfg, spi);
}

uint16_t iot_xpt2046_readdata(spi_device_handle_t spi, const uint8_t command)
{
    /**
     * Half duplex mode is not compatible with DMA when both writing and reading phases exist.
     * try to use command and address field to replace the write phase.
    */
    spi_transaction_ext_t t = (spi_transaction_ext_t) {
        .base = {
            .flags = SPI_TRANS_VARIABLE_CMD | SPI_TRANS_USE_RXDATA,
            .cmd = command,
            .rxlength = 2 * 8, // Total data length received, in bits
        },
        .command_bits = 8,
    };

    assert(spi_device_transmit(spi, (spi_transaction_t *)&t) == ESP_OK);
    return (t.base.rx_data[0] << 8 | t.base.rx_data[1]) >> 3;
}
