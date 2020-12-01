// Copyright 2015-2016 Espressif Systems (Shanghai) PTE LTD
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at

//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
#include <stdio.h>
#include "sdkconfig.h"
#include "driver/ledc.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "iot_led.h"

#define QUICK_BLINK_FREQ            CONFIG_STATUS_LED_QUICK_BLINK_FREQ     /*!< default 5 */
#define SLOW_BLINK_FREQ             CONFIG_STATUS_LED_SLOW_BLINK_FREQ      /*!< default 1 */
#define LED_SPEED_MODE              CONFIG_STATUS_LED_SPEED_MODE           /*!< default LEDC_HIGH_SPEED_MODE */

#define LED_QUICK_BLINK_CHANNEL     CONFIG_STATUS_LED_QUICK_BLINK_CHANNEL  /*!< default 0 */
#define LED_SLOW_BLINK_CHANNEL      CONFIG_STATUS_LED_SLOW_BLINK_CHANNEL   /*!< default 1 */
#define LED_NIGHT_MODE_CHANNEL      CONFIG_STATUS_LED_NIGHT_MODE_CHANNEL   /*!< default 2 */


#define LED_QUICK_BLINK_TIMER       CONFIG_STATUS_LED_QUICK_BLINK_TIMER    /*!< default 0 */
#define LED_SLOW_BLINK_TIMER        CONFIG_STATUS_LED_SLOW_BLINK_TIMER     /*!< default 1 */
#define LED_NORMAL_TIMER            CONFIG_STATUS_LED_NIGHT_MODE_TIMER     /*!< default 2 */

#define LED_LONG_BRIGHT_FRE         5000
#define LED_TIMER_BIT               LEDC_TIMER_13_BIT
#define LED_BRIGHT_DUTY             ((1 << LED_TIMER_BIT) - 1)
#define LED_SLOW_BLINK_DUTY         (LED_BRIGHT_DUTY / 2)
#define LED_QUICK_BLINK_DUTY        (LED_BRIGHT_DUTY / 2)
#define IOT_CHECK(tag, a, ret)  if(!(a)) {                                 \
        ESP_LOGE(tag,"%s:%d (%s)", __FILE__, __LINE__, __FUNCTION__);      \
        return (ret);                                                      \
        }

#define ERR_ASSERT(tag, param)      IOT_CHECK(tag, (param) == ESP_OK, ESP_FAIL)
#define POINT_ASSERT(tag, param)	IOT_CHECK(tag, (param) != NULL, ESP_FAIL)
#define RES_ASSERT(tag, res, ret)   IOT_CHECK(tag, (res) != pdFALSE, ret)

static const char* TAG = "LED";
static uint8_t g_night_duty = 0;
static bool g_init_flg = false;
typedef struct {
    uint8_t io_num;
    led_status_t state;
    led_dark_level_t dark_level;
    led_mode_t mode;
    uint8_t night_duty;
} led_dev_t;

static esp_err_t led_ledctimer_set(ledc_timer_t timer_num, uint32_t freq_hz, ledc_mode_t speed_mode)
{
    ledc_timer_config_t ledc_timer = {
        .duty_resolution = LED_TIMER_BIT,
        .freq_hz = freq_hz,
        .speed_mode = speed_mode,
        .timer_num = timer_num
    };
    ERR_ASSERT(TAG, ledc_timer_config(&ledc_timer)); 
    return ESP_OK;
}

static esp_err_t gpio_ledc_bind(ledc_timer_t timer_num, ledc_channel_t channel, int gpio_num,
                                   uint32_t duty, ledc_mode_t speed_mode)
{
    ledc_channel_config_t ledc_channel = {
        .channel = channel,
        .duty = duty,
        .gpio_num = gpio_num,
        .intr_type = LEDC_INTR_FADE_END,
        .speed_mode = speed_mode,
        .timer_sel = timer_num
    };
    ERR_ASSERT(TAG, ledc_channel_config(&ledc_channel));
    return ESP_OK;
}

static esp_err_t led_level_set(led_handle_t led_handle, uint8_t level)
{
    led_dev_t* led_dev = (led_dev_t*) led_handle;
    if (level) {
        if (led_dev->mode == LED_NIGHT_MODE) {
            ERR_ASSERT(TAG, gpio_ledc_bind(LED_NORMAL_TIMER, LED_NIGHT_MODE_CHANNEL, led_dev->io_num, 
                        g_night_duty * LED_BRIGHT_DUTY / 100, LED_SPEED_MODE));
        }
        else {
            gpio_set_level(led_dev->io_num, (~(led_dev->dark_level)) & 0x1);
            gpio_matrix_out(led_dev->io_num, SIG_GPIO_OUT_IDX, 0, 0);
        }
    }
    else {
        gpio_set_level(led_dev->io_num, led_dev->dark_level);
        gpio_matrix_out(led_dev->io_num, SIG_GPIO_OUT_IDX, 0, 0);
    }
    return ESP_OK;
}

static esp_err_t led_quick_blink(led_handle_t led_handle)
{
    led_dev_t* led_dev = (led_dev_t*) led_handle;
    ERR_ASSERT(TAG, gpio_ledc_bind(LED_QUICK_BLINK_TIMER, LED_QUICK_BLINK_CHANNEL, led_dev->io_num, LED_QUICK_BLINK_DUTY, LED_SPEED_MODE));
    return ESP_OK;
}

static esp_err_t led_slow_blink(led_handle_t led_handle)
{
    led_dev_t* led_dev = (led_dev_t*) led_handle;
    ERR_ASSERT(TAG, gpio_ledc_bind(LED_SLOW_BLINK_TIMER, LED_SLOW_BLINK_CHANNEL, led_dev->io_num, LED_SLOW_BLINK_DUTY, LED_SPEED_MODE));
    return ESP_OK;
}

esp_err_t iot_led_update_blink_freq(int quick_blink_freq, int slow_blink_freq)
{
    ERR_ASSERT(TAG, led_ledctimer_set(LED_QUICK_BLINK_TIMER, quick_blink_freq, LED_SPEED_MODE));
    ERR_ASSERT(TAG, led_ledctimer_set(LED_SLOW_BLINK_TIMER, slow_blink_freq, LED_SPEED_MODE));
    return ESP_OK;
}

esp_err_t iot_led_setup()
{
    ERR_ASSERT(TAG, led_ledctimer_set(LED_NORMAL_TIMER, LED_LONG_BRIGHT_FRE, LED_SPEED_MODE));
    iot_led_update_blink_freq(QUICK_BLINK_FREQ, SLOW_BLINK_FREQ);
    return ESP_OK;
}

led_handle_t iot_led_create(uint8_t io_num, led_dark_level_t dark_level)
{
    if(!g_init_flg) {
        iot_led_setup();
        g_init_flg = true;
    }
    led_dev_t *led_p = (led_dev_t*)calloc(1, sizeof(led_dev_t));
    led_p->io_num = io_num;
    led_p->dark_level = dark_level;
    led_p->state = LED_OFF;
    led_p->mode = LED_NORMAL_MODE;
    iot_led_state_write(led_p, LED_OFF);
    return (led_handle_t) led_p;
}

esp_err_t iot_led_delete(led_handle_t led_handle)
{
    POINT_ASSERT(TAG, led_handle);
    free(led_handle);
    return ESP_OK;
}

esp_err_t iot_led_state_write(led_handle_t led_handle, led_status_t state)
{
    led_dev_t* led_dev = (led_dev_t*) led_handle;
    POINT_ASSERT(TAG, led_dev);
    switch (state) {
        case LED_OFF:
            led_level_set(led_handle, 0);
            break;
        case LED_ON:
            led_level_set(led_handle, 1);
            break;
        case LED_QUICK_BLINK:
            led_quick_blink(led_handle);
            break;
        case LED_SLOW_BLINK:
            led_slow_blink(led_handle);
            break;
        default:
            break;
    }
    led_dev->state = state;
    return ESP_OK;
}

esp_err_t iot_led_mode_write(led_handle_t led_handle, led_mode_t mode)
{
    led_dev_t* led_dev = (led_dev_t*) led_handle;
    POINT_ASSERT(TAG, led_dev);
    led_dev->mode = mode;
    iot_led_state_write(led_handle, led_dev->state);
    return ESP_OK;
}

esp_err_t iot_led_night_duty_write(uint8_t duty)
{
    IOT_CHECK(TAG, duty <= 100, ESP_FAIL);
    g_night_duty = duty;
    return ESP_OK;
}

uint8_t iot_led_night_duty_read()
{
    return g_night_duty;
}

led_status_t iot_led_state_read(led_handle_t led_handle)
{
    led_dev_t* led_dev = (led_dev_t*) led_handle;
    return led_dev->state;
}

led_mode_t iot_led_mode_read(led_handle_t led_handle)
{
    led_dev_t* led_dev = (led_dev_t*) led_handle;
    return led_dev->mode;
}


