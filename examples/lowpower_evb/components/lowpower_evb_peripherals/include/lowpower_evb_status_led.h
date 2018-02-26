/* Lowpower EVB Example

   For other examples please check:
   https://github.com/espressif/esp-iot-solution/tree/master/examples

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
 */

#ifndef _IOT_LOWPOWER_EVB_STATUS_LED_H_
#define _IOT_LOWPOWER_EVB_STATUS_LED_H_

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief initialize status led.
 */
void status_led_init();

/**
 * @brief set WiFi led status as quick blink.
 */
void wifi_led_quick_blink();

/**
 * @brief set WiFi led status as slow blink.
 */
void wifi_led_slow_blink();

/**
 * @brief set WiFi led status as always on.
 */
void wifi_led_on();

/**
 * @brief set network led status as quick blink.
 */
void network_led_quick_blink();

/**
 * @brief set network led status as always on.
 */
void network_led_on();

#ifdef __cplusplus
}
#endif

#endif

