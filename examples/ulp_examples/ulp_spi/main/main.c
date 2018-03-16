/* ULP Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <stdio.h>
#include <math.h>
#include <unistd.h>
#include "esp_sleep.h"
#include "soc/rtc_cntl_reg.h"
#include "soc/rtc_io_reg.h"
#include "soc/sens_reg.h"
#include "soc/soc.h"
#include "driver/rtc_io.h"
#include "esp32/ulp.h"
#include "sdkconfig.h"
#include "ulp_main.h"
#include "math.h"

uint16_t Manufacturer = 0x00;   /* 16 bit reserved for manufacturer */
uint16_t C1 = 0x00;             /* Pressure sensitivity | SENS T1 */
uint16_t C2 = 0x00;             /* Pressure offset | OFF T1 */
uint16_t C3 = 0x00;             /* Temperature coefficient of pressure sensitivity | TCS */
uint16_t C4 = 0x00;             /* Temperature coefficient of pressure offset | TCO */
uint16_t C5 = 0x00;             /* Reference temperature | T REF */
uint16_t C6 = 0x00;             /* Temperature coefficient of the temperature | TEMPSENS */
uint16_t CRC = 0x00;            /* CRC-4 code */
uint32_t D1 = 0x00;             /* Digital pressure value, raw data */
uint32_t D2 = 0x00;             /* Digital temperature value */
int64_t dT;                     /* Difference between actual and reference temperature, */
int64_t OFF;                    /* Offset at actual temperature */
int64_t SENS;                   /* Sensitivity at actual temperature */

int32_t temperature;
int32_t pressure;

/* SECOND ORDER TEMPERATURE COMPENSATION */
uint64_t T2;
uint64_t OFF2;
uint64_t SENS2;

extern const uint8_t ulp_main_bin_start[] asm("_binary_ulp_main_bin_start");
extern const uint8_t ulp_main_bin_end[]   asm("_binary_ulp_main_bin_end");

const gpio_num_t GPIO_CS = GPIO_NUM_25;
const gpio_num_t GPIO_MOSI = GPIO_NUM_26;
const gpio_num_t GPIO_SCLK = GPIO_NUM_27;
const gpio_num_t GPIO_MISO = GPIO_NUM_4;

static void init_ulp_program()
{
    esp_err_t err = ulp_load_binary(0, ulp_main_bin_start,(ulp_main_bin_end - ulp_main_bin_start) / sizeof(uint32_t));         
    ESP_ERROR_CHECK(err);

    rtc_gpio_init(GPIO_CS);
    rtc_gpio_set_direction(GPIO_CS, RTC_GPIO_MODE_OUTPUT_ONLY);
    rtc_gpio_set_level(GPIO_CS, 0);

    rtc_gpio_init(GPIO_MOSI);
    rtc_gpio_set_direction(GPIO_MOSI, RTC_GPIO_MODE_OUTPUT_ONLY);
    rtc_gpio_set_level(GPIO_MOSI, 0);

    rtc_gpio_init(GPIO_SCLK);
    rtc_gpio_set_direction(GPIO_SCLK, RTC_GPIO_MODE_OUTPUT_ONLY);
    rtc_gpio_set_level(GPIO_SCLK, 0);

    rtc_gpio_init(GPIO_MISO);
    rtc_gpio_set_direction(GPIO_MISO, RTC_GPIO_MODE_INPUT_ONLY);
    rtc_gpio_pullup_en(GPIO_MISO);

    /* Set ULP wake up period to 2s */
    ulp_set_wakeup_period(0, 2000*1000);
}

static void second_order_temperature_compensation(int32_t temp)
{
    if(temp < 2000)
    {
        T2 = (uint64_t)(dT*dT) >> 31;
        OFF2 = (5 * ((uint64_t)((temp - 2000)*(temp - 2000)))) << 1;
        SENS2 = (5 * ((uint64_t)((temp - 2000)*(temp - 2000)))) << 2;
        if(temp < -1500)
        {
            OFF2 += 7*((uint64_t)((temp + 1500)*(temp + 1500)));
            SENS2 += (11*((uint64_t)((temp + 1500)*(temp + 1500)))) >> 1 ;
        }
    }else{
        T2 = 0x00;
        OFF2 =0x00;
        SENS2 = 0x00;
    }
}

void app_main()
{
    esp_sleep_wakeup_cause_t cause = esp_sleep_get_wakeup_cause();
    if (cause != ESP_SLEEP_WAKEUP_ULP) {
        printf("Not ULP wakeup, initializing ULP\n");
        init_ulp_program();
    } else {
        printf("Deep sleep wakeup\n");

        C1 = (uint16_t)*((&ulp_prom_table) + 1);
        C2 = (uint16_t)*((&ulp_prom_table) + 2);
        C3 = (uint16_t)*((&ulp_prom_table) + 3);
        C4 = (uint16_t)*((&ulp_prom_table) + 4);
        C5 = (uint16_t)*((&ulp_prom_table) + 5);
        C6 = (uint16_t)*((&ulp_prom_table) + 6);
        printf("PROM: \r\n");
        printf("  C1:0x%x\r\n",(uint32_t)C1);
        printf("  C2:0x%x\r\n",(uint32_t)C2);
        printf("  C3:0x%x\r\n",(uint32_t)C3);
        printf("  C4:0x%x\r\n",(uint32_t)C4);
        printf("  C5:0x%x\r\n",(uint32_t)C5);
        printf("  C6:0x%x\r\n",(uint32_t)C6);
        D1 = (((uint32_t)ulp_D1_H << 16) & (0x00ff0000)) | ((uint32_t)ulp_D1_L & (0x0000ffff));
        D2 = (((uint32_t)ulp_D2_H << 16) & (0x00ff0000)) | ((uint32_t)ulp_D2_L & (0x0000ffff));
        printf("ADC_Read: \r\n");
        printf("  D1:0x%x\n", D1);
        printf("  D2:0x%x\n", D2);
        dT = (int64_t)(D2 - ((uint32_t)C5 << 8));
        OFF = ((int64_t)C2 << 16) + ((int64_t)(C4*dT) >> 7);
        printf("Variables:\n");
        printf("  dT:0x%x%x\n", (uint32_t)(dT >> 32), (uint32_t)dT);
        SENS = ((int64_t)C1 << 15) + ((int64_t)(C3*dT) >> 8);
        temperature = ((((int64_t)(C6 * dT)) >> 23) + 2000);
        second_order_temperature_compensation(temperature);
        temperature -= T2;
        OFF -= OFF2;
        SENS -= SENS2;
        printf("  OFF:0x%x%x\n", (int32_t)(OFF >> 32),(int32_t) OFF);
        printf("  SENS:0x%x%x\n", (int32_t)(SENS >> 32),(int32_t) SENS);
        printf("MS5611_SENSOR: \r\n");
        printf("  temperature: %d.%d ℃\n",(int16_t)(temperature/100),(int16_t)(temperature%100));
        pressure = (((int64_t)(D1*SENS) >> 21) - OFF) >> 15;
        printf("  pressure: %d.%d mbar\n",(int32_t)(pressure/100),(int32_t)(pressure%100));
    }
    printf("Entering deep sleep\n\n");
    /* Start the ULP program */
    ESP_ERROR_CHECK( ulp_run((&ulp_entry - RTC_SLOW_MEM) / sizeof(uint32_t)));
    ESP_ERROR_CHECK( esp_sleep_enable_ulp_wakeup() );
    esp_deep_sleep_start();
}