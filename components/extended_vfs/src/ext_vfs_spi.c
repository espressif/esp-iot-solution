/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <fcntl.h>
#include <sys/errno.h>

#include "esp_vfs.h"
#include "esp_log.h"
#include "driver/spi_master.h"
#include "driver/spi_slave.h"
#include "driver/gpio.h"

#include "ext_vfs_export.h"

#include "ioctl/esp_spi_ioctl.h"

#define SPI_PORT_NUM            SOC_SPI_PERIPH_NUM  /*!< SPI port number */

#define SPI_TRANSFER_SZ         (512)
#define SPI_QUEUE_NUM           (4)
#define SPI_PIN_DISABLE         (-1)

#define SPI_TRANSFER_TIMEOUT    (1000 / portTICK_RATE_MS)  /*!< SPI transfer timeout time(FreeRTOS ticks) */

typedef struct spi_stat {
    uint32_t master :  1;       /*!< 1: SPI master mode, 0: slave mode */
    uint32_t configured :  1;   /*!< 1: SPI device is configured, 0: SPI device is not configured */
    spi_device_handle_t handle; /*!< SPI device handle, only SPI master use this */
} spi_stat_t;

static const char *TAG = "ext_vfs_spi";

static spi_stat_t spi_stat[SPI_PORT_NUM];

static int config_spi(int port, const spi_cfg_t *cfg)
{
    int ret;
    spi_device_handle_t handle;
    spi_bus_config_t buscfg;

    memset(&buscfg, 0, sizeof(buscfg));
    buscfg.miso_io_num = cfg->miso_pin;
    buscfg.mosi_io_num = cfg->mosi_pin;
    buscfg.sclk_io_num = cfg->sclk_pin;
    buscfg.quadhd_io_num = SPI_PIN_DISABLE;
    buscfg.quadwp_io_num = SPI_PIN_DISABLE;
    buscfg.max_transfer_sz = SPI_TRANSFER_SZ;

    if (cfg->flags_data.master) {
        spi_device_interface_config_t devcfg;

        ret = spi_bus_initialize(port, &buscfg, SPI_DMA_CH_AUTO);
        if (ret != ESP_OK) {
            return -1;
        }

        memset(&devcfg, 0, sizeof(devcfg));
        devcfg.clock_speed_hz = cfg->master.clock;
        devcfg.mode = cfg->flags_data.mode;
        devcfg.spics_io_num = cfg->cs_pin;
        devcfg.queue_size = SPI_QUEUE_NUM;
        if (cfg->flags_data.rx_lsb) {
            devcfg.flags |= SPI_DEVICE_RXBIT_LSBFIRST;
        }
        if (cfg->flags_data.tx_lsb) {
            devcfg.flags |= SPI_DEVICE_TXBIT_LSBFIRST;
        }

        ret = spi_bus_add_device(port, &devcfg, &handle);
        if (ret != ESP_OK) {
            return -1;
        }

        spi_stat[port].master = 1;
        spi_stat[port].handle = handle;
    } else {
        spi_slave_interface_config_t slvcfg;

        memset(&slvcfg, 0, sizeof(slvcfg));
        slvcfg.mode = cfg->flags_data.mode;
        slvcfg.spics_io_num = cfg->cs_pin;
        slvcfg.queue_size = SPI_QUEUE_NUM;
        if (cfg->flags_data.rx_lsb) {
            slvcfg.flags |= SPI_SLAVE_RXBIT_LSBFIRST;
        }
        if (cfg->flags_data.tx_lsb) {
            slvcfg.flags |= SPI_SLAVE_TXBIT_LSBFIRST;
        }

        ret = spi_slave_initialize(port, &buscfg, &slvcfg, SPI_DMA_DISABLED);
        if (ret != ESP_OK) {
            return -1;
        }

        gpio_set_pull_mode(cfg->mosi_pin, GPIO_PULLUP_ONLY);
        gpio_set_pull_mode(cfg->sclk_pin, GPIO_PULLUP_ONLY);
        gpio_set_pull_mode(cfg->cs_pin, GPIO_PULLUP_ONLY);

        spi_stat[port].master = 0;
    }

    return 0;
}

static int spi_master_transfer(int port, spi_ex_msg_t *msg)
{
    int ret;
    spi_transaction_t t;

    memset(&t, 0, sizeof(t));
    t.length = msg->size * 8;
    t.tx_buffer = msg->tx_buffer;
    t.rx_buffer = msg->rx_buffer;
    
    ret = spi_device_transmit(spi_stat[port].handle, &t);

    return ret;
}

static int spi_slave_transfer(int port, spi_ex_msg_t *msg)
{
    int ret;
    spi_slave_transaction_t t;

    memset(&t, 0, sizeof(t));
    t.length = msg->size * 8;
    t.tx_buffer = msg->tx_buffer;
    t.rx_buffer = msg->rx_buffer;

    ret = spi_slave_transmit(port, &t, portMAX_DELAY);
    if (ret == ESP_OK) {
        msg->size = t.trans_len / 8;
    }

    return ret;
}

static int spi_transfer(int port, spi_ex_msg_t *msg)
{
    int ret;

    if (spi_stat[port].master) {
        ret = spi_master_transfer(port, msg);
    } else {
        ret = spi_slave_transfer(port, msg);
    }

    return ret == ESP_OK ? 0 : -1;
}

static int spi_open(const char *path, int flags, int mode)
{
    int fd;

    ESP_LOGV(TAG, "open(%s, %x, %x)", path, flags, mode);

    fd = atoi(path + 1);
    if (fd < 0 || fd >= SPI_PORT_NUM) {
        ESP_LOGE(TAG, "fd=%d is out of range", fd);
        errno = EINVAL;
        return -1;
    }

    return fd;
}

static int spi_close(int fd)
{
    esp_err_t err;

    ESP_LOGV(TAG, "close(%d)", fd);

    if (spi_stat[fd].master) {
        err = spi_bus_remove_device(spi_stat[fd].handle);
        if (err != ESP_OK) {
            errno = EIO;
            return -1;
        }

        err = spi_bus_free(fd);
        if (err != ESP_OK) {
            errno = EIO;
            return -1;
        }

        spi_stat[fd].handle = NULL;
    } else {
        err = spi_slave_free(fd);
        if (err != ESP_OK) {
            errno = EIO;
            return -1;
        }
    }

    spi_stat[fd].configured = 0;

    return 0;
}

static int spi_ioctl(int fd, int cmd, va_list va_args)
{
    int ret;

    ESP_LOGV(TAG, "cmd=%x", cmd);

    switch (cmd) {
        case SPIIOCSCFG: {
            spi_cfg_t *cfg = va_arg(va_args, spi_cfg_t *);

            if (!cfg || spi_stat[fd].configured) {
                errno = EINVAL;
                return -1;
            }

            ret = config_spi(fd, cfg);
            if (ret < 0) {
                errno = EIO;
                return -1;
            }

            spi_stat[fd].configured = 1;

            break;
        }
        case SPIIOCEXCHANGE: {
            spi_ex_msg_t *ex_msg = va_arg(va_args, spi_ex_msg_t *);

            if (!ex_msg || !spi_stat[fd].configured) {
                errno = EINVAL;
                return -1;
            }

            ret = spi_transfer(fd, ex_msg);
            if (ret < 0) {
                errno = EIO;
                return -1;
            }

            break;
        }
        default: {
            errno = EINVAL;
            return -1;
        }
    }

    return 0;
}

int ext_vfs_spi_init(void)
{
    static const esp_vfs_t vfs = {
        .flags   = ESP_VFS_FLAG_DEFAULT,
        .open    = spi_open,
        .ioctl   = spi_ioctl,
        .close   = spi_close,
    };
    const char *base_path = "/dev/spi";

    esp_err_t err = esp_vfs_register(base_path, &vfs, NULL);
    if (err != ESP_OK) {
        return err;
    }

    return 0;
}
