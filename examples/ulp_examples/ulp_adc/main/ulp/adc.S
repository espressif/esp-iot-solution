/* ULP Example: using ADC in deep sleep

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
#include "soc/soc_ulp.h"

	/* ADC1 channel 6, GPIO34 */
	.set adc_channel, 6

	/* Configure the number of ADC samples to average on each measurement.
	   For convenience, make it a power of 2. */
	.set adc_oversampling_factor_log, 2
	.set adc_oversampling_factor, (1 << adc_oversampling_factor_log)

.data 	/* .data section */

.global thermistor_lut
thermistor_lut:
		.long 0x0D33	//3379		-5 degree centigrade
		.long 0x0D1F	//3359		-4 
		.long 0x0D0C	//3340		-3 
		.long 0x0CF8	//3320		-2 
		.long 0x0CE4	//3300		-1 
		.long 0x0CD0	//3280		0
		.long 0x0CBB	//3259		1
		.long 0x0CA6	//3238		2
		.long 0x0C90	//3216		3
		.long 0x0C7A	//3194		4
		.long 0x0C63	//3171		5
		.long 0x0C4B	//3147		6
		.long 0x0C32	//3122		7
		.long 0x0C19	//3097		8
		.long 0x0BFE	//3070		9
		.long 0x0BE3	//3043		10
		.long 0x0BC6	//3014		11
		.long 0x0BA9	//2985		12
		.long 0x0B8C	//2956		13
		.long 0x0B6F	//2927		14
		.long 0x0B51	//2897		15
		.long 0x0B33	//2867		16
		.long 0x0B14	//2836		17
		.long 0x0AF4	//2804		18
		.long 0x0AD2	//2770		19
		.long 0x0AB0	//2736		20
		.long 0x0A8C	//2700		21
		.long 0x0A68	//2664		22
		.long 0x0A43	//2627		23
		.long 0x0A1E	//2590		24
		.long 0x09F8	//2552		25
		.long 0x09D4	//2516		26
		.long 0x09B0	//2480		27
		.long 0x098E	//2446		28
		.long 0x096D	//2413		29
		.long 0x0950	//2384		30
		.long 0x092D	//2349		31
		.long 0x0909	//2313		32
		.long 0x08E4	//2276		33
		.long 0x08BE	//2238		34
		.long 0x0896	//2198		35
		.long 0x086E	//2158		36
		.long 0x0845	//2117		37
		.long 0x081B	//2075		38
		.long 0x07F2	//2034		39
		.long 0x07C9	//1993		40
		.long 0x07A0	//1952		41
		.long 0x0778	//1912		42
		.long 0x0753	//1875		43
		.long 0x072F	//1839		44
		.long 0x070E	//1806		45
		.long 0x06E8	//1768		46
		.long 0x06C3	//1731		47
		.long 0x069E	//1694		48
		.long 0x067B	//1659		49
		.long 0x0657	//1623		50
		.long 0x0635	//1589		51
		.long 0x0613	//1555		52
		.long 0x05F1	//1521		53
		.long 0x05D0	//1488		54
		.long 0x05AF	//1455		55
		.long 0x058E	//1422		56
		.long 0x056D	//1389		57
		.long 0x054C	//1356		58
		.long 0x052A	//1322		59
		.long 0x0509	//1289		60
		.long 0x04EA	//1258		61
		.long 0x04CB	//1227		62
		.long 0x04AD	//1197		63
		.long 0x048F	//1167		64
		.long 0x0471	//1137		65
		.long 0x0454	//1108		66
		.long 0x0438	//1080		67
		.long 0x041C	//1052		68
		.long 0x0401	//1025		69
		.long 0x03E6	//998		70
		.long 0x03CD	//973		71
		.long 0x03B4	//948		72
		.long 0x039B	//923		73
		.long 0x0384	//900		74
		.long 0x036D	//877		75
		.long 0x0357	//855		76
		.long 0x0342	//834		77
		.long 0x032E	//814		78
		.long 0x031A	//794		79
		.long 0x0306	//774		80
		.long 0x02F3	//755		81
		.long 0x02E0	//736		82
		.long 0x02CE	//718		83
		.long 0x02BB	//699		84
		.long 0x02A9	//681		85
		.long 0x0297	//663		86
		.long 0x0285	//645		87
		.long 0x0273	//627		88
		.long 0x0262	//610		89
		.long 0x0250	//592		90
		.long 0x0240	//576		91
		.long 0x0230	//560		92
		.long 0x0222	//546		93
		.long 0x0213	//531		94
		.long 0x0205	//517		95
		.long 0x01F8	//504		96
		.long 0x01EB	//491		97
		.long 0x01DE	//478		98
		.long 0x01D2	//466		99
		.long 0x01C5	//453		100
		.long 0xffff	//table end

	/* Define variables, which go into .bss section (zero-initialized data) */
	.bss

	.global last_result
last_result:
	.long 0

	.global temperature
temperature:
	.long 0

	/* Code goes into .text section */
	.text
	.global entry
entry:
	/* do measurements using ADC */
	/* r0 will be used as accumulator */
	move r0, 0
	/* initialize the loop counter */
	stage_rst
measure:
	/* measure and add value to accumulator */
	adc r1, 0, adc_channel + 1
	add r0, r0, r1
	/* increment loop counter and check exit condition */
	stage_inc 1
	jumps measure, adc_oversampling_factor, lt

	/* divide accumulator by adc_oversampling_factor.
	   Since it is chosen as a power of two, use right shift */
	rsh r0, r0, adc_oversampling_factor_log
	/* averaged value is now in r0; store it into last_result */
	move r3, last_result
	st r0, r3, 0

	move r3, temperature
	ld r1, r3, 0
	/* initialize temperature every calculate loop */
	move r1, 0 
	st r1, r3, 0
	/* use r3 as thermistor_lut pointer */
	move r3, thermistor_lut 
look_up_table:
	/* load table data in R2 */
	ld r2, r3, 0 

	/* check if at the table_end */
	sub	r1, r2, 0xffff
	/* if hit the tail of table, jump to table_end */
	jump out_of_range, eq
	
	/* thermistor_lut - last_result */
	sub r2, r0, r2
	/* adc_value < table_value */
	jump move_step, ov 
	/* adc_value > table_value, get current value */
	jump wake_up
out_of_range:
	move r1, temperature
	ld r2, r1, 0
	/* Maximum range 100 degree centigrade */
	move r2, 105 
	st r2, r1, 0
	jump wake_up
move_step:
	/* move pointer a step */
	add r3, r3, 1 

	move r1, temperature
	ld r2, r1, 0
	/* temperature count 1 */
	add r2, r2, 1 
	st r2, r1, 0
	jump look_up_table

	/* value within range, end the program */
	.global exit
exit:
	halt

	.global wake_up
wake_up:
	/* Check if the system can be woken up */
	READ_RTC_FIELD(RTC_CNTL_LOW_POWER_ST_REG, RTC_CNTL_RDY_FOR_WAKEUP)

	and r0, r0, 1
	jump exit, eq

	/* Wake up the SoC, end program */
	wake
	WRITE_RTC_FIELD(RTC_CNTL_STATE0_REG, RTC_CNTL_ULP_CP_SLP_TIMER_EN, 0)
	halt
