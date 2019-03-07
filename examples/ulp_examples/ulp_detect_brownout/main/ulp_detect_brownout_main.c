// Copyright 2015-2018 Espressif Systems (Shanghai) PTE LTD
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

/* C includes */
#include <stdio.h>
#include <string.h>

/* FreeRTOS includes */
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

/* ESP32 includes */
#include "soc/rtc_cntl_reg.h"
#include "soc/sens_reg.h"
#include "soc/rtc.h"
#include "driver/gpio.h"
#include "driver/rtc_io.h"
#include "driver/adc.h"
#include "driver/dac.h"
#include "esp_sleep.h"
#include "esp32/ulp.h"
#include "ulp_main.h"

// voltage to raw data struct
typedef struct {
    float vol;
    uint32_t raw;
} vol_to_raw_t;

/* Set thresholds, approx. 2.00V - 3.30V */
#define VAL_TO_RAW_NUM   14
static const vol_to_raw_t g_v0l_to_raw[VAL_TO_RAW_NUM] = {
    {2.00, 3123},
    {2.10, 3207},
    {2.20, 3301},
    {2.27, 3361},
    {2.35, 3434},
    {2.43, 3515},
    {2.52, 3598},
    {2.60, 3682},
    {2.68, 3745},
    {2.76, 3825},
    {2.80, 3867},
    {2.90, 3891},
    {3.00, 3986},
    {3.30, 4095},
};

extern const uint8_t ulp_main_bin_start[] asm("_binary_ulp_main_bin_start");
extern const uint8_t ulp_main_bin_end[]   asm("_binary_ulp_main_bin_end");

static void ulp_program_start();
static void ulp_adc_enable();
static void ulp_rtc_wdt_enable(uint32_t time_ms);
static float ulp_adc_raw_to_vol(uint32_t adc_raw);
static uint32_t ulp_adc_vol_to_raw(float voltage);

void app_main()
{
    esp_sleep_wakeup_cause_t cause = esp_sleep_get_wakeup_cause();
    if (cause != ESP_SLEEP_WAKEUP_ULP) {
        printf("Not ULP wakeup, start ULP program\n");
        ulp_adc_enable();
        ulp_rtc_wdt_enable(1000);

        ulp_program_start();
    } else {
        printf("Deep sleep wakeup\n");
    }

    uint32_t i = 0;
    while (1) {
        ulp_last_result &= UINT16_MAX;
        uint32_t ulp_adc_raw = ulp_last_result;
        printf("system in active mode [%ds], voltage: [%d][%1.3f]\n",
               i++, ulp_last_result, ulp_adc_raw_to_vol(ulp_adc_raw));
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}

static void ulp_program_start()
{
    esp_err_t err = ulp_load_binary(0, ulp_main_bin_start,
                                    (ulp_main_bin_end - ulp_main_bin_start) / sizeof(uint32_t));
    ESP_ERROR_CHECK(err);

    /* Disable pullup on GPIO15, in case it is connected to ground to suppress boot messages.
     */
    rtc_gpio_pullup_dis(GPIO_NUM_15);
    rtc_gpio_hold_en(GPIO_NUM_15);

    /* Set ULP wake up period to 100ms */
    ulp_set_wakeup_period(0, 100000);

    /* Start the program */
    err = ulp_run(&ulp_entry - RTC_SLOW_MEM);
    ESP_ERROR_CHECK(err);
}

static uint32_t ulp_adc_vol_to_raw(float voltage)
{
    if (voltage < g_v0l_to_raw[0].vol
            || voltage > g_v0l_to_raw[VAL_TO_RAW_NUM - 1].vol) {
        return 0;
    }

    uint32_t ret_raw = 0;
    for (int i = 0; i < VAL_TO_RAW_NUM - 1; i++) {
        if (g_v0l_to_raw[i].vol <= voltage && g_v0l_to_raw[i + 1].vol >= voltage) {
            float vol_dif    = g_v0l_to_raw[i + 1].vol - g_v0l_to_raw[i].vol;
            uint32_t raw_dif = g_v0l_to_raw[i + 1].raw - g_v0l_to_raw[i].raw;
            ret_raw = g_v0l_to_raw[i].raw + (voltage - g_v0l_to_raw[i].vol) / vol_dif * raw_dif;
            break;
        }
    }
    return ret_raw;
}

static float ulp_adc_raw_to_vol(uint32_t adc_raw)
{
    if (adc_raw < g_v0l_to_raw[0].raw
            || adc_raw > g_v0l_to_raw[VAL_TO_RAW_NUM - 1].raw) {
        return 0;
    }

    float ret_vol = 0;
    for (int i = 0; i < VAL_TO_RAW_NUM - 1; i++) {
        if (g_v0l_to_raw[i].raw <= adc_raw && g_v0l_to_raw[i + 1].raw >= adc_raw) {
            uint32_t raw_dif = g_v0l_to_raw[i + 1].raw - g_v0l_to_raw[i].raw;
            float vol_dif    = g_v0l_to_raw[i + 1].vol - g_v0l_to_raw[i].vol;
            ret_vol = g_v0l_to_raw[i].vol + (float)(adc_raw - g_v0l_to_raw[i].raw) / (float)raw_dif * vol_dif;
            break;
        }
    }
    return ret_vol;
}

static void ulp_adc_enable()
{
    /* Note:
    when changing channel here, also change 'adc_channel' constant in adc.S */
    adc1_config_channel_atten(ADC1_CHANNEL_6, ADC_ATTEN_DB_11);
    adc1_config_width(ADC_WIDTH_BIT_12);
    adc1_ulp_enable();

    ulp_low_thr = ulp_adc_vol_to_raw(2.76);
    printf("ulp_low_thr: %d\n", (uint16_t)ulp_low_thr);
}

static void ulp_rtc_wdt_enable(uint32_t time_ms)
{
    WRITE_PERI_REG(RTC_CNTL_WDTWPROTECT_REG, RTC_CNTL_WDT_WKEY_VALUE);
    WRITE_PERI_REG(RTC_CNTL_WDTFEED_REG, 1);
    REG_SET_FIELD(RTC_CNTL_WDTCONFIG0_REG, RTC_CNTL_WDT_SYS_RESET_LENGTH, 7);
    REG_SET_FIELD(RTC_CNTL_WDTCONFIG0_REG, RTC_CNTL_WDT_CPU_RESET_LENGTH, 7);
    REG_SET_FIELD(RTC_CNTL_WDTCONFIG0_REG, RTC_CNTL_WDT_STG0, RTC_WDT_STG_SEL_RESET_RTC);
    WRITE_PERI_REG(RTC_CNTL_WDTCONFIG1_REG, rtc_clk_slow_freq_get_hz() * time_ms / 1000);
    SET_PERI_REG_MASK(RTC_CNTL_WDTCONFIG0_REG, RTC_CNTL_WDT_EN | RTC_CNTL_WDT_PAUSE_IN_SLP);
    WRITE_PERI_REG(RTC_CNTL_WDTWPROTECT_REG, 0);
}
