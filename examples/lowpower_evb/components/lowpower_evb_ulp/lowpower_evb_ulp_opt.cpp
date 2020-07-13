/* Lowpower EVB Example

   For other examples please check:
   https://github.com/espressif/esp-iot-solution/tree/master/examples

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
 */

#include "soc/rtc_cntl_reg.h"
#include "soc/rtc_io_reg.h"
#include "soc/rtc.h"
#include "soc/soc.h"
#include "soc/sens_reg.h"
#include "driver/rtc_io.h"
#include "esp32/ulp.h"
// #include "ulp_lowpower_evb_ulp.h"
#include "esp32/ulp.h"
#include "lowpower_evb_ulp_opt.h"

extern const uint8_t ulp_main_bin_start[] asm("_binary_ulp_lowpower_evb_ulp_bin_start");
extern const uint8_t ulp_main_bin_end[]   asm("_binary_ulp_lowpower_evb_ulp_bin_end");

/* This function is called once after power-on reset, to load ULP program into
 * RTC memory.
 */
void init_ulp_program()
{
    esp_err_t err = ulp_load_binary(0, ulp_main_bin_start,
            (ulp_main_bin_end - ulp_main_bin_start) / sizeof(uint32_t));
    ESP_ERROR_CHECK(err);
}
  
/* This function is called every time before going into deep sleep.
 * It starts the ULP program and resets measurement counter.
 */
void start_ulp_program()
{
    /* Start the program */
    esp_err_t err = ulp_run((&ulp_entry - RTC_SLOW_MEM) / sizeof(uint32_t));
    ESP_ERROR_CHECK(err);
}

void set_ulp_read_value_number(uint16_t value_number)
{
    ulp_value_number = value_number;
}

void set_ulp_read_interval(uint32_t time_ms)
{
    const uint32_t sleep_cycles = rtc_clk_slow_freq_get_hz() * time_ms / 1000;
    REG_WRITE(SENS_ULP_CP_SLEEP_CYC0_REG, sleep_cycles);
}

void get_ulp_hts221_humidity(float hum[], uint16_t value_num)
{
    int16_t h0_t0_out, h1_t0_out, h_t_out;
    int16_t h0_rh, h1_rh;
    int16_t hum_raw;
    int32_t tmp_32;

    h0_rh = (uint16_t)ulp_h0_rh_x2 >> 1;
    h1_rh = (uint16_t)ulp_h1_rh_x2 >> 1;

    h0_t0_out = (uint16_t)ulp_h0_t0_out_msb << 8 | (uint16_t)ulp_h0_t0_out_lsb;
    h1_t0_out = (uint16_t)ulp_h1_t0_out_msb << 8 | (uint16_t)ulp_h1_t0_out_lsb;

    for (int i=0; i<value_num; i++) {
        h_t_out =  (uint16_t)*((&ulp_raw_hum_data) +i);
        tmp_32 = ((int32_t)(h_t_out - h0_t0_out)) * ((int32_t)(h1_rh - h0_rh) * 10);
        hum_raw = tmp_32/(int32_t)(h1_t0_out - h0_t0_out)  + h0_rh * 10;

        if(hum_raw > 1000) {
            hum_raw = 1000;
        }
        
        hum[i] = (float)hum_raw / 10;
    } 
}
  
void get_ulp_hts221_temperature(float temp[], uint16_t value_num)
{
    int16_t t0_out, t1_out, t_out, t0_degc_x8_u16, t1_degc_x8_u16;
    int16_t t0_degc, t1_degc;
    int16_t temp_raw;
    uint32_t tmp_32;

    t0_degc_x8_u16 = ((uint16_t)ulp_t0_t1_degc_h2 & 0x03) << 8 | (uint16_t)ulp_t0_degc_x8;
    t1_degc_x8_u16 = ((uint16_t)ulp_t0_t1_degc_h2 & 0x0C) << 6 | (uint16_t)ulp_t1_degc_x8;
    t0_degc = t0_degc_x8_u16 >> 3;
    t1_degc = t1_degc_x8_u16 >> 3;

    t0_out = (uint16_t)ulp_t0_out_msb << 8 | (uint16_t)ulp_t0_out_lsb;  
    t1_out = (uint16_t)ulp_t1_out_msb << 8 | (uint16_t)ulp_t1_out_lsb;

    for(int i=0; i<value_num; i++) {
        t_out =  (uint16_t)*((&ulp_raw_temp_data) +i);
        tmp_32 = ((uint32_t)(t_out - t0_out)) * ((uint32_t)(t1_degc - t0_degc) * 10);
        temp_raw = tmp_32 / (t1_out - t0_out) + t0_degc * 10;
        temp[i] = (float)temp_raw / 10;
    }
}

void get_ulp_bh1750_luminance(float lum[], uint16_t value_num)
{
    uint16_t raw_lum_value = 0;

    for (int i=0; i<value_num; i++) {
        raw_lum_value = (uint16_t)*((&ulp_raw_lum_value) +i);
        lum[i] = (float)raw_lum_value *10 / 12;
    }
}

