/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

int ext_vfs_gpio_init(void);
int ext_vfs_i2c_init(void);
int ext_vfs_ledc_init(void);
int ext_vfs_spi_init(void);

#ifdef __cplusplus
}
#endif
