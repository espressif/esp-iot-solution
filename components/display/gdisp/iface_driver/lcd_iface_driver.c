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

#include "sdkconfig.h"
#include <string.h>
#include "lcd_iface_driver.h"
#include "i2s_lcd_driver.h"
#include "esp_log.h"

static const char *TAG = "lcd_iface";

#define LCD_IFACE_CHECK(a, str, ret)  if(!(a)) {                           \
        ESP_LOGE(TAG,"%s:%d (%s):%s", __FILE__, __LINE__, __FUNCTION__, str);   \
        return (ret);                                                           \
    }


/**--------------------- I2S interface driver ----------------------*/

i2s_lcd_handle_t g_i2s_handle = NULL;

esp_err_t _i2s_lcd_driver_init(const lcd_config_t *lcd_conf)
{
    LCD_IFACE_CHECK(LCD_IFACE_I2S == lcd_conf->iface_type, "lcd interface error", ESP_ERR_INVALID_ARG);

    i2s_lcd_config_t cfg = {0};
    cfg.data_width = lcd_conf->iface_8080.data_width;
    for (size_t i = 0; i < cfg.data_width; i++) {
        cfg.pin_data_num[i] = lcd_conf->iface_8080.pin_data_num[i];
    }
    cfg.pin_num_wr = lcd_conf->iface_8080.pin_num_wr;
    cfg.pin_num_rs = lcd_conf->iface_8080.pin_num_rs;
    cfg.clk_freq = lcd_conf->iface_8080.clk_freq;
    cfg.i2s_port = lcd_conf->iface_8080.i2s_port;
    cfg.buffer_size = 32 * 1024;
    g_i2s_handle = i2s_lcd_driver_init(&cfg);
    if (NULL == g_i2s_handle) {
        return ESP_FAIL;
    }

    return ESP_OK;
}

esp_err_t _i2s_lcd_driver_deinit(bool free_bus)
{
    return i2s_lcd_driver_deinit(g_i2s_handle);
}

esp_err_t _i2s_lcd_write_data(uint16_t data)
{
    return i2s_lcd_write_data(g_i2s_handle, data);
}

esp_err_t _i2s_lcd_write_cmd(uint16_t cmd)
{
    return i2s_lcd_write_cmd(g_i2s_handle, cmd);
}

esp_err_t _i2s_lcd_write(const uint8_t *data, uint32_t length)
{
    return i2s_lcd_write(g_i2s_handle, data, length);
}

esp_err_t _i2s_lcd_read(uint8_t *data, uint32_t length)
{
    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t _i2s_lcd_acquire(void)
{
    return i2s_lcd_acquire(g_i2s_handle);
}

esp_err_t _i2s_lcd_release(void)
{
    return i2s_lcd_release(g_i2s_handle);
}

/**
 * LCD interface driver abstract
 */
lcd_iface_driver_fun_t g_lcd_i2s_iface_default_driver = {
    .init = _i2s_lcd_driver_init,
    .deinit = _i2s_lcd_driver_deinit,
    .write_cmd = _i2s_lcd_write_cmd,
    .write_data = _i2s_lcd_write_data,
    .write = _i2s_lcd_write,
    .read = _i2s_lcd_read,
    .bus_acquire = _i2s_lcd_acquire,
    .bus_release = _i2s_lcd_release,
};


/**--------------------- I2C interface driver ----------------------*/

#include "i2c_bus.h"


#define SSD1306_WRITE_CMD           0x00
#define SSD1306_WRITE_DAT           0x40

#define ACK_CHECK_EN                1                   /*!< I2C master will check ack from slave*/
#define ACK_CHECK_DIS               0                   /*!< I2C master will not check ack from slave */


static i2c_bus_handle_t g_bus;
static uint16_t g_dev_addr;

esp_err_t i2c_lcd_driver_init(const lcd_config_t *lcd_conf)
{
    i2c_config_t conf;
    conf.mode = I2C_MODE_MASTER;
    conf.sda_io_num = lcd_conf->iface_i2c.pin_num_sda;
    conf.sda_pullup_en = GPIO_PULLUP_ENABLE;
    conf.scl_io_num = lcd_conf->iface_i2c.pin_num_scl;
    conf.scl_pullup_en = GPIO_PULLUP_ENABLE;
    conf.master.clk_speed = lcd_conf->iface_i2c.clk_freq;
    g_bus = i2c_bus_create(lcd_conf->iface_i2c.i2c_port, &conf);
    LCD_IFACE_CHECK(NULL != g_bus, "I2C bus initial failed", ESP_FAIL);
    g_dev_addr = lcd_conf->iface_i2c.i2c_addr;
    return ESP_OK;
}

esp_err_t i2c_lcd_driver_deinit(bool free_bus)
{
    i2c_bus_delete(&g_bus);
    g_bus = NULL;
    g_dev_addr = 0;
    return ESP_OK;
}

static esp_err_t i2c_lcd_write_byte(uint8_t ctrl, uint8_t data)
{
    esp_err_t ret;

    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (g_dev_addr << 1) | I2C_MASTER_WRITE, ACK_CHECK_EN);
    i2c_master_write_byte(cmd, ctrl, ACK_CHECK_EN);
    i2c_master_write_byte(cmd, data, ACK_CHECK_EN);
    i2c_master_stop(cmd);
    ret = iot_i2c_bus_cmd_begin(g_bus, cmd, 1000 / portTICK_RATE_MS);
    i2c_cmd_link_delete(cmd);
    LCD_IFACE_CHECK(ESP_OK == ret, "i2C send failed", ESP_FAIL);
    return ESP_OK;
}

esp_err_t i2c_lcd_write_cmd(uint16_t cmd)
{
    uint8_t v = cmd;
    return i2c_lcd_write_byte(SSD1306_WRITE_CMD, v);
}

esp_err_t i2c_lcd_write_data(uint16_t data)
{
    uint8_t v = data;
    return i2c_lcd_write_byte(SSD1306_WRITE_DAT, v);
}

esp_err_t i2c_lcd_write(const uint8_t *data, uint32_t length)
{
    esp_err_t ret;

    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (g_dev_addr << 1) | I2C_MASTER_WRITE, ACK_CHECK_EN);
    i2c_master_write_byte(cmd, SSD1306_WRITE_DAT, ACK_CHECK_EN);
    i2c_master_write(cmd, (uint8_t*)data, length, ACK_CHECK_EN);
    i2c_master_stop(cmd);
    ret = iot_i2c_bus_cmd_begin(g_bus, cmd, 1000 / portTICK_RATE_MS);
    LCD_IFACE_CHECK(ESP_OK == ret, "i2C send failed", ESP_FAIL);
    i2c_cmd_link_delete(cmd);
    return ESP_OK;
}

esp_err_t i2c_lcd_read(uint8_t *data, uint32_t length)
{
    ESP_LOGW(TAG, "lcd i2c unsupport read");
    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t i2c_lcd_acquire(void)
{
    ESP_LOGW(TAG, "lcd i2c unsupport acquire");
    return ESP_OK;
}

esp_err_t i2c_lcd_release(void)
{
    ESP_LOGW(TAG, "lcd i2c unsupport release");
    return ESP_OK;
}

/**
 * LCD interface driver abstract
 */
lcd_iface_driver_fun_t g_lcd_i2c_iface_default_driver = {
    .init = i2c_lcd_driver_init,
    .deinit = i2c_lcd_driver_deinit,
    .write_cmd = i2c_lcd_write_cmd,
    .write_data = i2c_lcd_write_data,
    .write = i2c_lcd_write,
    .read = i2c_lcd_read,
    .bus_acquire = i2c_lcd_acquire,
    .bus_release = i2c_lcd_release,
};


/**--------------------- SPI interface driver ----------------------*/
#define LCD_CMD_LEV   (0)
#define LCD_DATA_LEV  (1)

static spi_device_handle_t g_spi_wr_dev = NULL;
static int8_t g_pin_num_dc = -1;
static spi_host_device_t g_spi_host;


esp_err_t spi_lcd_driver_init(const lcd_config_t *lcd)
{
    LCD_IFACE_CHECK(GPIO_IS_VALID_GPIO(lcd->iface_spi.pin_num_miso), "gpio miso invalid", ESP_ERR_INVALID_ARG);
    LCD_IFACE_CHECK(GPIO_IS_VALID_OUTPUT_GPIO(lcd->iface_spi.pin_num_mosi), "gpio mosi invalid", ESP_ERR_INVALID_ARG);
    LCD_IFACE_CHECK(GPIO_IS_VALID_OUTPUT_GPIO(lcd->iface_spi.pin_num_clk), "gpio clk invalid", ESP_ERR_INVALID_ARG);
    LCD_IFACE_CHECK(GPIO_IS_VALID_OUTPUT_GPIO(lcd->iface_spi.pin_num_cs), "gpio cs invalid", ESP_ERR_INVALID_ARG);
    LCD_IFACE_CHECK(GPIO_IS_VALID_OUTPUT_GPIO(lcd->iface_spi.pin_num_dc), "gpio dc invalid", ESP_ERR_INVALID_ARG);

    esp_err_t ret;

    //Initialize non-SPI GPIOs
    gpio_pad_select_gpio(lcd->iface_spi.pin_num_dc);
    gpio_set_direction(lcd->iface_spi.pin_num_dc, GPIO_MODE_OUTPUT);
    g_pin_num_dc = lcd->iface_spi.pin_num_dc;
    g_spi_host = lcd->iface_spi.spi_host;

    if (lcd->iface_spi.init_spi_bus) {
        //Initialize SPI Bus for LCD
        spi_bus_config_t buscfg = {
            .miso_io_num = lcd->iface_spi.pin_num_miso,
            .mosi_io_num = lcd->iface_spi.pin_num_mosi,
            .sclk_io_num = lcd->iface_spi.pin_num_clk,
            .quadwp_io_num = -1,
            .quadhd_io_num = -1,
            .max_transfer_sz = lcd->width * lcd->height * 2,
        };
        ret = spi_bus_initialize(lcd->iface_spi.spi_host, &buscfg, lcd->iface_spi.dma_chan);
        LCD_IFACE_CHECK(ESP_OK == ret, "spi bus initialize failed", ESP_FAIL);
    }

    spi_device_interface_config_t devcfg = {
        .clock_speed_hz = lcd->iface_spi.clk_freq,     //Clock out frequency
        .mode = 0,                                //SPI mode 0
        .spics_io_num = lcd->iface_spi.pin_num_cs,     //CS pin
        .queue_size = 16,                          //We want to be able to queue 7 transactions at a time
        .flags = SPI_DEVICE_HALFDUPLEX,
    };
    ret = spi_bus_add_device(lcd->iface_spi.spi_host, &devcfg, &(g_spi_wr_dev));
    LCD_IFACE_CHECK(ESP_OK == ret, "spi bus initialize failed", ESP_FAIL);

    return ESP_OK;
}

esp_err_t spi_lcd_driver_deinit(bool free_bus)
{
    spi_bus_remove_device(g_spi_wr_dev);

    if (free_bus) {
        spi_bus_free(g_spi_host);
    }
    
    return ESP_OK;
}

esp_err_t spi_lcd_driver_acquire(void)
{
    return spi_device_acquire_bus(g_spi_wr_dev, portMAX_DELAY);
}

esp_err_t spi_lcd_driver_release(void)
{
    spi_device_release_bus(g_spi_wr_dev);
    return ESP_OK;
}

static esp_err_t _lcd_spi_rw(spi_device_handle_t spi, const uint8_t *input, uint8_t *output, uint32_t length)
{
    LCD_IFACE_CHECK(0 != length, "Length should not be 0", ESP_ERR_INVALID_ARG);
    spi_transaction_t t = {0};
    if(NULL != input){
        t.length = 8 * length;
        t.tx_buffer = input;
    }
    if(NULL != output){
        t.rxlength = 8 * length;
        t.rx_buffer = output;
    }
    
    return spi_device_transmit(spi, &t); //Transmit!
}

esp_err_t spi_lcd_driver_write_cmd(uint16_t value)
{
    esp_err_t ret;
    gpio_set_level(g_pin_num_dc, LCD_CMD_LEV);
    uint8_t data = value;
    ret = _lcd_spi_rw(g_spi_wr_dev, &data, NULL, 1);
    gpio_set_level(g_pin_num_dc, LCD_DATA_LEV);
    LCD_IFACE_CHECK(ESP_OK == ret, "Send cmd failed", ESP_FAIL);
    return ESP_OK;
}

esp_err_t spi_lcd_driver_write_data(uint16_t value)
{
    esp_err_t ret;
    uint8_t data = value;
    ret = _lcd_spi_rw(g_spi_wr_dev, &data, NULL, 1);
    LCD_IFACE_CHECK(ESP_OK == ret, "Send cmd failed", ESP_FAIL);
    return ESP_OK;
}

esp_err_t spi_lcd_driver_read(uint8_t *data, uint32_t length)
{
    esp_err_t ret;
    ret = _lcd_spi_rw(g_spi_wr_dev, NULL, data, length);
    LCD_IFACE_CHECK(ESP_OK == ret, "Read data failed", ESP_FAIL);
    return ESP_OK;
}

esp_err_t spi_lcd_driver_write(const uint8_t *data, uint32_t length)
{
    esp_err_t ret;
    ret = _lcd_spi_rw(g_spi_wr_dev, data, NULL, length);
    LCD_IFACE_CHECK(ESP_OK == ret, "Write data failed", ESP_FAIL);
    return ESP_OK;
}

/**
 * LCD interface driver abstract
 */
lcd_iface_driver_fun_t g_lcd_spi_iface_default_driver = {
    .init = spi_lcd_driver_init,
    .deinit = spi_lcd_driver_deinit,
    .write_cmd = spi_lcd_driver_write_cmd,
    .write_data = spi_lcd_driver_write_data,
    .write = spi_lcd_driver_write,
    .read = spi_lcd_driver_read,
    .bus_acquire = spi_lcd_driver_acquire,
    .bus_release = spi_lcd_driver_release,
};
