/* ULP Example: Read temperautre in deep sleep

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
i2c_started:
	.long 0
i2c_didInit:
	.long 0

.text
.global i2c_start_cond
.global i2c_stop_cond
.global i2c_write_bit
.global i2c_read_bit
.global i2c_write_byte
.global i2c_read_byte

.macro I2C_delay
	wait 50  	// if number equ 10 then clock gap is minimal 4.7us
.endm

.macro read_SCL 
	READ_RTC_REG(RTC_GPIO_IN_REG, RTC_GPIO_IN_NEXT_S + 9, 1) 
.endm

.macro read_SDA 
	READ_RTC_REG(RTC_GPIO_IN_REG, RTC_GPIO_IN_NEXT_S + 8, 1) 
.endm

.macro set_SCL 
	WRITE_RTC_REG(RTC_GPIO_ENABLE_W1TC_REG, RTC_GPIO_ENABLE_W1TC_S + 9, 1, 1)
.endm

.macro clear_SCL 
	WRITE_RTC_REG(RTC_GPIO_ENABLE_W1TS_REG, RTC_GPIO_ENABLE_W1TS_S + 9, 1, 1)
.endm

.macro set_SDA 
	WRITE_RTC_REG(RTC_GPIO_ENABLE_W1TC_REG, RTC_GPIO_ENABLE_W1TC_S + 8, 1, 1)
.endm

.macro clear_SDA 
	WRITE_RTC_REG(RTC_GPIO_ENABLE_W1TS_REG, RTC_GPIO_ENABLE_W1TS_S + 8, 1, 1)
.endm


i2c_start_cond:
	move r1,i2c_didInit
	ld r0,r1,0
	jumpr didInit,1,ge
	move r0,1
	st r0,r1,0
	WRITE_RTC_REG(RTC_GPIO_OUT_REG, RTC_GPIO_OUT_DATA_S + 9, 1, 0)
	WRITE_RTC_REG(RTC_GPIO_OUT_REG, RTC_GPIO_OUT_DATA_S + 8, 1, 0)
didInit:
	move r2,i2c_started
	ld r0,r2,0
	jumpr not_started,1,lt 	// if started, do a restart condition
	set_SDA					// set SDA to 1
	I2C_delay
	set_SCL
clock_stretch: 				// TODO: Add timeout
	read_SCL
	jumpr clock_stretch,1,lt
	I2C_delay 				// Repeated start setup time, minimum 4.7us
not_started:
	clear_SDA 				// SCL is high, set SDA from 1 to 0.
	I2C_delay
	clear_SCL
	move r0,1
	st r0,r2,0
	ret

i2c_stop_cond:
	clear_SDA 				// set SDA to 0
	I2C_delay
	set_SCL
clock_stretch_stop:
	read_SCL
	jumpr clock_stretch_stop,1,lt
	I2C_delay 				// Stop bit setup time, minimum 4us
	set_SDA  				// SCL is high, set SDA from 0 to 1
	I2C_delay
 	move r2,i2c_started
 	move r0,0
 	st r0,r2,0
 	ret

i2c_write_bit: 				// Write a bit to I2C bus
	jumpr bit0,1,lt
	set_SDA
	jump bit1
bit0:
	clear_SDA
bit1:
	I2C_delay 				// SDA change propagation delay
	set_SCL
	I2C_delay
clock_stretch_write:
	read_SCL
	jumpr clock_stretch_write,1,lt
 	clear_SCL
 	ret

i2c_read_bit: 				// Read a bit from I2C bus
	set_SDA 				// Let the slave drive data
	I2C_delay
 	set_SCL
clock_stretch_read:
	read_SCL
	jumpr clock_stretch_read,1,lt
	I2C_delay
	read_SDA 				// SCL is high, read out bit
	clear_SCL
	ret 					// bit in r0

i2c_write_byte: 			// Return 0 if ack by the slave.
	stage_rst
next_bit:
	and r0,r2,0x80
	psr
	jump i2c_write_bit
	lsh r2,r2,1
	stage_inc 1
	jumps next_bit,8,lt
	psr
	jump i2c_read_bit
	ret  					// nack

i2c_read_byte: 				// Read a byte from I2C bus
	push r2
	move r2,0
	stage_rst
next_bit_read:
	psr
	jump i2c_read_bit
	lsh r2,r2,1
	or r2,r2,r0
	stage_inc 1
	jumps next_bit_read,8,lt
	pop r0
	psr
	jump i2c_write_bit
	move r0,r2
	ret

