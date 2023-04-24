/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <fcntl.h>
#include <sys/errno.h>

#include "esp_vfs.h"
#include "esp_log.h"
#include "driver/ledc.h"

#include "ext_vfs_export.h"

#include "ioctl/esp_ledc_ioctl.h"

#define LEDC_PORT_NUM       LEDC_TIMER_MAX  /*!< LEDC timer number */
#define LEDC_CHANNEL_NUM    LEDC_TIMER_MAX  /*!< LEDC channel number */

#define LEDC_MODE           LEDC_LOW_SPEED_MODE
#define LEDC_DUTY_RES       LEDC_TIMER_13_BIT

#define LEDC_DUTY_MAX       100
#define LEDC_PHASE_MAX      360

#define LEDC_GPIO_IDLE      0

typedef struct ledc_stat {
    uint32_t configured :  1;   /*!< 1: LEDC device is configured, 0: LEDC device is not configured */
    uint32_t channels : LEDC_CHANNEL_NUM; /*!< Channel mask */
} ledc_stat_t;

static const char *TAG = "ext_vfs_ledc";

static ledc_stat_t ledc_stat[LEDC_PORT_NUM];

static int set_freq(int timer_port, uint32_t freq)
{
    esp_err_t ret;

    ret = ledc_set_freq(LEDC_MODE, timer_port, freq);

    return ret == ESP_OK ? 0 : -1;
}

static int config_ledc(int timer_port, const ledc_cfg_t *cfg)
{
    esp_err_t ret;
    ledc_timer_config_t ledc_timer;
    ledc_channel_config_t ledc_channel;
    const ledc_channel_cfg_t *channel_cfg;

    for (int i = 0 ; i < cfg->channel_num; i++) {
        channel_cfg = &cfg->channel_cfg[i];

        if (channel_cfg->phase >= LEDC_PHASE_MAX ||
            channel_cfg->duty > LEDC_DUTY_MAX) {
            errno = EINVAL;
            return -1;
        }
    }

    memset(&ledc_timer, 0 , sizeof(ledc_timer));
    ledc_timer.timer_num = timer_port;
    ledc_timer.freq_hz = cfg->frequency;
    ledc_timer.speed_mode = LEDC_MODE;
    ledc_timer.duty_resolution = LEDC_DUTY_RES;
    ledc_timer.clk_cfg = LEDC_AUTO_CLK;
    ret = ledc_timer_config(&ledc_timer);
    if (ret != ESP_OK) {
        return -1;
    }

    memset(&ledc_channel, 0, sizeof(ledc_channel));
    for (int i = 0 ; i < cfg->channel_num; i++) {
        const ledc_channel_cfg_t *channel_cfg = &cfg->channel_cfg[i];

        ledc_channel.speed_mode = LEDC_MODE;
        ledc_channel.channel = i;
        ledc_channel.timer_sel = timer_port;
        ledc_channel.intr_type = LEDC_INTR_DISABLE;
        ledc_channel.gpio_num = channel_cfg->output_pin;
        ledc_channel.duty = channel_cfg->duty * LEDC_DUTY_RES / LEDC_DUTY_MAX;
        ledc_channel.hpoint = channel_cfg->phase * LEDC_DUTY_RES / LEDC_PHASE_MAX;
        ret = ledc_channel_config(&ledc_channel);
        if (ret != ESP_OK) {
            ledc_timer_rst(LEDC_MODE, timer_port);
            return -1;
        }

        ledc_stat[timer_port].channels |= 1 << i;
    }

    return 0;
}

static int set_duty(int timer_port, const ledc_duty_cfg_t *duty_cfg)
{
    esp_err_t ret;

    ret = ledc_set_duty(LEDC_MODE, duty_cfg->channel, duty_cfg->duty);
    if (ret != ESP_OK) {
        return -1;
    }

    ret = ledc_update_duty(LEDC_MODE, duty_cfg->channel);
    if (ret != ESP_OK) {
        return -1;
    }

    return 0;
}

static int set_phase(int timer_port, const ledc_phase_cfg_t *duty_cfg)
{
    esp_err_t ret;
    uint32_t duty = ledc_get_duty(LEDC_MODE, duty_cfg->channel);
    
    ret = ledc_set_duty_with_hpoint(LEDC_MODE, duty_cfg->channel, duty, duty_cfg->phase);
    if (ret != ESP_OK) {
        return -1;
    }

    ret = ledc_update_duty(LEDC_MODE, duty_cfg->channel);
    if (ret != ESP_OK) {
        return -1;
    }

    return 0;
}

static int pause_timer(int timer_port)
{
    esp_err_t ret;

    ret = ledc_timer_pause(LEDC_MODE, timer_port);
    if (ret != ESP_OK) {
        return -1;
    }

    return 0;
}

static int resume_timer(int timer_port)
{
    esp_err_t ret;

    ret = ledc_timer_resume(LEDC_MODE, timer_port);
    if (ret != ESP_OK) {
        return -1;
    }

    return 0;
}

static int ledc_open(const char *path, int flags, int mode)
{
    int fd;

    ESP_LOGV(TAG, "open(%s, %x, %x)", path, flags, mode);

    fd = atoi(path + 1);
    if (fd < 0 || fd >= LEDC_PORT_NUM) {
        ESP_LOGE(TAG, "fd=%d is out of range", fd);
        errno = EINVAL;
        return -1;
    }

    return fd;
}

static int ledc_close(int fd)
{
    esp_err_t err;

    ESP_LOGV(TAG, "close(%d)", fd);

    for (int i = 0; i < LEDC_CHANNEL_NUM; i++) {
        int channel = 1 << i;

        if (channel & ledc_stat[fd].channels) {
            err = ledc_stop(LEDC_MODE, i, LEDC_GPIO_IDLE);
            if (err != ESP_OK) {
                errno = EIO;
                return -1;
            }
        }
    }

    err = ledc_timer_rst(LEDC_MODE, fd);
    if (err != ESP_OK) {
        errno = EIO;
        return -1;
    }

    ledc_stat[fd].configured = 0;
    ledc_stat[fd].channels = 0;

    return 0;
}

static int ledc_ioctl(int fd, int cmd, va_list va_args)
{
    int ret;

    ESP_LOGV(TAG, "cmd=%x", cmd);

    switch (cmd) {
        case LEDCIOCSCFG: {
            ledc_cfg_t *cfg = va_arg(va_args, ledc_cfg_t *);

            if (!cfg || ledc_stat[fd].configured) {
                errno = EINVAL;
                return -1;
            }

            ret = config_ledc(fd, cfg);
            if (ret < 0) {
                errno = EIO;
                return -1;
            }

            ledc_stat[fd].configured = 1;

            break;
        }
        case LEDCIOCSSETFREQ: {
            uint32_t freq = va_arg(va_args, uint32_t);

            if (!freq || !ledc_stat[fd].configured) {
                errno = EINVAL;
                return -1;
            }

            ret = set_freq(fd, freq);
            if (ret < 0) {
                errno = EIO;
                return -1;
            }

            break;
        }
        case LEDCIOCSSETDUTY: {
            ledc_duty_cfg_t *cfg = va_arg(va_args, ledc_duty_cfg_t *);

            if (!cfg || !ledc_stat[fd].configured) {
                errno = EINVAL;
                return -1;
            }

            ret = set_duty(fd, cfg);
            if (ret < 0) {
                errno = EIO;
                return -1;
            }

            break;
        }
        case LEDCIOCSSETPHASE: {
            ledc_phase_cfg_t *cfg = va_arg(va_args, ledc_phase_cfg_t *);

            if (!cfg || !ledc_stat[fd].configured) {
                errno = EINVAL;
                return -1;
            }

            ret = set_phase(fd, cfg);
            if (ret < 0) {
                errno = EIO;
                return -1;
            }

            break;
        }
        case LEDCIOCSPAUSE:
            if (!ledc_stat[fd].configured) {
                errno = EINVAL;
                return -1;
            }

            ret = pause_timer(fd);
            if (ret < 0) {
                errno = EIO;
                return -1;
            }

            break;
        case LEDCIOCSRESUME:
            if (!ledc_stat[fd].configured) {
                errno = EINVAL;
                return -1;
            }

            ret = resume_timer(fd);
            if (ret < 0) {
                errno = EIO;
                return -1;
            }

            break;
        default: {
            errno = EINVAL;
            return -1;
        }
    }

    return 0; 
}

int ext_vfs_ledc_init(void)
{
    static const esp_vfs_t vfs = {
        .flags   = ESP_VFS_FLAG_DEFAULT,
        .open    = ledc_open,
        .ioctl   = ledc_ioctl,
        .close   = ledc_close,
    };
    const char *base_path = "/dev/ledc";

    esp_err_t err = esp_vfs_register(base_path, &vfs, NULL);
    if (err != ESP_OK) {
        return err;
    }

    return 0;
}
