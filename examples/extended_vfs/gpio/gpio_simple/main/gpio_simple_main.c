/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <stdint.h>
#include <sys/ioctl.h>

#include "sdkconfig.h"
#include "ext_vfs.h"
#include "ioctl/esp_gpio_ioctl.h"

#define _COMBINE(a, b)      a #b
#define COMBINE(a, b)       _COMBINE(a, b)

#define GPIO_PIN_NUM        CONFIG_GPIO_SIMPLE_GPIO_PIN_NUM
#define GPIO_DEVICE_BASE    "/dev/gpio/"
#define GPIO_DEVICE         COMBINE(GPIO_DEVICE_BASE, GPIO_PIN_NUM)

#define GPIO_TEST_COUNT     CONFIG_GPIO_SIMPLE_TEST_COUNT

void app_main(void)
{
    int fd;
    int ret;
    gpioc_cfg_t cfg;
    uint8_t state = 0;
    const char device[] = GPIO_DEVICE;
    
    ext_vfs_init();

    fd = open(device, O_WRONLY);
    if (fd < 0) {
        printf("Opening device %s for writing failed, errno=%d.\n", device, errno);
        return;
    } else {
        printf("Opening device %s for writing OK, fd=%d.\n", device, fd);
    }

    cfg.flags = GPIOC_PULLUP_EN;
    ret = ioctl(fd, GPIOCSCFG, &cfg);
    if (ret < 0) {
        printf("Set GPIO-%d pull-up failed, errno=%d.\n", GPIO_PIN_NUM, errno);
        close(fd);
        return;
    }

    /**
     * Change GPIO's output value GPIO_TEST_COUNT * 2 times, see changing GPIO's output
     * value from 1 to 0 as 1 time.
     */
    for (int i = 0; i < GPIO_TEST_COUNT * 2; i++) {
        state = !state;
        ret = write(fd, &state, 1);
        if (ret < 0) {
            printf("Set GPIO-%d to be %d failed, errno=%d.\n", GPIO_PIN_NUM, state, errno);
            close(fd);
            return;
        } else {
            printf("Set GPIO-%d to be %d OK.\n", GPIO_PIN_NUM, state);
        }

        sleep(1);
    }

    ret = close(fd);
    if (ret < 0) {
        printf("Close device failed, errno=%d.\n", errno);
        return;
    } else {
        printf("Close device OK.\n");
    }
}
