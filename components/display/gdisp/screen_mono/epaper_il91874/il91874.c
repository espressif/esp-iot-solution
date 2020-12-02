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
#include "string.h"
#include "lcd_low_driver.h"
#include "il91874.h"

static const char *TAG = "IL91874";

#define LCD_EPAPER_CHECK(a, str, ret)  if(!(a)) {                               \
        ESP_LOGE(TAG,"%s:%d (%s):%s", __FILE__, __LINE__, __FUNCTION__, str);   \
        return (ret);                                                           \
    }

#define LCD_NAME "EPAPER IL91874"


// epd2in7 commands
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

#define IL91874_HEIGHT                         300
#define IL91874_COLUMNS                        320

static const unsigned char lut_vcom_dc[] = {
    0x00, 0x00,
    0x00, 0x1A, 0x1A, 0x00, 0x00, 0x01,
    0x00, 0x0A, 0x0A, 0x00, 0x00, 0x08,
    0x00, 0x0E, 0x01, 0x0E, 0x01, 0x10,
    0x00, 0x0A, 0x0A, 0x00, 0x00, 0x08,
    0x00, 0x04, 0x10, 0x00, 0x00, 0x05,
    0x00, 0x03, 0x0E, 0x00, 0x00, 0x0A,
    0x00, 0x23, 0x00, 0x00, 0x00, 0x01
};

//R21H
static const unsigned char lut_ww[] = {
    0x90, 0x1A, 0x1A, 0x00, 0x00, 0x01,
    0x40, 0x0A, 0x0A, 0x00, 0x00, 0x08,
    0x84, 0x0E, 0x01, 0x0E, 0x01, 0x10,
    0x80, 0x0A, 0x0A, 0x00, 0x00, 0x08,
    0x00, 0x04, 0x10, 0x00, 0x00, 0x05,
    0x00, 0x03, 0x0E, 0x00, 0x00, 0x0A,
    0x00, 0x23, 0x00, 0x00, 0x00, 0x01
};

//R22H    r
static const unsigned char lut_bw[] = {
    0xA0, 0x1A, 0x1A, 0x00, 0x00, 0x01,
    0x00, 0x0A, 0x0A, 0x00, 0x00, 0x08,
    0x84, 0x0E, 0x01, 0x0E, 0x01, 0x10,
    0x90, 0x0A, 0x0A, 0x00, 0x00, 0x08,
    0xB0, 0x04, 0x10, 0x00, 0x00, 0x05,
    0xB0, 0x03, 0x0E, 0x00, 0x00, 0x0A,
    0xC0, 0x23, 0x00, 0x00, 0x00, 0x01
};

//R23H    w
static const unsigned char lut_bb[] = {
    0x90, 0x1A, 0x1A, 0x00, 0x00, 0x01,
    0x40, 0x0A, 0x0A, 0x00, 0x00, 0x08,
    0x84, 0x0E, 0x01, 0x0E, 0x01, 0x10,
    0x80, 0x0A, 0x0A, 0x00, 0x00, 0x08,
    0x00, 0x04, 0x10, 0x00, 0x00, 0x05,
    0x00, 0x03, 0x0E, 0x00, 0x00, 0x0A,
    0x00, 0x23, 0x00, 0x00, 0x00, 0x01
};

//R24H    b
static const unsigned char lut_wb[] = {
    0x90, 0x1A, 0x1A, 0x00, 0x00, 0x01,
    0x20, 0x0A, 0x0A, 0x00, 0x00, 0x08,
    0x84, 0x0E, 0x01, 0x0E, 0x01, 0x10,
    0x10, 0x0A, 0x0A, 0x00, 0x00, 0x08,
    0x00, 0x04, 0x10, 0x00, 0x00, 0x05,
    0x00, 0x03, 0x0E, 0x00, 0x00, 0x0A,
    0x00, 0x23, 0x00, 0x00, 0x00, 0x01
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

static esp_err_t epaper_il91874_write_ram_data(uint16_t color);


lcd_driver_fun_t epaper_il91874_default_driver = {
    .init = epaper_il91874_init,
    .deinit = epaper_il91874_deinit,
    .set_direction = epaper_il91874_set_rotate,
    .set_window = epaper_il91874_set_window,
    .write_ram_data = epaper_il91874_write_ram_data,
    .draw_pixel = epaper_il91874_draw_pixel,
    .draw_bitmap = epaper_il91874_draw_bitmap,
    .get_info = epaper_il91874_get_info,
};

static void epaper_il91874_wait_idle(void)
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

static esp_err_t epaper_il91874_set_lut(void)
{
    esp_err_t ret = ESP_OK;
    uint8_t count;
    ret |= LCD_WRITE_CMD(E_PAPER_LUT_FOR_VCOM);                            //vcom
    for(count = 0; count < 44; count++) {
        ret |= LCD_WRITE_DATA(lut_vcom_dc[count]);
    }
    
    ret |= LCD_WRITE_CMD(E_PAPER_LUT_WHITE_TO_WHITE);                      //ww --
    for(count = 0; count < 42; count++) {
        ret |= LCD_WRITE_DATA(lut_ww[count]);
    }   
    
    ret |= LCD_WRITE_CMD(E_PAPER_LUT_BLACK_TO_WHITE);                      //bw r
    for(count = 0; count < 42; count++) {
        ret |= LCD_WRITE_DATA(lut_bw[count]);
    } 

    ret |= LCD_WRITE_CMD(E_PAPER_LUT_WHITE_TO_BLACK);                      //wb w
    for(count = 0; count < 42; count++) {
        ret |= LCD_WRITE_DATA(lut_bb[count]);
    } 

    ret |= LCD_WRITE_CMD(E_PAPER_LUT_BLACK_TO_BLACK);                      //bb b
    for(count = 0; count < 42; count++) {
        ret |= LCD_WRITE_DATA(lut_wb[count]);
    } 
    LCD_EPAPER_CHECK(ESP_OK == ret, "Set LUT failed", ESP_FAIL);
    return ESP_OK;
}

esp_err_t epaper_il91874_init(const lcd_config_t *lcd_conf)
{
    LCD_EPAPER_CHECK(lcd_conf->width <= IL91874_COLUMNS, "Width greater than maximum", ESP_ERR_INVALID_ARG);
    LCD_EPAPER_CHECK(lcd_conf->height <= IL91874_HEIGHT, "Height greater than maximum", ESP_ERR_INVALID_ARG);
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
    
    LCD_WRITE_CMD(E_PAPER_POWER_ON);
    epaper_il91874_wait_idle();

    LCD_WRITE_CMD(E_PAPER_PANEL_SETTING);
    LCD_WRITE_DATA(0xaf);        //KW-BF   KWR-AF    BWROTP 0f

    LCD_WRITE_CMD(E_PAPER_PLL_CONTROL);
    LCD_WRITE_DATA(0x3a);       //3A 100HZ   29 150Hz 39 200HZ    31 171HZ

    LCD_WRITE_CMD(E_PAPER_POWER_SETTING);
    LCD_WRITE_DATA(0x03);                  // VDS_EN, VDG_EN
    LCD_WRITE_DATA(0x00);                  // VCOM_HV, VGHL_LV[1], VGHL_LV[0]
    LCD_WRITE_DATA(0x2b);                  // VDH
    LCD_WRITE_DATA(0x2b);                  // VDL
    LCD_WRITE_DATA(0x09);                  // VDHR

    LCD_WRITE_CMD(E_PAPER_BOOSTER_SOFT_START);
    LCD_WRITE_DATA(0x07);
    LCD_WRITE_DATA(0x07);
    LCD_WRITE_DATA(0x17);

    // Power optimization
    LCD_WRITE_CMD(0xF8);
    LCD_WRITE_DATA(0x60);
    LCD_WRITE_DATA(0xA5);

    // Power optimization
    LCD_WRITE_CMD(0xF8);
    LCD_WRITE_DATA(0x89);
    LCD_WRITE_DATA(0xA5);

    // Power optimization
    LCD_WRITE_CMD(0xF8);
    LCD_WRITE_DATA(0x90);
    LCD_WRITE_DATA(0x00);

    // Power optimization
    LCD_WRITE_CMD(0xF8);
    LCD_WRITE_DATA(0x93);
    LCD_WRITE_DATA(0x2A);

    // Power optimization
    LCD_WRITE_CMD(0xF8);
    LCD_WRITE_DATA(0x73);
    LCD_WRITE_DATA(0x41);

    LCD_WRITE_CMD(E_PAPER_VCM_DC_SETTING_REGISTER);
    LCD_WRITE_DATA(0x12);
    LCD_WRITE_CMD(E_PAPER_VCOM_AND_DATA_INTERVAL_SETTING);
    LCD_WRITE_DATA(0x87);        // define by OTP

    epaper_il91874_set_lut();

    LCD_WRITE_CMD(E_PAPER_PARTIAL_DISPLAY_REFRESH);
    LCD_WRITE_DATA(0x00);

    return ESP_OK;
    /* EPD hardware init end */
}

esp_err_t epaper_il91874_deinit(void)
{
    LCD_WRITE_CMD(E_PAPER_POWER_OFF);
    memset(&g_lcd_handle, 0, sizeof(epaper_dev_t));
    return LCD_IFACE_DEINIT(true);
}

esp_err_t epaper_il91874_set_rotate(lcd_dir_t dir)
{
    ESP_LOGW(TAG, "epaper unsupport set rotate");
    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t epaper_il91874_get_info(lcd_info_t *info)
{
    info->width = g_lcd_handle.width;
    info->height = g_lcd_handle.height;
    info->name = LCD_NAME;
    info->color_type = LCD_COLOR_TYPE_MONO;
    return ESP_OK;
}

esp_err_t epaper_il91874_select_transmission(uint8_t transmission)
{
    g_lcd_handle.transmission = transmission;
    return ESP_OK;
}

esp_err_t epaper_il91874_set_window(uint16_t x, uint16_t y, uint16_t w, uint16_t h)
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

static esp_err_t epaper_il91874_write_ram_data(uint16_t color)
{
    ESP_LOGW(TAG, "epaper unsupport write ram data");
    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t epaper_il91874_draw_pixel(uint16_t x, uint16_t y, uint16_t chPoint)
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
    ESP_LOGW(TAG, "SSD1306 not support draw pixel without buffer");
    return ESP_ERR_NOT_SUPPORTED;
#endif
}

esp_err_t epaper_il91874_draw_bitmap(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t *bitmap)
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
                ret |= epaper_il91874_draw_pixel(x + i, y + j, 1);
            } else {
                ret |= epaper_il91874_draw_pixel(x + i, y + j, 0);
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

void epaper_il91874_refresh_partial(uint16_t x, uint16_t y, uint16_t w, uint16_t h)
{
    LCD_WRITE_CMD(E_PAPER_PARTIAL_DISPLAY_REFRESH);
    LCD_WRITE_DATA(x >> 8);
    LCD_WRITE_DATA(x & 0xf8);     // x should be the multiple of 8, the last 3 bit will always be ignored
    LCD_WRITE_DATA(y >> 8);
    LCD_WRITE_DATA(y & 0xff);
    LCD_WRITE_DATA(w >> 8);
    LCD_WRITE_DATA(w & 0xf8);     // w (width) should be the multiple of 8, the last 3 bit will always be ignored
    LCD_WRITE_DATA(h >> 8);
    LCD_WRITE_DATA(h & 0xff);

    epaper_il91874_wait_idle();
}

void epaper_il91874_refresh(void)
{
    LCD_WRITE_CMD(E_PAPER_DISPLAY_REFRESH);
    epaper_il91874_wait_idle();
}

void epaper_il91874_sleep(void)
{
    LCD_WRITE_CMD(E_PAPER_DEEP_SLEEP);
    LCD_WRITE_DATA(0xa5);
}



void EPD_Display(uint16_t *Imageblack, uint16_t *Imagered)
{
    uint16_t Width, Height;
    Width = (g_lcd_handle.width % 8 == 0)? (g_lcd_handle.width / 8 ): (g_lcd_handle.width / 8 + 1);
    Height = g_lcd_handle.height;
    uint8_t *p = (uint8_t*)Imagered;
    
    // LCD_WRITE_CMD(E_PAPER_DATA_START_TRANSMISSION_1);
    // for (uint16_t j = 0; j < Height; j++) {
    //     for (uint16_t i = 0; i < Width; i++) {
    //         LCD_WRITE_DATA(0xff);
    //     }
    // }
    // LCD_WRITE_DATA(E_PAPER_DATA_STOP);
    
    LCD_WRITE_CMD(E_PAPER_DATA_START_TRANSMISSION_2);
    // LCD_WRITE(p, Width*Height);
    for (uint16_t j = 0; j < Height; j++) {
        for (uint16_t i = 0; i < Width; i++) {
            LCD_WRITE_DATA((*p++));
        }
    }
    LCD_WRITE_CMD(E_PAPER_DATA_STOP);
    
    LCD_WRITE_CMD(E_PAPER_DISPLAY_REFRESH);
    epaper_il91874_wait_idle();
}