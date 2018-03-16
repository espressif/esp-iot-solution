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


#ifdef SPI_BIT16 				/* if define SPI_BIT16 write and read 16bit */
	.set bit_mask, 	0x8000
	.set bit_len, 	0x10
#else 							/* default spi write and read 8bit */
	.set bit_mask, 	0x80
	.set bit_len, 	0x08
#endif

	.set SPI_MODE_1,	1		/* Mode_1, Clock Polarity is 0 and Clock Phase is 0 */
	.set SPI_MODE_2,	2		/* Mode_2, Clock Polarity is 0 and Clock Phase is 1 */
	.set SPI_MODE_3,	3		/* Mode_3, Clock Polarity is 1 and Clock Phase is 0 */
	.set SPI_MODE_4,	4		/* Mode_4, Clock Polarity is 1 and Clock Phase is 1 */
	.set SPI_MASTER,	0		/* SPI Master */
	.set SPI_SLAVE,		1		/* SPI Slave */
	.set SPI_MODE_SET,	SPI_MODE_1
	.set SPI_TYPE_SET,	SPI_MASTER

.bss
	.global spi_mode
spi_mode:
	.long	0

	/* Code goes into .text section */
	.text

.macro spi_delay
	wait 10
.endm

.macro read_MISO 
	READ_RTC_REG(RTC_GPIO_IN_REG, RTC_GPIO_IN_NEXT_S + 10, 1) 
.endm

.macro clear_SCLK
	WRITE_RTC_REG(RTC_GPIO_OUT_W1TC_REG, RTC_GPIO_OUT_DATA_W1TC_S + 17, 1, 1)
.endm
.macro set_SCLK
	WRITE_RTC_REG(RTC_GPIO_OUT_W1TS_REG, RTC_GPIO_OUT_DATA_W1TS_S + 17, 1, 1)
.endm

.macro clear_MOSI
	WRITE_RTC_REG(RTC_GPIO_OUT_W1TC_REG, RTC_GPIO_OUT_DATA_W1TC_S + 7, 1, 1)
.endm
.macro set_MOSI
	WRITE_RTC_REG(RTC_GPIO_OUT_W1TS_REG, RTC_GPIO_OUT_DATA_W1TS_S + 7, 1, 1)
.endm

.macro clear_CS 
	WRITE_RTC_REG(RTC_GPIO_OUT_W1TC_REG, RTC_GPIO_OUT_DATA_W1TC_S + 6, 1, 1)
.endm
.macro set_CS 
	WRITE_RTC_REG(RTC_GPIO_OUT_W1TS_REG, RTC_GPIO_OUT_DATA_W1TS_S + 6, 1, 1)
.endm


	.global SPI_Init
SPI_Init:
	set_CS 						/* disable CS bus */
	move r1, SPI_MODE_SET
	sub r0, r1, SPI_MODE_1
	jump gpio_init1, eq 		/* init spi mode 1 gpio */
	sub r0, r1, SPI_MODE_2
	jump gpio_init2, eq 		/* init spi mode 2 gpio */
	sub r0, r1, SPI_MODE_3
	jump gpio_init3, eq 		/* init spi mode 3 gpio */
	sub r0, r1, SPI_MODE_4
	jump gpio_init4, eq 		/* init spi mode 4 gpio */
	jump error_loop 
gpio_init1:
	clear_MOSI
	clear_SCLK					/* */
	ret
gpio_init2:
	clear_MOSI
	clear_SCLK
	ret
gpio_init3:
	clear_MOSI
	set_SCLK
	ret
gpio_init4:
	clear_MOSI
	set_SCLK
	ret

	.global CS_Disable 			/* CS high level signal disable */
CS_Disable:
	set_CS
	ret

	.global CS_Enable 			/* CS low level signal enable */
CS_Enable:
	clear_CS
	ret

	.global	SPI_Write_Byte 		/* r2 save the data to be sent out */
SPI_Write_Byte:
	move r1, SPI_MODE_SET
	sub r0, r1, SPI_MODE_1
	jump write_mode_1, eq 		/* spi mode 1 */
	sub r0, r1, SPI_MODE_2
	jump write_mode_2, eq 		/* spi mode 2 */
	sub r0, r1, SPI_MODE_3
	jump write_mode_3, eq 		/* spi mode 3 */
	sub r0, r1, SPI_MODE_4
	jump write_mode_4, eq 		/* spi mode 4 */
	jump error_loop 			/* should be never get here */
write_mode_1:					/* Clock Polarity is 0 and Clock Phase is 0 */
	stage_rst
	clear_SCLK
write_loop1:
	clear_SCLK
	and r0, r2, bit_mask
	lsh r2, r2, 1
	jumpr loop1_bit0, 1, lt
	set_MOSI
	jump loop1_bit1
loop1_bit0:
	clear_MOSI
loop1_bit1:
	spi_delay
	set_SCLK
	spi_delay
	stage_inc 1
	jumps write_loop1, bit_len, lt
	clear_SCLK
	jump spi_write_byte_end
write_mode_2:					/* Clock Polarity is 0 and Clock Phase is 1 */
	clear_SCLK
	stage_rst
write_loop2:
	set_SCLK
	and r0, r2, bit_mask
	lsh r2, r2, 1
	jumpr loop2_bit0, 1, lt
	set_MOSI
	jump loop2_bit1
loop2_bit0:
	clear_MOSI
loop2_bit1:
	spi_delay
	clear_SCLK
	spi_delay
	stage_inc 1
	jumps write_loop2, bit_len, lt
	clear_SCLK
	jump spi_write_byte_end
write_mode_3: 					/* Clock Polarity is 1 and Clock Phase is 0 */
	set_SCLK
	stage_rst
write_loop3:
	set_SCLK
	and r0, r2, bit_mask
	lsh r2, r2, 1
	jumpr loop3_bit0, 1, lt
	set_MOSI
	jump loop3_bit1
loop3_bit0:
	clear_MOSI
loop3_bit1:
	spi_delay
	clear_SCLK
	spi_delay
	stage_inc 1
	jumps write_loop3, bit_len, lt
	set_SCLK
	jump spi_write_byte_end
write_mode_4: 					/* Clock Polarity is 1 and Clock Phase is 1 */
	set_SCLK
	stage_rst
write_loop4:
	clear_SCLK
	and r0, r2, bit_mask
	lsh r2, r2, 1
	jumpr loop4_bit0, 1, lt
	set_MOSI
	jump loop4_bit1
loop4_bit0:
	clear_MOSI
loop4_bit1:
	spi_delay
	set_SCLK
	spi_delay
	stage_inc 1
	jumps write_loop4, bit_len, lt
	set_SCLK
	jump spi_write_byte_end
spi_write_byte_end:
	clear_MOSI
	ret

	.global SPI_Burst_Write
SPI_Burst_Write:
	clear_CS
	spi_delay
	ret


/* spi read function */
	.global	SPI_Read_Byte
SPI_Read_Byte:
	move r1, SPI_MODE_SET
	sub r0, r1, SPI_MODE_1
	jump read_mode_1, eq 		/* spi mode 1 */
	sub r0, r1, SPI_MODE_2
	jump read_mode_2, eq 		/* spi mode 2 */
	sub r0, r1, SPI_MODE_3
	jump read_mode_3, eq 		/* spi mode 3 */
	sub r0, r1, SPI_MODE_4
	jump read_mode_4, eq 		/* spi mode 4 */
	jump error_loop
read_mode_1: 					/* Clock Polarity is 0 and Clock Phase is 0 */
	clear_SCLK
	stage_rst
read_loop1:
	clear_SCLK
	spi_delay
	set_SCLK
	read_MISO
	spi_delay
	lsh r2, r2, 1
	or r2, r2, r0
	stage_inc 1
	jumps read_loop1, bit_len, lt
	clear_SCLK
	jump spi_read_byte_end
read_mode_2:					/* Clock Polarity is 0 and Clock Phase is 1 */
	clear_SCLK
	stage_rst
read_loop2:
	set_SCLK
	spi_delay
	clear_SCLK
	read_MISO
	spi_delay
	lsh r2, r2, 1
	or r2, r2, r0
	stage_inc 1
	jumps read_loop2, bit_len, lt
	clear_SCLK
	jump spi_read_byte_end
read_mode_3: 					/* Clock Polarity is 1 and Clock Phase is 0 */
	set_SCLK
	stage_rst
read_loop3:
	set_SCLK
	spi_delay
	clear_SCLK
	read_MISO
	spi_delay
	lsh r2, r2, 1
	or r2, r2, r0
	stage_inc 1
	jumps read_loop3, bit_len, lt
	set_SCLK
	jump spi_read_byte_end
read_mode_4: 					/* Clock Polarity is 1 and Clock Phase is 1 */
	set_SCLK
	stage_rst
read_loop4:
	clear_SCLK
	spi_delay
	set_SCLK
	read_MISO
	spi_delay
	lsh r2, r2, 1
	or r2, r2, r0
	stage_inc 1
	jumps read_loop4, bit_len, lt
	set_SCLK
	jump spi_read_byte_end
spi_read_byte_end:
	ret

error_loop:
	ret
