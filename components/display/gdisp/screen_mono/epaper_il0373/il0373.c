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
#include <stdio.h>

#include <string.h>
#include <stdlib.h>
#include <math.h>
#include "driver/gpio.h"
#include "driver/spi_master.h"

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/xtensa_api.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/ringbuf.h"

#include "esp_log.h"
#include "lcd_low_driver.h"
#include "il0373.h"

static const char *TAG = "IL0373";

#define LCD_EPAPER_CHECK(a, str, ret)  if(!(a)) {                               \
        ESP_LOGE(TAG,"%s:%d (%s):%s", __FILE__, __LINE__, __FUNCTION__, str);   \
        return (ret);                                                           \
    }

#define LCD_NAME "EPAPER IL0373"

#define EPAPER_CS_SETUP_NS      55
#define EPAPER_CS_HOLD_NS       60
#define EPAPER_1S_NS            1000000000
#define EPAPER_QUE_SIZE_DEFAULT 10

//epaper commands
#define E_PAPER_PANEL_SETTING                               0x00
#define E_PAPER_POWER_SETTING                               0x01
#define E_PAPER_POWER_OFF                                   0x02
#define E_PAPER_POWER_OFF_SEQUENCE_SETTING                  0x03
#define E_PAPER_POWER_ON                                    0x04
#define E_PAPER_POWER_ON_MEASURE                            0x05
#define E_PAPER_BOOSTER_SOFT_START                          0x06
#define E_PAPER_DEEP_SLEEP                                  0x07
#define E_PAPER_DATA_START_TRANSMISSION_1                   0x10
#define E_PAPER_DATA_STOP                                   0x11
#define E_PAPER_DISPLAY_REFRESH                             0x12
#define E_PAPER_DATA_START_TRANSMISSION_2                   0x13
#define E_PAPER_PARTIAL_DATA_START_TRANSMISSION_1           0x14
#define E_PAPER_PARTIAL_DATA_START_TRANSMISSION_2           0x15
#define E_PAPER_PARTIAL_DISPLAY_REFRESH                     0x16
#define E_PAPER_LUT_FOR_VCOM                                0x20
#define E_PAPER_LUT_WHITE_TO_WHITE                          0x21
#define E_PAPER_LUT_BLACK_TO_WHITE                          0x22
#define E_PAPER_LUT_WHITE_TO_BLACK                          0x23
#define E_PAPER_LUT_BLACK_TO_BLACK                          0x24
#define E_PAPER_PLL_CONTROL                                 0x30
#define E_PAPER_TEMPERATURE_SENSOR_COMMAND                  0x40
#define E_PAPER_TEMPERATURE_SENSOR_CALIBRATION              0x41
#define E_PAPER_TEMPERATURE_SENSOR_WRITE                    0x42
#define E_PAPER_TEMPERATURE_SENSOR_READ                     0x43
#define E_PAPER_VCOM_AND_DATA_INTERVAL_SETTING              0x50
#define E_PAPER_LOW_POWER_DETECTION                         0x51
#define E_PAPER_TCON_SETTING                                0x60
#define E_PAPER_TCON_RESOLUTION                             0x61
#define E_PAPER_SOURCE_AND_GATE_START_SETTING               0x62
#define E_PAPER_GET_STATUS                                  0x71
#define E_PAPER_AUTO_MEASURE_VCOM                           0x80
#define E_PAPER_VCOM_VALUE                                  0x81
#define E_PAPER_VCM_DC_SETTING_REGISTER                     0x82
#define E_PAPER_PROGRAM_MODE                                0xA0
#define E_PAPER_ACTIVE_PROGRAM                              0xA1
#define E_PAPER_READ_OTP_DATA                               0xA2


#define IL0373_HEIGHT                         104
#define IL0373_COLUMNS                        212

//////////////////////////////////////full screen update LUT////////////////////////////////////////////
const unsigned char lut_vcomDC[] ={
0x00	,0x08	,0x00	,0x00	,0x00	,0x02,	
0x60	,0x28	,0x28	,0x00	,0x00	,0x01,	
0x00	,0x14	,0x00	,0x00	,0x00	,0x01,	
0x00	,0x12	,0x12	,0x00	,0x00	,0x01,	
0x00	,0x00	,0x00	,0x00	,0x00	,0x00,	
0x00	,0x00	,0x00	,0x00	,0x00	,0x00,	
0x00	,0x00	,0x00	,0x00	,0x00	,0x00	
,0x00	,0x00,					};
const unsigned char lut_ww[] ={	
0x40	,0x08	,0x00	,0x00	,0x00	,0x02,	
0x90	,0x28	,0x28	,0x00	,0x00	,0x01,	
0x40	,0x14	,0x00	,0x00	,0x00	,0x01,	
0xA0	,0x12	,0x12	,0x00	,0x00	,0x01,	
0x00	,0x00	,0x00	,0x00	,0x00	,0x00,	
0x00	,0x00	,0x00	,0x00	,0x00	,0x00,	
0x00	,0x00	,0x00	,0x00	,0x00	,0x00,	};
const unsigned char lut_bw[] ={	
0x40	,0x17	,0x00	,0x00	,0x00	,0x02	,
0x90	,0x0F	,0x0F	,0x00	,0x00	,0x03	,
0x40	,0x0A	,0x01	,0x00	,0x00	,0x01	,
0xA0	,0x0E	,0x0E	,0x00	,0x00	,0x02	,
0x00	,0x00	,0x00	,0x00	,0x00	,0x00	,
0x00	,0x00	,0x00	,0x00	,0x00	,0x00	,
0x00	,0x00	,0x00	,0x00	,0x00	,0x00	,					};
const unsigned char lut_wb[] ={	
0x80	,0x08	,0x00	,0x00	,0x00	,0x02,	
0x90	,0x28	,0x28	,0x00	,0x00	,0x01,	
0x80	,0x14	,0x00	,0x00	,0x00	,0x01,	
0x50	,0x12	,0x12	,0x00	,0x00	,0x01,	
0x00	,0x00	,0x00	,0x00	,0x00	,0x00,	
0x00	,0x00	,0x00	,0x00	,0x00	,0x00,	
0x00	,0x00	,0x00	,0x00	,0x00	,0x00,	};
const unsigned char lut_bb[] ={	
0x80	,0x08	,0x00	,0x00	,0x00	,0x02,	
0x90	,0x28	,0x28	,0x00	,0x00	,0x01,	
0x80	,0x14	,0x00	,0x00	,0x00	,0x01,	
0x50	,0x12	,0x12	,0x00	,0x00	,0x01,	
0x00	,0x00	,0x00	,0x00	,0x00	,0x00,	
0x00	,0x00	,0x00	,0x00	,0x00	,0x00,	
0x00	,0x00	,0x00	,0x00	,0x00	,0x00,	};



/////////////////////////////////////partial screen update LUT///////////////////////////////////////////
const unsigned char lut_vcom1[] ={
0x00	,0x19	,0x01	,0x00	,0x00	,0x01,
0x00	,0x00	,0x00	,0x00	,0x00	,0x00,
0x00	,0x00	,0x00	,0x00	,0x00	,0x00,
0x00	,0x00	,0x00	,0x00	,0x00	,0x00,
0x00	,0x00	,0x00	,0x00	,0x00	,0x00,
0x00	,0x00	,0x00	,0x00	,0x00	,0x00,
0x00	,0x00	,0x00	,0x00	,0x00	,0x00
	,0x00	,0x00,					};
const unsigned char lut_ww1[] ={
0x00	,0x19	,0x01	,0x00	,0x00	,0x01,
0x00	,0x00	,0x00	,0x00	,0x00	,0x00,
0x00	,0x00	,0x00	,0x00	,0x00	,0x00,
0x00	,0x00	,0x00	,0x00	,0x00	,0x00,
0x00	,0x00	,0x00	,0x00	,0x00	,0x00,
0x00	,0x00	,0x00	,0x00	,0x00	,0x00,
0x00	,0x00	,0x00	,0x00	,0x00	,0x00,};
const unsigned char lut_bw1[] ={
0x80	,0x19	,0x01	,0x00	,0x00	,0x01,	
0x00	,0x00	,0x00	,0x00	,0x00	,0x00,	
0x00	,0x00	,0x00	,0x00	,0x00	,0x00,	
0x00	,0x00	,0x00	,0x00	,0x00	,0x00,	
0x00	,0x00	,0x00	,0x00	,0x00	,0x00,	
0x00	,0x00	,0x00	,0x00	,0x00	,0x00,	
0x00	,0x00	,0x00	,0x00	,0x00	,0x00,	};
const unsigned char lut_wb1[] ={
0x40	,0x19	,0x01	,0x00	,0x00	,0x01,	
0x00	,0x00	,0x00	,0x00	,0x00	,0x00,	
0x00	,0x00	,0x00	,0x00	,0x00	,0x00,	
0x00	,0x00	,0x00	,0x00	,0x00	,0x00,	
0x00	,0x00	,0x00	,0x00	,0x00	,0x00,	
0x00	,0x00	,0x00	,0x00	,0x00	,0x00,	
0x00	,0x00	,0x00	,0x00	,0x00	,0x00,	};
const unsigned char lut_bb1[] ={
0x00	,0x19	,0x01	,0x00	,0x00	,0x01,	
0x00	,0x00	,0x00	,0x00	,0x00	,0x00,	
0x00	,0x00	,0x00	,0x00	,0x00	,0x00,	
0x00	,0x00	,0x00	,0x00	,0x00	,0x00,	
0x00	,0x00	,0x00	,0x00	,0x00	,0x00,	
0x00	,0x00	,0x00	,0x00	,0x00	,0x00,	
0x00	,0x00	,0x00	,0x00	,0x00	,0x00,	};

const unsigned char lut_vcom11[] = {
0x00	,0x0A	,0x00	,0x00	,0x00	,0x01,
0x60	,0x14	,0x14	,0x00	,0x00	,0x01,
0x00	,0x14	,0x00	,0x00	,0x00	,0x01,
0x00	,0x13	,0x0A	,0x01	,0x00	,0x01,
0x00	,0x00	,0x00	,0x00	,0x00	,0x00,
0x00	,0x00	,0x00	,0x00	,0x00	,0x00,
0x00	,0x00	,0x00	,0x00	,0x00	,0x00				
};
//R21

const unsigned char lut_ww11[] = {
0x40	,0x0A	,0x00	,0x00	,0x00	,0x01,
0x90	,0x14	,0x14	,0x00	,0x00	,0x01,
0x10	,0x14	,0x0A	,0x00	,0x00	,0x01,
0xA0	,0x13	,0x01	,0x00	,0x00	,0x01,
0x00	,0x00	,0x00	,0x00	,0x00	,0x00,
0x00	,0x00	,0x00	,0x00	,0x00	,0x00,
0x00	,0x00	,0x00	,0x00	,0x00	,0x00,
};
//R22H	r

const unsigned char lut_bw11[] = {
0x40	,0x0A	,0x00	,0x00	,0x00	,0x01,
0x90	,0x14	,0x14	,0x00	,0x00	,0x01,
0x00	,0x14	,0x0A	,0x00	,0x00	,0x01,
0x99	,0x0C	,0x01	,0x03	,0x04	,0x01,
0x00	,0x00	,0x00	,0x00	,0x00	,0x00,
0x00	,0x00	,0x00	,0x00	,0x00	,0x00,
0x00	,0x00	,0x00	,0x00	,0x00	,0x00,
};
//R23H	w

const unsigned char lut_wb11[] = {
0x40	,0x0A	,0x00	,0x00	,0x00	,0x01,
0x90	,0x14	,0x14	,0x00	,0x00	,0x01,
0x00	,0x14	,0x0A	,0x00	,0x00	,0x01,
0x99	,0x0B	,0x04	,0x04	,0x01	,0x01,
0x00	,0x00	,0x00	,0x00	,0x00	,0x00,
0x00	,0x00	,0x00	,0x00	,0x00	,0x00,
0x00	,0x00	,0x00	,0x00	,0x00	,0x00,
};
//R24H	b

const unsigned char lut_bb11[] = {
0x80	,0x0A	,0x00	,0x00	,0x00	,0x01,
0x90	,0x14	,0x14	,0x00	,0x00	,0x01,
0x20	,0x14	,0x0A	,0x00	,0x00	,0x01,
0x50	,0x13	,0x01	,0x00	,0x00	,0x01,
0x00	,0x00	,0x00	,0x00	,0x00	,0x00,
0x00	,0x00	,0x00	,0x00	,0x00	,0x00,
0x00	,0x00	,0x00	,0x00	,0x00	,0x00,
};

const unsigned char lut_vcom1[] = {
0x00	,0x19	,0x01	,0x00	,0x00	,0x01,
0x00	,0x00	,0x00	,0x00	,0x00	,0x00,
0x00	,0x00	,0x00	,0x00	,0x00	,0x00,
0x00	,0x00	,0x00	,0x00	,0x00	,0x00,
0x00	,0x00	,0x00	,0x00	,0x00	,0x00,
0x00	,0x00	,0x00	,0x00	,0x00	,0x00,
0x00	,0x00	,0x00	,0x00	,0x00	,0x00,
0x00	,0x00,					
};

const unsigned char lut_ww1[] = {
0x00	,0x19	,0x01	,0x00	,0x00	,0x01,
0x00	,0x00	,0x00	,0x00	,0x00	,0x00,
0x00	,0x00	,0x00	,0x00	,0x00	,0x00,
0x00	,0x00	,0x00	,0x00	,0x00	,0x00,
0x00	,0x00	,0x00	,0x00	,0x00	,0x00,
0x00	,0x00	,0x00	,0x00	,0x00	,0x00,
0x00	,0x00	,0x00	,0x00	,0x00	,0x00,
};

const unsigned char lut_bw1[] = {
0x80	,0x19	,0x01	,0x00	,0x00	,0x01,	
0x00	,0x00	,0x00	,0x00	,0x00	,0x00,	
0x00	,0x00	,0x00	,0x00	,0x00	,0x00,	
0x00	,0x00	,0x00	,0x00	,0x00	,0x00,	
0x00	,0x00	,0x00	,0x00	,0x00	,0x00,	
0x00	,0x00	,0x00	,0x00	,0x00	,0x00,	
0x00	,0x00	,0x00	,0x00	,0x00	,0x00,	
};

const unsigned char lut_wb1[] = {
0x40	,0x19	,0x01	,0x00	,0x00	,0x01,	
0x00	,0x00	,0x00	,0x00	,0x00	,0x00,	
0x00	,0x00	,0x00	,0x00	,0x00	,0x00,	
0x00	,0x00	,0x00	,0x00	,0x00	,0x00,	
0x00	,0x00	,0x00	,0x00	,0x00	,0x00,	
0x00	,0x00	,0x00	,0x00	,0x00	,0x00,	
0x00	,0x00	,0x00	,0x00	,0x00	,0x00,	
};

const unsigned char lut_bb1[] = {
0x00	,0x19	,0x01	,0x00	,0x00	,0x01,	
0x00	,0x00	,0x00	,0x00	,0x00	,0x00,	
0x00	,0x00	,0x00	,0x00	,0x00	,0x00,	
0x00	,0x00	,0x00	,0x00	,0x00	,0x00,	
0x00	,0x00	,0x00	,0x00	,0x00	,0x00,	
0x00	,0x00	,0x00	,0x00	,0x00	,0x00,	
0x00	,0x00	,0x00	,0x00	,0x00	,0x00,	
};

typedef struct {
    uint16_t original_width;
    uint16_t original_height;
#if CONFIG_MONO_SCREEN_BUFFER
    uint8_t gram[SSD1306_PAGES][SSD1306_COLUMNS];
    uint16_t dirty_col_start;
    uint16_t dirty_col_end;
    uint16_t dirty_page_start;
    uint16_t dirty_page_end;
#endif
    uint16_t width;
    uint16_t height;
    uint8_t transmission;
    int8_t io_busy_pin;
} epaper_dev_t;

static epaper_dev_t g_lcd_handle;
static esp_err_t epaper_il0373_write_ram_data(uint16_t color);

lcd_driver_fun_t epaper_il0373_default_driver = {
    .init = epaper_il0373_init,
    .deinit = epaper_il0373_deinit,
    .set_direction = epaper_il0373_set_rotate,
    .set_window = epaper_il0373_set_window,
    .write_ram_data = epaper_il0373_write_ram_data,
    .draw_pixel = epaper_il0373_draw_pixel,
    .draw_bitmap = epaper_il0373_draw_bitmap,
    .get_info = epaper_il0373_get_info,
};

static void epaper_il0373_wait_idle(void)
{
    uint16_t timeout = 3000;
    while (gpio_get_level(g_lcd_handle.io_busy_pin) == 0) {      //0: busy, 1: idle
        vTaskDelay(10 / portTICK_RATE_MS);
        if (--timeout == 0) {
            ESP_LOGW(TAG, "Wait epaper idle timeout");
            break;
        }
    }
}

esp_err_t epaper_il0373_init(const lcd_config_t *lcd_conf)
{
    LCD_EPAPER_CHECK(lcd_conf->width <= IL0373_COLUMNS, "Width greater than maximum", ESP_ERR_INVALID_ARG);
    LCD_EPAPER_CHECK(lcd_conf->height <= IL0373_HEIGHT, "Height greater than maximum", ESP_ERR_INVALID_ARG);
    LCD_EPAPER_CHECK(GPIO_IS_VALID_GPIO(lcd_conf->pin_num_busy), "gpio busy invalid", ESP_ERR_INVALID_ARG);

    gpio_pad_select_gpio(lcd_conf->pin_num_busy);
    gpio_set_direction(lcd_conf->pin_num_busy, GPIO_MODE_INPUT);
    gpio_set_pull_mode(lcd_conf->pin_num_busy, GPIO_PULLDOWN_ONLY);
    g_lcd_handle.io_busy_pin = lcd_conf->pin_num_busy;

    esp_err_t ret;
    ret = LCD_IFACE_INIT(lcd_conf);
    LCD_EPAPER_CHECK(ESP_OK == ret, "i2c lcd driver initial failed", ESP_FAIL);

    // Reset the display
    if (lcd_conf->pin_num_rst >= 0) {
        gpio_pad_select_gpio(lcd_conf->pin_num_rst);
        gpio_set_direction(lcd_conf->pin_num_rst, GPIO_MODE_OUTPUT);
        gpio_set_level(lcd_conf->pin_num_rst, (~(lcd_conf->rst_active_level)) & 0x1);
        vTaskDelay(200 / portTICK_RATE_MS);
        gpio_set_level(lcd_conf->pin_num_rst, (lcd_conf->rst_active_level) & 0x1);
        vTaskDelay(200 / portTICK_RATE_MS);
        gpio_set_level(lcd_conf->pin_num_rst, (~(lcd_conf->rst_active_level)) & 0x1);
        vTaskDelay(200 / portTICK_RATE_MS);
    }
    g_lcd_handle.original_width = lcd_conf->width;
    g_lcd_handle.original_height = lcd_conf->height;
    g_lcd_handle.width = lcd_conf->width;
    g_lcd_handle.height = lcd_conf->height;
    
    LCD_WRITE_CMD(E_PAPER_BOOSTER_SOFT_START);
    LCD_WRITE_DATA(0x17);
    LCD_WRITE_DATA(0x17);
    LCD_WRITE_DATA(0x17);

    LCD_WRITE_CMD(E_PAPER_POWER_ON);
    epaper_il0373_wait_idle();

    LCD_WRITE_CMD(E_PAPER_PANEL_SETTING);
    LCD_WRITE_DATA(0x1f);        //lut from otp
	LCD_WRITE_DATA(0x0d);		//VCOM to 0V

    LCD_WRITE_CMD(E_PAPER_TCON_RESOLUTION);//resolution setting
    LCD_WRITE_DATA(0x68);        //104
	LCD_WRITE_DATA(0x00);		//212
	LCD_WRITE_DATA(0xd4);		

    LCD_WRITE_CMD(E_PAPER_VCM_DC_SETTING_REGISTER);
    LCD_WRITE_DATA(0x12);
    LCD_WRITE_CMD(E_PAPER_VCOM_AND_DATA_INTERVAL_SETTING);
    LCD_WRITE_DATA(0x97);        // define by OTP

    //epaper_il91874_set_lut();

    LCD_WRITE_CMD(E_PAPER_PARTIAL_DISPLAY_REFRESH);
    LCD_WRITE_DATA(0x00);
	ESP_LOGI(TAG,"init OK");
    return ESP_OK;
    /* EPD hardware init end */
}

esp_err_t epaper_il0373_deinit(void)
{
    ESP_LOGI(TAG,"deini ok");
	LCD_WRITE_CMD(E_PAPER_POWER_OFF);
    memset(&g_lcd_handle, 0, sizeof(epaper_dev_t));
    return LCD_IFACE_DEINIT(true);
}

esp_err_t epaper_il0373_set_rotate(lcd_dir_t dir)
{
    ESP_LOGW(TAG, "epaper unsupport set rotate");
    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t epaper_il0373_set_window(uint16_t x, uint16_t y, uint16_t w, uint16_t h)
{
    if (1 == g_lcd_handle.transmission) {
        LCD_WRITE_CMD(E_PAPER_PARTIAL_DATA_START_TRANSMISSION_1);
    } else if (2 == g_lcd_handle.transmission) {
        LCD_WRITE_CMD(E_PAPER_PARTIAL_DATA_START_TRANSMISSION_2);
    } else {
        ESP_LOGE(TAG, "Selected transmission invalid");
        return ESP_FAIL;
    }
    LCD_WRITE_DATA(x >> 8);
    LCD_WRITE_DATA(x & 0xf8);     // x should be the multiple of 8, the last 3 bit will always be ignored
    LCD_WRITE_DATA(y >> 8);
    LCD_WRITE_DATA(y & 0xff);
    LCD_WRITE_DATA(w >> 8);
    LCD_WRITE_DATA(w & 0xf8);     // w (width) should be the multiple of 8, the last 3 bit will always be ignored
    LCD_WRITE_DATA(h >> 8);
    LCD_WRITE_DATA(h & 0xff);
    return ESP_OK;
}

esp_err_t epaper_il0373_draw_bitmap(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t *bitmap)
{
    LCD_EPAPER_CHECK((x + w <= g_lcd_handle.width) && (y + h <= g_lcd_handle.height), "The set coordinates exceed the screen size", ESP_ERR_INVALID_ARG);
    esp_err_t ret = ESP_OK;
    uint8_t *p = (uint8_t *)bitmap;
    LCD_IFACE_ACQUIRE();

#if CONFIG_MONO_SCREEN_BUFFER
    uint16_t i, j;
    for (j = 0; j < h; j++) {
        for (i = 0; i < w; i++) {
            uint8_t bits = p[((j) / 8) * (w) + (i)];
            if (bits & (1 << (j % 8))) {
                ret |= epaper_il0373_draw_pixel(x + i, y + j, 1);
            } else {
                ret |= epaper_il0373_draw_pixel(x + i, y + j, 0);
            }
        }
    }
#else
    LCD_EPAPER_CHECK((0 == (x % 8)), "X should be a multiple of 8", ESP_ERR_INVALID_ARG);
    LCD_EPAPER_CHECK((0 == (w % 8)), "Width should be a multiple of 8", ESP_ERR_INVALID_ARG);
    uint8_t col1 = x >> 3;
    uint8_t col2 = (w + x) / 8; printf("[%d][%d]\n", col1, col2);
    LCD_WRITE_CMD(E_PAPER_PARTIAL_DATA_START_TRANSMISSION_2);
    // epaper_il91874_set_window(col1, y, col2 - 1, y + h - 1);
    for (size_t i = 0; i < w/8; i++)
    {
        for (size_t j = 0; j < h; j++)
        {
            LCD_WRITE_DATA(*p++);
        }
        
    }
    // LCD_WRITE_CMD(E_PAPER_DATA_STOP);
    
    // ret |= LCD_WRITE(p, w * (col2 - col1));
#endif
    LCD_IFACE_RELEASE();

    LCD_EPAPER_CHECK(ESP_OK == ret, "Draw bitmap failed", ESP_FAIL);
    return ESP_OK;
}

esp_err_t epaper_il0373_draw_pixel(uint16_t x, uint16_t y, uint16_t chPoint)
{
    LCD_EPAPER_CHECK((x < g_lcd_handle.width) && (y < g_lcd_handle.height), "The set coordinates exceed the screen size", ESP_ERR_INVALID_ARG);
#if CONFIG_MONO_SCREEN_BUFFER
    uint8_t chPos, chBx, chTemp = 0;

    chPos = y / 8; //
    chBx = y % 8;
    chTemp = 1 << (chBx);

    if (chPoint) {
        g_lcd_handle.gram[chPos][x] |= chTemp;
    } else {
        g_lcd_handle.gram[chPos][x] &= ~chTemp;
    }
#if DIRTY
    if (x <= g_lcd_handle.dirty_col_start) {
        g_lcd_handle.dirty_col_start = x;
    } else {
        g_lcd_handle.dirty_col_start = x - 1;
    }
    if (chPos <= g_lcd_handle.dirty_page_start) {
        g_lcd_handle.dirty_page_start = chPos;
    } else {
        g_lcd_handle.dirty_page_start = chPos - 1;
    }

    if (x > g_lcd_handle.dirty_col_end) {
        g_lcd_handle.dirty_col_end = x;
    }
    if (chPos > g_lcd_handle.dirty_page_end) {
        g_lcd_handle.dirty_page_end = chPos;
    }
#endif
    return ESP_OK;
#else
    ESP_LOGW(TAG, "il0373 not support draw pixel without buffer");
    return ESP_ERR_NOT_SUPPORTED;
#endif
}

esp_err_t epaper_il0373_get_info(lcd_info_t *info)
{
    info->width = g_lcd_handle.width;
    info->height = g_lcd_handle.height;
    info->name = LCD_NAME;
    info->color_type = LCD_COLOR_TYPE_MONO;
    return ESP_OK;
}

esp_err_t epaper_il0373_select_transmission(uint8_t transmission)
{
    g_lcd_handle.transmission = transmission;
    return ESP_OK;
}

esp_err_t epaper_il0373_init_4Gray()
{
    
    LCD_WRITE_CMD(E_PAPER_POWER_SETTING);   /*setting power*/
    LCD_WRITE_DATA(0x03);
    LCD_WRITE_DATA(0x00);
    LCD_WRITE_DATA(0x2b);

    LCD_WRITE_DATA(0x2b);
    LCD_WRITE_DATA(0x13);

    LCD_WRITE_CMD(E_PAPER_BOOSTER_SOFT_START);
    LCD_WRITE_DATA(0x17);
    LCD_WRITE_DATA(0x17);
    LCD_WRITE_DATA(0x17);

    LCD_WRITE_CMD(E_PAPER_POWER_ON);
    epaper_il0373_wait_idle();

    LCD_WRITE_CMD(E_PAPER_PANEL_SETTING);			//panel setting
	LCD_WRITE_DATA(0xbf);			//LUT from OTP
    
    LCD_WRITE_CMD(E_PAPER_PLL_CONTROL );			
	LCD_WRITE_DATA(0x3c);		
    	
	LCD_WRITE_CMD(E_PAPER_TCON_RESOLUTION);			//resolution setting
	LCD_WRITE_DATA(0x68);        	 //104
	LCD_WRITE_DATA(0x00);		    //212
	LCD_WRITE_DATA(0xd4);

	LCD_WRITE_CMD(E_PAPER_VCM_DC_SETTING_REGISTER);			//vcom_DC setting
	LCD_WRITE_DATA(0x12);

	LCD_WRITE_CMD(E_PAPER_VCOM_AND_DATA_INTERVAL_SETTING);			//VCOM AND DATA INTERVAL SETTING			
	LCD_WRITE_DATA(0x97);		//WBmode:VBDF 17|D7 VBDW 97 VBDB 57	
	return ESP_OK;
}

static esp_err_t epaper_il0373_write_ram_data(uint16_t color)
{
    ESP_LOGW(TAG, "epaper unsupport write ram data");
    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t epaper_il0703_display_Clean(void)
{
	unsigned int i;
	LCD_WRITE_CMD(0x10);
	for(i=0;i<2756;i++) {
		LCD_WRITE_DATA(0xff);  
	}  
	vTaskDelay(2 / portTICK_RATE_MS);	

	LCD_WRITE_CMD(0x13);
	for(i=0;i<2756;i++)	{
		LCD_WRITE_DATA(0xff);  
	}  
	vTaskDelay(2 / portTICK_RATE_MS);	
	return 	ESP_OK; 
}

esp_err_t epaper_il0373_sleep(void)
{
    LCD_WRITE_CMD(E_PAPER_DEEP_SLEEP);
    LCD_WRITE_DATA(0xa5);
	return ESP_OK;
}

esp_err_t epaper_il0373_refresh(void)
{
    LCD_WRITE_CMD(E_PAPER_DISPLAY_REFRESH);
    epaper_il0373_wait_idle();
	return ESP_OK;
}

void epaper_il0373_pic_display_fullscreen(const unsigned char* picData)
{
    ESP_LOGI(TAG,"show picture");
	unsigned int i;
	LCD_WRITE_CMD(0x10);	       //Transfer old data
	for(i=0;i<2756;i++) {
		LCD_WRITE_DATA(0xff); 
	}

	LCD_WRITE_CMD(0x13);		     //Transfer new data
	for(i=0;i<2756;i++) {
	  LCD_WRITE_DATA(*picData);
	  picData++;
    }
   
}

void epaper_il0373_pic_display_4bit (const unsigned char* picData)
{
    uint32_t i,j;
	uint8_t temp1,temp2,temp3;

	LCD_WRITE_CMD(0x10);	       		
	for(i=0;i<2756;i++) {	               //2756*4  212*104
		temp3=0;
        for(j=0;j<4;j++) {
			temp1 = picData[i*4+j];
			temp2 = temp1&0xF0 ;
			if(temp2 == 0xF0) {
				temp3 |= 0x01;
			} else if(temp2 == 0x00){
				temp3 |= 0x00;
			} else if((temp2>0xA0)&&(temp2<0xF0)) { 
				temp3 |= 0x01;  //gray1
			}
			else {
				temp3 |= 0x00; //gray2
			}
			temp3 <<= 1;	
			temp1 <<= 4;
			temp2 = temp1&0xF0 ;
			if(temp2 == 0xF0) {  //white
				temp3 |= 0x01;
			} else if(temp2 == 0x00) {//black
				temp3 |= 0x00;
			} else if((temp2>0xA0)&&(temp2<0xF0)) {
				temp3 |= 0x01; //gray1
			}
			else { 
				temp3 |= 0x00;	//gray2	
			}
        	if(j!=3) {				
				temp3 <<= 1;
			}		
		}	
        LCD_WRITE_DATA(temp3);			
	}

	LCD_WRITE_CMD(0x13);	       

	for(i=0;i<2756;i++)	{              //2756*4   212*104 
		temp3=0;
        for(j=0;j<4;j++) {
			temp1 = picData[i*4+j];
			temp2 = temp1&0xF0 ;
			if(temp2 == 0xF0) {
				temp3 |= 0x01;//white
			} else if(temp2 == 0x00) {
				temp3 |= 0x00;  //black
			} else if((temp2>0xA0)&&(temp2<0xF0)) {
				temp3 |= 0x00;  //gray1
			}
			else {
				temp3 |= 0x01; //gray2
			}
			temp3 <<= 1;	
			temp1 <<= 4;
			temp2 = temp1&0xF0;
			if(temp2 == 0xF0) {  //white
				temp3 |= 0x01;
			} else if(temp2 == 0x00) {//black
				temp3 |= 0x00;
			} else if((temp2>0xA0)&&(temp2<0xF0)) {
				temp3 |= 0x00;//gray1
			}
			else {  
				temp3 |= 0x01;	//gray2
			}
        	if(j!=3){				
			  temp3 <<= 1;
			}				
		}	
       	LCD_WRITE_DATA(temp3);			
	}
    lut11();
	LCD_WRITE_CMD(0X12);  /*refresh*/
    vTaskDelay(500 / portTICK_PERIOD_MS);
    epaper_il0373_wait_idle();  
}

void lut11(void)
{
	unsigned int count;	 
	    LCD_WRITE_CMD(0x20);							//vcom
		for(count=0;count<42;count++) {
			LCD_WRITE_DATA(lut_vcom11[count]);
		}

	    LCD_WRITE_CMD(0x21);							//red not use
	    for(count=0;count<42;count++) {
			LCD_WRITE_DATA(lut_ww11[count]);
		}

	    LCD_WRITE_CMD(0x22);							//bw r
	    for(count=0;count<42;count++) {
			LCD_WRITE_DATA(lut_bw11[count]);
		}

	    LCD_WRITE_CMD(0x23);							//wb w
	    for(count=0;count<42;count++) {
			LCD_WRITE_DATA(lut_wb11[count]);
		}

		LCD_WRITE_CMD(0x24);							//bb b
		for(count=0;count<42;count++) {
			LCD_WRITE_DATA(lut_bb11[count]);
		}

		LCD_WRITE_CMD(0x25);							//vcom
		for(count=0;count<42;count++) {
			LCD_WRITE_DATA(lut_ww11[count]);
		}
		         
}



void epaper_il0373_partial_display(uint16_t x_start,uint16_t y_start,const unsigned char *old_data,const unsigned char *new_data,unsigned int PART_COLUMN,unsigned int PART_LINE,unsigned char mode)
{
	unsigned int i,count;  
	unsigned int x_end,y_start1,y_start2,y_end1,y_end2;
	x_start=x_start;
	x_end=x_start+PART_LINE-1; 
	
	y_start1=0;
	y_start2=y_start;
	if(y_start>=256) {
		y_start1=y_start2/256;
		y_start2=y_start2%256;
	}
	y_end1=0;
	y_end2=y_start+PART_COLUMN-1;
	if(y_end2>=256) {
		y_end1=y_end2/256;
		y_end2=y_end2%256;		
	}		
	
	count=PART_COLUMN*PART_LINE/8;
	
	LCD_WRITE_CMD(0x82);			//vcom_DC setting  	
    LCD_WRITE_DATA(0x08);	
	LCD_WRITE_CMD(0X50);
	LCD_WRITE_DATA(0x47);		
	lut1();
	LCD_WRITE_CMD(0x91);		//This command makes the display enter partial mode
	LCD_WRITE_CMD(0x90);		//resolution setting
	LCD_WRITE_DATA (x_start);   //x-start     
	LCD_WRITE_DATA (x_end);	 //x-end	

	LCD_WRITE_DATA (y_start1);
	LCD_WRITE_DATA (y_start2);   //y-start    
		
	LCD_WRITE_DATA (y_end1);		
	LCD_WRITE_DATA (y_end2);  //y-end
	LCD_WRITE_DATA (0x28);	

	LCD_WRITE_CMD(0x10);	       //writes Old data to SRAM for programming
 
	if(mode==0){
		for(i=0;i<count;i++) {
	 		LCD_WRITE_DATA(0x00);  
		}
	} 
	else {
	 	for(i=0;i<count;i++) {
	 		LCD_WRITE_DATA(~old_data[i]);  
		}
	}  
	
	LCD_WRITE_CMD(0x13);				 //writes New data to SRAM.
	if(mode!=2) {  						//new  datas
		for(i=0;i<count;i++) {
			LCD_WRITE_DATA(~new_data[i]);  
		}
  	}
  	else  {								//white
		for(i=0;i<count;i++) {
			LCD_WRITE_DATA(0x00);  
		}		
	}		
    	
	LCD_WRITE_CMD(0x12);		 //DISPLAY REFRESH 		             
	vTaskDelay(10 / portTICK_PERIOD_MS);     //!!!The delay here is necessary, 200uS at least!!!     
	epaper_il0373_wait_idle();
}

void lut1(void)
{
	unsigned int count;

	LCD_WRITE_CMD(E_PAPER_LUT_FOR_VCOM );
	for(count=0;count<44;count++) {
		LCD_WRITE_DATA(lut_vcom1[count]);
	}	     

	LCD_WRITE_CMD(E_PAPER_LUT_WHITE_TO_WHITE);
	for(count=0;count<42;count++) {
		LCD_WRITE_DATA(lut_ww1[count]);
	}	      
	
	LCD_WRITE_CMD(E_PAPER_LUT_BLACK_TO_WHITE);
	for(count=0;count<42;count++) {
		LCD_WRITE_DATA(lut_bw1[count]);
	}

	LCD_WRITE_CMD(E_PAPER_LUT_WHITE_TO_BLACK);
	for(count=0;count<42;count++) {
		LCD_WRITE_DATA(lut_wb1[count]);
	}	     

	LCD_WRITE_CMD(E_PAPER_LUT_BLACK_TO_BLACK);
	for(count=0;count<42;count++) {
		LCD_WRITE_DATA(lut_bb1[count]);
	}	      
}

void lut(void)
{
	unsigned int count;

	LCD_WRITE_CMD(E_PAPER_LUT_FOR_VCOM );
	for(count=0;count<44;count++) {
		LCD_WRITE_DATA(lut_vcomDC[count]);
	}	     

	LCD_WRITE_CMD(E_PAPER_LUT_WHITE_TO_WHITE);
	for(count=0;count<42;count++) {
		LCD_WRITE_DATA(lut_ww[count]);
	}	      
	
	LCD_WRITE_CMD(E_PAPER_LUT_BLACK_TO_WHITE);
	for(count=0;count<42;count++) {
		LCD_WRITE_DATA(lut_bw[count]);
	}

	LCD_WRITE_CMD(E_PAPER_LUT_WHITE_TO_BLACK);
	for(count=0;count<42;count++) {
		LCD_WRITE_DATA(lut_wb[count]);
	}	     

	LCD_WRITE_CMD(E_PAPER_LUT_BLACK_TO_BLACK);
	for(count=0;count<42;count++) {
		LCD_WRITE_DATA(lut_bb[count]);
	}	      
}