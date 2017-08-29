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

#ifndef _IOT_JOYLINK_OBJ_H_
#define _IOT_JOYLINK_OBJ_H_

#include "jd_innet.h"
#include "esp_joylink.h"
#include "joylink_config.h"

#ifdef __cplusplus
#include "cloud_obj.h"

/**
 * class of joylink cloud
 * simple usage:
 * joylink_info_t product_info;
 * ......(set product_info)
 * p_joylink = CJoylink::GetInstance(product_info);
 * p_joylink->read(down_cmd, JOYLINK_DATA_LEN, portMAX_DELAY);
 * esp_err_t ret = p_joylink->write(up_cmd, JOYLINK_DATA_LEN, 1000);
 */
class CJoylink: public CCloud
{
private:
    // the only instance of CJoylink for singleton mode
    static CJoylink* m_instance;

    /**
     * prevent constructing in singleton mode
     */
    CJoylink(const joylink_info_t &product_info);
    CJoylink(const CJoylink&);
    CJoylink& operator=(const CJoylink&);

    /**
     * prevent memory leak of m_instance  
     */
    class CGarbo
    {
    public:
        ~CGarbo()
        {
            if (CJoylink::m_instance) {
                delete CJoylink::m_instance;
            }
        }
    };
    static CGarbo garbo;

public:
    /**
     * @brief get the only instance of CJoylink
     *
     * @param product_info the information of product
     *
     * @return pointer to a CJoylink instance
     */
    static CJoylink* GetInstance(const joylink_info_t &product_info);

    /**
     * @brief register the event callback function
     *
     * @param cb a pointer to callback function
     * 
     * @reurn
     *      - 0  sucess
     *      - -1 fail
     */
    int event_init(joylink_event_cb_t cb);

    /**
     * @brief send data to server
     *
     * @param up_cmd a pointer to json str
     * @param size length of up_cmd
     * @param micro_seconds blocking time
     *
     * @return
     *      - 0  success
     *      - -1 fail
     */
    virtual int write(void *up_cmd, size_t size, int micro_seconds);

    /**
     * @brief receive data from server
     *
     * @param down_cmd a pointer to json str which saves down commands
     * @param size length of down_cmd
     * @param micro_seconds blocking time
     *
     * @return
     *      - 0  success
     *      - -1 fail
     */
    virtual int read(void *down_cmd, size_t size, int  micro_seconds);
    
    virtual ~CJoylink();
};

#endif

#endif 
