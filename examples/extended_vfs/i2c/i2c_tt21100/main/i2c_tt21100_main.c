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
#include <stdbool.h>
#include <sys/ioctl.h>

#include "sdkconfig.h"
#include "ext_vfs.h"
#include "ioctl/esp_i2c_ioctl.h"
#include "ioctl/esp_gpio_ioctl.h"

#define _COMBINE(a, b)          a #b
#define COMBINE(a, b)           _COMBINE(a, b)

#define MIN(a, b)               (a < b ? a : b)

#define I2C_DEVICE              "/dev/i2c/0"
#define I2C_DEVICE_CLOCK        (400 * 1000)
#define I2C_DEVICE_SDA_PIN      CONFIG_I2C_SDA_PIN_NUM
#define I2C_DEVICE_SCL_PIN      CONFIG_I2C_SCL_PIN_NUM

#define GPIO_PIN_NUM            CONFIG_TT21100_READY_PIN_NUM
#define GPIO_DEVICE_BASE        "/dev/gpio/"
#define GPIO_DEVICE             COMBINE(GPIO_DEVICE_BASE, GPIO_PIN_NUM) 

#define I2C_TEST_COUNT          CONFIG_I2C_TEST_COUNT

#define I2C_TT21100_ADDRESS     0x24

#define TT21100_TOUCH_POINT_NUM 2
#define TT21100_BUTTON_NUM      4

#define TT21100_PACK_ID_TOUCH   0x1
#define TT21100_PACK_ID_BUTTON  0x3

typedef struct touch_record {
    uint8_t  type       : 3;
    uint8_t  reserved_0 : 5;

    uint8_t  touch_id   : 5;
    uint8_t  event_id   : 2;
    uint8_t  tip        : 1;

    uint16_t x;
    uint16_t y;

    uint8_t  pressure;

    uint16_t major_axis_length;
    uint8_t  orientation;
} __attribute__((packed)) touch_record_t;

typedef struct report_hdr {
    uint16_t length;
    uint8_t  id;
    uint16_t time_stamp;
} __attribute__((packed)) report_hdr_t;

typedef struct touch_report {
    report_hdr_t  hdr;

    uint8_t  data_num     : 5;
    uint8_t  large_object : 1;
    uint8_t  reserved_0   : 2;

    uint8_t  noise_efect    : 3;
    uint8_t  reserved_1     : 3;
    uint8_t  report_counter : 2;

    touch_record_t record[0];
} __attribute__((packed)) touch_report_t;

typedef struct button_report {
    report_hdr_t  hdr;

    union {
        struct {
            uint8_t button_1 : 1;
            uint8_t button_2 : 1;
            uint8_t button_3 : 1;
            uint8_t button_4 : 1;
        } button_data;

        uint8_t button;
    };

    union {
        struct {
            uint16_t button_1_signal;
            uint16_t button_2_signal;
            uint16_t button_3_signal;
            uint16_t button_4_signal;
        } button_signal_data;

        uint16_t button_signal[4];
    };
} __attribute__((packed)) button_report_t;

typedef struct tt21100_device {
    int i2c_fd;
    int gpio_fd;
} tt21100_device_t;

typedef union tt21100_input_data {
    struct {
        bool valid;
        uint16_t x;
        uint16_t y;
    } touch[TT21100_TOUCH_POINT_NUM];

    struct {
        bool valid;
        uint16_t signal;
    } button[TT21100_BUTTON_NUM];
} tt21100_input_data_t;

static int tt21100_clear(tt21100_device_t *dev)
{
    int ret;
    uint8_t gpio_level;

    while (1) {
        i2c_msg_t msg;
        uint8_t buffer[256];

        ret = read(dev->gpio_fd, &gpio_level, 1);
        if (ret != 1 || gpio_level != 0) {
            break;
        }

        msg.flags  = 0;
        msg.addr   = I2C_TT21100_ADDRESS;
        msg.buffer = buffer;
        msg.size   = sizeof(buffer);
        ret = ioctl(dev->i2c_fd, I2CIOCRDWR, &msg);
        if (ret < 0) {
            printf("Read data from TT21100 failed, errno=%d.\n", errno);
            return -1;
        }
    }

    return 0;
}

static int tt21100_init(tt21100_device_t *dev)
{
    int ret;
    i2c_cfg_t i2c_cfg;
    gpioc_cfg_t gpio_cfg;

    ext_vfs_init();

    dev->i2c_fd = open(I2C_DEVICE, O_RDONLY);
    if (dev->i2c_fd < 0) {
        printf("Opening device %s for reading failed, errno=%d.\n", I2C_DEVICE, errno);
        goto errout_open_i2c;
    }

    i2c_cfg.flags   = I2C_MASTER;
#ifdef CONFIG_I2C_SDA_SCL_PIN_PULLUP
    i2c_cfg.flags  |= I2C_SDA_PULLUP | I2C_SCL_PULLUP;
#endif
    i2c_cfg.sda_pin = I2C_DEVICE_SDA_PIN;
    i2c_cfg.scl_pin = I2C_DEVICE_SCL_PIN;
    i2c_cfg.master.clock = I2C_DEVICE_CLOCK;
    ret = ioctl(dev->i2c_fd, I2CIOCSCFG, &i2c_cfg);
    if (ret < 0) {
        printf("Configure I2C failed, errno=%d.\n", errno);
        goto errout_ioctl_i2c;
    }

    dev->gpio_fd = open(GPIO_DEVICE, O_RDONLY);
    if (dev->gpio_fd < 0) {
        printf("Opening device %s for read failed, errno=%d.\n", GPIO_DEVICE, errno);
        goto errout_ioctl_i2c;
    }

    gpio_cfg.flags = GPIOC_PULLUP_EN;
    ret = ioctl(dev->gpio_fd, GPIOCSCFG, &gpio_cfg);
    if (ret < 0) {
        printf("Set GPIO-%d pull-up failed, errno=%d.\n", GPIO_PIN_NUM, errno);
        goto errout_ioctl_gpio;
    }

    ret = tt21100_clear(dev);
    if (ret < 0) {
        printf("Clear TT21100 failed, errno=%d.\n", errno);
        goto errout_ioctl_gpio;
    }

    return 0;

errout_ioctl_gpio:
    close(dev->gpio_fd);
errout_ioctl_i2c:
    close(dev->i2c_fd);
errout_open_i2c:
    return -1;
}

static int tt21100_read(tt21100_device_t *dev, int *msg_id, tt21100_input_data_t *input_data)
{
    int ret;
    uint8_t gpio_level;

    ret = read(dev->gpio_fd, &gpio_level, 1);
    if (ret == 1 && gpio_level == 0) {
        i2c_msg_t msg;
        uint16_t length;
        uint8_t buffer[256];
        report_hdr_t *hdr;

        msg.flags  = 0;
        msg.addr   = I2C_TT21100_ADDRESS;
        msg.buffer = (uint8_t *)&length;
        msg.size   = sizeof(length);
        ret = ioctl(dev->i2c_fd, I2CIOCRDWR, &msg);
        if (ret < 0) {
            printf("Read data from TT21100 failed, errno=%d.\n", errno);
            return -1;
        }

        msg.flags  = 0;
        msg.addr   = I2C_TT21100_ADDRESS;
        msg.buffer = buffer;
        msg.size   = length;
        ret = ioctl(dev->i2c_fd, I2CIOCRDWR, &msg);
        if (ret < 0) {
            printf("Read data from TT21100 failed, errno=%d.\n", errno);
            return -1;
        }

        hdr = (report_hdr_t *)buffer;
        switch (hdr->id) {
            case TT21100_PACK_ID_TOUCH: {
                    touch_report_t *report = (touch_report_t *)buffer;

                    if (report->data_num) {
                        int num = MIN(report->data_num, TT21100_TOUCH_POINT_NUM);
                        touch_record_t *record = report->record;

                        for (int i = 0; i < num; i++) {
                            input_data->touch[i].valid = true;
                            input_data->touch[i].x = record[i].x;
                            input_data->touch[i].y = record[i].y;
                        }

                        *msg_id = TT21100_PACK_ID_TOUCH;

                        return 1;
                    }
                }
                break;
            case TT21100_PACK_ID_BUTTON: {
                    button_report_t *report = (button_report_t *)buffer;

                    for (int i = 0; i < 4; i++) {
                        if (report->button & (1 << i)) {
                            input_data->button[i].valid  = true;
                            input_data->button[i].signal = report->button_signal[i];
                        }
                    }

                    *msg_id = TT21100_PACK_ID_BUTTON;

                    return 1;
                }
                break;
            default:
                break;
        }

    }

    return 0;
}

static void tt21100_deinit(tt21100_device_t *dev)
{
    close(dev->gpio_fd);
    close(dev->i2c_fd);    
}

void app_main(void)
{
    int ret;
    int test_count;
    tt21100_device_t device;

    ret = tt21100_init(&device);
    if (ret < 0) {
        return;
    }

    printf("TT21100 touch screen is initialized, and start reading touch point information:\n");

    test_count = 0;
    while (test_count < I2C_TEST_COUNT) {
        int msg_id;
        tt21100_input_data_t input_data = { 0 };

        ret = tt21100_read(&device, &msg_id, &input_data);
        if (ret == 1) {
            switch (msg_id) {
                case TT21100_PACK_ID_TOUCH:
                    printf("  X0=%d Y0=%d", input_data.touch[0].x,
                                            input_data.touch[0].y);
                    for (int i = 1; i < TT21100_TOUCH_POINT_NUM; i++) {
                        if (input_data.touch[i].valid) {
                            printf(" X%d=%d Y%d=%d", i, input_data.touch[i].x,
                                                     i, input_data.touch[i].y);
                        }
                    }
                    printf("\n");
                    break;
                case TT21100_PACK_ID_BUTTON:
                    printf(" ");
                    for (int i = 0; i < TT21100_TOUCH_POINT_NUM; i++) {
                        if (input_data.button[i].valid) {
                            printf(" Button%d=%d", i, input_data.button[i].signal);
                        }
                    }
                    printf("\n");
                    test_count++;
                default:
                    break;
            }

            test_count++;
        } else if (ret == 0) {
            usleep(10 * 1000);
        } else {
            break;
        }
    }

    tt21100_deinit(&device);

    printf("Test done and de-initialize TT21100 touch screen.\n");

    return;
}
