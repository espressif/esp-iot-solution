/*
 * SPDX-FileCopyrightText: 2023-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _SENSOR_AS5600_H
#define _SENSOR_AS5600_H

#ifdef __cplusplus
extern "C"
{
#endif

/**
 * @brief As5600 initialization function.
 *
 * @param i2c_num I2C port to configure
 * @param scl_io scl pin
 * @param sda_io sda pin
 */
void sensor_as5600_init(int i2c_num, int scl_io, int sda_io);

/**
 * @brief Get As5600 angle info.
 *
 * @param i2c_num I2C port to configure
 * @return
 *     - (-1) i2c read exception
 *     - other i2c read success
 */
float sensor_as5600_getAngle(int i2c_num);
#ifdef __cplusplus
}
#endif

#endif
