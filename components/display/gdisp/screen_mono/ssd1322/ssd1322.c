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
#include "esp_log.h"
#include "string.h"
#include "lcd_iface_driver.h"
#include "ssd1322.h"

static const char *TAG = "lcd ssd1322";

#define LCD_SSD1322_CHECK(a, str, ret)  if(!(a)) {                               \
        ESP_LOGE(TAG,"%s:%d (%s):%s", __FILE__, __LINE__, __FUNCTION__, str);   \
        return (ret);                                                           \
    }

#define LCD_NAME "OLED SSD1322"

// Some fundamental define for screen controller
#define SSD1322_HEIGHT                         128
#define SSD1322_WIDTH                          480
#define SSD1322_COLUMNS                        120
#define SSD1322_BITS_PER_PIXEL                 4

// Commands
#define SSD1322_SETCOMMANDLOCK            0xFD
#define SSD1322_DISPLAYOFF                0xAE
#define SSD1322_DISPLAYON                 0xAF
#define SSD1322_SETCLOCKDIVIDER           0xB3
#define SSD1322_SETDISPLAYOFFSET          0xA2
#define SSD1322_SETSTARTLINE              0xA1
#define SSD1322_SETREMAP                  0xA0
#define SSD1322_FUNCTIONSEL               0xAB
#define SSD1322_DISPLAYENHANCE            0xB4
#define SSD1322_SETCONTRASTCURRENT        0xC1
#define SSD1322_MASTERCURRENTCONTROL      0xC7
#define SSD1322_SETPHASELENGTH            0xB1
#define SSD1322_DISPLAYENHANCEB           0xD1
#define SSD1322_SETPRECHARGEVOLTAGE       0xBB
#define SSD1322_SETSECONDPRECHARGEPERIOD  0xB6
#define SSD1322_SETVCOMH                  0xBE
#define SSD1322_SETMUXRATIO               0xCA
#define SSD1322_SETCOLUMNADDR             0x15
#define SSD1322_SETROWADDR                0x75
#define SSD1322_WRITERAM                  0x5C
#define SSD1322_ENTIREDISPLAYOFF          0xA4
#define SSD1322_ENTIREDISPLAYON           0xA5
#define SSD1322_NORMALDISPLAY             0xA6
#define SSD1322_INVERSEDISPLAY            0xA7
#define SSD1322_SETGPIO                   0xB5
#define SSD1322_EXITPARTIALDISPLAY        0xA9
#define SSD1322_SELECTDEFAULTGRAYSCALE    0xB9


#define CONFIG_MONO_SCREEN_BUFFER 0

typedef struct {
    uint16_t original_width;
    uint16_t original_height;
#if CONFIG_MONO_SCREEN_BUFFER
    uint8_t gram[SSD1322_COLUMNS][SSD1322_HEIGHT];
#endif
    uint16_t width;
    uint16_t height;
} ssd1322_dev_t;

static ssd1322_dev_t g_lcd_handle;

static esp_err_t lcd_ssd1322_write_ram_data(uint16_t color);

lcd_driver_fun_t lcd_ssd1322_default_driver = {
    .init = lcd_ssd1322_init,
    .deinit = lcd_ssd1322_deinit,
    .set_direction = lcd_ssd1322_set_rotate,
    .set_window = lcd_ssd1322_set_window,
    .write_ram_data = lcd_ssd1322_write_ram_data,
    .draw_pixel = lcd_ssd1322_draw_pixel,
    .draw_bitmap = lcd_ssd1322_draw_bitmap,
    .get_info = lcd_ssd1322_get_info,
};

esp_err_t lcd_ssd1322_init(const lcd_config_t *lcd_conf)
{
    LCD_SSD1322_CHECK(lcd_conf->width <= SSD1322_WIDTH, "Width greater than maximum", ESP_ERR_INVALID_ARG);
    LCD_SSD1322_CHECK(lcd_conf->height <= SSD1322_HEIGHT, "Height greater than maximum", ESP_ERR_INVALID_ARG);
    // Reset the display
    if (lcd_conf->pin_num_rst >= 0) {
        gpio_pad_select_gpio(lcd_conf->pin_num_rst);
        gpio_set_direction(lcd_conf->pin_num_rst, GPIO_MODE_OUTPUT);
        gpio_set_level(lcd_conf->pin_num_rst, (lcd_conf->rst_active_level) & 0x1);
        vTaskDelay(100 / portTICK_RATE_MS);
        gpio_set_level(lcd_conf->pin_num_rst, (~(lcd_conf->rst_active_level)) & 0x1);
        vTaskDelay(100 / portTICK_RATE_MS);
    }
    g_lcd_handle.original_width = lcd_conf->width;
    g_lcd_handle.original_height = lcd_conf->height;

    esp_err_t ret;
    ret = LCD_IFACE_INIT(lcd_conf);
    LCD_SSD1322_CHECK(ESP_OK == ret, "Interface driver initial failed", ESP_FAIL);

    LCD_WRITE_CMD(SSD1322_SETCOMMANDLOCK);// 0xFD
    LCD_WRITE_DATA(0x12);// Unlock OLED driver IC

    LCD_WRITE_CMD(SSD1322_DISPLAYOFF);// 0xAE

    LCD_WRITE_CMD(SSD1322_SETCLOCKDIVIDER);// 0xB3
    LCD_WRITE_DATA(0x91);// 0xB3

    LCD_WRITE_CMD(SSD1322_SETMUXRATIO);// 0xCA
    LCD_WRITE_DATA(0x3F);// duty = 1/64

    LCD_WRITE_CMD(SSD1322_SETDISPLAYOFFSET);// 0xA2
    LCD_WRITE_DATA(0x00);

    LCD_WRITE_CMD(SSD1322_SETSTARTLINE);// 0xA1
    LCD_WRITE_DATA(0x00);

    LCD_WRITE_CMD(SSD1322_SETREMAP);// 0xA0
    LCD_WRITE_DATA(0x14);//Horizontal address increment,Disable Column Address Re-map,Enable Nibble Re-map,Scan from COM[N-1] to COM0,Disable COM Split Odd Even
    LCD_WRITE_DATA(0x11);//Enable Dual COM mode

    LCD_WRITE_CMD(SSD1322_SETGPIO);// 0xB5
    LCD_WRITE_DATA(0x00);// Disable GPIO Pins Input

    LCD_WRITE_CMD(SSD1322_FUNCTIONSEL);// 0xAB
    LCD_WRITE_DATA(0x01);// selection external vdd

    LCD_WRITE_CMD(SSD1322_DISPLAYENHANCE);// 0xB4
    LCD_WRITE_DATA(0xA0);// enables the external VSL
    LCD_WRITE_DATA(0xFD);// 0xfFD,Enhanced low GS display quality;default is 0xb5(normal),

    LCD_WRITE_CMD(SSD1322_SETCONTRASTCURRENT);// 0xC1
    LCD_WRITE_DATA(0xFF);// 0xFF - default is 0x7f

    LCD_WRITE_CMD(SSD1322_MASTERCURRENTCONTROL);// 0xC7
    LCD_WRITE_DATA(0x0F);// default is 0x0F

    // Set grayscale
    LCD_WRITE_CMD(SSD1322_SELECTDEFAULTGRAYSCALE); // 0xB9

    LCD_WRITE_CMD(SSD1322_SETPHASELENGTH);// 0xB1
    LCD_WRITE_DATA(0xE2);// default is 0x74

    LCD_WRITE_CMD(SSD1322_DISPLAYENHANCEB);// 0xD1
    LCD_WRITE_DATA(0x82);// Reserved;default is 0xa2(normal)
    LCD_WRITE_DATA(0x20);//

    LCD_WRITE_CMD(SSD1322_SETPRECHARGEVOLTAGE);// 0xBB
    LCD_WRITE_DATA(0x1F);// 0.6xVcc

    LCD_WRITE_CMD(SSD1322_SETSECONDPRECHARGEPERIOD);// 0xB6
    LCD_WRITE_DATA(0x08);// default

    LCD_WRITE_CMD(SSD1322_SETVCOMH);// 0xBE
    LCD_WRITE_DATA(0x07);// 0.86xVcc;default is 0x04

    LCD_WRITE_CMD(SSD1322_NORMALDISPLAY);// 0xA6

    LCD_WRITE_CMD(SSD1322_EXITPARTIALDISPLAY);// 0xA9
    LCD_WRITE_CMD(SSD1322_DISPLAYON);   //Sleep Out

    lcd_ssd1322_set_rotate(lcd_conf->rotate);
    return ESP_OK;
}

esp_err_t lcd_ssd1322_deinit(void)
{
    memset(&g_lcd_handle, 0, sizeof(ssd1322_dev_t));
    return LCD_IFACE_DEINIT(true);
}

esp_err_t lcd_ssd1322_get_info(lcd_info_t *info)
{
    info->width = g_lcd_handle.width;
    info->height = g_lcd_handle.height;
    info->name = LCD_NAME;
    info->color_type = LCD_COLOR_TYPE_GRAY;
    return ESP_OK;
}

esp_err_t lcd_ssd1322_set_rotate(lcd_dir_t dir)
{
    LCD_SSD1322_CHECK(dir < 4, "Unsupport rotate direction", ESP_ERR_INVALID_ARG);
    esp_err_t ret = ESP_OK;
    uint8_t reg_data = 0x04;
    switch (dir) {
    case LCD_DIR_LRTB:
        /* @---> X
           |
           Y
        */
        reg_data |= 0x00;
        g_lcd_handle.width = g_lcd_handle.original_width;
        g_lcd_handle.height = g_lcd_handle.original_height;
        break;
    case LCD_DIR_LRBT:
        /*  Y
            |
            @---> X
        */
        reg_data |= 0x10;
        g_lcd_handle.width = g_lcd_handle.original_width;
        g_lcd_handle.height = g_lcd_handle.original_height;
        break;
    case LCD_DIR_RLTB:
        /* X <---@
                 |
                 Y
        */
        reg_data |= 0x02;
        g_lcd_handle.width = g_lcd_handle.original_width;
        g_lcd_handle.height = g_lcd_handle.original_height;
        break;
    case LCD_DIR_RLBT:
        /*        Y
                  |
            X <---@
        */
        reg_data |= 0x12;
        g_lcd_handle.width = g_lcd_handle.original_width;
        g_lcd_handle.height = g_lcd_handle.original_height;
        break;
    default:
        g_lcd_handle.width = g_lcd_handle.original_width;
        g_lcd_handle.height = g_lcd_handle.original_height;
        break;
    }
    ESP_LOGI(TAG, "Set rotate 0x%x", reg_data);
    ret |= LCD_WRITE_CMD(SSD1322_SETREMAP);
    ret |= LCD_WRITE_DATA(reg_data);
    ret |= LCD_WRITE_DATA(0x11);
    LCD_SSD1322_CHECK(ESP_OK == ret, "Set screen rotate failed", ESP_FAIL);
    return ESP_OK;
}

esp_err_t lcd_ssd1322_set_window(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1)
{
    esp_err_t ret = ESP_OK;
    x1 += 1;
    LCD_SSD1322_CHECK((0 == (x0 % 4)), "x0 should be a multiple of 4", ESP_ERR_INVALID_ARG);
    LCD_SSD1322_CHECK((0 == (x1 % 4)), "(x1+1) should be a multiple of 4", ESP_ERR_INVALID_ARG);
    uint8_t col0 = x0 / 4;
    uint8_t col1 = x1 / 4;
    ret |= LCD_WRITE_CMD(SSD1322_SETCOLUMNADDR);
    ret |= LCD_WRITE_DATA(0x1c + col0);
    ret |= LCD_WRITE_DATA(0x1c + col1 - 1);
    ret |= LCD_WRITE_CMD(SSD1322_SETROWADDR);
    ret |= LCD_WRITE_DATA(y0);
    ret |= LCD_WRITE_DATA(y1);
    ret |= LCD_WRITE_CMD(SSD1322_WRITERAM);
    LCD_SSD1322_CHECK(ESP_OK == ret, "Set window failed", ESP_FAIL);
    return ESP_OK;
}

static esp_err_t lcd_ssd1322_write_ram_data(uint16_t color)
{
    ESP_LOGW(TAG, "Unsupport write ram data");
    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t lcd_ssd1322_draw_pixel(uint16_t x, uint16_t y, uint16_t chPoint)
{
    LCD_SSD1322_CHECK((x < g_lcd_handle.width) && (y < g_lcd_handle.height), "The set coordinates exceed the screen size", ESP_ERR_INVALID_ARG);
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
    return ESP_OK;
#else
    ESP_LOGW(TAG, "Unsupport draw pixel without buffer");
    return ESP_ERR_NOT_SUPPORTED;
#endif
}

esp_err_t lcd_ssd1322_draw_bitmap(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t *bitmap)
{
    LCD_SSD1322_CHECK((x + w <= g_lcd_handle.width) && (y + h <= g_lcd_handle.height), "The set coordinates exceed the screen size", ESP_ERR_INVALID_ARG);
    esp_err_t ret = ESP_OK;
    uint8_t *p = (uint8_t *)bitmap;

#if CONFIG_MONO_SCREEN_BUFFER
    uint16_t i, j;
    for (j = 0; j < h; j++) {
        for (i = 0; i < w; i++) {
            uint8_t bits = p[((j) / 8) * (w) + (i)];
            if (bits & (1 << (j % 8))) {
                ret |= lcd_ssd1322_draw_pixel(x + i, y + j, 1);
            } else {
                ret |= lcd_ssd1322_draw_pixel(x + i, y + j, 0);
            }
        }
    }
#else
    ret |= lcd_ssd1322_set_window(x, y, x + w - 1, y + h - 1);
    ret |= LCD_WRITE(p, w * SSD1322_BITS_PER_PIXEL / 8 * h);
#endif
    LCD_SSD1322_CHECK(ESP_OK == ret, "Draw bitmap failed", ESP_FAIL);
    return ESP_OK;
}

esp_err_t lcd_ssd1322_refresh_gram(void)
{
#if CONFIG_MONO_SCREEN_BUFFER
    esp_err_t ret = ESP_OK;

    ret |= LCD_WRITE_CMD(SSD1306_CMD_SET_MEMORY_ADDR_MODE);
    ret |= LCD_WRITE_CMD(0); /**< Set to Horizontal Addressing Mode */
    ret |= LCD_WRITE_CMD(SSD1306_CMD_SET_COLUMN_RANGE);
    ret |= LCD_WRITE_CMD(0);
    ret |= LCD_WRITE_CMD(SSD1306_COLUMNS - 1);
    ret |= LCD_WRITE_CMD(SSD1306_CMD_SET_PAGE_RANGE);
    ret |= LCD_WRITE_CMD(0);
    ret |= LCD_WRITE_CMD(SSD1306_PAGES - 1);
    ret |= LCD_WRITE(&g_lcd_handle.gram[0][0], SSD1306_COLUMNS * SSD1306_PAGES);
    LCD_SSD1322_CHECK(ESP_OK == ret, "refresh GRAM failed", ESP_FAIL);
#else
    ESP_LOGW(TAG, "Not need to refresh gram without buffer");
#endif
    return ESP_OK;
}

esp_err_t lcd_ssd1322_clear_buffer(void)
{
#if CONFIG_MONO_SCREEN_BUFFER
    uint8_t i, j;

    for (i = 0; i < SSD1306_PAGES; i++) {
        for (j = 0; j < SSD1306_COLUMNS; j++) {
            g_lcd_handle.gram[i][j] = 0;
        }
    }
#else

#endif
    return ESP_OK;
}

esp_err_t lcd_ssd1322_set_contrast(uint8_t contrast)
{
    esp_err_t ret;
    ret = LCD_WRITE_CMD(SSD1322_SETCONTRASTCURRENT);
    ret |= LCD_WRITE_DATA(contrast);
    LCD_SSD1322_CHECK(ESP_OK == ret, "Set contrast failed", ESP_FAIL);
    return ESP_OK;
}

esp_err_t lcd_ssd1322_set_invert(uint8_t is_invert)
{
    esp_err_t ret;
    ret = LCD_WRITE_CMD(is_invert ? SSD1322_INVERSEDISPLAY : SSD1322_NORMALDISPLAY);
    LCD_SSD1322_CHECK(ESP_OK == ret, "Set contrast failed", ESP_FAIL);
    return ESP_OK;
}
