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
#include "stdio.h"
#include "string.h"
#include "freertos/FreeRTOS.h"
#include "esp_system.h"
#include "i2s_lcd_com.h"
#include "iot_ili9806.h"
#include "asc8x16.h"
#include "sdkconfig.h"

void iot_ili9806_set_orientation(ili9806_handle_t ili9806_handle, lcd_orientation_t orientation)
{
    uint16_t swap = 0;
    ili9806_dev_t *device = (ili9806_dev_t *)ili9806_handle;
    i2s_lcd_handle_t i2s_lcd_handle = device->i2s_lcd_handle;
    switch (orientation) {
    case LCD_DISP_ROTATE_0:
        iot_i2s_lcd_write_reg(i2s_lcd_handle, ILI9806_MADCTL, 0x00 | 0x00);
        device->xset_cmd = ILI9806_CASET;
        device->yset_cmd = ILI9806_RASET;
        break;
    case LCD_DISP_ROTATE_90:
        iot_i2s_lcd_write_reg(i2s_lcd_handle, ILI9806_MADCTL, 0xA0 | 0x00);
        swap = device->x_size;
        device->x_size = device->y_size;
        device->y_size = swap;
        device->xset_cmd = ILI9806_RASET;
        device->yset_cmd = ILI9806_CASET;
        break;
    case LCD_DISP_ROTATE_180:
        iot_i2s_lcd_write_reg(i2s_lcd_handle, ILI9806_MADCTL, 0xC0 | 0x00);
        device->xset_cmd = ILI9806_CASET;
        device->yset_cmd = ILI9806_RASET;
        break;
    case LCD_DISP_ROTATE_270:
        iot_i2s_lcd_write_reg(i2s_lcd_handle, ILI9806_MADCTL, 0x60 | 0x00);
        swap = device->x_size;
        device->x_size = device->y_size;
        device->y_size = swap;
        device->xset_cmd = ILI9806_RASET;
        device->yset_cmd = ILI9806_CASET;
        break;
    default:
        iot_i2s_lcd_write_reg(i2s_lcd_handle, ILI9806_MADCTL, 0x00 | 0x00);
        device->xset_cmd = ILI9806_CASET;
        device->yset_cmd = ILI9806_RASET;
        break;
    }
}

void iot_ili9806_set_box(ili9806_handle_t ili9806_handle, uint16_t x, uint16_t y, uint16_t x_size, uint16_t y_size)
{
    uint16_t x_end = x + (x_size - 1);
    uint16_t y_end = y + (y_size - 1);
    ili9806_dev_t *device = (ili9806_dev_t *)ili9806_handle;
    i2s_lcd_handle_t i2s_lcd_handle = device->i2s_lcd_handle;
    iot_i2s_lcd_write_cmd(i2s_lcd_handle, ILI9806_CASET);
    iot_i2s_lcd_write_data(i2s_lcd_handle, x >> 8);
    iot_i2s_lcd_write_data(i2s_lcd_handle, x & 0xff);
    iot_i2s_lcd_write_data(i2s_lcd_handle, x_end >> 8);
    iot_i2s_lcd_write_data(i2s_lcd_handle, x_end & 0xff);
    iot_i2s_lcd_write_cmd(i2s_lcd_handle, ILI9806_RASET);
    iot_i2s_lcd_write_data(i2s_lcd_handle, y >> 8);
    iot_i2s_lcd_write_data(i2s_lcd_handle, y & 0xff);
    iot_i2s_lcd_write_data(i2s_lcd_handle, y_end >> 8);
    iot_i2s_lcd_write_data(i2s_lcd_handle, y_end & 0xff);
    iot_i2s_lcd_write_cmd(i2s_lcd_handle, ILI9806_RAMWR);
}

void iot_ili9806_refresh(ili9806_handle_t ili9806_handle)
{
    ili9806_dev_t *device = (ili9806_dev_t *)ili9806_handle;
    i2s_lcd_handle_t i2s_lcd_handle = device->i2s_lcd_handle;
    iot_i2s_lcd_write(i2s_lcd_handle, device->lcd_buf, device->x_size * device->y_size * device->pix);
}

void iot_ili9806_fill_screen(ili9806_handle_t ili9806_handle, uint16_t color)
{
    ili9806_dev_t *device = (ili9806_dev_t *)ili9806_handle;
    i2s_lcd_handle_t i2s_lcd_handle = device->i2s_lcd_handle;
    uint16_t *p = device->lcd_buf;
    iot_ili9806_set_box(ili9806_handle, 0, 0, device->x_size, device->y_size);
    for (int i = 0; i < device->x_size * device->y_size; i++) {
        p[i] = color;
    }
    iot_i2s_lcd_write(i2s_lcd_handle, device->lcd_buf, device->x_size * device->y_size * device->pix);
}

void iot_ili9806_draw_bmp(ili9806_handle_t ili9806_handle, uint16_t *bmp, uint16_t x, uint16_t y, uint16_t x_size, uint16_t y_size)
{
    ili9806_dev_t *device = (ili9806_dev_t *)ili9806_handle;
    i2s_lcd_handle_t i2s_lcd_handle = device->i2s_lcd_handle;
    iot_ili9806_set_box(ili9806_handle, x, y, x_size, y_size);
    iot_i2s_lcd_write(i2s_lcd_handle, bmp, x_size * y_size * device->pix);
}

void iot_ili9806_put_char(ili9806_handle_t ili9806_handle, uint8_t *str, uint16_t x, uint16_t y, uint16_t x_size, uint16_t y_size, uint16_t wcolor, uint16_t bcolor)
{
    uint16_t *pbuf;
    uint8_t *pdata = str;
    ili9806_dev_t *device = (ili9806_dev_t *)ili9806_handle;
    for (int i = 0; i < y_size; i++) {
        pbuf = device->lcd_buf + (x + (i + y) * device->x_size);
        for (int j = 0; j < x_size / 8; j++) {
            for (int k = 0; k < 8; k++) {
                if (*pdata & (0x80 >> k)) {
                    *pbuf = wcolor;
                } else {
                    *pbuf = bcolor;
                }
                pbuf++;
            }
            pdata++;
        }
    }
    iot_ili9806_refresh(ili9806_handle);
}

void inline iot_ili9806_asc8x16_to_men(ili9806_handle_t ili9806_handle, char str, uint16_t x, uint16_t y, uint16_t wcolor, uint16_t bcolor)
{
    uint16_t *pbuf;
    uint8_t *pdata = (uint8_t*)font_asc8x16 + (str - ' ') * 16;
    ili9806_dev_t *device = (ili9806_dev_t *)ili9806_handle;
    for (int i = 0; i < 16; i++) {
        pbuf = device->lcd_buf + (x + (i + y) * device->x_size);
        for (int k = 0; k < 8; k++) {
            if (*pdata & (0x80 >> k)) {
                *pbuf = wcolor;
            } else {
                *pbuf = bcolor;
            }
            pbuf++;
        }
        pdata++;
    }
}

void iot_ili9806_put_asc8x16(ili9806_handle_t ili9806_handle, char str, uint16_t x, uint16_t y, uint16_t wcolor, uint16_t bcolor)
{
    iot_ili9806_asc8x16_to_men(ili9806_handle, str, x, y, wcolor, bcolor);
    iot_ili9806_refresh(ili9806_handle);
}

void iot_ili9806_put_string8x16(ili9806_handle_t ili9806_handle, char *str, uint16_t x, uint16_t y, uint16_t wcolor, uint16_t bcolor)
{
    uint32_t x_ofsset = 0;
    uint32_t y_offset = 0;
    ili9806_dev_t *device = (ili9806_dev_t *)ili9806_handle;
    while (*str != '\0') {
        iot_ili9806_asc8x16_to_men(ili9806_handle, *str, x + x_ofsset, y + y_offset, wcolor, bcolor);
        x_ofsset = x_ofsset + 8;
        if (x_ofsset > device->x_size - 8) {
            y_offset += 16;
            x_ofsset = 0;
            if (y_offset > device->y_size - 16) {
                break;
            }
        }
        str++;
    }
    iot_ili9806_refresh(ili9806_handle);
}

void iot_ili9806_init(ili9806_handle_t ili9806_handle)
{
    ili9806_dev_t *device = (ili9806_dev_t *)ili9806_handle;
    i2s_lcd_handle_t i2s_lcd_handle = device->i2s_lcd_handle;
    iot_i2s_lcd_write_cmd(i2s_lcd_handle, 0x01);
    vTaskDelay(10 / portTICK_RATE_MS);
    iot_i2s_lcd_write_cmd(i2s_lcd_handle, 0xFF); // EXTC Command Set enable register
    iot_i2s_lcd_write_data(i2s_lcd_handle, 0xFF);
    iot_i2s_lcd_write_data(i2s_lcd_handle, 0x98);
    iot_i2s_lcd_write_data(i2s_lcd_handle, 0x06);

    iot_i2s_lcd_write_cmd(i2s_lcd_handle, 0xBA); // SPI Interface Setting
    iot_i2s_lcd_write_data(i2s_lcd_handle, 0xE0);

    iot_i2s_lcd_write_cmd(i2s_lcd_handle, 0xBC); // GIP 1
    iot_i2s_lcd_write_data(i2s_lcd_handle, 0x03);
    iot_i2s_lcd_write_data(i2s_lcd_handle, 0x0F);
    iot_i2s_lcd_write_data(i2s_lcd_handle, 0x63);
    iot_i2s_lcd_write_data(i2s_lcd_handle, 0x69);
    iot_i2s_lcd_write_data(i2s_lcd_handle, 0x01);
    iot_i2s_lcd_write_data(i2s_lcd_handle, 0x01);
    iot_i2s_lcd_write_data(i2s_lcd_handle, 0x1B);
    iot_i2s_lcd_write_data(i2s_lcd_handle, 0x11);
    iot_i2s_lcd_write_data(i2s_lcd_handle, 0x70);
    iot_i2s_lcd_write_data(i2s_lcd_handle, 0x73);
    iot_i2s_lcd_write_data(i2s_lcd_handle, 0xFF);
    iot_i2s_lcd_write_data(i2s_lcd_handle, 0xFF);
    iot_i2s_lcd_write_data(i2s_lcd_handle, 0x08);
    iot_i2s_lcd_write_data(i2s_lcd_handle, 0x09);
    iot_i2s_lcd_write_data(i2s_lcd_handle, 0x05);
    iot_i2s_lcd_write_data(i2s_lcd_handle, 0x00);
    iot_i2s_lcd_write_data(i2s_lcd_handle, 0xEE);
    iot_i2s_lcd_write_data(i2s_lcd_handle, 0xE2);
    iot_i2s_lcd_write_data(i2s_lcd_handle, 0x01);
    iot_i2s_lcd_write_data(i2s_lcd_handle, 0x00);
    iot_i2s_lcd_write_data(i2s_lcd_handle, 0xC1);

    iot_i2s_lcd_write_cmd(i2s_lcd_handle, 0xBD); // GIP 2
    iot_i2s_lcd_write_data(i2s_lcd_handle, 0x01);
    iot_i2s_lcd_write_data(i2s_lcd_handle, 0x23);
    iot_i2s_lcd_write_data(i2s_lcd_handle, 0x45);
    iot_i2s_lcd_write_data(i2s_lcd_handle, 0x67);
    iot_i2s_lcd_write_data(i2s_lcd_handle, 0x01);
    iot_i2s_lcd_write_data(i2s_lcd_handle, 0x23);
    iot_i2s_lcd_write_data(i2s_lcd_handle, 0x45);
    iot_i2s_lcd_write_data(i2s_lcd_handle, 0x67);

    iot_i2s_lcd_write_cmd(i2s_lcd_handle, 0xBE); // GIP 3
    iot_i2s_lcd_write_data(i2s_lcd_handle, 0x00);
    iot_i2s_lcd_write_data(i2s_lcd_handle, 0x22);
    iot_i2s_lcd_write_data(i2s_lcd_handle, 0x27);
    iot_i2s_lcd_write_data(i2s_lcd_handle, 0x6A);
    iot_i2s_lcd_write_data(i2s_lcd_handle, 0xBC);
    iot_i2s_lcd_write_data(i2s_lcd_handle, 0xD8);
    iot_i2s_lcd_write_data(i2s_lcd_handle, 0x92);
    iot_i2s_lcd_write_data(i2s_lcd_handle, 0x22);
    iot_i2s_lcd_write_data(i2s_lcd_handle, 0x22);

    iot_i2s_lcd_write_cmd(i2s_lcd_handle, 0xC7); // Vcom
    iot_i2s_lcd_write_data(i2s_lcd_handle, 0x1E);

    iot_i2s_lcd_write_cmd(i2s_lcd_handle, 0xED); // EN_volt_reg
    iot_i2s_lcd_write_data(i2s_lcd_handle, 0x7F);
    iot_i2s_lcd_write_data(i2s_lcd_handle, 0x0F);
    iot_i2s_lcd_write_data(i2s_lcd_handle, 0x00);

    iot_i2s_lcd_write_cmd(i2s_lcd_handle, 0xC0); // Power Control 1
    iot_i2s_lcd_write_data(i2s_lcd_handle, 0xE3);
    iot_i2s_lcd_write_data(i2s_lcd_handle, 0x0B);
    iot_i2s_lcd_write_data(i2s_lcd_handle, 0x00);

    iot_i2s_lcd_write_cmd(i2s_lcd_handle, 0xFC);
    iot_i2s_lcd_write_data(i2s_lcd_handle, 0x08);

    iot_i2s_lcd_write_cmd(i2s_lcd_handle, 0xDF); // Engineering Setting
    iot_i2s_lcd_write_data(i2s_lcd_handle, 0x00);
    iot_i2s_lcd_write_data(i2s_lcd_handle, 0x00);
    iot_i2s_lcd_write_data(i2s_lcd_handle, 0x00);
    iot_i2s_lcd_write_data(i2s_lcd_handle, 0x00);
    iot_i2s_lcd_write_data(i2s_lcd_handle, 0x00);
    iot_i2s_lcd_write_data(i2s_lcd_handle, 0x02);

    iot_i2s_lcd_write_cmd(i2s_lcd_handle, 0xF3); // DVDD Voltage Setting
    iot_i2s_lcd_write_data(i2s_lcd_handle, 0x74);

    iot_i2s_lcd_write_cmd(i2s_lcd_handle, 0xB4); // Display Inversion Control
    iot_i2s_lcd_write_data(i2s_lcd_handle, 0x00);
    iot_i2s_lcd_write_data(i2s_lcd_handle, 0x00);
    iot_i2s_lcd_write_data(i2s_lcd_handle, 0x00);

    iot_i2s_lcd_write_cmd(i2s_lcd_handle, 0xF7); // 480x854
    iot_i2s_lcd_write_data(i2s_lcd_handle, 0x81);

    iot_i2s_lcd_write_cmd(i2s_lcd_handle, 0xB1); // Frame Rate
    iot_i2s_lcd_write_data(i2s_lcd_handle, 0x00);
    iot_i2s_lcd_write_data(i2s_lcd_handle, 0x10);
    iot_i2s_lcd_write_data(i2s_lcd_handle, 0x14);

    iot_i2s_lcd_write_cmd(i2s_lcd_handle, 0xF1); // Panel Timing Control
    iot_i2s_lcd_write_data(i2s_lcd_handle, 0x29);
    iot_i2s_lcd_write_data(i2s_lcd_handle, 0x8A);
    iot_i2s_lcd_write_data(i2s_lcd_handle, 0x07);

    iot_i2s_lcd_write_cmd(i2s_lcd_handle, 0xF2); //Panel Timing Control
    iot_i2s_lcd_write_data(i2s_lcd_handle, 0x40);
    iot_i2s_lcd_write_data(i2s_lcd_handle, 0xD2);
    iot_i2s_lcd_write_data(i2s_lcd_handle, 0x50);
    iot_i2s_lcd_write_data(i2s_lcd_handle, 0x28);

    iot_i2s_lcd_write_cmd(i2s_lcd_handle, 0xC1); // Power Control 2
    iot_i2s_lcd_write_data(i2s_lcd_handle, 0x17);
    iot_i2s_lcd_write_data(i2s_lcd_handle, 0X85);
    iot_i2s_lcd_write_data(i2s_lcd_handle, 0x85);
    iot_i2s_lcd_write_data(i2s_lcd_handle, 0x20);

    iot_i2s_lcd_write_cmd(i2s_lcd_handle, 0xE0);
    iot_i2s_lcd_write_data(i2s_lcd_handle, 0x00); //P1
    iot_i2s_lcd_write_data(i2s_lcd_handle, 0x0C); //P2
    iot_i2s_lcd_write_data(i2s_lcd_handle, 0x15); //P3
    iot_i2s_lcd_write_data(i2s_lcd_handle, 0x0D); //P4
    iot_i2s_lcd_write_data(i2s_lcd_handle, 0x0F); //P5
    iot_i2s_lcd_write_data(i2s_lcd_handle, 0x0C); //P6
    iot_i2s_lcd_write_data(i2s_lcd_handle, 0x07); //P7
    iot_i2s_lcd_write_data(i2s_lcd_handle, 0x05); //P8
    iot_i2s_lcd_write_data(i2s_lcd_handle, 0x07); //P9
    iot_i2s_lcd_write_data(i2s_lcd_handle, 0x0B); //P10
    iot_i2s_lcd_write_data(i2s_lcd_handle, 0x10); //P11
    iot_i2s_lcd_write_data(i2s_lcd_handle, 0x10); //P12
    iot_i2s_lcd_write_data(i2s_lcd_handle, 0x0D); //P13
    iot_i2s_lcd_write_data(i2s_lcd_handle, 0x17); //P14
    iot_i2s_lcd_write_data(i2s_lcd_handle, 0x0F); //P15
    iot_i2s_lcd_write_data(i2s_lcd_handle, 0x00); //P16

    iot_i2s_lcd_write_cmd(i2s_lcd_handle, 0xE1);
    iot_i2s_lcd_write_data(i2s_lcd_handle, 0x00); //P1
    iot_i2s_lcd_write_data(i2s_lcd_handle, 0x0D); //P2
    iot_i2s_lcd_write_data(i2s_lcd_handle, 0x15); //P3
    iot_i2s_lcd_write_data(i2s_lcd_handle, 0x0E); //P4
    iot_i2s_lcd_write_data(i2s_lcd_handle, 0x10); //P5
    iot_i2s_lcd_write_data(i2s_lcd_handle, 0x0D); //P6
    iot_i2s_lcd_write_data(i2s_lcd_handle, 0x08); //P7
    iot_i2s_lcd_write_data(i2s_lcd_handle, 0x06); //P8
    iot_i2s_lcd_write_data(i2s_lcd_handle, 0x07); //P9
    iot_i2s_lcd_write_data(i2s_lcd_handle, 0x0C); //P10
    iot_i2s_lcd_write_data(i2s_lcd_handle, 0x11); //P11
    iot_i2s_lcd_write_data(i2s_lcd_handle, 0x11); //P12
    iot_i2s_lcd_write_data(i2s_lcd_handle, 0x0E); //P13
    iot_i2s_lcd_write_data(i2s_lcd_handle, 0x17); //P14
    iot_i2s_lcd_write_data(i2s_lcd_handle, 0x0F); //P15
    iot_i2s_lcd_write_data(i2s_lcd_handle, 0x00); //P16

    iot_i2s_lcd_write_cmd(i2s_lcd_handle, 0x35); //Tearing Effect ON
    iot_i2s_lcd_write_data(i2s_lcd_handle, 0x00);

    iot_i2s_lcd_write_cmd(i2s_lcd_handle, 0x36); //Tearing Effect ON
    iot_i2s_lcd_write_data(i2s_lcd_handle, 0x60);

    // iot_i2s_lcd_write_cmd(i2s_lcd_handle, 0x38);

    iot_i2s_lcd_write_cmd(i2s_lcd_handle, 0x3A);
    iot_i2s_lcd_write_data(i2s_lcd_handle, 0x55);

    iot_i2s_lcd_write_cmd(i2s_lcd_handle, 0x11); //Exit Sleep
    vTaskDelay(10 / portTICK_RATE_MS);
    iot_i2s_lcd_write_cmd(i2s_lcd_handle, 0x29); // Display On

    iot_i2s_lcd_write_cmd(i2s_lcd_handle, 0x36);
    iot_i2s_lcd_write_data(i2s_lcd_handle, (1 << 6) | (1 << 5));

    iot_i2s_lcd_write_cmd(i2s_lcd_handle, 0x2A);
    iot_i2s_lcd_write_data(i2s_lcd_handle, 0x00);
    iot_i2s_lcd_write_data(i2s_lcd_handle, 0x00);
    iot_i2s_lcd_write_data(i2s_lcd_handle, 0x03);
    iot_i2s_lcd_write_data(i2s_lcd_handle, 0x55);

    iot_i2s_lcd_write_cmd(i2s_lcd_handle, 0x2B);
    iot_i2s_lcd_write_data(i2s_lcd_handle, 0x00);
    iot_i2s_lcd_write_data(i2s_lcd_handle, 0x00);
    iot_i2s_lcd_write_data(i2s_lcd_handle, 0x01);
    iot_i2s_lcd_write_data(i2s_lcd_handle, 0xDF);
    iot_i2s_lcd_write_cmd(i2s_lcd_handle, 0x2C);
}

ili9806_handle_t iot_ili9806_create(uint16_t x_size, uint16_t y_size, i2s_port_t i2s_port, i2s_lcd_config_t *pin_conf)
{
    ili9806_dev_t *device = NULL;
    device = (ili9806_dev_t *)malloc(sizeof(ili9806_dev_t));
    if (device == NULL) {
        LCD_LOG("LCD dev malloc fail\n");
        goto error;
    }
    memset(device, 0, sizeof(ili9806_dev_t));
    device->x_size = x_size;
    device->y_size = y_size;
    device->pix = sizeof(uint16_t);
    uint16_t *p = malloc(sizeof(uint16_t) * y_size);
    if (p == NULL) {
        LCD_LOG("malloc fail\n");
        goto error;
    }
    device->lcd_buf = p;
    device->i2s_lcd_handle = iot_i2s_lcd_pin_cfg(i2s_port, pin_conf);
    iot_ili9806_init((ili9806_handle_t)device);
    iot_ili9806_set_orientation((ili9806_handle_t)device, LCD_DISP_ROTATE_270);
    return (ili9806_handle_t)device;
error:
    if (device) {
        free(device);
    }
    return NULL;
}