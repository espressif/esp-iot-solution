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

#ifndef _IOT_ALINK_OBJ_H_
#define _IOT_ALINK_OBJ_H_

#include "product.h"
#include "esp_alink.h"
#include "alink_config.h"

#ifdef __cplusplus
#include "cloud_obj.h"

/**
 * class of alink cloud
 * simple usage:
 * alink_product_t product_info;
 * ......(set product_info)
 * p_alink = CAlink::GetInstance(product_info);
 * p_alink->read(down_cmd, ALINK_DATA_LEN, portMAX_DELAY);
 * esp_err_t ret = p_alink->write(up_cmd, ALINK_DATA_LEN, 1000);
 */
class CAlink: public CCloud
{
private:
    // the only instance of CAlink for singleton mode
    static CAlink* m_instance;

    /**
     * prevent constructing in singleton mode
     */
    CAlink(const alink_product_t &product_info);
    CAlink(const CAlink&);
    CAlink& operator=(const CAlink&);

    /**
     * prevent memory leak of m_instance  
     */
    class CGarbo
    {
    public:
        ~CGarbo()
        {
            if (CAlink::m_instance) {
                delete CAlink::m_instance;
            }
        }
    };
    static CGarbo garbo;

public:
    /**
     * @brief get the only instance of CAlink
     * 
     * @param product_info the information of product
     *
     * @return pointer to a CAlink instance
     */
    static CAlink* GetInstance(const alink_product_t& product_info);

    /**
     * @brief register the event callback function
     * 
     * @param cb a pointer to callback function
     *
     * @return
     *      - 0  success
     *      - -1 fail
     */
    int event_init(alink_event_cb_t cb);

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
   
    virtual ~CAlink();
};

#endif

#endif
