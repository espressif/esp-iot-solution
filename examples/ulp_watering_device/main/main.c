/* ULP Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "nvs.h"
#include "nvs_flash.h"
#include "soc/rtc_cntl_reg.h"
#include "soc/sens_reg.h"
#include "driver/gpio.h"
#include "driver/rtc_io.h"
#include "driver/adc.h"
#include "driver/dac.h"
#include "esp32/ulp.h"
#include "esp_sleep.h"
#include "ulp_main.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"


#define ERROR_INDICATE_LED      26
#define RELAY_CONTROLS_PIN      4

extern const uint8_t ulp_main_bin_start[] asm("_binary_ulp_main_bin_start");
extern const uint8_t ulp_main_bin_end[]   asm("_binary_ulp_main_bin_end");

/* This function is called once after power-on reset, to load ULP program into
 * RTC memory and configure the ADC.
 */
static void init_ulp_program();

/* This function is called every time before going into deep sleep.
 * It starts the ULP program and resets measurement counter.
 */
static void start_ulp_program();


void app_main()
{
    esp_sleep_wakeup_cause_t cause = esp_sleep_get_wakeup_cause();
    if (cause != ESP_SLEEP_WAKEUP_ULP) {
        printf("Not ULP wakeup\n");
        init_ulp_program();
    } else {
        printf("Deep sleep wakeup\n");
        printf("ULP did %d measurements since last reset\n", ulp_sample_counter & UINT16_MAX);
        printf("Thresholds:  low_thr=%d  error_thr=%d\n", ulp_low_thr, ulp_error_thr);
        ulp_last_result &= UINT16_MAX;
        printf("adc Value=%d \n", ulp_last_result);

        /* if sensor error, LED blink 5 times */
        if(ulp_last_result > 4000)  
        {
            rtc_gpio_hold_dis(ERROR_INDICATE_LED);
            for(uint32_t i = 0; i < 10; i++)
            {              
                if (i % 2 == 0) {
                    rtc_gpio_set_level(ERROR_INDICATE_LED, 1);
                }else{
                    rtc_gpio_set_level(ERROR_INDICATE_LED, 0);
                }
                vTaskDelay(50);   
            }
            rtc_gpio_hold_en(ERROR_INDICATE_LED);
        }
    }
    printf("Entering deep sleep\n\n");
    start_ulp_program();
    ESP_ERROR_CHECK( esp_sleep_enable_ulp_wakeup() );
    esp_deep_sleep_start();
}

static void init_ulp_program()
{
    esp_err_t err = ulp_load_binary(0, ulp_main_bin_start,
            (ulp_main_bin_end - ulp_main_bin_start) / sizeof(uint32_t));
    ESP_ERROR_CHECK(err);

    /* Configure ADC channel */
    /* Note: when changing channel here, also change 'adc_channel' constant
       in adc.S */
    adc1_config_channel_atten(ADC1_CHANNEL_6, ADC_ATTEN_DB_11);
    adc1_config_width(ADC_WIDTH_BIT_12);
    adc1_ulp_enable();

    /* configure sensor indicate LED */
    rtc_gpio_init(ERROR_INDICATE_LED);
    rtc_gpio_set_direction(ERROR_INDICATE_LED, RTC_GPIO_MODE_OUTPUT_ONLY);
    rtc_gpio_set_level(ERROR_INDICATE_LED, 0);

    /* configure relay control pin */
    rtc_gpio_init(RELAY_CONTROLS_PIN);
    rtc_gpio_set_direction(RELAY_CONTROLS_PIN, RTC_GPIO_MODE_OUTPUT_ONLY);

    /* Set low and high thresholds, approx. 1.35V - 1.75V*/
    ulp_low_thr = 2000;
    ulp_error_thr = 4000;

    /* Set ULP wake up period to 3s */
    ulp_set_wakeup_period(0, 3*1000*1000);

}

static void start_ulp_program()
{
    /* Reset sample counter */
    ulp_sample_counter = 0;

    /* Start the program */
    esp_err_t err = ulp_run((&ulp_entry - RTC_SLOW_MEM) / sizeof(uint32_t));
    ESP_ERROR_CHECK(err);
}


