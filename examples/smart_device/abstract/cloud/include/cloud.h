/* Smart device Example

   For other examples please check:
   https://github.com/espressif/esp-iot-solution/tree/master/examples

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
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
