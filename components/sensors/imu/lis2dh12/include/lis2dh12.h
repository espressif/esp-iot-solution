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
#ifndef _LIS2DH12_H_
#define _LIS2DH12_H_

#include "i2c_bus.h"
#ifdef __cplusplus
extern "C" {
#endif

#define WRITE_BIT      I2C_MASTER_WRITE  /*!< I2C master write */
#define READ_BIT       I2C_MASTER_READ   /*!< I2C master read */
#define ACK_CHECK_EN   0x1               /*!< I2C master will check ack from slave*/
#define ACK_CHECK_DIS  0x0               /*!< I2C master will not check ack from slave */
#define ACK_VAL        0x0               /*!< I2C ack value */
#define NACK_VAL       0x1               /*!< I2C nack value */

/**
* @brief  State enable
*/
typedef enum {
    LIS2DH12_DISABLE = 0,
    LIS2DH12_ENABLE,
} lis2dh12_state_t;

/**
* @brief  Bit status
*/
typedef enum {
    LIS2DH12_RESET = 0,
    LIS2DH12_SET,
} lis2dh12_bit_status_t;

/**
* @brief Temperature sensor enable
*/
typedef enum {
    LIS2DH12_TEMP_DISABLE   = 0x00,   /*!< Temperature sensor disable*/
    LIS2DH12_TEMP_ENABLE    = 0x03    /*!< Temperature sensor enable */
} lis2dh12_temp_en_t;

/**
* @brief  Output data rate configuration
*/
typedef enum {
    LIS2DH12_OPT_NORMAL        = 0x00,         /*!< Normal mode */
    LIS2DH12_OPT_HIGH_RES      = 0x01,         /*!<  High resolution */
    LIS2DH12_OPT_LOW_POWER     = 0x02,         /*!< low power mode */
} lis2dh12_opt_mode_t;

/**
* @brief  Output data rate configuration
*/
typedef enum {
    LIS2DH12_ODR_LOW_POWER   = 0x00,       /*!< Power-down mode */
    LIS2DH12_ODR_1HZ         = 0x01,       /*!< HR / Normal / Low-power mode (1 Hz) */
    LIS2DH12_ODR_10HZ        = 0x02,       /*!< HR / Normal / Low-power mode (10 Hz) */
    LIS2DH12_ODR_25HZ        = 0x03,       /*!< HR / Normal / Low-power mode (25 Hz) */
    LIS2DH12_ODR_50HZ        = 0x04,       /*!< HR / Normal / Low-power mode (50 Hz) */
    LIS2DH12_ODR_100HZ       = 0x05,       /*!< HR / Normal / Low-power mode (100 Hz) */
    LIS2DH12_ODR_200HZ       = 0x06,       /*!< HR / Normal / Low-power mode (200 Hz) */
    LIS2DH12_ODR_400HZ       = 0x07,       /*!< HR/ Normal / Low-power mode (400 Hz) */
    LIS2DH12_ODR_1620HZ      = 0x08,       /*!< Low-power mode (1.620 kHz) */
    LIS2DH12_ODR_1344HZ      = 0x09,       /*!< HR/ Normal (1.344 kHz); Low-power mode (5.376 kHz) */
} lis2dh12_odr_t;

/**
* @brief  High-pass filter mode selection
*/
typedef enum {
    LIS2DH12_HPM_NORMAL_RESET    = 0x00,      /*!< Normal mode (reset by reading REFERENCE (26h) register) */
    LIS2DH12_HPM_REF_SIG_FILTER  = 0x01,      /*!< Reference signal for filtering */
    LIS2DH12_HPM_NORMAL_MODE     = 0x02,      /*!< Normal mode */
    LIS2DH12_HPM_AUTO_IA         = 0x03,      /*!< Autoreset on interrupt event */
} lis2dh12_hpm_mode_t;

/**
* @brief  Full-scale selection
*/
typedef enum {
    LIS2DH12_FS_2G       = 0x00,    /*!< Full scale: +/-2g */
    LIS2DH12_FS_4G       = 0x01,    /*!< Full scale: +/-4g  */
    LIS2DH12_FS_8G       = 0x02,    /*!< Full scale: +/-8g  */
    LIS2DH12_FS_16G      = 0x03,    /*!< Full scale: +/-16g  */
} lis2dh12_fs_t;

/**
* @brief  Self-test mode selection
*/
typedef enum {
    LIS2DH12_ST_DISABLE     = 0x00,    /*!< Normal mode */
    LIS2DH12_ST_MODE0       = 0x01,    /*!< Self test 0  */
    LIS2DH12_ST_MODE1       = 0x02,    /*!< Self test 1  */
} lis2dh12_self_test_t;


/**
* @brief  LIS2DH12 Init structure definition.
*/
typedef struct {
    lis2dh12_state_t      sdo_pu_disc;     /*!< Disconnect SDO/SA0 pull-up  */
    lis2dh12_temp_en_t    temp_enable;     /*!< Temperature sensor enable  */
    lis2dh12_odr_t        odr;             /*!< Data rate selection  */
    lis2dh12_opt_mode_t   opt_mode;        /*!< Operating mode selection  */
    lis2dh12_state_t      z_enable;        /*!< Z-axis enable  */
    lis2dh12_state_t      y_enable;        /*!< Y-axis enable  */
    lis2dh12_state_t      x_enable;        /*!< X-axis enable  */
    lis2dh12_hpm_mode_t   hmp_mode;        /*!< High-pass filter mode selection  */
    lis2dh12_state_t      fds;             /*!< Filtered data selection  */
    lis2dh12_state_t      bdu_status;      /*!< Block data update  */
    lis2dh12_state_t      ble_status;      /*!< Big/Little Endian data selection, can be activated only in high-resolution mode  */
    lis2dh12_fs_t         fs;              /*!< Full-scale selection  */
    lis2dh12_state_t      fifo_enable;     /*!< FIFO enable  */
} lis2dh12_config_t;

typedef struct {
    int16_t raw_acce_x;
    int16_t raw_acce_y;
    int16_t raw_acce_z;
} lis2dh12_raw_acce_value_t;

typedef struct {
    float acce_x;
    float acce_y;
    float acce_z;
} lis2dh12_acce_value_t;

/**
* @brief  Bitfield positioning.
*/
#define LIS2DH12_BIT(x)   (x)

/**
* @brief  I2C address.
*/
#define LIS2DH12_I2C_ADDRESS   0x19        /*If the SA0 pad is connected to ground, address is 0x18; if connected to vcc, address is 0x19*/
#define LIS2DH12_I2C_MULTI_REG_ONCE   0x80 /*If read/write multipul register once*/

/**
* @brief  Temperature data status register
*/
#define LIS2DH12_STATUS_REG_AUX_REG      0x07

#define LIS2DH12_TOR_BIT      LIS2DH12_BIT(6)
#define LIS2DH12_TDA_BIT      LIS2DH12_BIT(2)

#define LIS2DH12_TOR_MASK     0x40
#define LIS2DH12_TDA_MASK     0x04

/**
* @brief   Temperature data (LSB)
*/
#define LIS2DH12_OUT_TEMP_L_REG      0x0C

/**
* @brief   Temperature data (HSB)
*/
#define LIS2DH12_OUT_TEMP_H_REG      0x0D

/**
* @brief Device Identification register.
*/
#define LIS2DH12_WHO_AM_I_REG        0x0F

/**
* @brief Device Identification value.
*/
#define LIS2DH12_WHO_AM_I_VAL        0x33

/**
* @brief  Control register 0
*/
#define LIS2DH12_CTRL_REG0             0x1E
#define LIS2DH12_SDO_PDU_DISC_BIT      LIS2DH12_BIT(7)
#define LIS2DH12_SDO_PDU_DISC_MASK     0x80

/**
* @brief  Temperature config register
*/
#define LIS2DH12_TEMP_CFG_REG     0x1F
#define LIS2DH12_TEMP_EN_BIT      LIS2DH12_BIT(6)
#define LIS2DH12_TEMP_EN_MASK     0xC0

/**
* @brief  Control register 1
*/
#define LIS2DH12_CTRL_REG1        0x20

#define LIS2DH12_ODR_BIT          LIS2DH12_BIT(4)
#define LIS2DH12_LP_EN_BIT        LIS2DH12_BIT(3)
#define LIS2DH12_Z_EN_BIT         LIS2DH12_BIT(2)
#define LIS2DH12_Y_EN_BIT         LIS2DH12_BIT(1)
#define LIS2DH12_X_EN_BIT         LIS2DH12_BIT(0)

#define LIS2DH12_ODR_MASK         0xF0
#define LIS2DH12_LP_EN_MASK       0x08
#define LIS2DH12_Z_EN_MASK        0x04
#define LIS2DH12_Y_EN_MASK        0x02
#define LIS2DH12_X_EN_MASK        0x01

/**
* @brief  Control register 2
*/
#define LIS2DH12_CTRL_REG2         0x21

#define LIS2DH12_HMP_BIT           LIS2DH12_BIT(6)
#define LIS2DH12_HPCF_BIT          LIS2DH12_BIT(4)
#define LIS2DH12_FDS_BIT           LIS2DH12_BIT(3)
#define LIS2DH12_HPCLICK_BIT       LIS2DH12_BIT(2)
#define LIS2DH12_HP_IA2_BIT        LIS2DH12_BIT(1)
#define LIS2DH12_HP_IA1_BIT        LIS2DH12_BIT(0)

#define LIS2DH12_HMP_MASK          0xC0
#define LIS2DH12_HPCF_MASK         0x30
#define LIS2DH12_FDS_MASK          0x08
#define LIS2DH12_HPCLICK_MASK      0x04
#define LIS2DH12_HP_IA2_MASK       0x02
#define LIS2DH12_HP_IA1_MASK       0x01

/**
* @brief  Control register 3
*/
#define LIS2DH12_CTRL_REG3            0x22

#define LIS2DH12_I1_CLICK_BIT         LIS2DH12_BIT(7)
#define LIS2DH12_I1_IA1_BIT           LIS2DH12_BIT(6)
#define LIS2DH12_I1_IA2_BIT           LIS2DH12_BIT(5)
#define LIS2DH12_I1_ZYXDA_BIT         LIS2DH12_BIT(4)
#define LIS2DH12_I1_WTM_BIT           LIS2DH12_BIT(2)
#define LIS2DH12_I1_OVERRUAN_BIT      LIS2DH12_BIT(1)

#define LIS2DH12_I1_CLICK_MASK        0x80
#define LIS2DH12_I1_IA1_MASK          0x40
#define LIS2DH12_I1_IA2_MASK          0x20
#define LIS2DH12_I1_ZYXDA_MASK        0x10
#define LIS2DH12_I1_WTM_MASK          0x04
#define LIS2DH12_I1_OVERRUAN_MASK     0x02

/**
* @brief  Control register 4
*/
#define LIS2DH12_CTRL_REG4      0x23

#define LIS2DH12_BDU_BIT        LIS2DH12_BIT(7)
#define LIS2DH12_BLE_BIT        LIS2DH12_BIT(6)
#define LIS2DH12_FS_BIT         LIS2DH12_BIT(4)
#define LIS2DH12_HR_BIT         LIS2DH12_BIT(3)
#define LIS2DH12_ST_BIT         LIS2DH12_BIT(1)
#define LIS2DH12_SIM_BIT        LIS2DH12_BIT(0)

#define LIS2DH12_BDU_MASK       0x80
#define LIS2DH12_BLE_MASK       0x40
#define LIS2DH12_FS_MASK        0x30
#define LIS2DH12_HR_MASK        0x08
#define LIS2DH12_ST_MASK        0x06
#define LIS2DH12_SIM_MASK       0x01

/**
* @brief  Control register 5
*/
#define LIS2DH12_CTRL_REG5          0x24

#define LIS2DH12_BOOT_BIT           LIS2DH12_BIT(7)
#define LIS2DH12_FIFO_EN_BIT        LIS2DH12_BIT(6)
#define LIS2DH12_LIR_INT1_BIT       LIS2DH12_BIT(3)
#define LIS2DH12_D4D_INT1_BIT       LIS2DH12_BIT(2)
#define LIS2DH12_LIR_INT2_BIT       LIS2DH12_BIT(1)
#define LIS2DH12_D4D_INT2_BIT       LIS2DH12_BIT(0)

#define LIS2DH12_BOOT_MASK          0x80
#define LIS2DH12_FIFO_EN_MASK       0x40
#define LIS2DH12_LIR_INT1_MASK      0x08
#define LIS2DH12_D4D_INT1_MASK      0x04
#define LIS2DH12_LIR_INT2_MASK      0x02
#define LIS2DH12_D4D_INT2_MASK      0x01

/**
* @brief  Control register 6
*/
#define LIS2DH12_CTRL_REG6            0x25

#define LIS2DH12_I2_CLICK_BIT         LIS2DH12_BIT(7)
#define LIS2DH12_I2_IA1_BIT           LIS2DH12_BIT(6)
#define LIS2DH12_I2_IA2_BIT           LIS2DH12_BIT(5)
#define LIS2DH12_I2_BOOT_BIT          LIS2DH12_BIT(4)
#define LIS2DH12_I2_ACT_BIT           LIS2DH12_BIT(3)
#define LIS2DH12_I2_POLARITY_BIT      LIS2DH12_BIT(1)

#define LIS2DH12_I2_CLICK_MASK        0x80
#define LIS2DH12_I2_IA1_MASK          0x40
#define LIS2DH12_I2_IA2_MASK          0x20
#define LIS2DH12_I2_BOOT_MASK         0x10
#define LIS2DH12_I2_ACT_MASK          0x08
#define LIS2DH12_I2_POLARITY_MASK     0x02

/**
* @brief  Reference value register
*/
#define LIS2DH12_REFERENCE_REG        0x26

#define LIS2DH12_REFERENCE_BIT        LIS2DH12_BIT(0)

#define LIS2DH12_REFERENCE_MASK       0xFF

/**
* @brief  status register
*/
#define LIS2DH12_STATUS_REG      0x27

/**
* @brief  X-axis acceleration data(LSB)
*/
#define LIS2DH12_OUT_X_L_REG      0x28

/**
* @brief  X-axis acceleration data(MSB)
*/
#define LIS2DH12_OUT_X_H_REG      0x29

/**
* @brief  Y-axis acceleration data(LSB)
*/
#define LIS2DH12_OUT_Y_L_REG      0x2A

/**
* @brief  Y-axis acceleration data(MSB)
*/
#define LIS2DH12_OUT_Y_H_REG      0x2B

/**
* @brief  Z-axis acceleration data(LSB)
*/
#define LIS2DH12_OUT_Z_L_REG      0x2C

/**
* @brief  Z-axis acceleration data(MSB)
*/
#define LIS2DH12_OUT_Z_H_REG      0x2D

/**
* @brief  FIFO control register
*/
#define LIS2DH12_FIFO_CTRL_REG     0x2E
#define LIS2DH12_FM_BIT            LIS2DH12_BIT(6)
#define LIS2DH12_TR_BIT            LIS2DH12_BIT(5)
#define LIS2DH12_FTH_BIT           LIS2DH12_BIT(0)
#define LIS2DH12_FM_MASK           0xC0
#define LIS2DH12_TR_MASK           0x20
#define LIS2DH12_FTH_MASK          0x1F

/**
* @brief  FIFO source register
*/
#define LIS2DH12_FIFO_SRC_CTRL_REG  0x2F

#define LIS2DH12_WTM_BIT            LIS2DH12_BIT(7)
#define LIS2DH12_OVRN_FIFO_BIT      LIS2DH12_BIT(6)
#define LIS2DH12_EMPTY_BIT          LIS2DH12_BIT(5)
#define LIS2DH12_FSS_BIT            LIS2DH12_BIT(0)

#define LIS2DH12_WTM_MASK           0x80
#define LIS2DH12_OVRN_FIFO_MASK     0x40
#define LIS2DH12_EMPTY_MASK         0x20
#define LIS2DH12_FSS_MASK           0x1F

/**
* @brief  Interrupt 1 config register
*/
#define LIS2DH12_INT1_CFG_REG             0x30
#define LIS2DH12_INT1_AOI_BIT             LIS2DH12_BIT(7)
#define LIS2DH12_INT1_6D_BIT              LIS2DH12_BIT(6)
#define LIS2DH12_INT1_ZHIE_BIT            LIS2DH12_BIT(5)
#define LIS2DH12_INT1_ZLIE_BIT            LIS2DH12_BIT(4)
#define LIS2DH12_INT1_YHIE_BIT            LIS2DH12_BIT(3)
#define LIS2DH12_INT1_YLIE_BIT            LIS2DH12_BIT(2)
#define LIS2DH12_INT1_XHIE_BIT            LIS2DH12_BIT(1)
#define LIS2DH12_INT1_XLIE_BIT            LIS2DH12_BIT(0)
#define LIS2DH12_INT1_AOI_MASK            0x80
#define LIS2DH12_INT1_6D_MASK             0x40
#define LIS2DH12_INT1_ZHIE_MASK           0x20
#define LIS2DH12_INT1_ZLIE_MASK           0x10
#define LIS2DH12_INT1_YHIE_MASK           0x08
#define LIS2DH12_INT1_YLIE_MASK           0x04
#define LIS2DH12_INT1_XHIE_MASK           0x02
#define LIS2DH12_INT1_XLIE_MASK           0x01

/**
* @brief  Interrupt 1 source register
*/
#define LIS2DH12_INT1_SRC_REG           0x31
#define LIS2DH12_INT1_IA_BIT            LIS2DH12_BIT(6)
#define LIS2DH12_INT1_ZH_BIT            LIS2DH12_BIT(5)
#define LIS2DH12_INT1_ZL_BIT            LIS2DH12_BIT(4)
#define LIS2DH12_INT1_YH_BIT            LIS2DH12_BIT(3)
#define LIS2DH12_INT1_YL_BIT            LIS2DH12_BIT(2)
#define LIS2DH12_INT1_XH_BIT            LIS2DH12_BIT(1)
#define LIS2DH12_INT1_XL_BIT            LIS2DH12_BIT(0)
#define LIS2DH12_INT1_IA_MASK           0x40
#define LIS2DH12_INT1_ZH_MASK           0x20
#define LIS2DH12_INT1_ZL_MASK           0x10
#define LIS2DH12_INT1_YH_MASK           0x08
#define LIS2DH12_INT1_YL_MASK           0x04
#define LIS2DH12_INT1_XH_MASK           0x02
#define LIS2DH12_INT1_XL_MASK           0x01

/**
* @brief  Interrupt 1 threshold register
*/
#define LIS2DH12_INT1_THS_REG            0x32
#define LIS2DH12_INT1_THS_BIT            LIS2DH12_BIT(0)
#define LIS2DH12_INT1_THS_MASK           0x7F

/**
* @brief  Interrupt 1 duration register
*/
#define LIS2DH12_INT1_DURATION_REG        0x33
#define LIS2DH12_INT1_DURATION_BIT        LIS2DH12_BIT(0)
#define LIS2DH12_INT1_DURATION_MASK       0x7F

/**
* @brief  Interrupt 2 config register
*/
#define LIS2DH12_INT2_CFG_REG             0x34

#define LIS2DH12_INT2_AOI_BIT             LIS2DH12_BIT(7)
#define LIS2DH12_INT2_6D_BIT              LIS2DH12_BIT(6)
#define LIS2DH12_INT2_ZHIE_BIT            LIS2DH12_BIT(5)
#define LIS2DH12_INT2_ZLIE_BIT            LIS2DH12_BIT(4)
#define LIS2DH12_INT2_YHIE_BIT            LIS2DH12_BIT(3)
#define LIS2DH12_INT2_YLIE_BIT            LIS2DH12_BIT(2)
#define LIS2DH12_INT2_XHIE_BIT            LIS2DH12_BIT(1)
#define LIS2DH12_INT2_XLIE_BIT            LIS2DH12_BIT(0)

#define LIS2DH12_INT2_AOI_MASK            0x80
#define LIS2DH12_INT2_6D_MASK             0x40
#define LIS2DH12_INT2_ZHIE_MASK           0x20
#define LIS2DH12_INT2_ZLIE_MASK           0x10
#define LIS2DH12_INT2_YHIE_MASK           0x08
#define LIS2DH12_INT2_YLIE_MASK           0x04
#define LIS2DH12_INT2_XHIE_MASK           0x02
#define LIS2DH12_INT2_XLIE_MASK           0x01

/**
* @brief  Interrupt 2 source register
*/
#define LIS2DH12_INT2_SRC_REG           0x35

#define LIS2DH12_INT2_IA_BIT            LIS2DH12_BIT(6)
#define LIS2DH12_INT2_ZH_BIT            LIS2DH12_BIT(5)
#define LIS2DH12_INT2_ZL_BIT            LIS2DH12_BIT(4)
#define LIS2DH12_INT2_YH_BIT            LIS2DH12_BIT(3)
#define LIS2DH12_INT2_YL_BIT            LIS2DH12_BIT(2)
#define LIS2DH12_INT2_XH_BIT            LIS2DH12_BIT(1)
#define LIS2DH12_INT2_XL_BIT            LIS2DH12_BIT(0)

#define LIS2DH12_INT2_IA_MASK           0x40
#define LIS2DH12_INT2_ZH_MASK           0x20
#define LIS2DH12_INT2_ZL_MASK           0x10
#define LIS2DH12_INT2_YH_MASK           0x08
#define LIS2DH12_INT2_YL_MASK           0x04
#define LIS2DH12_INT2_XH_MASK           0x02
#define LIS2DH12_INT2_XL_MASK           0x01

/**
* @brief   Interrupt 2 threshold register
*/
#define LIS2DH12_INT2_THS_REG           0x36
#define LIS2DH12_INT2_THS_BIT           LIS2DH12_BIT(0)
#define LIS2DH12_INT2_THS_MASK          0x7F

/**
* @brief  Interrupt 2 duration register
*/
#define LIS2DH12_INT2_DURATION_REG      0x37

#define LIS2DH12_INT2_DURATION_BIT      LIS2DH12_BIT(0)

#define LIS2DH12_INT2_DURATION_MASK     0x7F

/**
* @brief  Click config register
*/
#define LIS2DH12_CLICK_CFG_REG      0x38

#define LIS2DH12_ZD_BIT            LIS2DH12_BIT(5)
#define LIS2DH12_ZS_BIT            LIS2DH12_BIT(4)
#define LIS2DH12_YD_BIT            LIS2DH12_BIT(3)
#define LIS2DH12_YS_BIT            LIS2DH12_BIT(2)
#define LIS2DH12_XD_BIT            LIS2DH12_BIT(1)
#define LIS2DH12_XS_BIT            LIS2DH12_BIT(0)
#define LIS2DH12_ZD_MASK           0x20
#define LIS2DH12_ZS_MASK           0x10
#define LIS2DH12_YD_MASK           0x08
#define LIS2DH12_YS_MASK           0x04
#define LIS2DH12_XD_MASK           0x02
#define LIS2DH12_XS_MASK           0x01

/**
* @brief  Click source register
*/
#define LIS2DH12_CLICK_SRC_REG      0x39

#define LIS2DH12_CLICK_IA_BIT              LIS2DH12_BIT(6)
#define LIS2DH12_CLICK_DCLICK_BIT          LIS2DH12_BIT(5)
#define LIS2DH12_CLICK_SCLICK_BIT          LIS2DH12_BIT(4)
#define LIS2DH12_CLICK_SIGN_BIT            LIS2DH12_BIT(3)
#define LIS2DH12_CLICK_Z_BIT               LIS2DH12_BIT(2)
#define LIS2DH12_CLICK_Y_BIT               LIS2DH12_BIT(1)
#define LIS2DH12_CLICK_X_BIT               LIS2DH12_BIT(0)

#define LIS2DH12_CLICK_IA_MASK             0x40
#define LIS2DH12_CLICK_DCLICK_MASK         0x20
#define LIS2DH12_CLICK_SCLICK_MASK         0x10
#define LIS2DH12_CLICK_SIGN_MASK           0x08
#define LIS2DH12_CLICK_Z_MASK              0x04
#define LIS2DH12_CLICK_Y_MASK              0x02
#define LIS2DH12_CLICK_X_MASK              0x01

/**
* @brief  Click threshold register
*/
#define LIS2DH12_CLICK_THS_REG      0x3A
#define LIS2DH12_CLICK_LIR_BIT             LIS2DH12_BIT(7)
#define LIS2DH12_CLICK_THS_BIT             LIS2DH12_BIT(0)
#define LIS2DH12_CLICK_LIR_MASK            0x80
#define LIS2DH12_CLICK_THS_MASK            0x7F

/**
* @brief  Click time limit register
*/
#define LIS2DH12_TIME_LIMIT_REG      0x3B
#define LIS2DH12_TLI_BIT             LIS2DH12_BIT(0)
#define LIS2DH12_TLI_MASK            0x7F

/**
* @brief  Click time latency register
*/
#define LIS2DH12_TIME_LATENCY_REG      0x3C
#define LIS2DH12_TLA_BIT              LIS2DH12_BIT(0)
#define LIS2DH12_TLA_MASK             0xFF

/**
* @brief  Click time window register
*/
#define LIS2DH12_TIME_WINDOW_REG      0x3D
#define LIS2DH12_TW_BIT              LIS2DH12_BIT(0)
#define LIS2DH12_TW_MASK             0xFF

/**
* @brief  ACT threshold register
*/
#define LIS2DH12_ACT_THS_REG      0x3E
#define LIS2DH12_ACTH_BIT             LIS2DH12_BIT(0)
#define LIS2DH12_ACTH_MASK            0x7F

/**
* @brief  ACT duration register
*/
#define LIS2DH12_ACT_DUR_REG      0x3F
#define LIS2DH12_TW_BIT              LIS2DH12_BIT(0)
#define LIS2DH12_TW_MASK             0xFF

#define LIS2DH12_FROM_FS_2g_HR_TO_mg(lsb)  ((float)((int16_t)lsb>>4) * 1.0f)
#define LIS2DH12_FROM_FS_4g_HR_TO_mg(lsb)  ((float)((int16_t)lsb>>4) * 2.0f)
#define LIS2DH12_FROM_FS_8g_HR_TO_mg(lsb)  ((float)((int16_t)lsb>>4) * 4.0f)
#define LIS2DH12_FROM_FS_16g_HR_TO_mg(lsb) ((float)((int16_t)lsb>>4) * 12.0f)
#define LIS2DH12_FROM_LSB_TO_degC_HR(lsb)  ((float)((int16_t)lsb>>6) / 4.0f+25.0f)

typedef void *lis2dh12_handle_t;

/**
 * @brief Create and init sensor object and return a sensor handle
 *
 * @param bus I2C bus object handle
 * @param dev_addr I2C device address of sensor
 *
 * @return
 *     - NULL Fail
 *     - Others Success
 */
lis2dh12_handle_t lis2dh12_create(i2c_bus_handle_t bus, uint8_t dev_addr);

/**
 * @brief Delete and release a sensor object
 *
 * @param sensor Point to object handle of lis2dh12
 *
 * @return
 *     - ESP_OK Success
 *     - ESP_FAIL Fail
 */
esp_err_t lis2dh12_delete(lis2dh12_handle_t *sensor);

/**
 * @brief Get device identification of LIS2DH12
 *
 * @param sensor object handle of LIS2DH12
 * @param deviceid a pointer of device ID
 *
 * @return
 *     - ESP_OK Success
 *     - ESP_FAIL Fail
 */
esp_err_t lis2dh12_get_deviceid(lis2dh12_handle_t sensor, uint8_t *deviceid);

/**
 * @brief Set configration of LIS2DH12
 *
 * @param sensor object handle of LIS2DH12
 * @param lis2dh12_config a structure pointer of configration
 *
 * @return
 *     - ESP_OK Success
 *     - ESP_FAIL Fail
 */
esp_err_t lis2dh12_set_config(lis2dh12_handle_t sensor, lis2dh12_config_t *lis2dh12_config);

/**
 * @brief Get configration of LIS2DH12
 *
 * @param sensor object handle of LIS2DH12
 * @param lis2dh12_config a structure pointer of configration
 *
 * @return
 *     - ESP_OK Success
 *     - others Fail
 */
esp_err_t lis2dh12_get_config(lis2dh12_handle_t sensor, lis2dh12_config_t *lis2dh12_config);

/**
 * @brief Enable the temperature sensor of LIS2DH12
 *
 * @param sensor object handle of LIS2DH12
 * @param temp_en temperature enable status
 *
 * @return
 *     - ESP_OK Success
 *     - others Fail
 */
esp_err_t lis2dh12_set_temp_enable(lis2dh12_handle_t sensor, lis2dh12_temp_en_t temp_en);

/**
 * @brief Set output data rate
 *
 * @param sensor object handle of LIS2DH12
 * @param odr output data rate value
 *
 * @return
 *     - ESP_OK Success
 *     - ESP_FAIL Fail
 */
esp_err_t lis2dh12_set_odr(lis2dh12_handle_t sensor, lis2dh12_odr_t odr);

/**
 * @brief Enable z axis
 *
 * @param sensor object handle of LIS2DH12
 * @param status enable status
 *
 * @return
 *     - ESP_OK Success
 *     - ESP_FAIL Fail
 */
esp_err_t lis2dh12_set_z_enable(lis2dh12_handle_t sensor, lis2dh12_state_t status);

/**
 * @brief Enable y axis
 *
 * @param sensor object handle of LIS2DH12
 * @param status enable status
 *
 * @return
 *     - ESP_OK Success
 *     - ESP_FAIL Fail
 */
esp_err_t lis2dh12_set_y_enable(lis2dh12_handle_t sensor, lis2dh12_state_t status);

/**
 * @brief Enable x axis
 *
 * @param sensor object handle of LIS2DH12
 * @param status enable status
 *
 * @return
 *     - ESP_OK Success
 *     - ESP_FAIL Fail
 */
esp_err_t lis2dh12_set_x_enable(lis2dh12_handle_t sensor, lis2dh12_state_t status);

/**
 * @brief Enable block data update
 *
 * @param sensor object handle of LIS2DH12
 * @param status enable status
 *
 * @return
 *     - ESP_OK Success
 *     - ESP_FAIL Fail
 */
esp_err_t lis2dh12_set_bdumode(lis2dh12_handle_t sensor, lis2dh12_state_t status);

/**
 * @brief Set full scale
 *
 * @param sensor object handle of LIS2DH12
 * @param fs full scale value
 *
 * @return
 *     - ESP_OK Success
 *     - ESP_FAIL Fail
 */
esp_err_t lis2dh12_set_fs(lis2dh12_handle_t sensor, lis2dh12_fs_t fs);

/**
 * @brief Get full scale
 *
 * @param sensor object handle of LIS2DH12
 * @param fs full scale value
 * @return esp_err_t
 *  -  - ESP_OK Success
 *     - ESP_FAIL Fail
 */
esp_err_t lis2dh12_get_fs(lis2dh12_handle_t sensor, lis2dh12_fs_t *fs);
/**
 * @brief Set operation mode
 *
 * @param sensor object handle of LIS2DH12
 * @param opt_mode operation mode
 *
 * @return
 *     - ESP_OK Success
 *     - ESP_FAIL Fail
 */
esp_err_t lis2dh12_set_opt_mode(lis2dh12_handle_t sensor, lis2dh12_opt_mode_t opt_mode);

/**
 * @brief Get x axis acceleration
 *
 * @param sensor object handle of LIS2DH12
 * @param x_acc a poionter of x axis acceleration
 *
 * @return
 *     - ESP_OK Success
 *     - ESP_FAIL Fail
 */
esp_err_t lis2dh12_get_x_acc(lis2dh12_handle_t sensor, uint16_t *x_acc);

/**
 * @brief Get y axis acceleration
 *
 * @param sensor object handle of LIS2DH12
 * @param y_acc a poionter of y axis acceleration
 *
 * @return
 *     - ESP_OK Success
 *     - ESP_FAIL Fail
 */
esp_err_t lis2dh12_get_y_acc(lis2dh12_handle_t sensor, uint16_t *y_acc);
/**
 * @brief Get z axis acceleration
 *
 * @param sensor object handle of LIS2DH12
 * @param z_acc a poionter of z axis acceleration
 *
 * @return
 *     - ESP_OK Success
 *     - ESP_FAIL Fail
 */
esp_err_t lis2dh12_get_z_acc(lis2dh12_handle_t sensor, uint16_t *z_acc);

/**
 * @brief Read raw accelerometer measurements
 *
 * @param sensor object handle of LIS2DH12
 * @param raw_acce_value raw accelerometer measurements value
 * @return esp_err_t
 */
esp_err_t lis2dh12_get_raw_acce(lis2dh12_handle_t sensor, lis2dh12_raw_acce_value_t *raw_acce_value);

/**
 * @brief Read accelerometer measurements
 *
 * @param sensor object handle of LIS2DH12
 * @param acce_value accelerometer measurements， g
 * @return esp_err_t
 */
esp_err_t lis2dh12_get_acce(lis2dh12_handle_t sensor, lis2dh12_acce_value_t *acce_value);

/***implements of imu hal interface****/
#ifdef CONFIG_SENSOR_IMU_INCLUDED_LIS2DH12

/**
 * @brief initialize lis2dh12 with default configurations
 * 
 * @param i2c_bus i2c bus handle the sensor will attached to
 * @return
 *     - ESP_OK Success
 *     - ESP_FAIL Fail
 */
esp_err_t imu_lis2dh12_init(i2c_bus_handle_t i2c_bus);

/**
 * @brief 
 * 
 * @return esp_err_t 
 */
esp_err_t imu_lis2dh12_deinit(void);

/**
 * @brief de-initialize lis2dh12
 * 
 * @return
 *     - ESP_OK Success
 *     - ESP_FAIL Fail
 */
esp_err_t imu_lis2dh12_test(void);

/**
 * @brief acquire lis2dh12 accelerometer result one time.
 * 
 * @param acce_x result data (unit:g)
 * @param acce_y result data (unit:g)
 * @param acce_z result data (unit:g)
 * @return
 *     - ESP_OK Success
 *     - ESP_FAIL Fail
 */
esp_err_t imu_lis2dh12_acquire_acce(float *acce_x, float *acce_y, float *acce_z);
#endif

#ifdef __cplusplus
}
#endif

#endif
