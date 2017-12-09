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
#ifndef _IOT_CLOUD_H_
#define _IOT_CLOUD_H_

#if CONFIG_CLOUD_ALINK
#define CLOUD_ALINK

#define ALINK_INFO_NAME            CONFIG_ALINK_INFO_NAME
#define ALINK_INFO_VERSION         CONFIG_ALINK_INFO_VERSION
#define ALINK_INFO_MODEL           CONFIG_ALINK_INFO_MODEL
#define ALINK_INFO_KEY             CONFIG_ALINK_INFO_KEY
#define ALINK_INFO_SECRET          CONFIG_ALINK_INFO_SECRET
#define ALINK_INFO_KEY_SANDBOX     CONFIG_ALINK_INFO_KEY_SANDBOX
#define ALINK_INFO_SECRET_SANDBOX  CONFIG_ALINK_INFO_SECRET_SANDBOX

#if CONFIG_ALINK_VERSION_SDS
#define ALINK_INFO_KEY_DEVICE      CONFIG_ALINK_INFO_KEY_DEVICE
#define ALINK_INFO_SECRET_DEVICE   CONFIG_ALINK_INFO_SECRET_DEVICE
#endif
#endif

#if CONFIG_CLOUD_JOYLINK
#define CLOUD_JOYLINK
#endif

#ifdef CLOUD_ALINK
typedef enum {
    CLOUD_EVENT_CLOUD_CONNECTED = 0,
    CLOUD_EVENT_CLOUD_DISCONNECTED,
    CLOUD_EVENT_GET_DEVICE_DATA,
    CLOUD_EVENT_SET_DEVICE_DATA,
    CLOUD_EVENT_POST_CLOUD_DATA,
    CLOUD_EVENT_STA_GOT_IP,
    CLOUD_EVENT_STA_DISCONNECTED,
} cloud_event_t;
#else
typedef enum {
    CLOUD_EVENT_NONE = 0,
    CLOUD_VETNT_SMART_CONFIG,
    CLOUD_EVENT_STA_GOT_IP,
    CLOUD_EVENT_STA_DISCONNECTED,
    CLOUD_EVENT_CLOUD_CONNECTED,
    CLOUD_EVENT_CLOUD_DISCONNECTED,
    CLOUD_EVENT_GET_DEVICE_DATA,
    CLOUD_EVENT_SET_DEVICE_DATA,
    CLOUD_EVENT_POST_CLOUD_DATA,
} cloud_event_t;
#endif

esp_err_t cloud_init();
esp_err_t cloud_read(int ms_wait);
esp_err_t cloud_write(int ms_wait);
int cloud_sys_net_is_ready(void);
#endif
