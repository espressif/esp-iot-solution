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
#include "ioctl/esp_ledc_ioctl.h"

#ifdef CONFIG_LEDC_DEVICE_LEDC0
#define LEDC_DEVICE "/dev/ledc/0"
#elif defined(CONFIG_LEDC_DEVICE_LEDC1)
#define LEDC_DEVICE "/dev/ledc/1"
#elif defined(CONFIG_LEDC_DEVICE_LEDC2)
#define LEDC_DEVICE "/dev/ledc/2"
#endif

#define LEDC_FREQUENCY              CONFIG_LEDC_FREQUENCY
#define LEDC_CHANNEL_NUM            1

#define LEDC_CHANNEL_OUPUT_PIN      CONFIG_LEDC_CHANNEL_OUPUT_PIN
#define LEDC_CHANNEL_DUTY           0
#define LEDC_CHANNEL_PHASE          0

#define LEDC_SIMPLE_TIME            10
#define LEDC_SIMPLE_DUTY_STEP       20

int app_main(void)
{
    int fd;
    int ret;
    ledc_cfg_t cfg;
    const ledc_channel_cfg_t channel_cfg[LEDC_CHANNEL_NUM] = {
        {
            .output_pin = LEDC_CHANNEL_OUPUT_PIN,
            .duty = LEDC_CHANNEL_DUTY,
            .phase = LEDC_CHANNEL_PHASE
        }
    };
    const char device[] = LEDC_DEVICE;

    ext_vfs_init();

    fd = open(device, O_RDWR);
    if (fd < 0) {
        printf("Opening device %s for writing failed, errno=%d.\n", device, errno);
        return -1;
    } else {
        printf("Opening device %s for writing OK, fd=%d.\n", device, fd);
    }

    cfg.frequency = LEDC_FREQUENCY;
    cfg.channel_num = LEDC_CHANNEL_NUM;
    cfg.channel_cfg = channel_cfg;
    ret = ioctl(fd, LEDCIOCSCFG, &cfg);
    if (ret < 0) {
        printf("Configure device failed, errno=%d.\n", errno);
        close(fd);
        return -1;
    }

    for (int i = 0; i < LEDC_SIMPLE_TIME; i++) {
        uint32_t duty_value = (channel_cfg[0].duty + i * LEDC_SIMPLE_DUTY_STEP) % 100;
        ledc_duty_cfg_t duty = {
            .channel = 0,
            .duty = duty_value
        };

        ret = ioctl(fd, LEDCIOCSSETDUTY, &duty);
        if (ret < 0) {
            printf("Set device duty, errno=%d.\n", errno);
            close(fd);
            return -1;
        }

        printf("Set LEDC duty to be %d\n", (int)duty_value);
        sleep(1);
    }

    ret = close(fd);
    if (ret < 0) {
        printf("Close device failed, errno=%d.\n", errno);
        return -1;
    } else {
        printf("Close device OK.\n");
    }

    return 0;
}
