/*
 ILI9341 SPI Transmit Data Functions

 Based on ESP8266 library https://github.com/Sermus/ESP8266_Adafruit_ILI9341

 Copyright (c) 2015-2016 Andrey Filimonov.  All rights reserved.

 Additions for ESP32 Copyright (c) 2016-2017 Espressif Systems (Shanghai) PTE LTD
 
 Redistribution and use in source and binary forms, with or without
 modification, are permitted provided that the following conditions are met:
 
 - Redistributions of source code must retain the above copyright notice,
   this list of conditions and the following disclaimer.
 - Redistributions in binary form must reproduce the above copyright notice,
   this list of conditions and the following disclaimer in the documentation
   and/or other materials provided with the distribution.
 
 THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 POSSIBILITY OF SUCH DAMAGE

*/

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

uint8_t data_command; /*Pin config for setting GPIO in SPI callback*/
SemaphoreHandle_t spi_mux = NULL;
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

/*This function is called (in irq context!) just before a transmission starts.
It will set the D/C line to the value indicated in the user field */
void lcd_spi_pre_transfer_callback(spi_transaction_t *t)
{
    int dc = (int)t->user;
    gpio_set_level(data_command, dc);
}

esp_err_t _lcd_spi_send(spi_device_handle_t spi, spi_transaction_t* t)
{
    xSemaphoreTake(spi_mux, portMAX_DELAY);
    esp_err_t ret = spi_device_transmit(spi, t); //Transmit!
    xSemaphoreGive(spi_mux);
    return ret;
}

void lcd_cmd(spi_device_handle_t spi, const uint8_t cmd)
{
    esp_err_t ret;
    spi_transaction_t t = {
        .length = 8,                   //Command is 8 bits
        .tx_buffer = &cmd,             //The data is the cmd itself
        .user = (void *) 0,            //D/C needs to be set to 0
    };
    ret = _lcd_spi_send(spi, &t); //Transmit!
    assert(ret == ESP_OK);              //Should have had no issues.
}

void lcd_data(spi_device_handle_t spi, const uint8_t *data, int len)
{
    esp_err_t ret;
    if (len == 0) {
        return;    //no need to send anything
    }

    spi_transaction_t t = {
        .length = len * 8,              //Len is in bytes, transaction length is in bits.
        .tx_buffer = data,              //Data
        .user = (void *) 1,             //D/C needs to be set to 1
    };
    ret = _lcd_spi_send(spi, &t); //Transmit!
    assert(ret == ESP_OK);              //Should have had no issues.
}

void lcd_init(lcd_conf_t* lcd_conf, spi_device_handle_t *spi_dev)
{
    if(spi_mux == NULL) {
        spi_mux = xSemaphoreCreateMutex();
    }
    data_command = lcd_conf->pin_num_dc;
    //Initialize SPI Bus for LCD
    spi_bus_config_t buscfg = {
        .miso_io_num = lcd_conf->pin_num_miso,
        .mosi_io_num = lcd_conf->pin_num_mosi,
        .sclk_io_num = lcd_conf->pin_num_clk,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
    };
    spi_bus_initialize(HSPI_HOST, &buscfg, 1);

    spi_device_interface_config_t devcfg = {
        .clock_speed_hz = lcd_conf->clk_freq,               //Clock out at 20 MHz
        .mode = 0,                                //SPI mode 0
        .spics_io_num = lcd_conf->pin_num_cs,     //CS pin
        .queue_size = 7,                          //We want to be able to queue 7 transactions at a time
        .pre_cb = lcd_spi_pre_transfer_callback,  //Specify pre-transfer callback to handle D/C line
    };
    spi_bus_add_device(HSPI_HOST, &devcfg, spi_dev);
        
    int cmd = 0;
    //Initialize non-SPI GPIOs
    gpio_pad_select_gpio(lcd_conf->pin_num_dc);
    gpio_pad_select_gpio(lcd_conf->pin_num_rst);
    gpio_pad_select_gpio(lcd_conf->pin_num_bckl);
    gpio_set_direction(lcd_conf->pin_num_dc, GPIO_MODE_OUTPUT);
    gpio_set_direction(lcd_conf->pin_num_rst, GPIO_MODE_OUTPUT);
    gpio_set_direction(lcd_conf->pin_num_bckl, GPIO_MODE_OUTPUT);

    //Reset the display
    gpio_set_level(lcd_conf->pin_num_rst, 0);
    vTaskDelay(100 / portTICK_RATE_MS);
    gpio_set_level(lcd_conf->pin_num_rst, 1);
    vTaskDelay(100 / portTICK_RATE_MS);

    if(lcd_conf->lcd_model == ST7789)
    {
        //Send all the commands for ST7789 Init
        while (st7789_init_cmds[cmd].databytes != 0xff) {
            lcd_cmd(*spi_dev, st7789_init_cmds[cmd].cmd);
            lcd_data(*spi_dev, st7789_init_cmds[cmd].data, st7789_init_cmds[cmd].databytes & 0x1F);
            if (st7789_init_cmds[cmd].databytes & 0x80) {
                vTaskDelay(100 / portTICK_RATE_MS);
            }
            cmd++;
        }
    }
    else if(lcd_conf->lcd_model == ILI9341)
    {
        //Send all the commands for ILI9341 Init
        while (ili_init_cmds[cmd].databytes != 0xff) {
            lcd_cmd(*spi_dev, ili_init_cmds[cmd].cmd);
            lcd_data(*spi_dev, ili_init_cmds[cmd].data, ili_init_cmds[cmd].databytes & 0x1F);
            if (ili_init_cmds[cmd].databytes & 0x80) {
                vTaskDelay(100 / portTICK_RATE_MS);
            }
            cmd++;
        }
    }
    
    //Enable backlight
    gpio_set_level(lcd_conf->pin_num_bckl, 0);
}

void lcd_send_uint16_r(spi_device_handle_t spi, const uint16_t data, int32_t repeats)
{
    uint32_t i;
    uint32_t word = data << 16 | data;
    uint32_t word_tmp[16];
    spi_transaction_t t;

    while (repeats > 0) {
        uint16_t bytes_to_transfer = MIN(repeats*sizeof(uint16_t), SPIFIFOSIZE*sizeof(uint32_t));
        for (i = 0; i < (bytes_to_transfer + 3) / 4; i++) {
            word_tmp[i] = word;
        }

        memset(&t, 0, sizeof(t));           //Zero out the transaction
        t.length = bytes_to_transfer * 8;   //Len is in bytes, transaction length is in bits.
        t.tx_buffer = word_tmp;             //Data
        t.user = (void *) 1;                //D/C needs to be set to 1
        _lcd_spi_send(spi, &t);       //Transmit!
        repeats -= bytes_to_transfer / 2;
    }
}

void lcd_read_id(spi_device_handle_t spi, lcd_id_t *ili_id_num)
{    
    uint8_t read_cmd = 0x04;
    lcd_data(spi, &read_cmd, 1);        //Send Read Identity command
    
    spi_transaction_t t;
    memset(&t, 0, sizeof(t));           //Zero out the transaction
    t.flags |= SPI_TRANS_USE_RXDATA;
    t.rxlength = 4 * 8;           //read 4 bytes
    _lcd_spi_send(spi, &t);

    //byte 0 is dummy read
    ili_id_num->mfg_id = t.rx_data[1];
    ili_id_num->lcd_driver_id = t.rx_data[2];
    ili_id_num->lcd_id = t.rx_data[3];
}
