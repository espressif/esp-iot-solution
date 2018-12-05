// Copyright 2015-2016 Espressif Systems (Shanghai) PTE LTD
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
#include <sys/param.h>
#include "spi_lcd.h"
#include "driver/gpio.h"
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/xtensa_api.h"
#include "freertos/task.h"
#define SPIFIFOSIZE 16

/*
 This struct stores a bunch of command values to be initialized for ILI9341
*/
typedef struct {
    uint8_t cmd;
    uint8_t data[16];
    uint8_t databytes; //No of data in data; bit 7 = delay after set; 0xFF = end of cmds.
} lcd_init_cmd_t;


DRAM_ATTR static const lcd_init_cmd_t ili_init_cmds[]={
    {0xCF, {0x00, 0x83, 0x30}, 3},
    {0xED, {0x64, 0x03, 0x12, 0x81}, 4},
    {0xE8, {0x85, 0x01, 0x79}, 3},
    {0xCB, {0x39, 0x2C, 0x00, 0x34, 0x02}, 5},
    {0xF7, {0x20}, 1},
    {0xEA, {0x00, 0x00}, 2},
    {0xC0, {0x26}, 1},
    {0xC1, {0x11}, 1},
    {0xC5, {0x35, 0x3E}, 2},
    {0xC7, {0xBE}, 1},
    {0x36, {0x28}, 1},
    {0x3A, {0x55}, 1},
    {0xB1, {0x00, 0x1B}, 2},
    {0xF2, {0x08}, 1},
    {0x26, {0x01}, 1},
    {0xE0, {0x1F, 0x1A, 0x18, 0x0A, 0x0F, 0x06, 0x45, 0X87, 0x32, 0x0A, 0x07, 0x02, 0x07, 0x05, 0x00}, 15},
    {0XE1, {0x00, 0x25, 0x27, 0x05, 0x10, 0x09, 0x3A, 0x78, 0x4D, 0x05, 0x18, 0x0D, 0x38, 0x3A, 0x1F}, 15},
    {0x2A, {0x00, 0x00, 0x00, 0xEF}, 4},
    {0x2B, {0x00, 0x00, 0x01, 0x3f}, 4}, 
    {0x2C, {0}, 0},
    {0xB7, {0x07}, 1},
    {0xB6, {0x0A, 0x82, 0x27, 0x00}, 4},
    {0x11, {0}, 0x80},
    {0x29, {0}, 0x80},
    {0, {0}, 0xff},
};

DRAM_ATTR static const lcd_init_cmd_t st7789_init_cmds[] = {
    {0xC0, {0x00}, 1},           //LCMCTRL: LCM Control [2C] //sumpremely related to 0x36, MADCTL
    {0xC2, {0x01, 0xFF}, 2},     //VDVVRHEN: VDV and VRH Command Enable [01 FF]
    {0xC3, {0x13}, 1},           //VRHS: VRH Set VAP=???, VAN=-??? [0B]
    {0xC4, {0x20}, 1},           //VDVS: VDV Set [20]
    {0xC6, {0x0F}, 1},           //FRCTRL2: Frame Rate control in normal mode [0F]
    {0xCA, {0x0F}, 1},           //REGSEL2 [0F]
    {0xC8, {0x08}, 1},           //REGSEL1 [08]
    {0x55, {0xB0}, 1},           //WRCACE  [00]
    {0x36, {0x00}, 1},
    {0x3A, {0x55}, 1},             //this says 0x05
    {0xB1, {0x40, 0x02, 0x14}, 3}, //sync setting not reqd
    {0x26, {0x01}, 1}, 
    {0x2A, {0x00, 0x00, 0x00, 0xEF}, 4},
    {0x2B, {0x00, 0x00, 0x01, 0x3F}, 4},
    {0x2C, {0x00}, 1},
    {0xE0, {0xD0, 0x00, 0x05, 0x0E, 0x15, 0x0D, 0x37, 0x43, 0x47, 0x09, 0x15, 0x12, 0x16, 0x19}, 14},    //PVGAMCTRL: Positive Voltage Gamma control        
    {0xE1, {0xD0, 0x00, 0x05, 0x0D, 0x0C, 0x06, 0x2D, 0x44, 0x40, 0x0E, 0x1C, 0x18, 0x16, 0x19}, 14},    //NVGAMCTRL: Negative Voltage Gamma control
    {0x11, {0}, 0x80}, 
    {0x29, {0}, 0x80},
    {0, {0}, 0xff},
};

#define LCD_CMD_LEV   (0)
#define LCD_DATA_LEV  (1)

/*This function is called (in irq context!) just before a transmission starts.
It will set the D/C line to the value indicated in the user field */
void lcd_spi_pre_transfer_callback(spi_transaction_t *t)
{
    lcd_dc_t *dc = (lcd_dc_t *) t->user;
    gpio_set_level((int)dc->dc_io, (int)dc->dc_level);
}

static SemaphoreHandle_t _spi_mux = NULL;
static esp_err_t _lcd_spi_send(spi_device_handle_t spi, spi_transaction_t* t)
{
    xSemaphoreTake(_spi_mux, portMAX_DELAY);
    esp_err_t res = spi_device_transmit(spi, t); //Transmit!
    xSemaphoreGive(_spi_mux);
    return res;
}

void lcd_cmd(spi_device_handle_t spi, const uint8_t cmd, lcd_dc_t *dc)
{
    esp_err_t ret;
    dc->dc_level = LCD_CMD_LEV;
    spi_transaction_t t = {
        .length = 8,                    // Command is 8 bits
        .tx_buffer = &cmd,              // The data is the cmd itself
        .user = (void *) dc,            // D/C needs to be set to 0
    };
    ret = _lcd_spi_send(spi, &t);       // Transmit!
    assert(ret == ESP_OK);              // Should have had no issues.
}

void lcd_data(spi_device_handle_t spi, const uint8_t *data, int len, lcd_dc_t *dc)
{
    esp_err_t ret;
    if (len == 0) {
        return;    //no need to send anything
    }
    dc->dc_level = LCD_DATA_LEV;

    spi_transaction_t t = {
        .length = len * 8,              // Len is in bytes, transaction length is in bits.
        .tx_buffer = data,              // Data
        .user = (void *) dc,            // D/C needs to be set to 1
    };
    ret = _lcd_spi_send(spi, &t);       // Transmit!
    assert(ret == ESP_OK);              // Should have had no issues.
}

uint32_t lcd_init(lcd_conf_t* lcd_conf, spi_device_handle_t *spi_wr_dev, lcd_dc_t *dc, int dma_chan)
{

    if (_spi_mux == NULL) {
        _spi_mux = xSemaphoreCreateMutex();
    }
    //Initialize non-SPI GPIOs
    gpio_pad_select_gpio(lcd_conf->pin_num_dc);
    gpio_set_direction(lcd_conf->pin_num_dc, GPIO_MODE_OUTPUT);

    //Reset the display
    if (lcd_conf->pin_num_rst < GPIO_NUM_MAX) {
        gpio_pad_select_gpio(lcd_conf->pin_num_rst);
        gpio_set_direction(lcd_conf->pin_num_rst, GPIO_MODE_OUTPUT);
        gpio_set_level(lcd_conf->pin_num_rst, (lcd_conf->rst_active_level) & 0x1);
        vTaskDelay(100 / portTICK_RATE_MS);
        gpio_set_level(lcd_conf->pin_num_rst, (~(lcd_conf->rst_active_level)) & 0x1);
        vTaskDelay(100 / portTICK_RATE_MS);
    }

    if (lcd_conf->init_spi_bus) {
        //Initialize SPI Bus for LCD
        spi_bus_config_t buscfg = {
            .miso_io_num = lcd_conf->pin_num_miso,
            .mosi_io_num = lcd_conf->pin_num_mosi,
            .sclk_io_num = lcd_conf->pin_num_clk,
            .quadwp_io_num = -1,
            .quadhd_io_num = -1,
        };
        spi_bus_initialize(lcd_conf->spi_host, &buscfg, dma_chan);
    }

    spi_device_interface_config_t devcfg = {
        // Use low speed to read ID.
        .clock_speed_hz = 1 * 1000 * 1000,     //Clock out frequency
        .mode = 0,                                //SPI mode 0
        .spics_io_num = lcd_conf->pin_num_cs,     //CS pin
        .queue_size = 7,                          //We want to be able to queue 7 transactions at a time
        .pre_cb = lcd_spi_pre_transfer_callback,  //Specify pre-transfer callback to handle D/C line
    };
    spi_device_handle_t rd_id_handle;
    spi_bus_add_device(lcd_conf->spi_host, &devcfg, &rd_id_handle);
    uint32_t lcd_id = lcd_get_id(rd_id_handle, dc);
    spi_bus_remove_device(rd_id_handle);

    // Use high speed to write LCD
    devcfg.clock_speed_hz = lcd_conf->clk_freq;
    devcfg.flags = SPI_DEVICE_HALFDUPLEX;
    spi_bus_add_device(lcd_conf->spi_host, &devcfg, spi_wr_dev);

    int cmd = 0;
    const lcd_init_cmd_t* lcd_init_cmds = NULL;
    if(lcd_conf->lcd_model == LCD_MOD_ST7789) {
        lcd_init_cmds = st7789_init_cmds;
    } else if(lcd_conf->lcd_model == LCD_MOD_ILI9341) {
        lcd_init_cmds = ili_init_cmds;
    } else if(lcd_conf->lcd_model == LCD_MOD_AUTO_DET) {
        if (((lcd_id >> 8) & 0xff) == 0x42) {
            lcd_init_cmds = st7789_init_cmds;
        } else {
            lcd_init_cmds = ili_init_cmds;
        }
    }
    assert(lcd_init_cmds != NULL);
    //Send all the commands
    while (lcd_init_cmds[cmd].databytes!=0xff) {
        lcd_cmd(*spi_wr_dev, lcd_init_cmds[cmd].cmd, dc);
        lcd_data(*spi_wr_dev, lcd_init_cmds[cmd].data, lcd_init_cmds[cmd].databytes&0x1F, dc);
        if (lcd_init_cmds[cmd].databytes&0x80) {
            vTaskDelay(100 / portTICK_RATE_MS);
        }
        cmd++;
    }

    //Enable backlight
    if (lcd_conf->pin_num_bckl < GPIO_NUM_MAX) {
        gpio_pad_select_gpio(lcd_conf->pin_num_bckl);
        gpio_set_direction(lcd_conf->pin_num_bckl, GPIO_MODE_OUTPUT);
        gpio_set_level(lcd_conf->pin_num_bckl, (lcd_conf->bckl_active_level) & 0x1);
    }
    return lcd_id;
}

void lcd_send_uint16_r(spi_device_handle_t spi, const uint16_t data, int32_t repeats, lcd_dc_t *dc)
{
    uint32_t i;
    uint32_t word = data << 16 | data;
    uint32_t word_tmp[16];
    spi_transaction_t t;
    dc->dc_level = LCD_DATA_LEV;

    while (repeats > 0) {
        uint16_t bytes_to_transfer = MIN(repeats * sizeof(uint16_t), SPIFIFOSIZE * sizeof(uint32_t));
        for (i = 0; i < (bytes_to_transfer + 3) / 4; i++) {
            word_tmp[i] = word;
        }

        memset(&t, 0, sizeof(t));           //Zero out the transaction
        t.length = bytes_to_transfer * 8;   //Len is in bytes, transaction length is in bits.
        t.tx_buffer = word_tmp;             //Data
        t.user = (void *) dc;               //D/C needs to be set to 1
        _lcd_spi_send(spi, &t);             //Transmit!
        repeats -= bytes_to_transfer / 2;
    }
}

uint32_t lcd_get_id(spi_device_handle_t spi, lcd_dc_t *dc)
{
    //get_id cmd
    lcd_cmd( spi, 0x04, dc);

    spi_transaction_t t;
    dc->dc_level = LCD_DATA_LEV;
    memset(&t, 0, sizeof(t));
    t.length = 8 * 4;
    t.flags = SPI_TRANS_USE_RXDATA;
    t.user = (void *) dc;
    esp_err_t ret = _lcd_spi_send(spi, &t);
    assert( ret == ESP_OK );

    return *(uint32_t*) t.rx_data;
}

