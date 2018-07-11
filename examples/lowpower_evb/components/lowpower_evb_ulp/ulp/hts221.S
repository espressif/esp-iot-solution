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

/* HTS221 */
.set HTS221_ADDR_W,            0xBE 
.set HTS221_ADDR_R,            0xBF

.set HTS221_WHO_AM_I_REG,      0x0F

.set HTS221_CTRL_REG1,         0x20
.set HTS221_CTRL_REG1_VALUE,   0x81  /* active mode; ODR=1Hz; BDU disable*/

.set HTS221_AV_CONF_REG,       0x10
.set HTS221_AV_CONF_REG_VALUE, 0x1B  /* Humidity average:32 samples; Temperature average:16 samples */

.set HTS221_HR_OUT_L_REG,      0x28  /* Humidity data (LSB) */
.set HTS221_HR_OUT_H_REG,      0x29  /* Humidity data (MSB) */

.set HTS221_TEMP_OUT_L_REG,    0x2A  /* Temperature data (LSB) */
.set HTS221_TEMP_OUT_H_REG,    0x2B  /* Temperature data (MSB) */

/* Calibration registers */
.set HTS221_H0_RH_X2,          0x30
.set HTS221_H1_RH_X2,          0x31
.set HTS221_T0_DEGC_X8,        0x32
.set HTS221_T1_DEGC_X8,        0x33
.set HTS221_T0_T1_DEGC_H2,     0x35
.set HTS221_H0_T0_OUT_L,       0x36
.set HTS221_H0_T0_OUT_H,       0x37
.set HTS221_H1_T0_OUT_L,       0x3A
.set HTS221_H1_T0_OUT_H,       0x3B
.set HTS221_T0_OUT_L,          0x3C
.set HTS221_T0_OUT_H,          0x3D
.set HTS221_T1_OUT_L,          0x3E
.set HTS221_T1_OUT_H,          0x3F

.bss   
    .global hum_counter
hum_counter:
    .long 0

    .global temp_counter
temp_counter:
    .long 0
   
    .global raw_hum_data
raw_hum_data:
    .skip 600

    .global raw_temp_data
raw_temp_data:
    .skip 600
    
    .global raw_hum_lsb
raw_hum_lsb:
    .long 0
    .global raw_hum_msb
raw_hum_msb:
    .long 0
    .global raw_temp_lsb
raw_temp_lsb:
    .long 0
    .global raw_temp_msb
raw_temp_msb:
    .long 0

    .global h0_rh_x2
h0_rh_x2:
    .long 0
    .global h1_rh_x2
h1_rh_x2:
    .long 0
    .global t0_degc_x8
t0_degc_x8:
    .long 0
    .global t1_degc_x8
t1_degc_x8:
    .long 0
    .global t0_t1_degc_h2
t0_t1_degc_h2:
    .long 0
    .global h0_t0_out_lsb
h0_t0_out_lsb:
    .long 0
    .global h0_t0_out_msb
h0_t0_out_msb:
    .long 0
    .global h1_t0_out_lsb
h1_t0_out_lsb:
    .long 0
    .global h1_t0_out_msb
h1_t0_out_msb:
    .long 0
    .global t0_out_lsb
t0_out_lsb:
    .long 0
    .global t0_out_msb
t0_out_msb:
    .long 0
    .global t1_out_lsb
t1_out_lsb:
    .long 0
    .global t1_out_msb
t1_out_msb:
    .long 0
   
.text
.global Task_HTS221
.global hum_counter_clear
.global temp_counter_clear

/* read one byte from hts221, aquired value stored in r0 */
    .global Read_onebyte_HTS221
Read_onebyte_HTS221:
    /* write one hts221 register address */
    psr 
    jump i2c_start_cond           // i2c Start

    ld r2, r3, 16                 // Address+Write
    psr
    jump i2c_write_byte

    ld r2, r3, 8                  // Register
    psr
    jump i2c_write_byte

    psr
    jump i2c_stop_cond            // i2c Stop

    /* read one byte from one hts221 register */
    psr 
    jump i2c_start_cond           // i2c Start

    ld r2, r3, 12                 // Address+Read
    psr
    jump i2c_write_byte

    move r2,0
    psr
    jump i2c_read_byte
    push r0
    psr
    jump i2c_send_nack  
    psr
    jump i2c_stop_cond

    pop r0
    ret

    .global Cmd_Write_HTS221
Cmd_Write_HTS221:
    psr 
    jump i2c_start_cond            // i2c Start

    ld r2, r3, 16                  // Address+Write
    psr
    jump i2c_write_byte

    ld r2, r3, 12                  // Register
    psr
    jump i2c_write_byte

    ld r2, r3, 8                   // Value
    psr
    jump i2c_write_byte

    psr
    jump i2c_stop_cond             // i2c Stop
    ret

    .global Start_HTS221
Start_HTS221:
    move r1, HTS221_ADDR_W
    push r1

    /* set active mode, odr, and bdu */
    move r1, HTS221_CTRL_REG1
    push r1
    move r1, HTS221_CTRL_REG1_VALUE
    push r1

    psr 
    jump Cmd_Write_HTS221
    pop r1

    /* set Humidity average and Temperature average */
    pop r1
    move r1, HTS221_AV_CONF_REG
    push r1
    move r1, HTS221_AV_CONF_REG_VALUE
    push r1

    psr 
    jump Cmd_Write_HTS221
    pop r1
    pop r1
    pop r1
    ret

    .global Read_HTS221_Calibration
Read_HTS221_Calibration:
    move r1, HTS221_ADDR_W
    push r1

    move r1, HTS221_ADDR_R
    push r1

    /* read humidity calibration */
    move r1, HTS221_H0_RH_X2
    push r1
    psr 
    jump Read_onebyte_HTS221
    move r2, h0_rh_x2
    st r0, r2, 0

    pop r1
    move r1, HTS221_H1_RH_X2
    push r1
    psr
    jump Read_onebyte_HTS221
    move r2, h1_rh_x2
    st r0, r2, 0

    pop r1
    move r1, HTS221_H0_T0_OUT_L
    push r1
    psr 
    jump Read_onebyte_HTS221
    move r2, h0_t0_out_lsb
    st r0, r2, 0

    pop r1
    move r1, HTS221_H0_T0_OUT_H
    push r1
    psr 
    jump Read_onebyte_HTS221
    move r2, h0_t0_out_msb
    st r0, r2, 0

    pop r1
    move r1, HTS221_H1_T0_OUT_L
    push r1
    psr 
    jump Read_onebyte_HTS221
    move r2, h1_t0_out_lsb
    st r0, r2, 0

    pop r1
    move r1, HTS221_H1_T0_OUT_H
    push r1
    psr 
    jump Read_onebyte_HTS221
    move r2, h1_t0_out_msb
    st r0, r2, 0

    /* read temperature calibration */
    pop r1
    move r1, HTS221_T0_DEGC_X8
    push r1
    psr 
    jump Read_onebyte_HTS221
    move r2, t0_degc_x8
    st r0, r2, 0

    pop r1
    move r1, HTS221_T1_DEGC_X8
    push r1
    psr 
    jump Read_onebyte_HTS221
    move r2, t1_degc_x8
    st r0, r2, 0

    pop r1
    move r1, HTS221_T0_T1_DEGC_H2
    push r1
    psr 
    jump Read_onebyte_HTS221
    move r2, t0_t1_degc_h2
    st r0, r2, 0

    pop r1
    move r1, HTS221_T0_OUT_L
    push r1
    psr 
    jump Read_onebyte_HTS221
    move r2, t0_out_lsb
    st r0, r2, 0

    pop r1
    move r1, HTS221_T0_OUT_H
    push r1
    psr 
    jump Read_onebyte_HTS221
    move r2, t0_out_msb
    st r0, r2, 0

    pop r1
    move r1, HTS221_T1_OUT_L
    push r1
    psr 
    jump Read_onebyte_HTS221
    move r2, t1_out_lsb
    st r0, r2, 0

    pop r1
    move r1, HTS221_T1_OUT_H
    push r1
    psr 
    jump Read_onebyte_HTS221
    move r2, t1_out_msb
    st r0, r2, 0

    pop r1
    pop r1
    pop r1
    ret

    .global Read_HTS221
Read_HTS221:
    move r1, HTS221_ADDR_W
    push r1

    move r1, HTS221_ADDR_R
    push r1

    /* read humidity */
    move r1, HTS221_HR_OUT_L_REG
    push r1
    psr 
    jump Read_onebyte_HTS221
    move r2, raw_hum_lsb
    st r0, r2, 0
    
    pop r1
    move r1, HTS221_HR_OUT_H_REG
    push r1
    psr 
    jump Read_onebyte_HTS221
    move r2, raw_hum_lsb
    ld r2, r2, 0
    lsh r0, r0, 8
    or r0, r0, r2
    move r1, hum_counter
    ld r1, r1, 0
    move r2, raw_hum_data
    add r2, r2, r1
    st r0, r2, 0
    psr
    jump hum_counter_increment

    /* read temperature */
    pop r1
    move r1, HTS221_TEMP_OUT_L_REG
    push r1
    psr 
    jump Read_onebyte_HTS221
    move r2, raw_temp_lsb
    st r0, r2, 0

    pop r1
    move r1, HTS221_TEMP_OUT_H_REG
    push r1
    psr
    jump Read_onebyte_HTS221
    move r2, raw_temp_lsb
    ld r2, r2, 0
    lsh r0, r0, 8
    or r0, r0, r2
    move r1, temp_counter
    ld r1, r1, 0
    move r2, raw_temp_data
    add r2, r2, r1
    st r0, r2, 0
    psr
    jump temp_counter_increment

    pop r1
    pop r1
    pop r1
    ret
   
    .global Task_HTS221
Task_HTS221:
    psr
    jump Start_HTS221

    move r2, 50                  // Wait 50 ms
    psr
    jump waitMs

    psr
    jump Read_HTS221_Calibration
    psr
    jump Read_HTS221
    ret 

   .global hum_counter_increment
hum_counter_increment:
    move r1, hum_counter
    ld r0, r1, 0
    add r0, r0, 1
    st r0, r1, 0
    ret
   
    .global hum_counter_clear
hum_counter_clear:
    move r1, hum_counter
    ld r0, r1, 0
    .set zero, 0x00
    move r0, zero
    st r0, r1, 0
    ret

    .global temp_counter_increment
temp_counter_increment:
    move r1, temp_counter
    ld r0, r1, 0
    add r0, r0, 1
    st r0, r1, 0
    ret
   
    .global temp_counter_clear
temp_counter_clear:
    move r1, temp_counter
    ld r0, r1, 0
    .set zero, 0x00
    move r0, zero
    st r0, r1, 0
    ret