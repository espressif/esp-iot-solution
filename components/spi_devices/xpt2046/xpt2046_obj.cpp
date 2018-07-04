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

CXpt2046::CXpt2046(xpt_conf_t * xpt_conf, int rotation)
{
    iot_xpt2046_init(xpt_conf, &m_spi);
    m_pressed = false;
    m_rotation = rotation;
    m_io_irq = xpt_conf->pin_num_irq;
}

bool CXpt2046::is_pressed()
{
    sample();
    if(gpio_get_level((gpio_num_t) m_io_irq) == 0){
        m_pressed = true;
    } else {
        m_pressed = false;
    }
    return m_pressed;
}

int CXpt2046::get_irq()
{
    return gpio_get_level((gpio_num_t) m_io_irq);
}

void CXpt2046::set_rotation(int rotation)
{
    m_rotation = rotation % 4;
}

position CXpt2046::get_raw_position()
{
    return m_pos;
}

int CXpt2046::get_sample(uint8_t command)
{
    return iot_xpt2046_readdata(m_spi, command, 1);
}

void CXpt2046::sample()
{
    position samples[XPT2046_SMPSIZE];
    position distances[XPT2046_SMPSIZE];
    m_pressed = true;

    int aveX = 0;
    int aveY = 0;

    for (int i = 0; i < XPT2046_SMPSIZE; i++) {
        samples[i].x = get_sample(TOUCH_CMD_X);
        samples[i].y = get_sample(TOUCH_CMD_Y);

        if (samples[i].x == 0 || samples[i].x == XPT2046_SMP_MAX || samples[i].y == 0
                || samples[i].y == XPT2046_SMP_MAX) {
            m_pressed = false;
        }
        aveX += samples[i].x;
        aveY += samples[i].y;
    }

    aveX /= XPT2046_SMPSIZE;
    aveY /= XPT2046_SMPSIZE;
    for (int i = 0; i < XPT2046_SMPSIZE; i++) {
        distances[i].x = i;
        distances[i].y = ((aveX - samples[i].x) * (aveX - samples[i].x))
                         + ((aveY - samples[i].y) * (aveY - samples[i].y));
    }

    // sort by distance
    for (int i = 0; i < XPT2046_SMPSIZE - 1; i++) {
        for (int j = 0; j < XPT2046_SMPSIZE - 1; j++) {
            if (samples[j].y > samples[j + 1].y) {
                int yy = samples[j + 1].y;
                samples[j + 1].y = samples[j].y;
                samples[j].y = yy;
                int xx = samples[j + 1].x;
                samples[j + 1].x = samples[j].x;
                samples[j].x = xx;
            }
        }
    }

    int tx = 0;
    int ty = 0;
    for (int i = 0; i < (XPT2046_SMPSIZE / 2); i++) {
        tx += samples[distances[i].x].x;
        ty += samples[distances[i].x].y;
    }

    tx = tx / (XPT2046_SMPSIZE / 2);
    ty = ty / (XPT2046_SMPSIZE / 2);

    switch (m_rotation) {
        case 0:
            m_pos.x = tx;
            m_pos.y = ty;
            break;
        case 3:
            m_pos.x = XPT2046_SMP_MAX - ty;
            m_pos.y = tx;
            break;
        case 2:
            m_pos.x = XPT2046_SMP_MAX - tx;
            m_pos.y = XPT2046_SMP_MAX - ty;
            break;
        case 1:
            m_pos.x = ty;
            m_pos.y = XPT2046_SMP_MAX - tx;
            break;
    }
}
