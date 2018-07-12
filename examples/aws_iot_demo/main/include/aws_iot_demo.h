/*
   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#ifndef _IOT_AWS_IOT_TASK_H_
#define _IOT_AWS_IOT_TASK_H_


/* The examples use simple WiFi configuration that you can set via
 'make menuconfig'.

 If you'd rather not, just change the below entries to strings with
 the config you want - ie #define EXAMPLE_WIFI_SSID "mywifissid"
 */
#define EXAMPLE_WIFI_SSID CONFIG_WIFI_SSID
#define EXAMPLE_WIFI_PASS CONFIG_WIFI_PASSWORD
#define SUBSCRIBE_TOPIC CONFIG_ESP32_SUBSCRIBE_TOPIC
#define PUBLISH_TOPIC   CONFIG_ESP32_PUBLISH_TOPIC
#define PUB_TOPIC_LEN   strlen(PUBLISH_TOPIC)
#define TOPIC_LEN       strlen(SUBSCRIBE_TOPIC)

void aws_iot_task(void *param);
void app_lcd_init();
void app_lcd_wifi_connecting();
void app_lcd_aws_sub_cb(void* pdata);
void app_lcd_aws_connecting();
void app_lcd_aws_connected();
void app_lcd_aws_pub_cb(void* pdata);

#endif
