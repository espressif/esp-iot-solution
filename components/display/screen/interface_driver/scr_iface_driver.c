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

#include "sdkconfig.h"
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "scr_iface_driver.h"
#include "driver/gpio.h"

static const char *TAG = "scr_iface";

#define LCD_IFACE_CHECK(a, str, ret)  if(!(a)) {                           \
        ESP_LOGE(TAG,"%s:%d (%s):%s", __FILE__, __LINE__, __FUNCTION__, str);   \
        return (ret);                                                           \
    }

/**--------------------- I2S interface driver ----------------------*/
typedef struct {
    i2s_lcd_handle_t i2s_lcd_handle;
    scr_iface_driver_fun_t iface_drv;
} iface_i2s_handle_t;


static esp_err_t _i2s_lcd_write_data(void *handle, uint16_t data)
{
    iface_i2s_handle_t *iface_i2s = __containerof(handle, iface_i2s_handle_t, iface_drv);
    return i2s_lcd_write_data(iface_i2s->i2s_lcd_handle, data);
}

static esp_err_t _i2s_lcd_write_cmd(void *handle, uint16_t cmd)
{
    iface_i2s_handle_t *iface_i2s = __containerof(handle, iface_i2s_handle_t, iface_drv);
    return i2s_lcd_write_cmd(iface_i2s->i2s_lcd_handle, cmd);
}

static esp_err_t _i2s_lcd_write(void *handle, const uint8_t *data, uint32_t length)
{
    iface_i2s_handle_t *iface_i2s = __containerof(handle, iface_i2s_handle_t, iface_drv);
    return i2s_lcd_write(iface_i2s->i2s_lcd_handle, data, length);
}

static esp_err_t _i2s_lcd_read(void *handle, uint8_t *data, uint32_t length)
{
    return ESP_ERR_NOT_SUPPORTED;
}

static esp_err_t _i2s_lcd_acquire(void *handle)
{
    iface_i2s_handle_t *iface_i2s = __containerof(handle, iface_i2s_handle_t, iface_drv);
    return i2s_lcd_acquire(iface_i2s->i2s_lcd_handle);
}

static esp_err_t _i2s_lcd_release(void *handle)
{
    iface_i2s_handle_t *iface_i2s = __containerof(handle, iface_i2s_handle_t, iface_drv);
    return i2s_lcd_release(iface_i2s->i2s_lcd_handle);
}

/**--------------------- I2C interface driver ----------------------*/
#define SSD1306_WRITE_CMD           0x00
#define SSD1306_WRITE_DAT           0x40

#define ACK_CHECK_EN                1                   /*!< I2C master will check ack from slave*/
#define ACK_CHECK_DIS               0                   /*!< I2C master will not check ack from slave */

typedef struct {
    i2c_bus_handle_t i2c_bus;
    uint16_t dev_addr;
    scr_iface_driver_fun_t iface_drv;
} iface_i2c_handle_t;

static esp_err_t i2c_lcd_driver_init(const iface_i2c_config_t *cfg, iface_i2c_handle_t *out_iface_i2c)
{
    i2c_config_t conf;
    conf.mode = I2C_MODE_MASTER;
    conf.sda_io_num = cfg->sda_io_num;
    conf.sda_pullup_en = GPIO_PULLUP_ENABLE;
    conf.scl_io_num = cfg->scl_io_num;
    conf.scl_pullup_en = GPIO_PULLUP_ENABLE;
    conf.master.clk_speed = cfg->clk_speed;
    out_iface_i2c->i2c_bus = i2c_bus_create(cfg->i2c_num, &conf);
    LCD_IFACE_CHECK(NULL != out_iface_i2c->i2c_bus, "I2C bus initial failed", ESP_FAIL);
    out_iface_i2c->dev_addr = cfg->slave_addr;
    return ESP_OK;
}

static esp_err_t i2c_lcd_driver_deinit(iface_i2c_handle_t *iface_i2c)
{
    i2c_bus_delete(&iface_i2c->i2c_bus);
    return ESP_OK;
}

static esp_err_t i2c_lcd_write_byte(i2c_bus_handle_t bus, uint16_t dev_addr, uint8_t ctrl, uint8_t data)
{
    esp_err_t ret;

    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (dev_addr << 1) | I2C_MASTER_WRITE, ACK_CHECK_EN);
    i2c_master_write_byte(cmd, ctrl, ACK_CHECK_EN);
    i2c_master_write_byte(cmd, data, ACK_CHECK_EN);
    i2c_master_stop(cmd);
    ret = iot_i2c_bus_cmd_begin(bus, cmd, 1000 / portTICK_RATE_MS);
    i2c_cmd_link_delete(cmd);
    if (ESP_OK != ret) {
        ESP_LOGE(TAG, "i2c send failed [%s]", esp_err_to_name(ret));
        return ESP_FAIL;
    }

    return ESP_OK;
}

static esp_err_t i2c_lcd_write_cmd(void *handle, uint16_t cmd)
{
    iface_i2c_handle_t *iface_i2c = __containerof(handle, iface_i2c_handle_t, iface_drv);
    uint8_t v = cmd;
    return i2c_lcd_write_byte(iface_i2c->i2c_bus, iface_i2c->dev_addr, SSD1306_WRITE_CMD, v);
}

static esp_err_t i2c_lcd_write_data(void *handle, uint16_t data)
{
    iface_i2c_handle_t *iface_i2c = __containerof(handle, iface_i2c_handle_t, iface_drv);
    uint8_t v = data;
    return i2c_lcd_write_byte(iface_i2c->i2c_bus, iface_i2c->dev_addr, SSD1306_WRITE_DAT, v);
}

static esp_err_t i2c_lcd_write(void *handle, const uint8_t *data, uint32_t length)
{
    iface_i2c_handle_t *iface_i2c = __containerof(handle, iface_i2c_handle_t, iface_drv);
    esp_err_t ret;

    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (iface_i2c->dev_addr << 1) | I2C_MASTER_WRITE, ACK_CHECK_EN);
    i2c_master_write_byte(cmd, SSD1306_WRITE_DAT, ACK_CHECK_EN);
    i2c_master_write(cmd, (uint8_t *)data, length, ACK_CHECK_EN);
    i2c_master_stop(cmd);
    ret = iot_i2c_bus_cmd_begin(iface_i2c->i2c_bus, cmd, 1000 / portTICK_RATE_MS);
    LCD_IFACE_CHECK(ESP_OK == ret, "i2C send failed", ESP_FAIL);
    i2c_cmd_link_delete(cmd);
    return ESP_OK;
}

static esp_err_t i2c_lcd_read(void *handle, uint8_t *data, uint32_t length)
{
    ESP_LOGW(TAG, "lcd i2c unsupport read");
    return ESP_ERR_NOT_SUPPORTED;
}

static esp_err_t i2c_lcd_acquire(void *handle)
{
    return ESP_ERR_NOT_SUPPORTED;
}

static esp_err_t i2c_lcd_release(void *handle)
{
    return ESP_ERR_NOT_SUPPORTED;
}

/**--------------------- SPI interface driver ----------------------*/
#define LCD_CMD_LEV   (0)
#define LCD_DATA_LEV  (1)

typedef struct {
    spi_device_handle_t g_spi_wr_dev;
    int8_t g_pin_num_dc;
    uint8_t swap_data;
    spi_host_device_t g_spi_host;
    uint8_t init_spi_bus;
    scr_iface_driver_fun_t iface_drv;
} iface_spi_handle_t;

static esp_err_t spi_lcd_driver_init(const iface_spi_config_t *cfg, iface_spi_handle_t *out_iface_spi)
{
    LCD_IFACE_CHECK(GPIO_IS_VALID_GPIO(cfg->pin_num_miso), "gpio miso invalid", ESP_ERR_INVALID_ARG);
    LCD_IFACE_CHECK(GPIO_IS_VALID_OUTPUT_GPIO(cfg->pin_num_mosi), "gpio mosi invalid", ESP_ERR_INVALID_ARG);
    LCD_IFACE_CHECK(GPIO_IS_VALID_OUTPUT_GPIO(cfg->pin_num_clk), "gpio clk invalid", ESP_ERR_INVALID_ARG);
    LCD_IFACE_CHECK(GPIO_IS_VALID_OUTPUT_GPIO(cfg->pin_num_cs), "gpio cs invalid", ESP_ERR_INVALID_ARG);
    LCD_IFACE_CHECK(GPIO_IS_VALID_OUTPUT_GPIO(cfg->pin_num_dc), "gpio dc invalid", ESP_ERR_INVALID_ARG);

    esp_err_t ret;

    //Initialize non-SPI GPIOs
    gpio_pad_select_gpio(cfg->pin_num_dc);
    gpio_set_direction(cfg->pin_num_dc, GPIO_MODE_OUTPUT);
    out_iface_spi->g_pin_num_dc = cfg->pin_num_dc;
    out_iface_spi->g_spi_host = cfg->spi_host;
    out_iface_spi->init_spi_bus = cfg->init_spi_bus;
    out_iface_spi->swap_data = cfg->swap_data;

    if (cfg->init_spi_bus) {
        //Initialize SPI Bus for LCD
        spi_bus_config_t buscfg = {
            .miso_io_num = cfg->pin_num_miso,
            .mosi_io_num = cfg->pin_num_mosi,
            .sclk_io_num = cfg->pin_num_clk,
            .quadwp_io_num = -1,
            .quadhd_io_num = -1,
            .max_transfer_sz = 240 * 420 * 2,
        };
        ret = spi_bus_initialize(cfg->spi_host, &buscfg, cfg->dma_chan);
        LCD_IFACE_CHECK(ESP_OK == ret, "spi bus initialize failed", ESP_FAIL);
    }

    spi_device_interface_config_t devcfg = {
        .clock_speed_hz = cfg->clk_freq,     //Clock out frequency
        .mode = 0,                                //SPI mode 0
        .spics_io_num = cfg->pin_num_cs,     //CS pin
        .queue_size = 16,                    //We want to be able to queue 7 transactions at a time
        .flags = SPI_DEVICE_HALFDUPLEX,
    };
    ret = spi_bus_add_device(cfg->spi_host, &devcfg, &(out_iface_spi->g_spi_wr_dev));
    LCD_IFACE_CHECK(ESP_OK == ret, "spi bus initialize failed", ESP_FAIL);

    return ESP_OK;
}

static esp_err_t spi_lcd_driver_deinit(iface_spi_handle_t *iface_spi)
{
    spi_bus_remove_device(iface_spi->g_spi_wr_dev);

    // TODO: how to free the device more convenient
    if (iface_spi->init_spi_bus) {
        spi_bus_free(iface_spi->g_spi_host);
    }

    return ESP_OK;
}

static esp_err_t spi_lcd_driver_acquire(void *handle)
{
    iface_spi_handle_t *iface_spi = __containerof(handle, iface_spi_handle_t, iface_drv);
    return spi_device_acquire_bus(iface_spi->g_spi_wr_dev, portMAX_DELAY);
}

static esp_err_t spi_lcd_driver_release(void *handle)
{
    iface_spi_handle_t *iface_spi = __containerof(handle, iface_spi_handle_t, iface_drv);
    spi_device_release_bus(iface_spi->g_spi_wr_dev);
    return ESP_OK;
}

static esp_err_t _lcd_spi_rw(spi_device_handle_t spi, const uint8_t *input, uint8_t *output, uint32_t length)
{
    LCD_IFACE_CHECK(0 != length, "Length should not be 0", ESP_ERR_INVALID_ARG);
    spi_transaction_t t = {0};
    if (NULL != input) {
        t.length = 8 * length;
        t.tx_buffer = input;
    }
    if (NULL != output) {
        t.rxlength = 8 * length;
        t.rx_buffer = output;
    }

    return spi_device_transmit(spi, &t); //Transmit!
}

static esp_err_t spi_lcd_driver_write_cmd(void *handle, uint16_t value)
{
    iface_spi_handle_t *iface_spi = __containerof(handle, iface_spi_handle_t, iface_drv);
    esp_err_t ret;
    gpio_set_level(iface_spi->g_pin_num_dc, LCD_CMD_LEV);
    uint8_t data = value;
    ret = _lcd_spi_rw(iface_spi->g_spi_wr_dev, &data, NULL, 1);
    gpio_set_level(iface_spi->g_pin_num_dc, LCD_DATA_LEV);
    LCD_IFACE_CHECK(ESP_OK == ret, "Send cmd failed", ESP_FAIL);
    return ESP_OK;
}

static esp_err_t spi_lcd_driver_write_data(void *handle, uint16_t value)
{
    iface_spi_handle_t *iface_spi = __containerof(handle, iface_spi_handle_t, iface_drv);
    esp_err_t ret;
    uint8_t data = value;
    ret = _lcd_spi_rw(iface_spi->g_spi_wr_dev, &data, NULL, 1);
    LCD_IFACE_CHECK(ESP_OK == ret, "Send cmd failed", ESP_FAIL);
    return ESP_OK;
}

static esp_err_t spi_lcd_driver_read(void *handle, uint8_t *data, uint32_t length)
{
    iface_spi_handle_t *iface_spi = __containerof(handle, iface_spi_handle_t, iface_drv);
    esp_err_t ret;
    ret = _lcd_spi_rw(iface_spi->g_spi_wr_dev, NULL, data, length);
    LCD_IFACE_CHECK(ESP_OK == ret, "Read data failed", ESP_FAIL);
    return ESP_OK;
}

static esp_err_t spi_lcd_driver_write(void *handle, uint8_t *data, uint32_t length)
{
    iface_spi_handle_t *iface_spi = __containerof(handle, iface_spi_handle_t, iface_drv);
    esp_err_t ret;

    /**< Swap the high and low byte of the data */
    uint32_t l = length / 2;
    uint16_t t;
    if (iface_spi->swap_data) {
        uint16_t *p = (uint16_t *)data;
        for (size_t i = 0; i < l; i++) {
            t = *p;
            *p = t >> 8 | t << 8;
            p++;
        }
    }
    ret = _lcd_spi_rw(iface_spi->g_spi_wr_dev, data, NULL, length);

    /**
     * @brief swap data to restore the order of data
     *
     * TODO: how to avoid swap data here
     *
     */
    if (iface_spi->swap_data) {
        uint16_t *_p = (uint16_t *)data;
        for (size_t i = 0; i < l; i++) {
            t = *_p;
            *_p = t >> 8 | t << 8;
            _p++;
        }
    }
    LCD_IFACE_CHECK(ESP_OK == ret, "Write data failed", ESP_FAIL);
    return ESP_OK;
}

/*********************************************************/
esp_err_t scr_iface_create(scr_iface_type_t type, void *config, scr_iface_driver_fun_t **out_driver)
{
    switch (type) {
    case SCREEN_IFACE_8080: {
        iface_i2s_handle_t *iface_i2s = heap_caps_malloc(sizeof(iface_i2s_handle_t), MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
        LCD_IFACE_CHECK(NULL != iface_i2s, "memory of iface i2s is not enough", ESP_ERR_NO_MEM);
        iface_i2s->i2s_lcd_handle = i2s_lcd_driver_init((i2s_lcd_config_t *)config);
        if (NULL == iface_i2s->i2s_lcd_handle) {
            ESP_LOGE(TAG, "%s:%d (%s):%s", __FILE__, __LINE__, __FUNCTION__, "screen 8080 interface create failed");
            heap_caps_free(iface_i2s);
            return ESP_FAIL;
        }

        iface_i2s->iface_drv.type        = type;
        iface_i2s->iface_drv.write_cmd   = _i2s_lcd_write_cmd;
        iface_i2s->iface_drv.write_data  = _i2s_lcd_write_data;
        iface_i2s->iface_drv.write       = _i2s_lcd_write;
        iface_i2s->iface_drv.read        = _i2s_lcd_read;
        iface_i2s->iface_drv.bus_acquire = _i2s_lcd_acquire;
        iface_i2s->iface_drv.bus_release = _i2s_lcd_release;

        *out_driver = &iface_i2s->iface_drv;
    } break;
    case SCREEN_IFACE_SPI: {
        iface_spi_handle_t *iface_spi = heap_caps_malloc(sizeof(iface_spi_handle_t), MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
        LCD_IFACE_CHECK(NULL != iface_spi, "memory of iface spi is not enough", ESP_ERR_NO_MEM);
        esp_err_t ret = spi_lcd_driver_init((iface_spi_config_t *)config, iface_spi);
        if (ESP_OK != ret) {
            ESP_LOGE(TAG, "%s:%d (%s):%s", __FILE__, __LINE__, __FUNCTION__, "screen spi interface create failed");
            heap_caps_free(iface_spi);
            return ESP_FAIL;
        }

        iface_spi->iface_drv.type        = type;
        iface_spi->iface_drv.write_cmd   = spi_lcd_driver_write_cmd;
        iface_spi->iface_drv.write_data  = spi_lcd_driver_write_data;
        iface_spi->iface_drv.write       = spi_lcd_driver_write;
        iface_spi->iface_drv.read        = spi_lcd_driver_read;
        iface_spi->iface_drv.bus_acquire = spi_lcd_driver_acquire;
        iface_spi->iface_drv.bus_release = spi_lcd_driver_release;

        *out_driver = &iface_spi->iface_drv;

    } break;
    case SCREEN_IFACE_I2C: {
        iface_i2c_handle_t *iface_i2c = heap_caps_malloc(sizeof(iface_i2c_handle_t), MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
        LCD_IFACE_CHECK(NULL != iface_i2c, "memory of iface i2c is not enough", ESP_ERR_NO_MEM);
        esp_err_t ret = i2c_lcd_driver_init((iface_i2c_config_t *)config, iface_i2c);
        if (ESP_OK != ret) {
            ESP_LOGE(TAG, "%s:%d (%s):%s", __FILE__, __LINE__, __FUNCTION__, "screen i2c interface create failed");
            heap_caps_free(iface_i2c);
            return ESP_FAIL;
        }

        iface_i2c->iface_drv.type        = type;
        iface_i2c->iface_drv.write_cmd   = i2c_lcd_write_cmd;
        iface_i2c->iface_drv.write_data  = i2c_lcd_write_data;
        iface_i2c->iface_drv.write       = i2c_lcd_write;
        iface_i2c->iface_drv.read        = i2c_lcd_read;
        iface_i2c->iface_drv.bus_acquire = i2c_lcd_acquire;
        iface_i2c->iface_drv.bus_release = i2c_lcd_release;

        *out_driver = &iface_i2c->iface_drv;
    }

    break;
    default:
        break;
    }
    return ESP_OK;
}

esp_err_t scr_iface_delete(const scr_iface_driver_fun_t *driver)
{
    switch (driver->type) {
    case SCREEN_IFACE_8080: {
        iface_i2s_handle_t *iface_i2s = __containerof(driver, iface_i2s_handle_t, iface_drv);
        i2s_lcd_driver_deinit(iface_i2s->i2s_lcd_handle);
        heap_caps_free(iface_i2s);
    } break;
    case SCREEN_IFACE_SPI: {
        iface_spi_handle_t *iface_spi = __containerof(driver, iface_spi_handle_t, iface_drv);
        spi_lcd_driver_deinit(iface_spi);
        heap_caps_free(iface_spi);
    } break;
    case SCREEN_IFACE_I2C: {
        iface_i2c_handle_t *iface_i2c = __containerof(driver, iface_i2c_handle_t, iface_drv);
        i2c_lcd_driver_deinit(iface_i2c);
        heap_caps_free(iface_i2c);
    }
    break;
    default:
        break;
    }
    return ESP_OK;
}

