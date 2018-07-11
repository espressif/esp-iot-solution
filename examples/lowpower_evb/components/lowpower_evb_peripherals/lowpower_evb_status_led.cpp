/* Lowpower EVB Example

   For other examples please check:
   https://github.com/espressif/esp-iot-solution/tree/master/examples

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
 */

#include "driver/gpio.h"
#include "iot_led.h"
#include "lowpower_evb_status_led.h"

static led_handle_t led_wifi      = NULL;
static led_handle_t led_network   = NULL;

/* Wifi and network status led */
#define WIFI_STATUS_LED_IO          ((gpio_num_t) 15)
#define NETWORK_STATUS_LED_IO       ((gpio_num_t) 4)

void status_led_init()
{
    led_wifi = iot_led_create(WIFI_STATUS_LED_IO, LED_DARK_HIGH);
    led_network = iot_led_create(NETWORK_STATUS_LED_IO, LED_DARK_LOW);
}

void wifi_led_quick_blink()
{
    iot_led_state_write(led_wifi, LED_QUICK_BLINK);
}

void wifi_led_slow_blink()
{
    iot_led_state_write(led_wifi, LED_SLOW_BLINK);
}

void wifi_led_on()
{
    iot_led_state_write(led_wifi, LED_ON);
}

void network_led_quick_blink()
{
    iot_led_state_write(led_network, LED_QUICK_BLINK);
}

void network_led_on()
{
    iot_led_state_write(led_network, LED_ON);
}

