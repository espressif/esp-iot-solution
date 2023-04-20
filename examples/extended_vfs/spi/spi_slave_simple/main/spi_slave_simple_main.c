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
#include <string.h>
#include <sys/ioctl.h>

#include "sdkconfig.h"
#include "ext_vfs.h"
#include "ioctl/esp_spi_ioctl.h"

#ifdef CONFIG_SPI_DEVICE_SPI2
#define SPI_DEVICE "/dev/spi/2"
#elif defined(CONFIG_SPI_DEVICE_SPI3)
#define SPI_DEVICE "/dev/spi/3"
#endif

#define SPI_DEVICE_CLOCK        (1000 * 1000)
#define SPI_DEVICE_CS_PIN       CONFIG_SPI_CS_PIN_NUM
#define SPI_DEVICE_SCLK_PIN     CONFIG_SPI_SCLK_PIN_NUM
#define SPI_DEVICE_MOSI_PIN     CONFIG_SPI_MOSI_PIN_NUM
#define SPI_DEVICE_MISO_PIN     CONFIG_SPI_MISO_PIN_NUM

#define SPI_RECV_BUF_SIZE       (32)

void app_main(void)
{
    int fd;
    int ret;
    spi_cfg_t cfg;
    const char device[] = SPI_DEVICE;
    __attribute__((aligned(4))) char recv_buffer[SPI_RECV_BUF_SIZE];

    ext_vfs_init();

    fd = open(device, O_RDWR);
    if (fd < 0) {
        printf("Opening device %s for writing failed, errno=%d.\n", device, errno);
        return;
    } else {
        printf("Opening device %s for writing OK, fd=%d.\n", device, fd);
    }

    cfg.flags = SPI_MODE_0;
    cfg.cs_pin = SPI_DEVICE_CS_PIN;
    cfg.sclk_pin = SPI_DEVICE_SCLK_PIN;
    cfg.mosi_pin = SPI_DEVICE_MOSI_PIN;
    cfg.miso_pin = SPI_DEVICE_MISO_PIN;
    cfg.master.clock = SPI_DEVICE_CLOCK;
    ret = ioctl(fd, SPIIOCSCFG, &cfg);
    if (ret < 0) {
        printf("Configure SPI failed, errno=%d.\n", errno);
        goto exit;
    }

    while (1) {
        spi_ex_msg_t msg;

        memset(recv_buffer, 0, SPI_RECV_BUF_SIZE);
        msg.rx_buffer = recv_buffer;
        msg.tx_buffer = NULL;
        msg.size = SPI_RECV_BUF_SIZE;
        ret = ioctl(fd, SPIIOCEXCHANGE, &msg);
        if (ret < 0) {
            printf("Receive total %d bytes from device failed, errno=%d", SPI_RECV_BUF_SIZE, errno);
            goto exit;
        } else {
            if (msg.size) {
                printf("Receive total %d bytes from device: %s\n", (int)msg.size, recv_buffer);
            }
        }
    }

exit:
    ret = close(fd);
    if (ret < 0) {
        printf("Close device failed, errno=%d.\n", errno);
        return;
    } else {
        printf("Close device OK.\n");
    }
}
