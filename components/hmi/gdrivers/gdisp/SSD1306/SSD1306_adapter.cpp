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

/* I2C Includes */
#include "iot_i2c_bus.h"
#include "iot_ssd1306.h"
#include "ssd1306_fonts.h"
#include "driver/gpio.h"
#include "lcd_adapter.h"

/* ESP Includes */
#include "sdkconfig.h"
#include "esp_log.h"

#define POWER_CNTL_IO 19

class CLcdAdapter : public CSsd1306
{
public:
    const uint8_t *pFrameBuffer = NULL;
    CLcdAdapter(CI2CBus *p_i2c_bus) : CSsd1306(p_i2c_bus)
    {
        gpio_config_t conf;
        conf.pin_bit_mask = (1 << POWER_CNTL_IO);
        conf.mode = GPIO_MODE_OUTPUT;
        conf.pull_up_en = (gpio_pullup_t)0;
        conf.pull_down_en = (gpio_pulldown_t)0;
        conf.intr_type = (gpio_int_type_t)0;
        gpio_config(&conf);
        gpio_set_level((gpio_num_t)POWER_CNTL_IO, 0);
    }
    void inline writeCmd(uint8_t cmd)
    {
        iot_ssd1306_write_byte(get_dev_handle(), cmd, SSD1306_CMD);
    }
    void inline writeData(uint8_t data)
    {
        iot_ssd1306_write_byte(get_dev_handle(), data, SSD1306_DAT);
    }
    void inline writeData(uint8_t *data, uint16_t length)
    {
        for (uint16_t i = 0; i < length; i++) {
            iot_ssd1306_write_byte(get_dev_handle(), data[i], SSD1306_DAT);
        }
    }
};

static CLcdAdapter *lcd_obj = NULL;
static CI2CBus *i2c_bus = NULL;

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
            lcd_obj->draw_bitmap(0, 0, (const uint8_t *)lcd_obj->pFrameBuffer, flush_width, flush_height);
            iot_ssd1306_refresh_gram(lcd_obj->get_dev_handle());
            vTaskDelay(CONFIG_UGFX_DRIVER_AUTO_FLUSH_INTERVAL / portTICK_RATE_MS);
        }
    }
}

#endif

#ifdef CONFIG_UGFX_GUI_ENABLE

void board_lcd_init()
{
    /*Initialize LCD*/
    i2c_bus = new CI2CBus((i2c_port_t)CONFIG_UGFX_LCD_IIC_NUM, (gpio_num_t)CONFIG_UGFX_LCD_SCL_GPIO, (gpio_num_t)CONFIG_UGFX_LCD_SDA_GPIO);

    if (lcd_obj == NULL) {
        lcd_obj = new CLcdAdapter(i2c_bus);
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

void board_lcd_flush(int16_t x, int16_t y, const uint8_t *bitmap, int16_t w, int16_t h)
{
#if CONFIG_UGFX_DRIVER_AUTO_FLUSH_ENABLE
    flush_width = w;
    flush_height = h;
    lcd_obj->pFrameBuffer = bitmap;
    xSemaphoreGive(flush_sem);
#else
    lcd_obj->draw_bitmap(x, y, bitmap, w, h);
#endif
}

void board_lcd_write_cmd(uint8_t cmd)
{
    lcd_obj->writeCmd(cmd);
}

void board_lcd_write_data(uint8_t data)
{
    lcd_obj->writeData(data);
}

void board_lcd_write_datas(uint8_t *data, uint16_t length)
{
    lcd_obj->writeData(data, length);
}

void board_lcd_set_backlight(uint16_t data)
{
    /* Code here*/
}

#ifdef CONFIG_LVGL_GUI_ENABLE

/* lvgl include */
#include "lvgl_disp_config.h"

/*Write the internal buffer (VDB) to the display. 'lv_flush_ready()' has to be called when finished*/
void ex_disp_flush(int32_t x1, int32_t y1, int32_t x2, int32_t y2, const lv_color_t *color_p)
{
    lcd_obj->draw_bitmap((uint8_t)x1, (uint8_t)y1, (uint8_t *)color_p, (uint8_t)(x2 - x1 + 1), (uint8_t)(y2 - y1 + 1));
    /* IMPORTANT!!!
     * Inform the graphics library that you are ready with the flushing*/
    lv_flush_ready();
}

/*Fill an area with a color on the display*/
void ex_disp_fill(int32_t x1, int32_t y1, int32_t x2, int32_t y2, lv_color_t color)
{
    lcd_obj->fill_rectangle_screen((uint8_t)x1, (uint8_t)y1, (uint8_t)(x2), (uint8_t)(y2), (uint8_t)color.full);
}

/*Write pixel map (e.g. image) to the display*/
void ex_disp_map(int32_t x1, int32_t y1, int32_t x2, int32_t y2, const lv_color_t *color_p)
{
    lcd_obj->draw_bitmap((uint8_t)x1, (uint8_t)y1, (uint8_t *)color_p, (uint8_t)(x2 - x1 + 1), (uint8_t)(y2 - y1 + 1));
}

lv_disp_drv_t lvgl_lcd_display_init()
{
    /*Initialize LCD*/
    i2c_bus = new CI2CBus((i2c_port_t)CONFIG_LVGL_LCD_IIC_NUM, (gpio_num_t)CONFIG_LVGL_LCD_SCL_GPIO, (gpio_num_t)CONFIG_LVGL_LCD_SDA_GPIO);

    if (lcd_obj == NULL) {
        lcd_obj = new CLcdAdapter(i2c_bus);
    }

    lv_disp_drv_t disp_drv;      /*Descriptor of a display driver*/
    lv_disp_drv_init(&disp_drv); /*Basic initialization*/

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
