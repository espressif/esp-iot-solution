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


	.set	MS5611_ADDR,             0x77 // MS5611
	.set	CMD_RESET,               0x1E // ADC reset command
	.set	CMD_ADC_READ,            0x00 // ADC read command
	.set	CMD_ADC_D1_256,          0x40 // ADC OSR=256
	.set	CMD_ADC_D1_512,          0x42 // ADC OSR=512
	.set	CMD_ADC_D1_1024,         0x44 // ADC OSR=1024
	.set	CMD_ADC_D1_2048,         0x46 // ADC OSR=2048
	.set	CMD_ADC_D1_4096,         0x48 // ADC OSR=4096
	.set	CMD_ADC_D2_256,          0x50 // ADC OSR=256
	.set	CMD_ADC_D2_512,          0x52 // ADC OSR=512
	.set	CMD_ADC_D2_1024,         0x54 // ADC OSR=1024
	.set	CMD_ADC_D2_2048,         0x56 // ADC OSR=2048
	.set	CMD_ADC_D2_4096,         0x58 // ADC OSR=4096
	.set	CMD_PROM_RD,             0xA0 // Prom read command
	.set	PROM_NB,                 0x08 // PROM lenth


	/* Define variables, which go into .bss section (zero-initialized data) */
	.bss
	.global stack
stack:
	.skip 100
	.global stackEnd
stackEnd:
	.long 0

#if 1
	.global D1_L 				/* Digital pressure value, low 16bit */
D1_L:
	.long 0
	.global D1_H  				/* Digital pressure value, high 16bit */
D1_H:
	.long 0
	.global D2_L 				/* Digital temperature value, low 16bit */
D2_L:
	.long 0
	.global D2_H 				/* Digital temperature value, high 16bit */
D2_H:
	.long 0
	.global reg_addr	 		/* MS5611 PROM address */
reg_addr:
	.long 0
	.global counter				/* PROM read lenth counter */
counter:
	.long 0
	.global prom_table
prom_table:
	.skip 32 					/* 128bit, 8 x 4(byte) */

	.global addr_pointer 		/* read PROM data pointer */
addr_pointer:
	.long 0
	.global temp
temp:
	.long 0

#endif

	/* Code goes into .text section */
	.text
	.global entry
entry:
	move r3, stackEnd
	psr
	jump init_param
	psr
	jump MS5611_Init
	psr
	jump MS5611_Read_PROM
	psr
	jump MS5611_Convert_D1
	psr
	jump MS5611_Convert_D2

	jump wake_up

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
	//jump wake_up, eq
	/* Wake up the SoC and stop ULP program */
	wake
	/* Stop the wakeup timer so it does not restart ULP */
	WRITE_RTC_FIELD(RTC_CNTL_STATE0_REG, RTC_CNTL_ULP_CP_SLP_TIMER_EN, 0)
	halt


	.global init_param
init_param:
	clear addr_pointer
	clear counter
	clear reg_addr
	ret


.global waitMs
waitMs:
	wait 8000
	sub r2,r2,1 				/* Wait for r2 milliseconds */
	jump doneWaitMs,eq
	jump waitMs
doneWaitMs:
	ret


	.global MS5611_Init
MS5611_Init:
	psr
	jump SPI_Init 				/* init bitbang rtc gpio */
	psr
	jump CS_Enable 				/* enable cs bus */
	move r2, CMD_RESET 			/* reset MS5611 */
	psr
	jump SPI_Write_Byte 		/* send the command */
	psr
	jump CS_Disable 			/* disbale CS */
	move r2, 3 					/* ms5611 will take 2.8 ms reload */
	psr
	jump waitMs
	ret


	.global MS5611_Save_Data
MS5611_Save_Data:
	move r0, addr_pointer 		/* table pointer */
	ld r0, r0, 0
	move r1, prom_table
	add r1, r1, r0 				/* addr = base + offset */
	st r2, r1, 0 				/* save data to prom table */
	move r0, addr_pointer 		/* update addr_pointer */
	ld r1, r0, 0 
	add r1,r1, 1 				/* move a step */
	st r1, r0, 0
	ret


	.global MS5611_Read_PROM
MS5611_Read_PROM:
	psr
	jump SPI_Init 				/* init bitbang rtc gpio */
	move r2, CMD_PROM_RD 		/* MS5611 PROM first address */
	move r1, reg_addr 
	st r2, r1, 0 				/* reg_addr save PROM first address */
prom_read_loop:
	psr
	jump CS_Enable 				/* enable cs bus */
	move r1, reg_addr
	ld r2, r1, 0 				/* load PROM address to r2 */
	psr
	jump SPI_Write_Byte 		/* sent address */
	psr
	jump SPI_Read_Byte 			/* read data back high byte */
	move r1, temp
	lsh r2, r2, 8
	st r2, r1, 0				/* save high 8bit to temp */
	psr
	jump SPI_Read_Byte 			/* read data back low byte */
	move r1, temp
	ld r1, r1, 0
	or r2, r2, r1 				/* 16bit value */
	psr
	jump MS5611_Save_Data
	psr
	jump CS_Disable 			/* disbale CS */
	move r1, reg_addr 			/* load reg_addr value to r0 */
	ld r0, r1, 0
	add r0, r0, 2 				/* 0xa0,0xa2,0xa4,0xa6,0xa8,0xaa,0xac,0xae */
	st r0, r1, 0 				/* save offset address to reg_addr */
	move r1, counter			/* load counter value to r0 */
	ld r0, r1, 0 
	add r0, r0, 1 				/* inc one step */
	st r0, r1, 0 				/* store value back to counter */
	sub r0, r0, PROM_NB 		/* check if r0 == 8 */
	jump prom_read_loop, ov
	ret


	.global MS5611_Convert_D1
MS5611_Convert_D1:
	psr
	jump CS_Enable 				/* enable cs bus */
	move r2, CMD_ADC_D1_4096
	psr
	jump SPI_Write_Byte 		/* convert CMD OSR = 4096 */
	psr
	jump CS_Disable 			/* disbale CS */

	/* the conversion will take a time <= 9.04 ms to have the output ready */
	move r2, 10 				/* wait 10ms */
	psr
	jump waitMs

	psr
	jump CS_Enable 				/* enable cs bus */
	move r2, CMD_ADC_READ
	psr 
	jump SPI_Write_Byte 		/* ADC read command */
	psr
	jump SPI_Read_Byte
	move r1, D1_H
	st r2, r1, 0 				/* save high 8bit value to D1_H */
	psr
	jump SPI_Read_Byte
	move r1, temp 
	lsh r2, r2, 8
	st r2, r1, 0 				/* save in temp */
	psr
	jump SPI_Read_Byte
	move r1, temp
	ld r1, r1, 0
	or r2, r2, r1 
	move r1, D1_L
	st r2, r1, 0 				/* save low 16bit value to D1_L */
	psr
	jump CS_Disable 			/* disbale CS */
	ret


	.global MS5611_Convert_D2
MS5611_Convert_D2:
	psr
	jump CS_Enable 				/* enable cs bus */
	move r2, CMD_ADC_D2_4096
	psr
	jump SPI_Write_Byte 		/* convert CMD OSR = 4096 */
	psr
	jump CS_Disable 			/* disbale CS */
	move r2, 10					/* wait 10ms */
	psr
	jump waitMs
	psr
	jump CS_Enable 				/* enable cs bus */
	move r2, CMD_ADC_READ
	psr 
	jump SPI_Write_Byte 		/* ADC read command */
	psr
	jump SPI_Read_Byte
	move r1, D2_H
	st r2, r1, 0 				/* save high 8bit value to D2_H */
	psr
	jump SPI_Read_Byte
	move r1, temp 
	lsh r2, r2, 8
	st r2, r1, 0 				/* save in temp */
	psr
	jump SPI_Read_Byte
	move r1, temp
	ld r1, r1, 0
	or r2, r2, r1 
	move r1, D2_L
	st r2, r1, 0 				/* save low 16bit value to D2_L */
	psr
	jump CS_Disable 			/* disbale CS */
	ret


