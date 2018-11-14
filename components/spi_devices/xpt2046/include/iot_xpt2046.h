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

#ifndef _XPT2046_H
#define _XPT2046_H

#include "string.h"
#include "stdio.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "freertos/semphr.h"
#include "esp_log.h"

#define TOUCH_CMD_X       0xD0
#define TOUCH_CMD_Y       0x90
#define XPT2046_SMPSIZE   10
#define XPT2046_SMP_MAX   4095

typedef struct position {
    int x;
    int y;
} position;

typedef struct {
    int8_t pin_num_cs;          /*!< SPI Chip Select Pin*/
    int8_t pin_num_irq;         /*!< Touch screen IRQ pin */
    int clk_freq;               /*!< spi clock frequency */
    spi_host_device_t spi_host; /*!< spi host index*/

    int8_t pin_num_miso;        /*!< MasterIn, SlaveOut pin*/
    int8_t pin_num_mosi;        /*!< MasterOut, SlaveIn pin*/
    int8_t pin_num_clk;         /*!< SPI Clock pin*/
    uint8_t dma_chan;
    bool init_spi_bus;          /*!< Whether to initialize SPI bus */
} xpt_conf_t;

#ifdef __cplusplus
extern "C"
{
#endif

/**
 * @brief   xpt2046 device initlize
 *
 * @param   xpt_conf pointer of xpt_conf_t
 * @param   spi pointer of spi_device_handle_t
 */
void iot_xpt2046_init(xpt_conf_t * xpt_conf, spi_device_handle_t * spi);

/**
 * @brief   read xpt2046 data
 *
 * @param   spi spi_handle_t
 * @param   command command of read data
 * @param   len command len
 *
 * @return
 *     - read data
 */
uint16_t iot_xpt2046_readdata(spi_device_handle_t spi, const uint8_t command);

#ifdef __cplusplus
}
#endif

#ifdef __cplusplus

class CXpt2046
{

private:
    spi_device_handle_t m_spi = NULL;
    position m_pos;
    bool m_pressed;
    int m_rotation;
    int m_io_irq;
    SemaphoreHandle_t m_spi_mux = NULL;

public:
    /**
     * @brief   constructor of CXpt2046
     *
     * @param   xpt_conf point of xpt_conf_t
     * @param   rotation screen rotation
     */
    CXpt2046(xpt_conf_t * xpt_conf, int rotation = 0);

    /**
     * @brief   get raw position struct(raw data)
     *
     * @return 
     *     - position of pressed
     */
    position get_raw_position(void);

    /**
     * @brief   read whether pressed
     *
     * @return 
     *     - true pressed
     *     - false not pressed
     */
    bool is_pressed(void);

    /**
     * @brief   read irq pin value
     *
     * @return 
     *     - value of irq pin value
     */
    int get_irq();

    /**
     * @brief   set rotation
     * 
     * @param   rotation screen rotation value, eg:0,1,2,3
     */
    void set_rotation(int rotation);

    /**
     * @brief   get sample value
     *
     * @param   command command of sample
     *
     * @return
     *     - sample value
     */
    int get_sample(uint8_t command);

    /**
     * @brief   sample and calculator
     */
    void sample(void);

};
#endif

#endif
