/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define _I2CCBASE               (0x8200) /*!< I2C ioctl command basic value */
#define _I2CC(nr)               (_I2CCBASE | (nr)) /*!< I2C ioctl command macro */

/**
 * @brief I2C ioctl commands.
 */
#define I2CIOCSCFG              _I2CC(0x0001) /*!< Set I2C configuration */
#define I2CIOCRDWR              _I2CC(0x0002) /*!< Read/Write data by I2C */
#define I2CIOCEXCHANGE          _I2CC(0x0003) /*!< I2C master exchange data with slave */

/**
 * @brief I2C configuration flag
 */
#define I2C_MASTER              (1 << 0)  /*!< Enable master mode */
#define I2C_SDA_PULLUP          (1 << 1)  /*!< Enable SDA pin pull-up */
#define I2C_SCL_PULLUP          (1 << 2)  /*!< Enable SCL pin pull-up */
#define I2C_ADDR_10BIT          (1 << 3)  /*!< Enable 10-bit address format */

/**
 * @brief I2C message flag
 */
#define I2C_MSG_WRITE           (1 << 0)  /*!< Write message */
#define I2C_MSG_CHECK_ACK       (1 << 1)  /*!< Check ack after writing data */
#define I2C_MSG_NO_START        (1 << 2)  /*!< Transfer should not begin with I2C start */
#define I2C_MSG_NO_END          (1 << 3)  /*!< Transfer should not end with I2C end */

/**
 * @brief I2C exchange message flag
 */
#define I2C_EX_MSG_READ_FIRST   (1 << 0)  /*!< Read and then write */ 
#define I2C_EX_MSG_CHECK_ACK    (1 << 1)  /*!< Check ack after writing data */
#define I2C_EX_MSG_DELAY_EN     (1 << 2)  /*!< Delay between write and read */ 

/**
 * @brief I2C configuration.
 */
typedef struct i2c_cfg {
    uint16_t sda_pin; /*!< I2C SDA pin number */
    uint16_t scl_pin; /*!< I2C SCL pin number */

    union {
        struct {
            uint32_t master     :  1; /*!< 1: master mode, 0: slave mode */
            uint32_t sda_pullup :  1; /*!< 1: SDA pin pull-up, 0: SDA pin not pull-up */
            uint32_t scl_pullup :  1; /*!< 1: SCL pin pull-up, 0: SCL pin not pull-up */
            uint32_t addr_10bit :  1; /*!< 1: 10-bit address, 0: 7-bit address */
        } flags_data;
        uint32_t flags; /*!< I2C configuration flags */
    };

    union {
        struct {
            uint32_t clock; /*!< I2C master clock frequency */
        } master;
        struct {
            uint32_t max_clock; /*!< I2C slave clock maximium frequency */
            uint16_t addr;      /*!< I2C slave address */
        } slave;
    };
} i2c_cfg_t;

/**
 * @brief I2C Read/Write Message.
 */
typedef struct i2c_msg {
    union {
        struct {
            uint32_t write     :  1; /*!< 1: write message, 0: read message */
            uint32_t check_ack :  1; /*!< 1: check ack after writing data, 0: not check */
            uint32_t no_start  :  1; /*!< 1: not begin with I2C start, 0: begin with I2C start */
            uint32_t no_end    :  1; /*!< 1: not end with I2C end, 0: end with I2C end */
        } flags_data;
        uint16_t flags;  /*!< I2C message flags */
    };
    uint16_t addr;      /*!< I2C peer address */

    uint8_t  *buffer;   /*!< I2C transmission data pointer */
    uint32_t size;      /*!< I2C transmission data size */
} i2c_msg_t;

/**
 * @brief I2C Exchange Message.
 */
typedef struct i2c_ex_msg {
    union {
        struct {
            uint32_t read_first :  1; /*!< 1: read and then write, 0: write and then read */
            uint32_t check_ack  :  1; /*!< 1: check ack after writing data, 0: not check */
            uint32_t delay_en   :  1; /*!< 1: delay between write and read, 0: not delay */
        } flags_data;
        uint16_t flags;     /*!< I2C exchange message flags */
    };
    uint16_t addr;          /*!< I2C exchange peer address */
    uint32_t delay_ms;      /*!< Delay time by millisecond, if the time is not multiple of OS tick, I2C VFS driver will make it be multiple of OS tick by adding time */

    uint8_t  *tx_buffer;    /*!< I2C write data pointer */
    uint32_t tx_size;       /*!< I2C write data size */

    uint8_t  *rx_buffer;    /*!< I2C read data pointer */
    uint32_t rx_size;       /*!< I2C read data size */
} i2c_ex_msg_t;

#ifdef __cplusplus
}
#endif
