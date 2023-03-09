/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <fcntl.h>
#include <sys/errno.h>

#include "esp_vfs.h"
#include "esp_log.h"
#include "driver/gpio.h"

#include "ext_vfs_export.h"

#include "ioctl/esp_gpio_ioctl.h"

#define GPIO_PIN_MAX    GPIO_PIN_COUNT

typedef struct gpio_stat {
    int flags;
} gpio_stat_t;

static const char *TAG = "ext_vfs_gpio";

static gpio_stat_t gpio_stat[GPIO_PIN_MAX];

static void set_opendrain(int fd, bool enable)
{
    gpio_mode_t mode = enable ? GPIO_MODE_DEF_OD : 0;
    int flags = gpio_stat[fd].flags;

    if (flags == O_RDONLY) {
        mode |= GPIO_MODE_INPUT;
    } else if (flags == O_WRONLY) {
        mode |= GPIO_MODE_OUTPUT;
    } else if (flags == O_RDWR) {
        mode |= GPIO_MODE_INPUT_OUTPUT;
    }

    gpio_set_direction(fd, mode);
}

static int gpio_open(const char *path, int flags, int mode)
{
    int fd;
    esp_err_t ret;
    gpio_config_t io_conf = { 0 };

    ESP_LOGV(TAG, "open(%s, %x, %x)", path, flags, mode);

    fd = atoi(path + 1);
    if (fd < 0 || fd >= GPIO_PIN_MAX) {
        ESP_LOGE(TAG, "fd=%d is out of range", fd);
        errno = EINVAL;
        return -1;
    }

    if (gpio_stat[fd].flags) {
        errno = EBUSY;
        return -1;
    }

    io_conf.intr_type    = GPIO_INTR_DISABLE;
    io_conf.pin_bit_mask = BIT64(fd);
    if (flags == O_RDONLY) {
        io_conf.mode = GPIO_MODE_INPUT;
    } else if (flags == O_WRONLY) {
        io_conf.mode = GPIO_MODE_OUTPUT;
    } else if (flags == O_RDWR) {
        io_conf.mode = GPIO_MODE_INPUT_OUTPUT;
    } else {
        errno = EINVAL;
        return -1;
    }
    ret = gpio_config(&io_conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "failed to gpio_config ret=%d", ret);
        errno = EIO;
        return -1;
    }

    gpio_stat[fd].flags = flags;

    ESP_LOGV(TAG, "fd=%d io_conf.mode=%d", fd, io_conf.mode);

    return fd;
}

static ssize_t gpio_write(int fd, const void *buffer, size_t size)
{
    uint32_t level;
    int flags = gpio_stat[fd].flags;
    const uint8_t *data = (const uint8_t *)buffer;

    ESP_LOGV(TAG, "write(%d, %p, %u)", fd, buffer, size);

    if (!(flags == O_WRONLY) && !(flags == O_RDWR)) {
        errno = EBADF;
        return -1;
    }

    if (!data || !size) {
        errno = EINVAL;
        return -1;
    }

    level = data[0] ? 1 : 0;

    ESP_LOGV(TAG, "gpio_set_level(%d, %d)", fd, (int)level);

    gpio_set_level(fd, level);

    return 1;
}

static ssize_t gpio_read(int fd, void *buffer, size_t size)
{
    int flags = gpio_stat[fd].flags;
    uint8_t *data = (uint8_t *)buffer;

    ESP_LOGV(TAG, "read(%d, %p, %u)", fd, buffer, size);

    if (!(flags == O_RDONLY) && !(flags == O_RDWR)) {
        errno = EBADF;
        return -1;
    }

    if (!data || !size) {
        errno = EINVAL;
        return -1;
    }

    data[0] = gpio_get_level(fd);

    ESP_LOGV(TAG, "gpio_get_level(%d)=%d", fd, data[0]);

    return 1;
}

static int gpio_close(int fd)
{
    int ret;
    gpio_config_t io_conf = { 0 };

    ESP_LOGV(TAG, "close(%d)", fd);

    io_conf.intr_type    = GPIO_INTR_DISABLE;
    io_conf.pin_bit_mask = BIT64(fd);
    io_conf.pull_up_en   = false;
    io_conf.pull_down_en = false;
    io_conf.mode         = GPIO_MODE_DISABLE;
    ret = gpio_config(&io_conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "failed to gpio_config ret=%d", ret);
        errno = EIO;
        return -1;
    }

    gpio_stat[fd].flags = 0;

    return 0;
}

static int gpio_ioctl(int fd, int cmd, va_list va_args)
{
    int ret;

    ESP_LOGV(TAG, "cmd=%x", cmd);

    switch(cmd) {
        case GPIOCSCFG: {
            gpioc_cfg_t *cfg = va_arg(va_args, gpioc_cfg_t *);

            if (!cfg) {
                errno = EINVAL;
                return -1;
            }

            ESP_LOGV(TAG, "cfg->flags=%x", (int)cfg->flags);

            if (cfg->flags & GPIOC_PULLDOWN_EN) {
                gpio_pullup_en(fd);
            } else {
                gpio_pullup_dis(fd);
            }

            if (cfg->flags & GPIOC_PULLUP_EN) {
                gpio_pulldown_en(fd);
            } else {
                gpio_pulldown_dis(fd);
            }

            if (cfg->flags & GPIOC_OPENDRAIN_EN) {
                set_opendrain(fd, true);
            } else {
                set_opendrain(fd, false);
            }

            ret = 0;
            break;
        }
        default: {
            errno = EINVAL;
            ret = -1;
            break;
        }
    }

    return ret;
}

int ext_vfs_gpio_init(void)
{
    static const esp_vfs_t vfs = {
        .flags   = ESP_VFS_FLAG_DEFAULT,
        .open    = gpio_open,
        .write   = gpio_write,
        .read    = gpio_read,
        .ioctl   = gpio_ioctl,
        .close   = gpio_close,
    };
    const char *base_path = "/dev/gpio";

    esp_err_t err = esp_vfs_register(base_path, &vfs, NULL);
    if (err != ESP_OK) {
        return err;
    }

    return 0;
}
