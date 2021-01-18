// Copyright 2020 Espressif Systems (Shanghai) Co. Ltd.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
#include <string.h>
#include "esp_log.h"
#include "i2c_bus.h"
#include "ns2016.h"
#include "driver/gpio.h"
#include "touch_calibration.h"

static const char *TAG = "NS2016";

#define TOUCH_CHECK(a, str, ret)  if(!(a)) {                                      \
        ESP_LOGE(TAG,"%s:%d (%s):%s", __FILE__, __LINE__, __FUNCTION__, str);   \
        return (ret);                                                                   \
    }

#define WRITE_BIT          (I2C_MASTER_WRITE)       /*!< I2C master write */
#define READ_BIT           (I2C_MASTER_READ)        /*!< I2C master read */
#define ACK_CHECK_EN       0x1                      /*!< I2C master will check ack from slave*/
#define ACK_CHECK_DIS      0x0                      /*!< I2C master will not check ack from slave */
#define ACK_VAL            0x0                      /*!< I2C ack value */
#define NACK_VAL           0x1                      /*!< I2C nack value */

#define NS2016_TOUCH_CMD_X   0xC0
#define NS2016_TOUCH_CMD_Y   0xD0
#define NS2016_TOUCH_CMD_Z1  0xE0
#define NS2016_TOUCH_CMD_Z2  0xF0

#define NS2016_SMP_SIZE  4
#define NS2016_THRESHOLD_Z CONFIG_TOUCH_PANEL_THRESHOLD_PRESS

#define TOUCH_SAMPLE_MAX 4000
#define TOUCH_SAMPLE_MIN 100

typedef struct {
    uint16_t x;
    uint16_t y;
} position_t;

typedef struct {
    i2c_bus_device_handle_t i2c_dev;
    int pin_num_int;
    touch_panel_dir_t direction;
    uint16_t width;
    uint16_t height;
} ns2016_dev_t;

static ns2016_dev_t g_touch_dev;

touch_panel_driver_t ns2016_default_driver = {
    .init = ns2016_init,
    .deinit = ns2016_deinit,
    .calibration_run = ns2016_calibration_run,
    .set_direction = ns2016_set_direction,
    .read_point_data = ns2016_sample,
};

static uint16_t ns2016_readdata(ns2016_dev_t *dev, const uint8_t command)
{
    esp_err_t ret = ESP_OK;
    uint8_t rx_data[2];

    ret |= i2c_bus_write_byte(dev->i2c_dev, NULL_I2C_MEM_ADDR, command);
    ret |= i2c_bus_read_bytes(dev->i2c_dev, NULL_I2C_MEM_ADDR, 2, rx_data);
    TOUCH_CHECK(ret == ESP_OK, "read data failed", 0xffff);
    return (rx_data[0] << 8 | rx_data[1]) >> 4;
}

static esp_err_t ns2016_get_sample(uint8_t command, uint16_t *out_data)
{
    uint16_t data = ns2016_readdata(&g_touch_dev, command);

    if (0xffff != data) {
        *out_data = data;
        return ESP_OK;
    }

    return ESP_FAIL;
}

esp_err_t ns2016_init(const touch_panel_config_t *config)
{
    TOUCH_CHECK(NULL != config, "Pointer invalid", ESP_ERR_INVALID_ARG);
    TOUCH_CHECK(TOUCH_PANEL_IFACE_I2C == config->interface_type, "Interface type not support", ESP_ERR_INVALID_ARG);
    TOUCH_CHECK(NULL == g_touch_dev.i2c_dev, "Already initialize", ESP_ERR_INVALID_STATE);

    if (config->pin_num_int >= 0) {
        gpio_pad_select_gpio(config->pin_num_int);
        gpio_set_direction(config->pin_num_int, GPIO_MODE_INPUT);
    }

    i2c_bus_device_handle_t i2c_dev = i2c_bus_device_create(config->interface_i2c.i2c_bus, config->interface_i2c.i2c_addr, config->interface_i2c.clk_freq);
    TOUCH_CHECK(NULL != i2c_dev, "i2c bus device create failed", ESP_FAIL);
    g_touch_dev.i2c_dev = i2c_dev;
    g_touch_dev.width = config->width;
    g_touch_dev.height = config->height;

    ns2016_set_direction(config->direction);
    g_touch_dev.pin_num_int = config->pin_num_int;

    uint16_t temp;
    ns2016_get_sample(NS2016_TOUCH_CMD_X, &temp);

    ESP_LOGI(TAG, "Touch panel size width: %d, height: %d", g_touch_dev.width, g_touch_dev.height);
    ESP_LOGI(TAG, "Initial successful | GPIO INT:%d | ADDR:0x%x | dir:%d",
             config->pin_num_int, config->interface_i2c.i2c_addr, config->direction);

    return ESP_OK;
}

esp_err_t ns2016_deinit(void)
{
    i2c_bus_device_delete(&g_touch_dev.i2c_dev);
    memset(&g_touch_dev, 0, sizeof(ns2016_dev_t));
    return ESP_OK;
}

int ns2016_is_pressed(void)
{
    /**
     * @note There are two ways to determine weather the touch panel is pressed
     * 1. Read the IRQ line of touch controller
     * 2. Read value of z axis
     * Only the second method is used here, so the IRQ line is not used.
     */
    uint16_t z;
    esp_err_t ret = ns2016_get_sample(NS2016_TOUCH_CMD_Z1, &z);
    TOUCH_CHECK(ret == ESP_OK, "Z sample failed", 0);
    if (z > NS2016_THRESHOLD_Z) { /** If z more than threshold, it is considered as pressed */
        return 1;
    }

    return 0;
}

esp_err_t ns2016_set_direction(touch_panel_dir_t dir)
{
    if (TOUCH_DIR_MAX < dir) {
        dir >>= 5;
    }
    g_touch_dev.direction = dir;
    return ESP_OK;
}

esp_err_t ns2016_get_rawdata(uint16_t *x, uint16_t *y)
{
    position_t samples[NS2016_SMP_SIZE];
    esp_err_t ret;
    uint32_t aveX = 0;
    uint32_t aveY = 0;

    for (int i = 0; i < NS2016_SMP_SIZE; i++) {
        ret = ns2016_get_sample(NS2016_TOUCH_CMD_X, &(samples[i].x));
        TOUCH_CHECK(ret == ESP_OK, "X sample failed", ESP_FAIL);
        ret = ns2016_get_sample(NS2016_TOUCH_CMD_Y, &(samples[i].y));
        TOUCH_CHECK(ret == ESP_OK, "Y sample failed", ESP_FAIL);

        aveX += samples[i].x;
        aveY += samples[i].y;
    }

    aveX /= NS2016_SMP_SIZE;
    aveY /= NS2016_SMP_SIZE;

    *x = aveX;
    *y = aveY;

    return ESP_OK;
}

static void ns2016_apply_rotate(uint16_t *x, uint16_t *y)
{
    uint16_t _x = *x;
    uint16_t _y = *y;

    switch (g_touch_dev.direction) {
    case TOUCH_DIR_LRTB:
        *x = _x;
        *y = _y;
        break;
    case TOUCH_DIR_LRBT:
        *x = _x;
        *y = g_touch_dev.height - _y;
        break;
    case TOUCH_DIR_RLTB:
        *x = g_touch_dev.width - _x;
        *y = _y;
        break;
    case TOUCH_DIR_RLBT:
        *x = g_touch_dev.width - _x;
        *y = g_touch_dev.height - _y;
        break;
    case TOUCH_DIR_TBLR:
        *x = _y;
        *y = _x;
        break;
    case TOUCH_DIR_BTLR:
        *x = _y;
        *y = g_touch_dev.width - _x;
        break;
    case TOUCH_DIR_TBRL:
        *x = g_touch_dev.height - _y;
        *y = _x;
        break;
    case TOUCH_DIR_BTRL:
        *x = g_touch_dev.height - _y;
        *y = g_touch_dev.width - _x;
        break;

    default:
        break;
    }
}

esp_err_t ns2016_sample(touch_panel_points_t *info)
{
    TOUCH_CHECK(NULL != info, "Pointer invalid", ESP_FAIL);
    TOUCH_CHECK(NULL != g_touch_dev.i2c_dev, "Uninitialized", ESP_ERR_INVALID_STATE);

    esp_err_t ret;
    uint16_t x, y;

    info->curx[0] = 0;
    info->cury[0] = 0;
    info->event = TOUCH_EVT_RELEASE;
    info->point_num = 0;

    int state = ns2016_is_pressed();
    if (0 == state) {
        return ESP_OK;
    }

    ret = ns2016_get_rawdata(&x, &y);
    TOUCH_CHECK(ret == ESP_OK, "Get raw data failed", ESP_FAIL);

    /**< If the data is not in the normal range, it is considered that it is not pressed */
    if ((x < TOUCH_SAMPLE_MIN) || (x > TOUCH_SAMPLE_MAX) ||
            (y < TOUCH_SAMPLE_MIN) || (y > TOUCH_SAMPLE_MAX)) {
        return ESP_OK;
    }

    int32_t _x = x;
    int32_t _y = y;
    ret = touch_calibration_transform(&_x, &_y);
    if (ESP_OK != ret) {
        ESP_LOGE(TAG, "Transform raw data failed. Maybe the xxx.calibration_run() function has been not call");
        _x = _y = 0;
        return ESP_FAIL;
    }

    x = _x;
    y = _y;

    ns2016_apply_rotate(&x, &y);

    info->curx[0] = x;
    info->cury[0] = y;
    info->event = state ? TOUCH_EVT_PRESS : TOUCH_EVT_RELEASE;
    info->point_num = 1;
    return ESP_OK;
}

esp_err_t ns2016_calibration_run(const scr_driver_t *screen, bool recalibrate)
{
    return touch_calibration_run(screen, ns2016_is_pressed, ns2016_get_rawdata, recalibrate);
}
