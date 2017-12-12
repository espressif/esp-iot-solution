/* ULP Example: Control RTC GPIO in deep sleep

   For other examples please check:
   https://github.com/espressif/esp-iot-solution/tree/master/examples

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
 */

#include <stdio.h>
#include "esp_sleep.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "soc/rtc_cntl_reg.h"
#include "soc/rtc_io_reg.h"
#include "soc/sens_reg.h"
#include "soc/soc.h"
#include "driver/gpio.h"
#include "driver/rtc_io.h"
#include "esp32/ulp.h"
#include "ulp_main.h"

extern const uint8_t ulp_main_bin_start[] asm("_binary_ulp_main_bin_start");
extern const uint8_t ulp_main_bin_end[]   asm("_binary_ulp_main_bin_end");


gpio_num_t cpu_up_num = GPIO_NUM_25;
gpio_num_t ulp_up_num = GPIO_NUM_4;

gpio_num_t cpu_toggle_num = GPIO_NUM_26;
gpio_num_t ulp_toggle_num = GPIO_NUM_27;

RTC_DATA_ATTR static int cpu_toggle_counter;

static void init_ulp_program()
{
    esp_err_t err = ulp_load_binary(0, ulp_main_bin_start,(ulp_main_bin_end - ulp_main_bin_start) / sizeof(uint32_t));         
    ESP_ERROR_CHECK(err);

    ulp_toggle_counter = 0;

    rtc_gpio_init(cpu_up_num);
    rtc_gpio_set_direction(cpu_up_num, RTC_GPIO_MODE_OUTPUT_ONLY);
    rtc_gpio_set_level(cpu_up_num, 1);

    rtc_gpio_init(ulp_up_num);
    rtc_gpio_set_direction(ulp_up_num, RTC_GPIO_MODE_OUTPUT_ONLY);

	rtc_gpio_init(cpu_toggle_num);
    rtc_gpio_set_direction(cpu_toggle_num, RTC_GPIO_MODE_OUTPUT_ONLY);

    rtc_gpio_init(ulp_toggle_num);
    rtc_gpio_set_direction(ulp_toggle_num, RTC_GPIO_MODE_OUTPUT_ONLY);

    /* Set ULP wake up period to 10ms */
    ulp_set_wakeup_period(0, 10*1000);
}

static void print_status()
{

	printf("CPU / ULP toggle counter \r\n0x%x / 0x%x\n", cpu_toggle_counter, ulp_toggle_counter & UINT16_MAX);

	rtc_gpio_hold_dis(cpu_toggle_num);
    if (cpu_toggle_counter % 2 == 0) {
    	rtc_gpio_set_level(cpu_toggle_num, 0);
    }else{
    	rtc_gpio_set_level(cpu_toggle_num, 1);
    }
	rtc_gpio_hold_en(cpu_toggle_num);

	cpu_toggle_counter++;
}

void app_main()
{
    esp_sleep_wakeup_cause_t cause = esp_sleep_get_wakeup_cause();
    if (cause != ESP_SLEEP_WAKEUP_ULP) {
        printf("Not ULP wakeup, initializing ULP\n");
        init_ulp_program();
    } else {
    	rtc_gpio_set_level(cpu_up_num, 1);

    	printf("ULP wakeup, printing status\n");
        print_status();
    }

    printf("Entering deep sleep\n\n");
    /* Start the ULP program */
    ESP_ERROR_CHECK( ulp_run((&ulp_entry - RTC_SLOW_MEM) / sizeof(uint32_t)));
    ESP_ERROR_CHECK( esp_sleep_enable_ulp_wakeup() );
	rtc_gpio_set_level(cpu_up_num, 0);
	esp_deep_sleep_start();
}
