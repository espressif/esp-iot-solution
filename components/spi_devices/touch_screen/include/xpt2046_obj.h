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

#ifndef _XPT2046_H
#define _XPT2046_H

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
    uint8_t pin_num_cs; /*!<SPI Chip Select Pin*/
    int clk_freq; /*!< spi clock frequency */
    spi_host_device_t spi_host; /*!< spi host index*/
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
 *
 * @return
 *     - void
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
 *     - int x position
 */
uint16_t iot_xpt2046_readdata(spi_device_handle_t spi, const uint8_t command,
                              int len);

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
    float m_xfactor;
    float m_yfactor;
    int m_offset_x;
    int m_offset_y;
    int m_width;
    int m_height;

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
