/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "sdkconfig.h"
#include "esp_err.h"
#include "esp_log.h"

#include "ext_vfs.h"
#include "ext_vfs_export.h"

static const char *TAG = "ext_vfs";

void ext_vfs_init(void)
{
    ESP_LOGI(TAG, "Extended VFS version: %d.%d.%d", EXTENDED_VFS_VER_MAJOR, EXTENDED_VFS_VER_MINOR, EXTENDED_VFS_VER_PATCH);

#ifdef CONFIG_EXTENDED_VFS_GPIO
    ESP_ERROR_CHECK(ext_vfs_gpio_init());
#endif

#ifdef CONFIG_EXTENDED_VFS_I2C
    ESP_ERROR_CHECK(ext_vfs_i2c_init());
#endif

#ifdef CONFIG_EXTENDED_VFS_LEDC
    ESP_ERROR_CHECK(ext_vfs_ledc_init());
#endif

#ifdef CONFIG_EXTENDED_VFS_SPI
    ESP_ERROR_CHECK(ext_vfs_spi_init());
#endif
}
