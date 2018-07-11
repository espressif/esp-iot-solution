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

/* BH1750 */
.set BH1750_ADDR_W,            0x46 
.set BH1750_ADDR_R,            0x47 
.set BH1750_POWER_ON,          0x01
.set BH1750_DATA_RESET,        0x07
.set BH1750_ONETIME_4LX_RES,   0x23
.set BH1750_CONTINUE_4LX_RES,  0x13

.bss
    .global raw_lum_value
raw_lum_value:
    .skip 600
    .global lum_counter
lum_counter:
    .long 0
   
.text
.global Task_BH1750
.global lum_counter_clear

    .global Read_BH1750
Read_BH1750:
    move r1, BH1750_ADDR_R
    push r1
    psr 
    jump i2c_start_cond          // i2c Start
    ld r2, r3, 4                 // Address+Read
    psr
    jump i2c_write_byte
    pop r1
    move r2, 0
    psr
    jump i2c_read_byte
    push r0

    psr
    jump i2c_send_ack
    
    move r2, 1                    // last byte
    psr
    jump i2c_read_byte
    push r0

    psr
    jump i2c_send_nack
    
    psr
    jump i2c_stop_cond
    pop r0                       // Low-byte
    pop r2                       // Hight-byte
    lsh r2, r2, 8
    or r2, r2, r0
    move r0, r2
    move r2, lum_counter
    ld r1, r2, 0
    move r2, raw_lum_value
    add r2, r2, r1  
    st r0, r2, 0
    move r2,0
    
    psr
    jump lum_counter_increment
    ret

    .global Cmd_Write_BH1750
Cmd_Write_BH1750:
    psr 
    jump i2c_start_cond           // i2c Start
    ld r2, r3, 12                 // Address+Write
    psr
    jump i2c_write_byte
    ld r2, r3, 8                  // Command
    psr
    jump i2c_write_byte
    psr
    jump i2c_stop_cond            // i2c Stop
    ret

    .global Start_BH1750
Start_BH1750:
    move r1, BH1750_ADDR_W
    push r1
    move r1, BH1750_POWER_ON
    push r1
    psr 
    jump Cmd_Write_BH1750         // power on
    pop r1
    move r1, BH1750_CONTINUE_4LX_RES
    push r1
    psr 
    jump Cmd_Write_BH1750         // continue low resolution
    pop r1
    pop r1
    ret

    .global Task_BH1750
Task_BH1750:
    psr
    jump Start_BH1750
    move r2, 50                   // wait 50 ms
    psr
    jump waitMs
    psr
    jump Read_BH1750
    ret

    .global lum_counter_increment
lum_counter_increment:
    move r1, lum_counter
    ld r0, r1, 0
    add r0, r0, 1
    st r0, r1, 0
    ret

    .global lum_counter_clear
lum_counter_clear:
    move r1, lum_counter
    ld r0, r1, 0
    .set zero, 0x00
    move r0, zero
    st r0, r1, 0
    ret   