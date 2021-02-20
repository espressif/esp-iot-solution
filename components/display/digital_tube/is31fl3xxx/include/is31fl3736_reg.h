// Copyright 2015-2016 Espressif Systems (Shanghai) PTE LTD
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at

//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
#ifndef _IS31FL3736_REG_H_
#define _IS31FL3736_REG_H_

#ifndef BIT
#define BIT(x) (1 << (x))
#endif

/**< contorl regs */
#define IS31FL3736_REG_CMD         0xfd /*!< Available page 0 to page 3 registers */
#define IS31FL3736_RET_CMD_LOCK    0xfe /*!< to lock/unlock command reg */
#define IS31FL3736_REG_INTR_MASK   0XF0 /*!< configure the interrupt func */
#define IS31FL3736_REG_INTR_STATUS 0XF1 /*!< show the interrupt status */

//CS-X 1~8; SW-Y 1 ~ 12; all == 8 * 12 == 96

/**< when REG_CMD is 0x00, control page 0 regs */
#define IS31FL3736_REG_PG0_SWITCH(i)       (0x00 + i) /*!< Set on or off state for each LED R(0~17h) (W) */
#define IS31FL3736_REG_PG0_OPEN_STATUS(i)  (0x18 + i) /*!< Store open state for each LED R(0~17h) (R) */
#define IS31FL3736_REG_PG0_SHORT_STATUS(i) (0x30 + i) /*!< Store short state for each LED R(0~17h) (R) */

/**< when REG_CMD is 0x01, control page 1 regs */
#define IS31FL3736_REG_PG1_PWM(i)    (i) /*!< Set PWM duty for LED R(0~BEh) (W) */

/**< when REG_CMD is 0x02, control page 2 regs */
#define IS31FL3736_REG_PG2_BREATH(i) (i) /*!< Set operating mode of each dot R(0~BEh) (W) */

/**< when REG_CMD is 0x03, control page 3 regs */
#define IS31FL3736_REG_PG3_CONFIG  0X00 /*!< Configure the operation mode (W) */
/* is31fl3736_NORMAL_EN_M : W ;bitpos:[0] ;default: 1'b0 ; */
/*description: When SSD is "0", IS31FL3736 works in software shutdown mode and to */
/*normal operate the SSD bit should set to "1".*/
#define IS31FL3736_NORMAL_EN_M  (BIT(0))
#define IS31FL3736_NORMAL_EN_V  (0x1)
#define IS31FL3736_NORMAL_EN_S  (0)
/* is31fl3736_BREATH_EN_M : W ;bitpos:[1] ;default: 1'b0 ; */
/*description: those dots select working in ABM-x mode will start to run the pre-established timing. */
/*If it is disabled, all dots work in PWM mode. */
#define IS31FL3736_BREATH_EN_M  (BIT(1))
#define IS31FL3736_BREATH_EN_V  (0x1)
#define IS31FL3736_BREATH_EN_S  (1)
/* is31fl3736_DETECT_EN_M : W ;bitpos:[2] ;default: 1'b0 ; */
/*description: open/short detection will be trigger once, the user could trigger OS detection */
/*again by set OSD from 0 to 1. */
#define IS31FL3736_DETECT_EN_M  (BIT(2))
#define IS31FL3736_DETECT_EN_V  (0x1)
#define IS31FL3736_DETECT_EN_S  (2)
/* is31fl3736_SYNC_EN_M : W ;bitpos:[7:6] ;default: 1'b0 ; */
/*description: When SSD is "0", When SYNC bits are set to "01", the IS31FL3736 is */
/*configured as the master clock source and the SYNC pin will generate a clock signal distributed to */
/*the clock slave devices.To be configured as a clock slave deviceand accept an external clock input the */
/*slave device��s SYNC bits must be set to "10". */
#define IS31FL3736_SYNC_EN_M  ((IS31FL3736_SYNC_EN_V)<<(IS31FL3736_SYNC_EN_S))
#define IS31FL3736_SYNC_EN_V  (0x3)
#define IS31FL3736_SYNC_EN_S  (6)

#define IS31FL3736_REG_PG3_CURR        0X01 /*!< Set the global current (W) */
#define IS31FL3736_REG_PG3_FADE_IN(i)  (0x02 + i*4) /*!< Set fade in and hold time for breath function of ABMi R(0~2) (W) */
#define IS31FL3736_REG_PG3_FADE_OUT(i) (0x03 + i*4) /*!< Set fade out and hold time for breath function of ABMi R(0~2) (W) */
#define IS31FL3736_REG_PG3_LOOP1(i)    (0x04 + i*4) /*!< Set loop characters of ABM-i R(0~2) (W) */
#define IS31FL3736_REG_PG3_LOOP2(i)    (0x05 + i*4) /*!< Set loop characters of ABM-i R(0~2) (W) */
#define IS31FL3736_REG_PG3_UPDATE      0X0E /*!< Update the setting of 02h ~ 0Dh registers (W) */
#define IS31FL3736_REG_PG3_SW_PULLUP   0X0F /*!< Set the pull-up resistor for SWy (W) */
#define IS31FL3736_REG_PG3_CS_PULLDOWN 0X10 /*!< Set the pull-down resistor for CSx (W) */
#define IS31FL3736_REG_PG3_RESET       0X11 /*!< Reset all register to POR state (R) */

#endif
