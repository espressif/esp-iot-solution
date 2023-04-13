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

void app_main(void)
{
    int fd;
    int ret;
    int count = 0;
    spi_cfg_t cfg;
    const char device[] = SPI_DEVICE;
    char text[32];
    int text_size = sizeof(text);

    ext_vfs_init();

    fd = open(device, O_RDWR);
    if (fd < 0) {
        printf("Opening device %s for writing failed, errno=%d.\n", device, errno);
        return;
    } else {
        printf("Opening device %s for writing OK, fd=%d.\n", device, fd);
    }

    cfg.flags = SPI_MASTER | SPI_MODE_0;
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

    while(1) {
        spi_ex_msg_t msg;

        snprintf(text, text_size, "SPI Tx %d", count++);

        msg.rx_buffer = NULL;
        msg.tx_buffer = text;
        msg.size = text_size;
        ret = ioctl(fd, SPIIOCEXCHANGE, &msg);
        if (ret < 0) {
            printf("Write text total %d bytes into device failed, errno=%d", text_size, errno);
            goto exit;
        } else {
            printf("Write text total %d bytes into device OK.\n", text_size);
        }

        /* Delay 100ms */
        usleep(100 * 1000);
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
