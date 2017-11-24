/* ULP Example: using RTC GPIO in deep sleep

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.

   This file contains assembly code which runs on the ULP.

   ULP wakes up to run this code at a certain period, determined by the values
   in SENS_ULP_CP_SLEEP_CYCx_REG registers. On each wake up, the program
   measures input voltage on the given ADC channel 'adc_oversampling_factor'
   times. Measurements are accumulated and average value is calculated.
   Average value is compared to the two thresholds: 'low_thr' and 'high_thr'.
   If the value is less than 'low_thr' or more than 'high_thr', ULP wakes up
   the chip from deep sleep.
*/

/* ULP assembly files are passed through C preprocessor first, so include directives
   and C macros may be used in these files 
 */

#include "soc/rtc_cntl_reg.h"
#include "soc/rtc_io_reg.h"
#include "soc/soc_ulp.h"

	/* Define variables, which go into .bss section (zero-initialized data) */
	.bss

	.global toggle_counter
toggle_counter:
	.long 0

	/* Number of restarts of ULP to wake up the main program.
	   See couple of lines below how this value is used */
	.set toggle_cycles_to_wakeup, 7

	/* Code goes into .text section */
	.text
	.global entry
entry:
	/* Disable hold of RTC_GPIO10 output */
	WRITE_RTC_REG(RTC_IO_TOUCH_PAD0_REG,RTC_IO_TOUCH_PAD0_HOLD_S,1,0)
	/* Set the RTC_GPIO10 output HIGH
	   to signal that ULP is now up */
	WRITE_RTC_REG(RTC_GPIO_OUT_W1TS_REG,RTC_GPIO_OUT_DATA_W1TS_S+10,1,1)
	/* Wait some cycles to have visible trace on the scope */
	wait 1000

	/* Read toggle counter */
	move r3, toggle_counter
	ld r0, r3, 0
	/* Increment */
	add r0, r0, 1
	/* Save counter in memory */
	st r0, r3, 0
	/* Save counter in r3 to use it later */
	move r3, r0

	/* Disable hold of RTC_GPIO17 output */
	WRITE_RTC_REG(RTC_IO_TOUCH_PAD7_REG,RTC_IO_TOUCH_PAD7_HOLD_S,1,0)

	/* Toggle RTC_GPIO17 output */
	and r0, r0, 0x01
	jump toggle_clear, eq

	/* Set the RTC_GPIO17 output HIGH */
	WRITE_RTC_REG(RTC_GPIO_OUT_W1TS_REG,RTC_GPIO_OUT_DATA_W1TS_S+17,1,1)
	jump toggle_complete

	.global toggle_clear
toggle_clear:
	/* Set the RTC_GPIO17 output LOW (clear output) */
	WRITE_RTC_REG(RTC_GPIO_OUT_W1TC_REG,RTC_GPIO_OUT_DATA_W1TC_S+17,1,1)

	.global toggle_complete
toggle_complete:

	/* Enable hold on RTC_GPIO17 output */
	WRITE_RTC_REG(RTC_IO_TOUCH_PAD7_REG,RTC_IO_TOUCH_PAD7_HOLD_S,1,1)

	/* Set the RTC_GPIO10 output LOW (clear output)
	   to signal that ULP is now going down */
	WRITE_RTC_REG(RTC_GPIO_OUT_W1TC_REG,RTC_GPIO_OUT_DATA_W1TC_S+10,1,1)

	/* Enable hold on RTC_GPIO10 output */
	WRITE_RTC_REG(RTC_IO_TOUCH_PAD0_REG,RTC_IO_TOUCH_PAD0_HOLD_S,1,1)

	/* Compare the toggle counter with toggle cycles to wakeup SoC
	   and wakup SoC if the values match */
	and r0, r3, toggle_cycles_to_wakeup
	jump wake_up, eq

	/* Get ULP back to sleep */
	.global exit
exit:
	halt

	.global wake_up
wake_up:
	/* Check if the SoC can be woken up */
	READ_RTC_REG(RTC_CNTL_DIAG0_REG, 19, 1)
	and r0, r0, 1
	jump exit, eq

	/* Wake up the SoC and stop ULP program */
	wake
	/* Stop the wakeup timer so it does not restart ULP */
	WRITE_RTC_FIELD(RTC_CNTL_STATE0_REG, RTC_CNTL_ULP_CP_SLP_TIMER_EN, 0)
	halt
