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

#include "esp_log.h"
#include "iot_xpt2046.h"
#include "iot_ft5x06.h"


#ifdef CONFIG_TOUCH_DRIVER_XPT2046
extern touch_driver_fun_t xpt2046_driver_fun;
static touch_driver_fun_t *g_touch_driver = &xpt2046_driver_fun;

#elif defined CONFIG_TOUCH_DRIVER_FT5X06
extern touch_driver_fun_t ft5x06_driver_fun;
static touch_driver_fun_t *g_touch_driver = &ft5x06_driver_fun;

#elif defined CONFIG_TOUCH_DRIVER_NS2016
extern touch_driver_fun_t ns2016_driver_fun;
static touch_driver_fun_t *g_touch_driver = &ns2016_driver_fun;

#endif

esp_err_t iot_touch_init(touch_driver_fun_t *driver)
{
    lcd_touch_config_t conf = {
#ifdef CONFIG_TOUCH_DRIVER_INTERFACE_I2C
        .iface_i2c = {
            .pin_num_sda = CONFIG_TOUCH_I2C_SDA_PIN,
            .pin_num_scl = CONFIG_TOUCH_I2C_SCL_PIN,
            .clk_freq = 100000,
            .i2c_port = CONFIG_TOUCH_I2C_PORT_NUM,
            .i2c_addr = CONFIG_TOUCH_I2C_ADDRESS,
        },
#endif
#ifdef CONFIG_TOUCH_DRIVER_INTERFACE_SPI
        .iface_spi = {
#ifdef CONFIG_TOUCH_SPI_USE_HOST
            .pin_num_miso = CONFIG_TOUCH_SPI_MISO_PIN,
            .pin_num_mosi = CONFIG_TOUCH_SPI_MOSI_PIN,
            .pin_num_clk = CONFIG_TOUCH_SPI_CLK_PIN,
            .clk_freq = CONFIG_TOUCH_SPI_CLOCK_FREQ,
            .spi_host = CONFIG_TOUCH_SPI_HOST,
            .init_spi_bus = true,
#else
            .pin_num_miso = -1,
            .pin_num_mosi = -1,
            .pin_num_clk = -1,
            .clk_freq = 5000000,
            .spi_host = CONFIG_LCD_SPI_HOST,
            .init_spi_bus = false,
#endif
            .pin_num_cs = CONFIG_TOUCH_SPI_CS_PIN,
            .dma_chan = 2,
            
        },
#endif
        .pin_num_int = CONFIG_TOUCH_INT_PIN_NUM,
        .direction = CONFIG_TOUCH_DIRECTION,
        .width = CONFIG_TOUCH_SCREEN_WIDTH,
        .height = CONFIG_TOUCH_SCREEN_HEIGHT,
    };
    esp_err_t ret = g_touch_driver->init(&conf);
    *driver = *g_touch_driver;
    return ret;
}


