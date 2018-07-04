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

/* C Includes */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* RTOS Includes */
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "freertos/semphr.h"

/* SPI Includes */
#include "iot_xpt2046.h"
#include "XPT2046_adapter.h"

/* uGFX Config Include */
#include "sdkconfig.h"

static CXpt2046* xpt = NULL;

class CTouchAdapter: public CXpt2046
{
public:
    CTouchAdapter(xpt_conf_t * xpt_conf, int rotation):CXpt2046(xpt_conf, rotation)
    {
        /* Code here*/
    }
};

void board_touch_init()
{
    xpt_conf_t xpt_conf;
    xpt_conf.pin_num_cs   = CONFIG_UGFX_TOUCH_CS_GPIO;        /*!<SPI Chip Select Pin*/
    xpt_conf.pin_num_irq  = CONFIG_UGFX_TOUCH_IRQ_GPIO;       /*!< Touch screen IRQ pin */
    xpt_conf.clk_freq     = 1 * 1000 * 1000;                  /*!< spi clock frequency */
    xpt_conf.spi_host     = HSPI_HOST;                        /*!< spi host index*/
    xpt_conf.pin_num_miso = -1;                               /*!<MasterIn, SlaveOut pin*/
    xpt_conf.pin_num_mosi = -1;                               /*!<MasterOut, SlaveIn pin*/
    xpt_conf.pin_num_clk  = -1;                               /*!<SPI Clock pin*/
    xpt_conf.dma_chan     = 1;
    xpt_conf.init_spi_bus = false;                            /*!< Whether to initialize SPI bus */

    if(xpt == NULL) {
        xpt= new CTouchAdapter(&xpt_conf, 0);
    }
}

bool board_touch_is_pressed()
{
    return xpt->is_pressed();
}

int board_touch_get_position(int port)
{
    return xpt->get_sample(port);
}
