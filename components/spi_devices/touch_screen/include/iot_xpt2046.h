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
#ifndef _IOT_XPT2046_H
#define _IOT_XPT2046_H

#include "string.h"
#include "stdio.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_partition.h"
#include "freertos/semphr.h"
#include "esp_log.h"

#define TOUCH_CMD_X       0xD0
#define TOUCH_CMD_Y       0x90
#define XPT2046_SMPSIZE   20

typedef struct position {
    int x;
    int y;
} position;

typedef struct {
    int8_t pin_num_cs;          /*!<SPI Chip Select Pin*/
    int8_t pin_num_irq;         /*!< Touch screen IRQ pin */
    int clk_freq;               /*!< spi clock frequency */
    spi_host_device_t spi_host; /*!< spi host index*/

    int8_t pin_num_miso;        /*!<MasterIn, SlaveOut pin*/
    int8_t pin_num_mosi;        /*!<MasterOut, SlaveIn pin*/
    int8_t pin_num_clk;         /*!<SPI Clock pin*/
    uint8_t dma_chan;
    bool init_spi_bus;          /*!< Whether to initialize SPI bus */
} xpt_conf_t;

#ifdef __cplusplus
class CXpt2046
{

private:
    spi_device_handle_t m_spi = NULL;
    position m_pos;
    bool m_pressed;
    int m_rotation;
    float m_xfactor;
    float m_yfactor;
    int m_offset_x;
    int m_offset_y;
    int m_width;
    int m_height;
    int m_io_irq;

public:
    /**
     * @brief   constructor of CXpt2046
     *
     * @param   xpt_conf point of xpt_conf_t
     * @param   w width
     * @param   h height
     */
    CXpt2046(xpt_conf_t * xpt_conf, int w, int h, float xfactor = 0.071006,
             float yfactor = 0.086557, int xoffset = -24, int yoffset = -13);

    /**
     * @brief   get position struct
     *
     * @return
     *     - position position of pressed
     */
    position getposition(void);

    /**
     * @brief   read position of pressed point
     *
     * @return
     *     - int x position
     */
    int x(void);

    /**
     * @brief   read position of pressed point
     *
     * @return
     *     - int y position
     */
    int y(void);

    /**
     * @brief   read whether pressed
     *
     * @return
     *     - bool is pressed
     */
    bool is_pressed(void);
    int get_irq();


    /**
     * @brief   set rotation
     *
     * touch self:
     *------------------------> (height)
     *|
     *|
     *|
     *|
     *|
     *|
     *V(width)
     * we make a rotate while use x() or y() function
     *
     * @return
     *     - void
     */
    void set_rotation(int r);

    /**
     * @brief   get sample data
     *
     * @param   command command of sample
     *
     * @return
     *     - int sample data x of y
     */
    int get_sample(uint8_t command);

    /**
     * @brief   sample and calculator
     *
     * @return
     *     - void
     */
    void sample(void);

    /**
     * @brief   calibration touch device
     *
     * @return
     *     - void
     */
    void calibration(void);

    /**
     * @brief   set offset
     *
     * @return
     *     - void
     */
    void set_offset(float xfactor, float yfactor, int x_offset, int y_offset);

};
#endif

#endif
