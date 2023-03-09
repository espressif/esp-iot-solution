/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <fcntl.h>
#include <sys/errno.h>

#include "esp_vfs.h"
#include "esp_log.h"
#include "driver/i2c.h"

#include "ext_vfs_export.h"

#include "ioctl/esp_i2c_ioctl.h"

#ifndef CONFIG_IDF_TARGET_ESP32C2
#define I2C_SLAVE_VFS
#endif

#define I2C_PORT_NUM    I2C_NUM_MAX  /*!< I2C port number */

#define I2C_TRANSFER_TIMEOUT    (1000 / portTICK_PERIOD_MS)  /*!< I2C transfer timeout time(FreeRTOS ticks) */

typedef struct i2c_stat {
    uint32_t opened :  1;  /*!< 1: I2C device is open, 0: I2C device is not open */
    uint32_t master :  1;  /*!< 1: I2C master mode, 0: slave mode */
} i2c_stat_t;

static const char *TAG = "ext_vfs_i2c";

static i2c_stat_t i2c_stat[I2C_PORT_NUM];

static int config_i2c(int port, const i2c_cfg_t *cfg)
{
    i2c_config_t conf = {};
    esp_err_t err;

    if (cfg->flags & I2C_MASTER) {
        conf.mode = I2C_MODE_MASTER;

        conf.master.clk_speed = cfg->master.clock;

        i2c_stat[port].master = 1;
    } else {
#ifdef I2C_SLAVE_VFS
        conf.mode = I2C_MODE_SLAVE;

        conf.slave.slave_addr    = cfg->slave.addr;
        conf.slave.maximum_speed = cfg->slave.max_clock;
        if (cfg->flags & I2C_ADDR_10BIT) {
            conf.slave.addr_10bit_en = true;
        }

        i2c_stat[port].master = 0;
#else
        errno = -EINVAL;
        return -1;
#endif
    }

    if (cfg->flags & I2C_SDA_PULLUP) {
        conf.sda_pullup_en = true;
    }

    if (cfg->flags & I2C_SCL_PULLUP) {
        conf.sda_pullup_en = true;
    }

    conf.sda_io_num = cfg->sda_pin;
    conf.scl_io_num = cfg->scl_pin;

    err = i2c_param_config(port, &conf);
    if (err != ESP_OK) {
        errno = EIO;
        return -1;
    }

    err = i2c_driver_install(port, conf.mode, 0, 0, 0);
    if (err != ESP_OK) {
        errno = EIO;
        return -1;
    }

    return 0;
}

static int i2c_master_transfer(int port, const i2c_msg_t *msg)
{
    esp_err_t err;
    i2c_cmd_handle_t cmd;
    uint8_t data;
    bool check_en;

    cmd = i2c_cmd_link_create();
    if (!cmd) {
        goto errout_create_cmdlink;
    }

    if (!(msg->flags & I2C_MSG_NO_START)) {
        err = i2c_master_start(cmd);
        if (err != ESP_OK) {
            goto errout_start_cmd;
        }
    }

    data  = msg->flags & I2C_MSG_WRITE ? I2C_MASTER_WRITE : I2C_MASTER_READ;
    data |= msg->addr << 1;
    check_en = msg->flags & I2C_MSG_CHECK_ACK ? true : false;
    err = i2c_master_write_byte(cmd, data, check_en);
    if (err != ESP_OK) {
        goto errout_start_cmd;
    }

    if (msg->flags & I2C_MSG_WRITE) {
        err = i2c_master_write(cmd, msg->buffer, msg->size, check_en);
        if (err != ESP_OK) {
            goto errout_start_cmd;
        }    
    } else {
        err = i2c_master_read(cmd, msg->buffer, msg->size, I2C_MASTER_LAST_NACK);
        if (err != ESP_OK) {
            goto errout_start_cmd;
        }
    }

    if (!(msg->flags & I2C_MSG_NO_END)) {
        err = i2c_master_stop(cmd);
        if (err != ESP_OK) {
            goto errout_start_cmd;
        }
    }

    err = i2c_master_cmd_begin(port, cmd, I2C_TRANSFER_TIMEOUT);
    i2c_cmd_link_delete(cmd);
    if (err != ESP_OK) {
        errno = EIO;
        return -1;
    }

    return 0;

errout_start_cmd:
    i2c_cmd_link_delete(cmd);
errout_create_cmdlink:
    errno = ENOMEM;
    return -1;
}

#ifdef I2C_SLAVE_VFS
static int i2c_slave_transfer(int port, const i2c_msg_t *msg)
{
    int ret;

    if (msg->flags & I2C_MSG_WRITE) {
        ret = i2c_slave_write_buffer(port, msg->buffer, msg->size, I2C_TRANSFER_TIMEOUT);
    } else {
        ret = i2c_slave_read_buffer(port, msg->buffer, msg->size, I2C_TRANSFER_TIMEOUT);
    }

    if (ret != ESP_OK) {
        errno = EIO;
        return -1;
    }
    
    return 0;
}
#endif

static int i2c_transfer(int port, const i2c_msg_t *msg)
{
    int ret;

    if (i2c_stat[port].master) {
        ret = i2c_master_transfer(port, msg);
    } else {
#ifdef I2C_SLAVE_VFS
        ret = i2c_slave_transfer(port, msg);
#else
        ret = -1;
#endif
    }

    return ret;
}

static int i2c_master_read_write(int port, uint16_t addr, uint8_t *buffer, uint32_t size, bool write, bool check_ack)
{
    int ret;
    int flags;
    i2c_msg_t msg = {0};

    flags  = write ? I2C_MSG_WRITE : 0;
    flags |= check_ack ? I2C_MSG_CHECK_ACK : 0;

    msg.flags  = flags;
    msg.addr   = addr;
    msg.buffer = buffer;
    msg.size   = size;
    ret = i2c_master_transfer(port, &msg);

    return ret;
}

static int i2c_master_exchange(int port, const i2c_ex_msg_t *ex_msg)
{
    int ret;
    uint32_t ticks;
    bool check_ack;

    if (ex_msg->flags & I2C_EX_MSG_DELAY_EN) {
        ticks = ex_msg->delay_ms / portTICK_PERIOD_MS;
        if (ex_msg->delay_ms % portTICK_PERIOD_MS) {
            ticks++;
        }
    } else {
        ticks = 0;
    }

    if (ex_msg->flags & I2C_EX_MSG_CHECK_ACK) {
        check_ack = true;
    } else {
        check_ack = false;
    }

    if (ex_msg->flags & I2C_EX_MSG_READ_FIRST) {
        ret = i2c_master_read_write(port, ex_msg->addr, ex_msg->rx_buffer,
                                    ex_msg->rx_size, false, check_ack);
        if (ret < 0) {
            return -1;
        }

        if (ticks) {
            vTaskDelay(ticks);
        }

        ret = i2c_master_read_write(port, ex_msg->addr, ex_msg->tx_buffer,
                                    ex_msg->tx_size, true, check_ack);
        if (ret < 0) {
            return -1;
        }
    } else {
        ret = i2c_master_read_write(port, ex_msg->addr, ex_msg->tx_buffer,
                                    ex_msg->tx_size, true, check_ack);
        if (ret < 0) {
            return -1;
        }

        if (ticks) {
            vTaskDelay(ticks);
        }

        ret = i2c_master_read_write(port, ex_msg->addr, ex_msg->rx_buffer,
                                    ex_msg->rx_size, false, check_ack);
        if (ret < 0) {
            return -1;
        }
    }

    return 0;
}

static int i2c_exchange(int port, const i2c_ex_msg_t *ex_msg)
{
    int ret;

    if (i2c_stat[port].master) {
        ret = i2c_master_exchange(port, ex_msg);
    } else {
        errno = EINVAL;
        ret = -1;
    }

    return ret;  
}

static int i2c_open(const char *path, int flags, int mode)
{
    int fd;

    ESP_LOGV(TAG, "open(%s, %x, %x)", path, flags, mode);

    fd = atoi(path + 1);
    if (fd < 0 || fd >= I2C_PORT_NUM) {
        ESP_LOGE(TAG, "fd=%d is out of range", fd);
        errno = EINVAL;
        return -1;
    }

    if (i2c_stat[fd].opened) {
        errno = EBUSY;
        return -1;
    }

    i2c_stat[fd].opened = 1;

    return fd;
}

static int i2c_close(int fd)
{
    esp_err_t err;

    ESP_LOGV(TAG, "close(%d)", fd);

    err = i2c_driver_delete(fd);
    if (err != ESP_OK) {
        errno = EIO;
        return -1;
    }

    i2c_stat[fd].opened = 0;

    return 0;
}

static int i2c_ioctl(int fd, int cmd, va_list va_args)
{
    int ret;

    ESP_LOGV(TAG, "cmd=%x", cmd);

    switch (cmd) {
        case I2CIOCSCFG: {
            i2c_cfg_t *cfg = va_arg(va_args, i2c_cfg_t *);

            if (!cfg) {
                errno = EINVAL;
                return -1;
            }

            ret = config_i2c(fd, cfg);
            if (ret < 0) {
                return -1;
            }

            break;
        }
        case I2CIOCRDWR: {
            i2c_msg_t *msg = va_arg(va_args, i2c_msg_t *);

            if (!msg) {
                errno = EINVAL;
                return -1;
            }

            ret = i2c_transfer(fd, msg);
            if (ret < 0) {
                return -1;
            }

            break;
        }
        case I2CIOCEXCHANGE: {
            i2c_ex_msg_t *ex_msg = va_arg(va_args, i2c_ex_msg_t *);

            if (!ex_msg) {
                errno = EINVAL;
                return -1;
            }

            ret = i2c_exchange(fd, ex_msg);
            if (ret < 0) {
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

int ext_vfs_i2c_init(void)
{
    static const esp_vfs_t vfs = {
        .flags   = ESP_VFS_FLAG_DEFAULT,
        .open    = i2c_open,
        .ioctl   = i2c_ioctl,
        .close   = i2c_close,
    };
    const char *base_path = "/dev/i2c";

    esp_err_t err = esp_vfs_register(base_path, &vfs, NULL);
    if (err != ESP_OK) {
        return err;
    }

    return 0;
}
