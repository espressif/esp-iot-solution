/* ULP Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.

   This file contains assembly code which runs on the ULP.
*/

/* ULP assembly files are passed through C preprocessor first, so include directives
   and C macros may be used in these files 
 */


#include "soc/rtc_cntl_reg.h"
#include "soc/rtc_io_reg.h"
#include "soc/soc_ulp.h"
#include "stack.S"

.bss
    .global value_number
value_number:
    .long 0
   
    .global sample_counter
sample_counter:
    .long 0
   
    .global stack
stack:
    .skip 100
    .global stackEnd
stackEnd:
    .long 0

.text
.global waitMs

    .global entry
entry:
    move r3, stackEnd
   
    psr
    jump Task_HTS221
    psr
    jump Task_BH1750

    move r1, sample_counter    /* Read sample counter */
    ld r0, r1, 0
    add r0, r0, 1              /* Increment */
    st r0, r1, 0               /* Save counter in memory */
    move r2, value_number
    ld r2, r2, 0
    sub r2, r2, r0
    jump clear, ov
    jump exit
clear:
    psr
    jump hum_counter_clear
    psr
    jump temp_counter_clear
    psr
    jump lum_counter_clear
    psr
    jump sample_counter_clear
    jump wake_up

    /* value within range, end the program */
    .global exit
exit:
    halt

    .global wake_up
wake_up:
    /* Check if the system can be woken up */
    READ_RTC_REG(RTC_CNTL_DIAG0_REG, 19, 1)
    and r0, r0, 1
    jump exit, eq
    /* Wake up the SoC, end program */
    wake
    WRITE_RTC_FIELD(RTC_CNTL_STATE0_REG, RTC_CNTL_ULP_CP_SLP_TIMER_EN, 0)
    halt

    .global sample_counter_clear
sample_counter_clear:
    move r1, sample_counter
    ld r0, r1, 0
    .set zero, 0x00
    move r0, zero
    st r0, r1, 0
    ret  
   
/* Wait for r2 milliseconds */
    .global waitMs
waitMs:
    wait 8000
    sub r2, r2, 1
    jump doneWaitMs, eq
    jump waitMs
doneWaitMs:
    ret
