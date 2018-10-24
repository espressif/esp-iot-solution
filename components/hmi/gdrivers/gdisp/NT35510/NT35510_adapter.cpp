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

/* C Includes */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* RTOS Includes */
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "freertos/semphr.h"

/* SPI Includes */
#include "NT35510.h"
#include "i2s_lcd_com.h"
#include "iot_nt35510.h"
#include "lcd_adapter.h"

/* ESP Includes */
#include "esp_log.h"

/* uGFX Config Include */
#include "sdkconfig.h"

static nt35510_handle_t nt35510_handle = NULL;
static uint16_t *pFrameBuffer = NULL;

#if CONFIG_UGFX_DRIVER_AUTO_FLUSH_ENABLE
SemaphoreHandle_t flush_sem = NULL;
static uint16_t flush_width;
static uint16_t flush_height;

void board_lcd_flush_task(void *arg)
{
    portBASE_TYPE res;
    while (1) {
        res = xSemaphoreTake(flush_sem, portMAX_DELAY);
        if (res == pdTRUE) {
            iot_nt35510_draw_bmp(nt35510_handle, (uint16_t *)pFrameBuffer, 0, 0, flush_width, flush_height);
            vTaskDelay(CONFIG_UGFX_DRIVER_AUTO_FLUSH_INTERVAL / portTICK_RATE_MS);
        }
    }
}
#endif

#ifdef CONFIG_UGFX_GUI_ENABLE

void board_lcd_init()
{
    /*Initialize LCD*/
    i2s_lcd_config_t i2s_lcd_pin_conf = {
#ifdef CONFIG_BIT_MODE_8BIT
        .data_width = 8,
        .data_io_num = {
            CONFIG_UGFX_LCD_D0_GPIO, CONFIG_UGFX_LCD_D1_GPIO, CONFIG_UGFX_LCD_D2_GPIO, CONFIG_UGFX_LCD_D3_GPIO,
            CONFIG_UGFX_LCD_D4_GPIO, CONFIG_UGFX_LCD_D5_GPIO, CONFIG_UGFX_LCD_D6_GPIO, CONFIG_UGFX_LCD_D7_GPIO,
        },
#else  // CONFIG_BIT_MODE_16BIT
        .data_width = 16,
        .data_io_num = {
            CONFIG_UGFX_LCD_D0_GPIO, CONFIG_UGFX_LCD_D1_GPIO, CONFIG_UGFX_LCD_D2_GPIO, CONFIG_UGFX_LCD_D3_GPIO,
            CONFIG_UGFX_LCD_D4_GPIO, CONFIG_UGFX_LCD_D5_GPIO, CONFIG_UGFX_LCD_D6_GPIO, CONFIG_UGFX_LCD_D7_GPIO,
            CONFIG_UGFX_LCD_D8_GPIO, CONFIG_UGFX_LCD_D9_GPIO, CONFIG_UGFX_LCD_D10_GPIO, CONFIG_UGFX_LCD_D11_GPIO,
            CONFIG_UGFX_LCD_D12_GPIO, CONFIG_UGFX_LCD_D13_GPIO, CONFIG_UGFX_LCD_D14_GPIO, CONFIG_UGFX_LCD_D15_GPIO,
        },
#endif // CONFIG_BIT_MODE_16BIT
        .ws_io_num = CONFIG_UGFX_LCD_WR_GPIO,
        .rs_io_num = CONFIG_UGFX_LCD_RS_GPIO,
    };

    if (nt35510_handle == NULL) {
        nt35510_handle = iot_nt35510_create(CONFIG_UGFX_DRIVER_SCREEN_WIDTH, CONFIG_UGFX_DRIVER_SCREEN_HEIGHT, (i2s_port_t)CONFIG_UGFX_LCD_I2S_NUM, &i2s_lcd_pin_conf);
    }

#if CONFIG_UGFX_DRIVER_AUTO_FLUSH_ENABLE
    // For framebuffer mode and flush
    if (flush_sem == NULL) {
        flush_sem = xSemaphoreCreateBinary();
    }
    xTaskCreate(board_lcd_flush_task, "flush_task", 1500, NULL, 5, NULL);
#endif
}

#endif

void board_lcd_flush(int16_t x, int16_t y, uint16_t *bitmap, int16_t w, int16_t h)
{
#if CONFIG_UGFX_DRIVER_AUTO_FLUSH_ENABLE
    flush_width = w;
    flush_height = h;
    pFrameBuffer = bitmap;
    xSemaphoreGive(flush_sem);
#else
    iot_nt35510_draw_bmp(nt35510_handle, bitmap, x, y, w, h);
#endif
}

void board_lcd_blit_area(int16_t x, int16_t y, uint16_t *bitmap, int16_t w, int16_t h)
{
    iot_nt35510_draw_bmp(nt35510_handle, bitmap, x, y, w, h);
}

void board_lcd_write_cmd(uint16_t cmd)
{
    iot_i2s_lcd_write_cmd(((nt35510_dev_t *)nt35510_handle)->i2s_lcd_handle, cmd);
}

void board_lcd_write_data(uint16_t data)
{
    iot_i2s_lcd_write_data(((nt35510_dev_t *)nt35510_handle)->i2s_lcd_handle, data);
}

void board_lcd_write_reg(uint16_t reg, uint16_t data)
{
    iot_i2s_lcd_write_reg(((nt35510_dev_t *)nt35510_handle)->i2s_lcd_handle, reg, data);
}

void board_lcd_write_data_byte(uint8_t data)
{
    iot_i2s_lcd_write_data(((nt35510_dev_t *)nt35510_handle)->i2s_lcd_handle, data);
}

void board_lcd_write_datas(uint16_t data, uint16_t x, uint16_t y)
{
    iot_nt35510_fill_area(nt35510_handle, data, x, y);
}

void board_lcd_set_backlight(uint16_t data)
{
    /* Code here*/
}

void board_lcd_set_orientation(uint16_t orientation)
{
    switch (orientation) {
    case 0:
        board_lcd_write_cmd(NT35510_MADCTL);
        board_lcd_write_data_byte(0x00 | 0x00);
        break;
    case 90:
        board_lcd_write_cmd(NT35510_MADCTL);
        board_lcd_write_data_byte(0xA0 | 0x00);
        break;
    case 180:
        board_lcd_write_cmd(NT35510_MADCTL);
        board_lcd_write_data_byte(0xC0 | 0x00);
        break;
    case 270:
        board_lcd_write_cmd(NT35510_MADCTL);
        board_lcd_write_data_byte(0x60 | 0x00);
        break;
    default:
        break;
    }
}

#ifdef CONFIG_LVGL_GUI_ENABLE

/* lvgl include */
#include "lvgl_disp_config.h"

/*Write the internal buffer (VDB) to the display. 'lv_flush_ready()' has to be called when finished*/
void ex_disp_flush(int32_t x1, int32_t y1, int32_t x2, int32_t y2, const lv_color_t *color_p)
{
    iot_nt35510_draw_bmp(nt35510_handle, (uint16_t *)color_p, (uint16_t)x1, (uint16_t)y1, (uint16_t)(x2 - x1 + 1), (uint16_t)(y2 - y1 + 1));
    /* IMPORTANT!!!
     * Inform the graphics library that you are ready with the flushing*/
    lv_flush_ready();
}

/*Fill an area with a color on the display*/
void ex_disp_fill(int32_t x1, int32_t y1, int32_t x2, int32_t y2, lv_color_t color)
{
    iot_nt35510_fill_rect(nt35510_handle, (uint16_t)color.full, (uint16_t)x1, (uint16_t)y1, (uint16_t)(x2 - x1 + 1), (uint16_t)(y2 - y1 + 1));
}

/*Write pixel map (e.g. image) to the display*/
void ex_disp_map(int32_t x1, int32_t y1, int32_t x2, int32_t y2, const lv_color_t *color_p)
{
    iot_nt35510_draw_bmp(nt35510_handle, (uint16_t *)color_p, (uint16_t)x1, (uint16_t)y1, (uint16_t)(x2 - x1 + 1), (uint16_t)(y2 - y1 + 1));
}

lv_disp_drv_t lvgl_lcd_display_init()
{
    /*Initialize LCD*/
    i2s_lcd_config_t i2s_lcd_pin_conf = {
#ifdef CONFIG_BIT_MODE_8BIT
        .data_width = 8,
        .data_io_num = {
            CONFIG_LVGL_LCD_D0_GPIO, CONFIG_LVGL_LCD_D1_GPIO, CONFIG_LVGL_LCD_D2_GPIO, CONFIG_LVGL_LCD_D3_GPIO,
            CONFIG_LVGL_LCD_D4_GPIO, CONFIG_LVGL_LCD_D5_GPIO, CONFIG_LVGL_LCD_D6_GPIO, CONFIG_LVGL_LCD_D7_GPIO,
        },
#else  // CONFIG_BIT_MODE_16BIT
        .data_width = 16,
        .data_io_num = {
            CONFIG_LVGL_LCD_D0_GPIO, CONFIG_LVGL_LCD_D1_GPIO, CONFIG_LVGL_LCD_D2_GPIO, CONFIG_LVGL_LCD_D3_GPIO,
            CONFIG_LVGL_LCD_D4_GPIO, CONFIG_LVGL_LCD_D5_GPIO, CONFIG_LVGL_LCD_D6_GPIO, CONFIG_LVGL_LCD_D7_GPIO,
            CONFIG_LVGL_LCD_D8_GPIO, CONFIG_LVGL_LCD_D9_GPIO, CONFIG_LVGL_LCD_D10_GPIO, CONFIG_LVGL_LCD_D11_GPIO,
            CONFIG_LVGL_LCD_D12_GPIO, CONFIG_LVGL_LCD_D13_GPIO, CONFIG_LVGL_LCD_D14_GPIO, CONFIG_LVGL_LCD_D15_GPIO,
        },
#endif // CONFIG_BIT_MODE_16BIT
        .ws_io_num = CONFIG_LVGL_LCD_WR_GPIO,
        .rs_io_num = CONFIG_LVGL_LCD_RS_GPIO,
    };

    if (nt35510_handle == NULL) {
        nt35510_handle = iot_nt35510_create(CONFIG_LVGL_DRIVER_SCREEN_WIDTH, CONFIG_LVGL_DRIVER_SCREEN_HEIGHT, (i2s_port_t)CONFIG_LVGL_LCD_I2S_NUM, &i2s_lcd_pin_conf);
    }

    lv_disp_drv_t disp_drv;      /*Descriptor of a display driver*/
    lv_disp_drv_init(&disp_drv); /*Basic initialization*/

#ifdef CONFIG_LVGL_DISP_ROTATE_0
    board_lcd_write_cmd(NT35510_MADCTL);
    board_lcd_write_data_byte(0x00 | 0x00);
#elif defined(CONFIG_LVGL_DISP_ROTATE_90)
    board_lcd_write_cmd(NT35510_MADCTL);
    board_lcd_write_data_byte(0xA0 | 0x00);
#elif defined(CONFIG_LVGL_DISP_ROTATE_180)
    board_lcd_write_cmd(NT35510_MADCTL);
    board_lcd_write_data_byte(0xC0 | 0x00);
#elif defined(CONFIG_LVGL_DISP_ROTATE_270)
    board_lcd_write_cmd(NT35510_MADCTL);
    board_lcd_write_data_byte(0x60 | 0x00);
#endif

    /* Set up the functions to access to your display */
    if (LV_VDB_SIZE != 0) {
        disp_drv.disp_flush = ex_disp_flush; /*Used in buffered mode (LV_VDB_SIZE != 0  in lv_conf.h)*/
    } else if (LV_VDB_SIZE == 0) {
        disp_drv.disp_fill = ex_disp_fill; /*Used in unbuffered mode (LV_VDB_SIZE == 0  in lv_conf.h)*/
        disp_drv.disp_map = ex_disp_map;   /*Used in unbuffered mode (LV_VDB_SIZE == 0  in lv_conf.h)*/
    }

    /* Finally register the driver */
    lv_disp_drv_register(&disp_drv);
    return disp_drv;
}

#endif /* CONFIG_LVGL_GUI_ENABLE */
