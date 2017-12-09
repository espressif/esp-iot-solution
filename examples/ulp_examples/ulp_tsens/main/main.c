/* ULP Example: Read temperature sensor in deep sleep

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
#include "soc/soc_ulp.h"
#include "driver/gpio.h"
#include "driver/rtc_io.h"
#include "esp32/ulp.h"
#include "ulp_main.h"

extern const uint8_t ulp_main_bin_start[] asm("_binary_ulp_main_bin_start");
extern const uint8_t ulp_main_bin_end[]   asm("_binary_ulp_main_bin_end");

static void init_ulp_program()
{
    esp_err_t err = ulp_load_binary(0, ulp_main_bin_start,(ulp_main_bin_end - ulp_main_bin_start) / sizeof(uint32_t));         
    ESP_ERROR_CHECK(err);

    /* Set ULP wake up period to 3s */
    ulp_set_wakeup_period(0, 3*1000*1000);
}

static void print_tsens()
{
    printf("ulp_tsens_value:%d.\r\n",(uint16_t)ulp_tsens_value);
}

void app_main()
{
    esp_sleep_wakeup_cause_t cause = esp_sleep_get_wakeup_cause();
    if (cause != ESP_SLEEP_WAKEUP_ULP) {
        printf("Not ULP wakeup, initializing ULP\n");
        init_ulp_program();
    } else {
    	printf("ULP wakeup, printing temperature sensor value\n");
        print_tsens();
    }

    printf("Entering deep sleep\n\n");
    /* Start the ULP program */
    ESP_ERROR_CHECK( ulp_run((&ulp_entry - RTC_SLOW_MEM) / sizeof(uint32_t)));
    ESP_ERROR_CHECK( esp_sleep_enable_ulp_wakeup() );
    esp_deep_sleep_start();
    
}
