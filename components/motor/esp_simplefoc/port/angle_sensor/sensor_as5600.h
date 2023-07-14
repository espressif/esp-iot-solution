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
     * @description: sensor as5600 init.
     * @return {*}
     */
    void sensor_as5600_init(int i2c_num, int scl_io, int sda_io);

    /**
     * @description: sensor as5600 get raw angle.
     * @return {*}
     */
    float sensor_as5600_getAngle(int i2c_num);
#ifdef __cplusplus
}
#endif

#endif
