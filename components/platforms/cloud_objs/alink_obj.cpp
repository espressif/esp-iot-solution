/*
  * ESPRESSIF MIT License
  *
  * Copyright (c) 2017 <ESPRESSIF SYSTEMS (SHANGHAI) PTE LTD>
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

#include "alink_obj.h"
static xSemaphoreHandle s_alink_mux = xSemaphoreCreateMutex();
CAlink* CAlink::m_instance = NULL;

CAlink* CAlink::GetInstance(const alink_product_t& product_info)
{
    if (m_instance == NULL) {
        xSemaphoreTake(s_alink_mux, portMAX_DELAY);
        if (m_instance == NULL) {
            m_instance = new CAlink(product_info);
        }
        xSemaphoreGive(s_alink_mux);
    }
    return m_instance;
}

CAlink::CAlink(const alink_product_t &product_info)
{
    esp_alink_init((void*)&product_info);
}

CAlink::~CAlink()
{

}

int CAlink::event_init(alink_event_cb_t cb)
{
    return esp_alink_event_init(cb);
}

int CAlink::write(void *up_cmd, size_t size, int micro_seconds)
{
    return esp_alink_write(up_cmd, size, micro_seconds);
}

int CAlink::read(void *down_cmd, size_t size, int micro_seconds)
{
    return esp_alink_read(down_cmd, size, micro_seconds);
}
