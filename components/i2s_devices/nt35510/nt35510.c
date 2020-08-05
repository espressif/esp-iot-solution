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
#include "iot_nt35510.h"
#include "asc8x16.h"
#include "sdkconfig.h"

void iot_nt35510_set_orientation(nt35510_handle_t nt35510_handle, lcd_orientation_t orientation)
{
    uint16_t swap = 0;
    nt35510_dev_t *device = (nt35510_dev_t *)nt35510_handle;
    i2s_lcd_handle_t i2s_lcd_handle = device->i2s_lcd_handle;
    switch (orientation) {
    case LCD_DISP_ROTATE_0:
        iot_i2s_lcd_write_reg(i2s_lcd_handle, NT35510_MADCTL, 0x00 | 0x00);
        device->xset_cmd = NT35510_CASET;
        device->yset_cmd = NT35510_RASET;
        break;
    case LCD_DISP_ROTATE_90:
        iot_i2s_lcd_write_reg(i2s_lcd_handle, NT35510_MADCTL, 0xA0 | 0x00);
        swap = device->x_size;
        device->x_size = device->y_size;
        device->y_size = swap;
        device->xset_cmd = NT35510_RASET;
        device->yset_cmd = NT35510_CASET;
        break;
    case LCD_DISP_ROTATE_180:
        iot_i2s_lcd_write_reg(i2s_lcd_handle, NT35510_MADCTL, 0xC0 | 0x00);
        device->xset_cmd = NT35510_CASET;
        device->yset_cmd = NT35510_RASET;
        break;
    case LCD_DISP_ROTATE_270:
        iot_i2s_lcd_write_reg(i2s_lcd_handle, NT35510_MADCTL, 0x60 | 0x00);
        swap = device->x_size;
        device->x_size = device->y_size;
        device->y_size = swap;
        device->xset_cmd = NT35510_RASET;
        device->yset_cmd = NT35510_CASET;
        break;
    default:
        iot_i2s_lcd_write_reg(i2s_lcd_handle, NT35510_MADCTL, 0x00 | 0x00);
        device->xset_cmd = NT35510_CASET;
        device->yset_cmd = NT35510_RASET;
        break;
    }
}

void iot_nt35510_set_box(nt35510_handle_t nt35510_handle, uint16_t x, uint16_t y, uint16_t x_size, uint16_t y_size)
{
    uint16_t x_end = x + (x_size - 1);
    uint16_t y_end = y + (y_size - 1);
    nt35510_dev_t *device = (nt35510_dev_t *)nt35510_handle;
    i2s_lcd_handle_t i2s_lcd_handle = device->i2s_lcd_handle;
    iot_i2s_lcd_write_reg(i2s_lcd_handle, device->xset_cmd, x >> 8);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, device->xset_cmd + 1, x & 0xff);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, device->xset_cmd + 2, x_end >> 8);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, device->xset_cmd + 3, x_end & 0xff);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, device->yset_cmd, y >> 8);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, device->yset_cmd + 1, y & 0xff);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, device->yset_cmd + 2, y_end >> 8);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, device->yset_cmd + 3, y_end & 0xff);
    iot_i2s_lcd_write_cmd(i2s_lcd_handle, NT35510_RAMWR);
}

void iot_nt35510_refresh(nt35510_handle_t nt35510_handle)
{
    nt35510_dev_t *device = (nt35510_dev_t *)nt35510_handle;
    i2s_lcd_handle_t i2s_lcd_handle = device->i2s_lcd_handle;
    iot_i2s_lcd_write(i2s_lcd_handle, device->lcd_buf, device->x_size * device->y_size * 2);
}

void iot_nt35510_fill_screen(nt35510_handle_t nt35510_handle, uint16_t color)
{
    nt35510_dev_t *device = (nt35510_dev_t *)nt35510_handle;
    i2s_lcd_handle_t i2s_lcd_handle = device->i2s_lcd_handle;
    uint16_t *p = device->lcd_buf;
    iot_nt35510_set_box(nt35510_handle, 0, 0, device->x_size, device->y_size);
    for (int i = 0; i < device->x_size; i++) {
        p[i] = color;
    }
    for (int i = 0; i < device->y_size; i++) {
        iot_i2s_lcd_write(i2s_lcd_handle, device->lcd_buf, device->x_size * device->pix);
    }
}

void iot_nt35510_fill_area(nt35510_handle_t nt35510_handle, uint16_t color, uint16_t x, uint16_t y)
{
    nt35510_dev_t *device = (nt35510_dev_t *)nt35510_handle;
    i2s_lcd_handle_t i2s_lcd_handle = device->i2s_lcd_handle;
    uint16_t *p = device->lcd_buf;
    for (int i = 0; i < x; i++) {
        p[i] = color;
    }
    for (int i = 0; i < y; i++) {
        iot_i2s_lcd_write(i2s_lcd_handle, device->lcd_buf, x * device->pix);
    }
}

void iot_nt35510_fill_rect(nt35510_handle_t nt35510_handle, uint16_t color, uint16_t x, uint16_t y, uint16_t x_size, uint16_t y_size)
{
    nt35510_dev_t *device = (nt35510_dev_t *)nt35510_handle;
    i2s_lcd_handle_t i2s_lcd_handle = device->i2s_lcd_handle;
    uint16_t *p = device->lcd_buf;
    iot_nt35510_set_box(nt35510_handle, x, y, x_size, y_size);
    for(int i = 0; i < x_size; i++) {
        p[i] = color;
    }
    for(int i = 0; i < y_size; i++) {
        iot_i2s_lcd_write(i2s_lcd_handle, device->lcd_buf, x_size * device->pix);
    }
}

void iot_nt35510_draw_bmp(nt35510_handle_t nt35510_handle, uint16_t *bmp, uint16_t x, uint16_t y, uint16_t x_size, uint16_t y_size)
{
    nt35510_dev_t *device = (nt35510_dev_t *)nt35510_handle;
    i2s_lcd_handle_t i2s_lcd_handle = device->i2s_lcd_handle;
    iot_nt35510_set_box(nt35510_handle, x, y, x_size, y_size);
    iot_i2s_lcd_write(i2s_lcd_handle, bmp, x_size * y_size * device->pix);
}

void iot_nt35510_put_char(nt35510_handle_t nt35510_handle, uint8_t *str, uint16_t x, uint16_t y, uint16_t x_size, uint16_t y_size, uint16_t wcolor, uint16_t bcolor)
{
    uint16_t *pbuf;
    uint8_t *pdata = str;
    nt35510_dev_t *device = (nt35510_dev_t *)nt35510_handle;
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
    iot_nt35510_refresh(nt35510_handle);
}

void iot_nt35510_asc8x16_to_men(nt35510_handle_t nt35510_handle, char str, uint16_t x, uint16_t y, uint16_t wcolor, uint16_t bcolor)
{
    uint16_t *pbuf;
    uint8_t *pdata = (uint8_t *)(get_font_asc8x16() + (str - ' ') * 16);
    nt35510_dev_t *device = (nt35510_dev_t *)nt35510_handle;
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

void iot_nt35510_put_asc8x16(nt35510_handle_t nt35510_handle, char str, uint16_t x, uint16_t y, uint16_t wcolor, uint16_t bcolor)
{
    iot_nt35510_asc8x16_to_men(nt35510_handle, str, x, y, wcolor, bcolor);
    iot_nt35510_refresh(nt35510_handle);
}

void iot_nt35510_put_string8x16(nt35510_handle_t nt35510_handle, char *str, uint16_t x, uint16_t y, uint16_t wcolor, uint16_t bcolor)
{
    uint32_t x_ofsset = 0;
    uint32_t y_offset = 0;
    nt35510_dev_t *device = (nt35510_dev_t *)nt35510_handle;
    while (*str != '\0') {
        iot_nt35510_asc8x16_to_men(nt35510_handle, *str, x + x_ofsset, y + y_offset, wcolor, bcolor);
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
    iot_nt35510_refresh(nt35510_handle);
}

void iot_nt35510_init(nt35510_handle_t nt35510_handle)
{
    nt35510_dev_t *device = (nt35510_dev_t *)nt35510_handle;
    i2s_lcd_handle_t i2s_lcd_handle = device->i2s_lcd_handle;
    iot_i2s_lcd_write_cmd(i2s_lcd_handle, 0x0100);
    vTaskDelay(10 / portTICK_RATE_MS);
    iot_i2s_lcd_write_cmd(i2s_lcd_handle, 0x1200);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xf000, 0x0055);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xf001, 0x00aa);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xf002, 0x0052);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xf003, 0x0008);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xf004, 0x0001);

    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xbc01, 0x0086);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xbc02, 0x006a);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xbd01, 0x0086);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xbd02, 0x006a);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xbe01, 0x0067);

    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xd100, 0x0000);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xd101, 0x005d);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xd102, 0x0000);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xd103, 0x006b);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xd104, 0x0000);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xd105, 0x0084);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xd106, 0x0000);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xd107, 0x009c);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xd108, 0x0000);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xd109, 0x00b1);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xd10a, 0x0000);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xd10b, 0x00d9);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xd10c, 0x0000);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xd10d, 0x00fd);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xd10e, 0x0001);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xd10f, 0x0038);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xd110, 0x0001);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xd111, 0x0068);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xd112, 0x0001);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xd113, 0x00b9);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xd114, 0x0001);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xd115, 0x00fb);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xd116, 0x0002);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xd117, 0x0063);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xd118, 0x0002);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xd119, 0x00b9);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xd11a, 0x0002);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xd11b, 0x00bb);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xd11c, 0x0003);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xd11d, 0x0003);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xd11e, 0x0003);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xd11f, 0x0046);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xd120, 0x0003);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xd121, 0x0069);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xd122, 0x0003);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xd123, 0x008f);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xd124, 0x0003);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xd125, 0x00a4);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xd126, 0x0003);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xd127, 0x00b9);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xd128, 0x0003);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xd129, 0x00c7);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xd12a, 0x0003);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xd12b, 0x00c9);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xd12c, 0x0003);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xd12d, 0x00cb);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xd12e, 0x0003);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xd12f, 0x00cb);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xd130, 0x0003);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xd131, 0x00cb);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xd132, 0x0003);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xd133, 0x00cc);

    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xd200, 0x0000);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xd201, 0x005d);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xd202, 0x0000);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xd203, 0x006b);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xd204, 0x0000);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xd205, 0x0084);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xd206, 0x0000);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xd207, 0x009c);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xd208, 0x0000);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xd209, 0x00b1);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xd20a, 0x0000);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xd20b, 0x00d9);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xd20c, 0x0000);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xd20d, 0x00fd);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xd20e, 0x0001);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xd20f, 0x0038);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xd210, 0x0001);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xd211, 0x0068);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xd212, 0x0001);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xd213, 0x00b9);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xd214, 0x0001);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xd215, 0x00fb);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xd216, 0x0002);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xd217, 0x0063);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xd218, 0x0002);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xd219, 0x00b9);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xd21a, 0x0002);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xd21b, 0x00bb);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xd21c, 0x0003);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xd21d, 0x0003);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xd21e, 0x0003);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xd21f, 0x0046);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xd220, 0x0003);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xd221, 0x0069);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xd222, 0x0003);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xd223, 0x008f);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xd224, 0x0003);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xd225, 0x00a4);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xd226, 0x0003);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xd227, 0x00b9);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xd228, 0x0003);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xd229, 0x00c7);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xd22a, 0x0003);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xd22b, 0x00c9);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xd22c, 0x0003);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xd22d, 0x00cb);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xd22e, 0x0003);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xd22f, 0x00cb);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xd230, 0x0003);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xd231, 0x00cb);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xd232, 0x0003);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xd233, 0x00cc);

    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xd300, 0x0000);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xd301, 0x005d);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xd302, 0x0000);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xd303, 0x006b);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xd304, 0x0000);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xd305, 0x0084);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xd306, 0x0000);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xd307, 0x009c);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xd308, 0x0000);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xd309, 0x00b1);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xd30a, 0x0000);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xd30b, 0x00d9);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xd30c, 0x0000);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xd30d, 0x00fd);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xd30e, 0x0001);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xd30f, 0x0038);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xd310, 0x0001);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xd311, 0x0068);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xd312, 0x0001);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xd313, 0x00b9);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xd314, 0x0001);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xd315, 0x00fb);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xd316, 0x0002);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xd317, 0x0063);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xd318, 0x0002);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xd319, 0x00b9);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xd31a, 0x0002);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xd31b, 0x00bb);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xd31c, 0x0003);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xd31d, 0x0003);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xd31e, 0x0003);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xd31f, 0x0046);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xd320, 0x0003);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xd321, 0x0069);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xd322, 0x0003);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xd323, 0x008f);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xd324, 0x0003);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xd325, 0x00a4);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xd326, 0x0003);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xd327, 0x00b9);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xd328, 0x0003);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xd329, 0x00c7);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xd32a, 0x0003);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xd32b, 0x00c9);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xd32c, 0x0003);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xd32d, 0x00cb);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xd32e, 0x0003);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xd32f, 0x00cb);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xd330, 0x0003);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xd331, 0x00cb);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xd332, 0x0003);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xd333, 0x00cc);

    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xd400, 0x0000);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xd401, 0x005d);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xd402, 0x0000);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xd403, 0x006b);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xd404, 0x0000);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xd405, 0x0084);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xd406, 0x0000);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xd407, 0x009c);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xd408, 0x0000);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xd409, 0x00b1);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xd40a, 0x0000);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xd40b, 0x00d9);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xd40c, 0x0000);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xd40d, 0x00fd);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xd40e, 0x0001);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xd40f, 0x0038);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xd410, 0x0001);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xd411, 0x0068);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xd412, 0x0001);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xd413, 0x00b9);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xd414, 0x0001);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xd415, 0x00fb);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xd416, 0x0002);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xd417, 0x0063);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xd418, 0x0002);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xd419, 0x00b9);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xd41a, 0x0002);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xd41b, 0x00bb);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xd41c, 0x0003);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xd41d, 0x0003);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xd41e, 0x0003);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xd41f, 0x0046);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xd420, 0x0003);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xd421, 0x0069);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xd422, 0x0003);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xd423, 0x008f);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xd424, 0x0003);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xd425, 0x00a4);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xd426, 0x0003);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xd427, 0x00b9);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xd428, 0x0003);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xd429, 0x00c7);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xd42a, 0x0003);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xd42b, 0x00c9);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xd42c, 0x0003);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xd42d, 0x00cb);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xd42e, 0x0003);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xd42f, 0x00cb);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xd430, 0x0003);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xd431, 0x00cb);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xd432, 0x0003);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xd433, 0x00cc);

    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xd500, 0x0000);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xd501, 0x005d);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xd502, 0x0000);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xd503, 0x006b);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xd504, 0x0000);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xd505, 0x0084);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xd506, 0x0000);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xd507, 0x009c);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xd508, 0x0000);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xd509, 0x00b1);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xd50a, 0x0000);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xd50b, 0x00D9);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xd50c, 0x0000);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xd50d, 0x00fd);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xd50e, 0x0001);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xd50f, 0x0038);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xd510, 0x0001);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xd511, 0x0068);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xd512, 0x0001);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xd513, 0x00b9);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xd514, 0x0001);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xd515, 0x00fb);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xd516, 0x0002);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xd517, 0x0063);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xd518, 0x0002);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xd519, 0x00b9);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xd51a, 0x0002);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xd51b, 0x00bb);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xd51c, 0x0003);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xd51d, 0x0003);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xd51e, 0x0003);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xd51f, 0x0046);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xd520, 0x0003);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xd521, 0x0069);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xd522, 0x0003);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xd523, 0x008f);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xd524, 0x0003);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xd525, 0x00a4);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xd526, 0x0003);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xd527, 0x00b9);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xd528, 0x0003);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xd529, 0x00c7);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xd52a, 0x0003);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xd52b, 0x00c9);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xd52c, 0x0003);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xd52d, 0x00cb);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xd52e, 0x0003);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xd52f, 0x00cb);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xd530, 0x0003);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xd531, 0x00cb);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xd532, 0x0003);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xd533, 0x00cc);

    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xd600, 0x0000);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xd601, 0x005d);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xd602, 0x0000);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xd603, 0x006b);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xd604, 0x0000);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xd605, 0x0084);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xd606, 0x0000);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xd607, 0x009c);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xd608, 0x0000);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xd609, 0x00b1);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xd60a, 0x0000);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xd60b, 0x00d9);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xd60c, 0x0000);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xd60d, 0x00fd);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xd60e, 0x0001);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xd60f, 0x0038);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xd610, 0x0001);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xd611, 0x0068);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xd612, 0x0001);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xd613, 0x00b9);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xd614, 0x0001);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xd615, 0x00fb);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xd616, 0x0002);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xd617, 0x0063);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xd618, 0x0002);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xd619, 0x00b9);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xd61a, 0x0002);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xd61b, 0x00bb);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xd61c, 0x0003);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xd61d, 0x0003);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xd61e, 0x0003);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xd61f, 0x0046);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xd620, 0x0003);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xd621, 0x0069);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xd622, 0x0003);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xd623, 0x008f);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xd624, 0x0003);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xd625, 0x00a4);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xd626, 0x0003);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xd627, 0x00b9);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xd628, 0x0003);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xd629, 0x00c7);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xd62a, 0x0003);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xd62b, 0x00c9);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xd62c, 0x0003);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xd62d, 0x00cb);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xd62e, 0x0003);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xd62f, 0x00cb);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xd630, 0x0003);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xd631, 0x00cb);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xd632, 0x0003);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xd633, 0x00cc);

    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xba00, 0x0024);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xba01, 0x0024);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xba02, 0x0024);

    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xb900, 0x0024);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xb901, 0x0024);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xb902, 0x0024);

    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xf000, 0x0055);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xf001, 0x00aa);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xf002, 0x0052);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xf003, 0x0008);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xf004, 0x0000);

    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xb100, 0x00cc);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xB500, 0x0050);

    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xbc00, 0x0005);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xbc01, 0x0005);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xbc02, 0x0005);

    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xb800, 0x0001);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xb801, 0x0003);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xb802, 0x0003);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xb803, 0x0003);

    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xbd02, 0x0007);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xbd03, 0x0031);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xbe02, 0x0007);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xbe03, 0x0031);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xbf02, 0x0007);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xbf03, 0x0031);

    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xff00, 0x00aa);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xff01, 0x0055);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xff02, 0x0025);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xff03, 0x0001);

    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xf304, 0x0011);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xf306, 0x0010);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0xf308, 0x0000);

    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0x3500, 0x0000);
    iot_i2s_lcd_write_reg(i2s_lcd_handle, 0x3A00, 0x0005);
    //Display On
    iot_i2s_lcd_write_cmd(i2s_lcd_handle, 0x2900);
    // Out sleep
    iot_i2s_lcd_write_cmd(i2s_lcd_handle, 0x1100);
    // Write continue
    iot_i2s_lcd_write_cmd(i2s_lcd_handle, 0x2C00);
}

nt35510_handle_t iot_nt35510_create(uint16_t x_size, uint16_t y_size, i2s_port_t i2s_port, i2s_lcd_config_t *pin_conf)
{
    nt35510_dev_t *device = NULL;
    device = (nt35510_dev_t *)malloc(sizeof(nt35510_dev_t));
    if (device == NULL) {
        LCD_LOG("LCD dev malloc fail\n");
        goto error;
    }
    memset(device, 0, sizeof(nt35510_dev_t));
    device->x_size = x_size;
    device->y_size = y_size;
    device->xset_cmd = NT35510_CASET;
    device->yset_cmd = NT35510_RASET;
    device->pix = sizeof(uint16_t);
    uint16_t *p = malloc(sizeof(uint16_t) * y_size);
    if (p == NULL) {
        LCD_LOG("malloc fail\n");
        goto error;
    }
    device->lcd_buf = p;
    device->i2s_lcd_handle = iot_i2s_lcd_pin_cfg(i2s_port, pin_conf);
    iot_nt35510_init((nt35510_handle_t)device);
    return (nt35510_handle_t)device;

error:
    if (device) {
        free(device);
    }
    return NULL;
}