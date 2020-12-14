// Copyright 2015-2020 Espressif Systems (Shanghai) PTE LTD
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at

//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
#include "esp_log.h"
#include "iot_xpt2046.h"
#include "driver/gpio.h"
#include "touch_calibration.h"

static const char *TAG = "XPT2046";

#define TOUCH_CHECK(a, str, ret)  if(!(a)) {                                      \
        ESP_LOGE(TAG,"%s:%d (%s):%s", __FILE__, __LINE__, __FUNCTION__, str);   \
        return (ret);                                                                   \
    }

#define XPT2046_TOUCH_CMD_X       0xD0
#define XPT2046_TOUCH_CMD_Y       0x90
#define XPT2046_SMP_SIZE  8

#define TOUCH_SAMPLE_MAX 4000
#define TOUCH_SAMPLE_MIN 100

typedef struct {
    uint16_t x;
    uint16_t y;
} position_t;

typedef struct {
    spi_host_device_t spi_host;
    spi_device_handle_t spi_dev;
    bool is_init_spi_bus;
    SemaphoreHandle_t spi_mux;

    int io_irq;
    touch_dir_t direction;
    uint16_t width;
    uint16_t height;
} xpt2046_dev_t;

static xpt2046_dev_t g_dev = {0};

touch_driver_fun_t xpt2046_driver_fun = {
    .init = iot_xpt2046_init,
    .deinit = iot_xpt2046_deinit,
    .calibration_run = iot_xpt2046_calibration_run,
    .set_direction = iot_xpt2046_set_direction,
    .is_pressed = iot_xpt2046_is_pressed,
    .read_info = iot_xpt2046_sample,
};

static uint16_t xpt2046_readdata(spi_device_handle_t spi, const uint8_t command)
{
    /**
     * Half duplex mode is not compatible with DMA when both writing and reading phases exist.
     * try to use command and address field to replace the write phase.
    */
    spi_transaction_ext_t t = {
        .base = {
            .flags = SPI_TRANS_VARIABLE_CMD | SPI_TRANS_USE_RXDATA,
            .cmd = command,
            .rxlength = 2 * 8, // Total data length received, in bits
        },
        .command_bits = 8,
    };
    esp_err_t ret = spi_device_transmit(spi, (spi_transaction_t *)&t);
    TOUCH_CHECK(ret == ESP_OK, "read data failed", 0xffff);
    return (t.base.rx_data[0] << 8 | t.base.rx_data[1]) >> 3;
}

static esp_err_t xpt2046_get_sample(uint8_t command, uint16_t *out_data)
{
    uint16_t data = xpt2046_readdata(g_dev.spi_dev, command);

    if (0xffff != data) {
        *out_data = data;
        return ESP_OK;
    }

    return ESP_FAIL;
}

esp_err_t iot_xpt2046_init(const touch_panel_config_t *config)
{
    TOUCH_CHECK(NULL != config, "Pointer invalid", ESP_ERR_INVALID_ARG);
    TOUCH_CHECK(TOUCH_IFACE_SPI == config->iface_type, "Interface type not support", ESP_ERR_INVALID_ARG);

    esp_err_t ret;

    g_dev.is_init_spi_bus = config->iface_spi.init_spi_bus;
    if (g_dev.is_init_spi_bus) {
        //Initialize SPI Bus for LCD
        spi_bus_config_t buscfg = {
            .miso_io_num = config->iface_spi.pin_num_miso,
            .mosi_io_num = config->iface_spi.pin_num_mosi,
            .sclk_io_num = config->iface_spi.pin_num_clk,
            .quadwp_io_num = -1,
            .quadhd_io_num = -1,
        };
        ret = spi_bus_initialize(config->iface_spi.spi_host, &buscfg, config->iface_spi.dma_chan);
        TOUCH_CHECK(ESP_OK == ret, "spi bus initialize failed", ESP_FAIL);
    }
    g_dev.spi_host = config->iface_spi.spi_host;

    //Initialize non-SPI GPIOs
    if (config->pin_num_int >= 0) {
        gpio_pad_select_gpio(config->pin_num_int);
        gpio_set_direction(config->pin_num_int, GPIO_MODE_INPUT);
    }

    spi_device_interface_config_t devcfg = {
        .clock_speed_hz = config->iface_spi.clk_freq, //Clock out frequency
        .mode = 0,                            //SPI mode 0
        .spics_io_num = config->iface_spi.pin_num_cs, //CS pin
        .queue_size = 7,                      //We want to be able to queue 7 transactions at a time
    };

    devcfg.flags = SPI_DEVICE_HALFDUPLEX;
    ret = spi_bus_add_device(config->iface_spi.spi_host, &devcfg, &g_dev.spi_dev);
    TOUCH_CHECK(ESP_OK == ret, "spi bus add device failed", ESP_FAIL);

    iot_xpt2046_set_direction(config->direction);
    g_dev.io_irq = config->pin_num_int;
    g_dev.width = config->width;
    g_dev.height = config->height;

    uint16_t temp;
    xpt2046_get_sample(XPT2046_TOUCH_CMD_X, &temp);

    /** TODO: Dealing with thread safety issues. Others have similar problems
     */
    if (g_dev.spi_mux == NULL) {
        g_dev.spi_mux = xSemaphoreCreateRecursiveMutex();
    } else {
        ESP_LOGE(TAG, "touch spi_mux already init");
    }

    ESP_LOGI(TAG, "Touch panel size width: %d, height: %d", g_dev.width, g_dev.height);
    ESP_LOGI(TAG, "Initial successful | GPIO INT:%d | GPIO CS:%d | dir:%d", config->pin_num_int, config->iface_spi.pin_num_cs, config->direction);

    return ESP_OK;
}

esp_err_t iot_xpt2046_deinit(bool free_bus)
{
    vSemaphoreDelete(g_dev.spi_mux);
    g_dev.spi_mux = NULL;
    spi_bus_remove_device(g_dev.spi_dev);

    if ((0 != free_bus) && (g_dev.is_init_spi_bus)) {
        spi_bus_free(g_dev.spi_host);
    }

    return ESP_OK;
}

int iot_xpt2046_is_pressed(void)
{
    if (g_dev.io_irq >= 0) {
        return (gpio_get_level(g_dev.io_irq) ? 0 : 1);
    }

    return 1;
}

esp_err_t iot_xpt2046_set_direction(touch_dir_t dir)
{
    g_dev.direction = dir;
    return ESP_OK;
}

esp_err_t iot_xpt2046_get_rawdata(uint16_t *x, uint16_t *y)
{
    position_t samples[XPT2046_SMP_SIZE];
    esp_err_t ret;
    uint32_t aveX = 0;
    uint32_t aveY = 0;

    for (int i = 0; i < XPT2046_SMP_SIZE; i++) {
        ret = xpt2046_get_sample(XPT2046_TOUCH_CMD_X, &(samples[i].x));
        TOUCH_CHECK(ret == ESP_OK, "X sample failed", ESP_FAIL);
        ret = xpt2046_get_sample(XPT2046_TOUCH_CMD_Y, &(samples[i].y));
        TOUCH_CHECK(ret == ESP_OK, "Y sample failed", ESP_FAIL);

        aveX += samples[i].x;
        aveY += samples[i].y;
    }

    aveX /= XPT2046_SMP_SIZE;
    aveY /= XPT2046_SMP_SIZE;

    *x = aveX;
    *y = aveY;

    return ESP_OK;
}

static void xpt2046_apply_rotate(uint16_t *x, uint16_t *y)
{
    uint16_t _x = *x;
    uint16_t _y = *y;

    switch (g_dev.direction) {
    case TOUCH_DIR_LRTB:
        *x = _x;
        *y = _y;
        break;
    case TOUCH_DIR_LRBT:
        *x = _x;
        *y = g_dev.height - _y;
        break;
    case TOUCH_DIR_RLTB:
        *x = g_dev.width - _x;
        *y = _y;
        break;
    case TOUCH_DIR_RLBT:
        *x = g_dev.width - _x;
        *y = g_dev.height - _y;
        break;
    case TOUCH_DIR_TBLR:
        *x = _y;
        *y = _x;
        break;
    case TOUCH_DIR_BTLR:
        *x = _y;
        *y = g_dev.width - _x;
        break;
    case TOUCH_DIR_TBRL:
        *x = g_dev.height - _y;
        *y = _x;
        break;
    case TOUCH_DIR_BTRL:
        *x = g_dev.height - _y;
        *y = g_dev.width - _x;
        break;

    default:
        break;
    }
}

esp_err_t iot_xpt2046_sample(touch_info_t *info)
{
    esp_err_t ret;
    uint16_t x, y;
    ret = iot_xpt2046_get_rawdata(&x, &y);
    TOUCH_CHECK(ret == ESP_OK, "Get raw data failed", ESP_FAIL);

    info->curx[0] = 0;
    info->cury[0] = 0;
    info->event = TOUCH_EVT_RELEASE;
    info->point_num = 0;

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

    xpt2046_apply_rotate(&x, &y);

    info->curx[0] = x;
    info->cury[0] = y;
    info->event = iot_xpt2046_is_pressed();
    info->point_num = 1;
    return ESP_OK;
}

esp_err_t iot_xpt2046_calibration_run(const scr_driver_fun_t *screen, bool recalibrate)
{
    return touch_calibration_run(screen, iot_xpt2046_is_pressed, iot_xpt2046_get_rawdata, recalibrate);
}

