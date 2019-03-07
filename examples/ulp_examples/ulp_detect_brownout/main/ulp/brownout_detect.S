/* ULP Example: using ADC in deep sleep

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.

   This file contains assembly code which runs on the ULP.

   ULP wakes up to run this code at a certain period, determined by the values
   in SENS_ULP_CP_SLEEP_CYCx_REG registers. On each wake up, the program
   measures input voltage on the given ADC channel 'oversample'
   times. Measurements are accumulated and average value is calculated.
   Average value is compared to the 'low_thr'. If the value is less than 'low_thr',
   ULP wakes up the chip from deep sleep.
*/

/* ULP assembly files are passed through C preprocessor first, so include directives
   and C macros may be used in these files 
 */
#include "soc/rtc_cntl_reg.h"
#include "soc/rtc_io_reg.h"
#include "soc/soc_ulp.h"

    /* ADC1 channel 6, GPIO34 */
    .set adc1_chn_6, 6

    /* Configure the number of ADC samples to average on each measurement.
       For convenience, make it a power of 2. */
    .set oversample_power, 2
    .set oversample, (1 << oversample_power)

    /* Define variables, which go into .bss section (zero-initialized data) */
    .bss

    /* Low threshold of ADC reading. Set by the main program. */
    .global low_thr
low_thr:
    .long 0

    /* if voltage < low_thr, Set 1 */
    .global brownout_flag
brownout_flag:
    .long 0

    .global last_result
last_result:
    .long 0

    /* Code goes into .text section */
    .text
    .global entry
entry:
    /* initialize r0, r0 will be used as accumulator*/
    move r0, 0
    /* initialize the loop counter */
    stage_rst
measure:
    /* measure and add value to accumulator */
    adc r1, 0, adc1_chn_6 + 1
    add r0, r0, r1
    /* increment loop counter and check exit condition */
    stage_inc 1
    jumps measure, oversample, lt

    /* divide accumulator by oversample.
       Since it is chosen as a power of two, use right shift */
    rsh r0, r0, oversample_power
    /* averaged value is now in r0, store it into last result */
    move r3, last_result
    st r0, r3, 0
    /* compare value with low_thr */
    move r3, low_thr
    ld r3, r3, 0
    sub r3, r0, r3
    jump low_voltage, ov

valid_voltage:
    /* voltage is valid, if brownout happened befor
        wait rwdt to reset chip */
    move r3, brownout_flag
    ld r0, r3, 0
    jumpr feed_dog, 1, lt
    halt

low_voltage:
    /* set brownout_flag to 1 */
    move r3, brownout_flag
    move r2, 1
    st r2, r3, 0
    /* power down digital core in sleep */
    WRITE_RTC_REG(RTC_CNTL_DIG_PWC_REG, RTC_CNTL_DG_WRAP_PD_EN_S, 1, 0)
    /* power down Wi-Fi in sleep */
    WRITE_RTC_REG(RTC_CNTL_DIG_PWC_REG, RTC_CNTL_WIFI_PD_EN_S, 1, 0)
    /* software stall CPU */
    WRITE_RTC_REG(RTC_CNTL_SW_CPU_STALL_REG, RTC_CNTL_SW_STALL_PROCPU_C1_S, 6, 0x21)
    WRITE_RTC_REG(RTC_CNTL_SW_CPU_STALL_REG, RTC_CNTL_SW_STALL_APPCPU_C1_S, 6, 0x21)
    WRITE_RTC_REG(RTC_CNTL_OPTIONS0_REG, RTC_CNTL_SW_STALL_PROCPU_C0_S, 2, 2)
    WRITE_RTC_REG(RTC_CNTL_OPTIONS0_REG, RTC_CNTL_SW_STALL_APPCPU_C1_S, 2, 2)
    jump feed_dog

    .global feed_dog
feed_dog:
    /* Write 0x50d83aa1 to RTC_CNTL_WDTWPROTECT_REG unlock RTC-WDT registers,
        please refer to soc/rtc_cntl_reg.h for more info */
    WRITE_RTC_REG(RTC_CNTL_WDTWPROTECT_REG, 0, 8, 0xa1)
    WRITE_RTC_REG(RTC_CNTL_WDTWPROTECT_REG, 8, 8, 0x3a)
    WRITE_RTC_REG(RTC_CNTL_WDTWPROTECT_REG, 16, 8, 0xd8)
    WRITE_RTC_REG(RTC_CNTL_WDTWPROTECT_REG, 24, 8, 0x50)
    /* feed RTC-WDT */
    WRITE_RTC_REG(RTC_CNTL_WDTFEED_REG, RTC_CNTL_WDT_FEED_S, 1, 1)
    /* write any data to lock RTC-WDT registers */
    WRITE_RTC_REG(RTC_CNTL_WDTWPROTECT_REG, 0, 8, 0)
    halt
