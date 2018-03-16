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

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "sdkconfig.h"

#if CONFIG_CLOUD_ALINK
#define CLOUD_ALINK

#if CONFIG_SMART_LIGHT_DEVICE
#define ALINK_INFO_NAME            CONFIG_ALINK_INFO_LIGHT_NAME
#define ALINK_INFO_VERSION         CONFIG_ALINK_INFO_LIGHT_VERSION
#define ALINK_INFO_MODEL           CONFIG_ALINK_INFO_LIGHT_MODEL
#define ALINK_INFO_KEY             CONFIG_ALINK_INFO_LIGHT_KEY
#define ALINK_INFO_SECRET          CONFIG_ALINK_INFO_LIGHT_SECRET
#define ALINK_INFO_KEY_SANDBOX     CONFIG_ALINK_INFO_LIGHT_KEY_SANDBOX
#define ALINK_INFO_SECRET_SANDBOX  CONFIG_ALINK_INFO_LIGHT_SECRET_SANDBOX
#if CONFIG_ALINK_VERSION_SDS
#define ALINK_INFO_KEY_DEVICE      CONFIG_ALINK_INFO_LIGHT_KEY_DEVICE
#define ALINK_INFO_SECRET_DEVICE   CONFIG_ALINK_INFO_LIGHT_SECRET_DEVICE
#endif

#elif CONFIG_SMART_PLUG_DEVICE
#define ALINK_INFO_NAME            CONFIG_ALINK_INFO_PLUG_NAME
#define ALINK_INFO_VERSION         CONFIG_ALINK_INFO_PLUG_VERSION
#define ALINK_INFO_MODEL           CONFIG_ALINK_INFO_PLUG_MODEL
#define ALINK_INFO_KEY             CONFIG_ALINK_INFO_PLUG_KEY
#define ALINK_INFO_SECRET          CONFIG_ALINK_INFO_PLUG_SECRET
#define ALINK_INFO_KEY_SANDBOX     CONFIG_ALINK_INFO_PLUG_KEY_SANDBOX
#define ALINK_INFO_SECRET_SANDBOX  CONFIG_ALINK_INFO_PLUG_SECRET_SANDBOX
#if CONFIG_ALINK_VERSION_SDS
#define ALINK_INFO_KEY_DEVICE      CONFIG_ALINK_INFO_PLUG_KEY_DEVICE
#define ALINK_INFO_SECRET_DEVICE   CONFIG_ALINK_INFO_PLUG_SECRET_DEVICE
#endif
#endif
#endif

#if CONFIG_CLOUD_JOYLINK
#define CLOUD_JOYLINK
#endif

esp_err_t cloud_init(SemaphoreHandle_t xSemWriteInfo);
esp_err_t cloud_read(int ms_wait);
esp_err_t cloud_write(int ms_wait);
int cloud_sys_net_is_ready(void);
#endif
