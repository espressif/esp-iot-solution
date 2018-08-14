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
#include "driver/rtc_io.h"
#include "lowpower_evb_status_led.h"

static led_handle_t led_wifi      = NULL;
static led_handle_t led_network   = NULL;

/* Wifi and network status led */
#define WIFI_STATUS_LED_IO          (15)
#define NETWORK_STATUS_LED_IO       (4)

void status_led_init()
{
    gpio_config_t led_io_config;
    led_io_config.pin_bit_mask = ((1 << WIFI_STATUS_LED_IO) | (1 << NETWORK_STATUS_LED_IO));
    led_io_config.mode = GPIO_MODE_OUTPUT;
    led_io_config.pull_up_en = GPIO_PULLUP_DISABLE;
    led_io_config.pull_down_en = GPIO_PULLDOWN_DISABLE;
    led_io_config.intr_type = GPIO_INTR_DISABLE;

    gpio_config(&led_io_config);
    gpio_set_level((gpio_num_t)WIFI_STATUS_LED_IO, LED_DARK_HIGH);
    gpio_set_level((gpio_num_t)NETWORK_STATUS_LED_IO, LED_DARK_LOW);

    /* the GPIO led used is rtc IO, and its status is not fixed in deep-sleep mode,
     * so we need to hold the io level before enter deep-sleep mode, and disable 
     * the hold function after wake up.
     */
    rtc_gpio_hold_dis((gpio_num_t)WIFI_STATUS_LED_IO);
    rtc_gpio_hold_dis((gpio_num_t)NETWORK_STATUS_LED_IO);

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

void wifi_led_off()
{
    iot_led_state_write(led_wifi, LED_OFF);
}

void network_led_quick_blink()
{
    iot_led_state_write(led_network, LED_QUICK_BLINK);
}

void network_led_slow_blink()
{
    iot_led_state_write(led_network, LED_SLOW_BLINK);
}

void network_led_on()
{
    iot_led_state_write(led_network, LED_ON);
}

void network_led_off()
{
    iot_led_state_write(led_network, LED_OFF);
}

void led_io_rtc_hold_en()
{
    gpio_set_level((gpio_num_t)WIFI_STATUS_LED_IO, LED_DARK_HIGH);
    gpio_set_level((gpio_num_t)NETWORK_STATUS_LED_IO, LED_DARK_LOW);
    rtc_gpio_hold_en((gpio_num_t)WIFI_STATUS_LED_IO);
    rtc_gpio_hold_en((gpio_num_t)NETWORK_STATUS_LED_IO);
}
