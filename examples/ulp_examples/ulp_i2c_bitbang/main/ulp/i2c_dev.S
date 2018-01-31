/* ULP Example: using ADC in deep sleep

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

.set BH1750_ADDR_W, 0x46 
.set BH1750_ADDR_R, 0x47 
.set BH1750_ON,0x01
.set BH1750_RSET,0x07
.set BH1750_ONE, 0x20
.set BH1750_ONE_LOW, 0x23

.bss
   .global sample_counter
sample_counter:
   .long 0
   .global result
result:
   .long 0
   .global stack
stack:
   .skip 100
   .global stackEnd
stackEnd:
   .long 0

.text
   .global entry
entry:
   move r3,stackEnd
   psr
   jump Task_BH1750
   move r1, sample_counter    /* Read sample counter */
   ld r0, r1, 0
   add r0, r0, 1              /* Increment */
   st r0, r1, 0               /* Save counter in memory */
   jumpr clear, 3, ge
   jump exit
clear:
   move r1, sample_counter
   ld r0, r1, 0
   .set zero, 0x00
   move r0, zero
   st r0, r1, 0
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

.global Read_BH1750
Read_BH1750:
   move r1, BH1750_ADDR_R
   push r1
   psr 
   jump i2c_start_cond          // i2c Start
   ld r2, r3, 4                 // Address+Read
   psr
   jump i2c_write_byte
   jumpr popfail, 1, ge
   pop r1
   move r2,0
   psr
   jump i2c_read_byte
   push r0
   move r2,1 // last byte
   psr
   jump i2c_read_byte
   push r0  
   psr
   jump i2c_stop_cond
   pop r0 // Low-byte
   pop r2 // Hight-byte
   lsh r2,r2,8
   or r2,r2,r0
   move r0,r2
   move r1, result
   st r0, r1, 0
   move r2,0 // OK
   ret


.global Cmd_Write_BH1750
Cmd_Write_BH1750:
   psr 
   jump i2c_start_cond           // i2c Start
   ld r2, r3, 12                 // Address+Write
   psr
   jump i2c_write_byte
   jumpr popfail,1,ge
   ld r2, r3, 8                  // Command
   psr
   jump i2c_write_byte
   jumpr popfail, 1, ge
   psr
   jump i2c_stop_cond            // i2c Stop
   ret

.global Start_BH1750
Start_BH1750:
   move r1, BH1750_ADDR_W
   push r1
   move r1, BH1750_ON
   push r1
   psr 
   jump Cmd_Write_BH1750         // power on
   pop r1
   move r1, BH1750_ONE    
   push r1
   psr 
   jump Cmd_Write_BH1750         // once H
   pop r1
   pop r1
   ret

.global Task_BH1750
Task_BH1750:
   psr
   jump Start_BH1750
   move r2, 200                  // Wait 150ms
   psr
   jump waitMs
   psr
   jump Read_BH1750
   ret

popfail:
   pop r1                        // pop caller return address
   move r2,1
   ret

// Wait for r2 milliseconds
.global waitMs
waitMs:
   wait 8000
   sub r2,r2,1
   jump doneWaitMs,eq
   jump waitMs
doneWaitMs:
   ret
