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
#include "ssd1306.h"

static const char *TAG = "lcd ssd1306";

#define LCD_SSD1306_CHECK(a, str, ret)  if(!(a)) {                               \
        ESP_LOGE(TAG,"%s:%d (%s):%s", __FILE__, __LINE__, __FUNCTION__, str);   \
        return (ret);                                                           \
    }

#define LCD_NAME "OLED SSD1306"

// Some fundamental define for screen controller
#define SSD1306_PAGES                          8
#define SSD1306_HEIGHT                         64
#define SSD1306_COLUMNS                        128

// Control byte
#define SSD1306_CONTROL_BYTE_CMD_SINGLE        0x80
#define SSD1306_CONTROL_BYTE_CMD_STREAM        0x00
#define SSD1306_CONTROL_BYTE_DATA_STREAM       0x40

// Fundamental commands (pg.28)
#define SSD1306_CMD_SET_CONTRAST               0x81    // follow with 0x7F
#define SSD1306_CMD_DISPLAY_RAM                0xA4
#define SSD1306_CMD_DISPLAY_ALLON              0xA5
#define SSD1306_CMD_DISPLAY_NORMAL             0xA6
#define SSD1306_CMD_DISPLAY_INVERTED           0xA7
#define SSD1306_CMD_DISPLAY_OFF                0xAE
#define SSD1306_CMD_DISPLAY_ON                 0xAF

// Display Scrolling Parameters
#define SSD1306_CMD_RIGHT_HORIZONTAL_SCROLL              0x26  // Init rt scroll
#define SSD1306_CMD_LEFT_HORIZONTAL_SCROLL               0x27  // Init left scroll
#define SSD1306_CMD_VERTICAL_AND_RIGHT_HORIZONTAL_SCROLL 0x29  // Init diag scroll
#define SSD1306_CMD_VERTICAL_AND_LEFT_HORIZONTAL_SCROLL  0x2A  // Init diag scroll
#define SSD1306_CMD_DEACTIVATE_SCROLL                    0x2E  // Stop scroll
#define SSD1306_CMD_ACTIVATE_SCROLL                      0x2F  // Start scroll
#define SSD1306_CMD_SET_VERTICAL_SCROLL_AREA             0xA3  // Set scroll range

// Addressing Command Table (pg.30)
#define SSD1306_CMD_SET_LOWER_COLUMN_ADDR      0x00    // Set Lower Column Start Address for Page Addressing Mode, using X[3:0]
#define SSD1306_CMD_SET_HIGHER_COLUMN_ADDR     0x10    // Set Higher Column Start Address for Page Addressing Mode, using X[3:0]
#define SSD1306_CMD_SET_MEMORY_ADDR_MODE       0x20    // follow with 00b= Horizontal Addressing Mode; 01b=Vertical Addressing Mode; 10b= Page Addressing Mode (RESET)
#define SSD1306_CMD_SET_COLUMN_RANGE           0x21    // can be used only in HORZ/VERT mode - follow with 0x00 and 0x7F = COL127
#define SSD1306_CMD_SET_PAGE_RANGE             0x22    // can be used only in HORZ/VERT mode - follow with 0x00 and 0x07 = PAGE7
#define SSD1306_CMD_SET_PAGE_ADDR              0xB0

// Hardware Config (pg.31)
#define SSD1306_CMD_SET_DISPLAY_START_LINE     0x40
#define SSD1306_CMD_SET_SEGMENT_REMAP          0xA1
#define SSD1306_CMD_SET_MUX_RATIO              0xA8    // follow with 0x3F = 64 MUX
#define SSD1306_CMD_SET_COM_SCAN_MODE_NORMAL   0xC0
#define SSD1306_CMD_SET_COM_SCAN_MODE_REMAP    0xC8
#define SSD1306_CMD_SET_DISPLAY_OFFSET         0xD3    // follow with 0x00
#define SSD1306_CMD_SET_COM_PIN_MAP            0xDA    // follow with 0x12
#define SSD1306_CMD_NOP                        0xE3    // NOP

// Timing and Driving Scheme (pg.32)
#define SSD1306_CMD_SET_DISPLAY_CLK_DIV        0xD5    // follow with 0x80
#define SSD1306_CMD_SET_PRECHARGE              0xD9    // follow with 0xF1
#define SSD1306_CMD_SET_VCOMH_DESELCT          0xDB    // follow with 0x30

// Charge Pump (pg.62)
#define OLED_CMD_SET_CHARGE_PUMP               0x8D    // follow with 0x14

#define SSD1306_COLUMN_ADDR 0x00
#define SSD1306_LOWER_ADDRESS   (SSD1306_CMD_SET_LOWER_COLUMN_ADDR + (SSD1306_COLUMN_ADDR&0x0f))
#define SSD1306_HIGHER_ADDRESS  (SSD1306_CMD_SET_HIGHER_COLUMN_ADDR + ((SSD1306_COLUMN_ADDR&0xf0)>>4))

#define CONFIG_MONO_SCREEN_BUFFER 1

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
} ssd1306_dev_t;

static ssd1306_dev_t g_lcd_handle;

static esp_err_t lcd_ssd1306_set_window(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1);
static esp_err_t lcd_ssd1306_write_ram_data(uint16_t color);

lcd_driver_fun_t lcd_ssd1306_default_driver = {
    .init = lcd_ssd1306_init,
    .deinit = lcd_ssd1306_deinit,
    .set_direction = lcd_ssd1306_set_rotate,
    .set_window = lcd_ssd1306_set_window,
    .write_ram_data = lcd_ssd1306_write_ram_data,
    .draw_pixel = lcd_ssd1306_draw_pixel,
    .draw_bitmap = lcd_ssd1306_draw_bitmap,
    .get_info = lcd_ssd1306_get_info,
};


esp_err_t lcd_ssd1306_init(const lcd_config_t *lcd_conf)
{
    LCD_SSD1306_CHECK(lcd_conf->width <= SSD1306_COLUMNS, "Width greater than maximum", ESP_ERR_INVALID_ARG);
    LCD_SSD1306_CHECK(lcd_conf->height <= SSD1306_HEIGHT, "Height greater than maximum", ESP_ERR_INVALID_ARG);
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
    LCD_SSD1306_CHECK(ESP_OK == ret, "i2c lcd driver initial failed", ESP_FAIL);

    LCD_WRITE_CMD(0xAE); //--turn off oled panel
    LCD_WRITE_CMD(0x00); //---set low column address
    LCD_WRITE_CMD(0x10); //---set high column address
    LCD_WRITE_CMD(0x40); //--set start line address  Set Mapping RAM Display Start Line (0x00~0x3F)
    LCD_WRITE_CMD(0x81); //--set contrast control register
    LCD_WRITE_CMD(0xCF); // Set SEG Output Current Brightness
    LCD_WRITE_CMD(0xA1); //--Set SEG/Column Mapping
    LCD_WRITE_CMD(0xC0); //Set COM/Row Scan Direction
    LCD_WRITE_CMD(0xA6); //--set normal display
    LCD_WRITE_CMD(0xA8); //--set multiplex ratio(1 to 64)
    LCD_WRITE_CMD(0x3f); //--1/64 duty
    LCD_WRITE_CMD(0xD3); //-set display offset  Shift Mapping RAM Counter (0x00~0x3F)
    LCD_WRITE_CMD(0x00); //-not offset
    LCD_WRITE_CMD(0xd5); //--set display clock divide ratio/oscillator frequency
    LCD_WRITE_CMD(0x80); //--set divide ratio, Set Clock as 100 Frames/Sec
    LCD_WRITE_CMD(0xD9); //--set pre-charge period
    LCD_WRITE_CMD(0xF1); //Set Pre-Charge as 15 Clocks & Discharge as 1 Clock
    LCD_WRITE_CMD(0xDA); //--set com pins hardware configuration
    LCD_WRITE_CMD(0x12);
    LCD_WRITE_CMD(0xDB); //--set vcomh
    LCD_WRITE_CMD(0x40); //Set VCOM Deselect Level
    LCD_WRITE_CMD(0x20); //-Set Page Addressing Mode (0x00/0x01/0x02)
    LCD_WRITE_CMD(0x02);
    LCD_WRITE_CMD(0x8D); //--set Charge Pump enable/disable
    LCD_WRITE_CMD(0x14); //--set(0x10) disable
    LCD_WRITE_CMD(0xA4); // Disable Entire Display On (0xa4/0xa5)
    LCD_WRITE_CMD(0xA6); // Disable Inverse Display On (0xa6/a7)
    LCD_WRITE_CMD(0xAF); //--turn on oled panel

    lcd_ssd1306_set_rotate(lcd_conf->rotate);
    return ESP_OK;
}

esp_err_t lcd_ssd1306_deinit(void)
{
    memset(&g_lcd_handle, 0, sizeof(ssd1306_dev_t));
    return LCD_IFACE_DEINIT(true);
}

esp_err_t lcd_ssd1306_get_info(lcd_info_t *info)
{
    info->width = g_lcd_handle.width;
    info->height = g_lcd_handle.height;
    info->name = LCD_NAME;
    info->color_type = LCD_COLOR_TYPE_MONO;
    return ESP_OK;
}

esp_err_t lcd_ssd1306_set_rotate(lcd_dir_t dir)
{
    LCD_SSD1306_CHECK(dir < 4, "Unsupport rotate direction", ESP_ERR_INVALID_ARG);
    esp_err_t ret = ESP_OK;
    ESP_LOGI(TAG, "Set rotate %d", dir);
    switch (dir) {
    case LCD_DIR_LRTB:
        /* @---> X
           |
           Y
        */
        g_lcd_handle.width = g_lcd_handle.original_width;
        g_lcd_handle.height = g_lcd_handle.original_height;
        ret |= LCD_WRITE_CMD(0xA0);
        ret |= LCD_WRITE_CMD(0xC0);
        break;
    case LCD_DIR_LRBT:
        /*  Y
            |
            @---> X
        */
        ret |= LCD_WRITE_CMD(0xA0);
        ret |= LCD_WRITE_CMD(0xC8);
        g_lcd_handle.width = g_lcd_handle.original_width;
        g_lcd_handle.height = g_lcd_handle.original_height;
        break;
    case LCD_DIR_RLTB:
        /* X <---@
                 |
                 Y
        */
        ret |= LCD_WRITE_CMD(0xA1);
        ret |= LCD_WRITE_CMD(0xC0);
        g_lcd_handle.width = g_lcd_handle.original_width;
        g_lcd_handle.height = g_lcd_handle.original_height;
        break;
    case LCD_DIR_RLBT:
        /*        Y
                  |
            X <---@
        */
        ret |= LCD_WRITE_CMD(0xA1);
        ret |= LCD_WRITE_CMD(0xC8);
        g_lcd_handle.width = g_lcd_handle.original_width;
        g_lcd_handle.height = g_lcd_handle.original_height;
        break;
    default:
        g_lcd_handle.width = g_lcd_handle.original_width;
        g_lcd_handle.height = g_lcd_handle.original_height;
        break;
    }
    LCD_SSD1306_CHECK(ESP_OK == ret, "Set screen rotate failed", ESP_FAIL);
    return ESP_OK;
}

static esp_err_t lcd_ssd1306_set_window(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1)
{
    ESP_LOGW(TAG, "lcd ssd1306 unsupport set window");
    return ESP_ERR_NOT_SUPPORTED;
}

static esp_err_t lcd_ssd1306_write_ram_data(uint16_t color)
{
    ESP_LOGW(TAG, "lcd ssd1306 unsupport write ram data");
    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t lcd_ssd1306_draw_pixel(uint16_t x, uint16_t y, uint16_t chPoint)
{
    LCD_SSD1306_CHECK((x < g_lcd_handle.width) && (y < g_lcd_handle.height), "The set coordinates exceed the screen size", ESP_ERR_INVALID_ARG);
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

esp_err_t lcd_ssd1306_draw_bitmap(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t *bitmap)
{
    LCD_SSD1306_CHECK((x + w <= g_lcd_handle.width) && (y + h <= g_lcd_handle.height), "The set coordinates exceed the screen size", ESP_ERR_INVALID_ARG);
    esp_err_t ret = ESP_OK;
    uint8_t *p = (uint8_t *)bitmap;

#if CONFIG_MONO_SCREEN_BUFFER
    uint16_t i, j;
    for (j = 0; j < h; j++) {
        for (i = 0; i < w; i++) {
            uint8_t bits = p[((j) / 8) * (w) + (i)];
            if (bits & (1 << (j % 8))) {
                ret |= lcd_ssd1306_draw_pixel(x + i, y + j, 1);
            } else {
                ret |= lcd_ssd1306_draw_pixel(x + i, y + j, 0);
            }
        }
    }
#else
    LCD_SSD1306_CHECK((0 == (y % 8)), "Y should be a multiple of 8", ESP_ERR_INVALID_ARG);
    LCD_SSD1306_CHECK((0 == (h % 8)), "Height should be a multiple of 8", ESP_ERR_INVALID_ARG);
    uint8_t row1 = y >> 3;
    uint8_t row2 = (h + y) / 8; printf("[%d][%d]\n", row1, row2);
    ret |= LCD_WRITE_CMD(SSD1306_CMD_SET_MEMORY_ADDR_MODE);
    ret |= LCD_WRITE_CMD(0); /**< Set to Horizontal Addressing Mode */
    ret |= LCD_WRITE_CMD(SSD1306_CMD_SET_COLUMN_RANGE);
    ret |= LCD_WRITE_CMD(x);
    ret |= LCD_WRITE_CMD(x + w - 1);
    ret |= LCD_WRITE_CMD(SSD1306_CMD_SET_PAGE_RANGE);
    ret |= LCD_WRITE_CMD(row1);
    ret |= LCD_WRITE_CMD(row2 - 1);
    ret |= LCD_WRITE(p, w * (row2 - row1));
#endif

    LCD_SSD1306_CHECK(ESP_OK == ret, "Draw bitmap failed", ESP_FAIL);
    return ESP_OK;
}

esp_err_t lcd_ssd1306_refresh_gram(void)
{
#if CONFIG_MONO_SCREEN_BUFFER
    esp_err_t ret = ESP_OK;
#if DIRTY
    ESP_LOGI(TAG, "(%d, %d) (%d, %d)", g_lcd_handle.dirty_col_start, g_lcd_handle.dirty_page_start, g_lcd_handle.dirty_col_end, g_lcd_handle.dirty_page_end);

    ret |= LCD_WRITE_CMD(SSD1306_CMD_SET_MEMORY_ADDR_MODE);
    ret |= LCD_WRITE_CMD(0); /**< Set to Horizontal Addressing Mode */
    ret |= LCD_WRITE_CMD(SSD1306_CMD_SET_COLUMN_RANGE);
    ret |= LCD_WRITE_CMD(g_lcd_handle.dirty_col_start);
    ret |= LCD_WRITE_CMD(g_lcd_handle.dirty_col_end);
    ret |= LCD_WRITE_CMD(SSD1306_CMD_SET_PAGE_RANGE);
    ret |= LCD_WRITE_CMD(g_lcd_handle.dirty_page_start);
    ret |= LCD_WRITE_CMD(g_lcd_handle.dirty_page_end);
    ret |= LCD_WRITE(&g_lcd_handle.gram[g_lcd_handle.dirty_page_start][g_lcd_handle.dirty_col_start],
                     (g_lcd_handle.dirty_col_end - g_lcd_handle.dirty_col_start) * (g_lcd_handle.dirty_page_end - g_lcd_handle.dirty_page_start));

    g_lcd_handle.dirty_col_start = 0;
    g_lcd_handle.dirty_page_start = 0;
    g_lcd_handle.dirty_col_end = 0;
    g_lcd_handle.dirty_page_end = 0;
#else
    ret |= LCD_WRITE_CMD(SSD1306_CMD_SET_MEMORY_ADDR_MODE);
    ret |= LCD_WRITE_CMD(0); /**< Set to Horizontal Addressing Mode */
    ret |= LCD_WRITE_CMD(SSD1306_CMD_SET_COLUMN_RANGE);
    ret |= LCD_WRITE_CMD(0);
    ret |= LCD_WRITE_CMD(SSD1306_COLUMNS - 1);
    ret |= LCD_WRITE_CMD(SSD1306_CMD_SET_PAGE_RANGE);
    ret |= LCD_WRITE_CMD(0);
    ret |= LCD_WRITE_CMD(SSD1306_PAGES - 1);
    ret |= LCD_WRITE(&g_lcd_handle.gram[0][0], SSD1306_COLUMNS * SSD1306_PAGES);
#endif
    LCD_SSD1306_CHECK(ESP_OK == ret, "refresh GRAM failed", ESP_FAIL);
#else
    ESP_LOGW(TAG, "SSD1306 not need to refresh gram without buffer");
#endif
    return ESP_OK;
}


esp_err_t lcd_ssd1306_clear_buffer(void)
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


esp_err_t lcd_ssd1306_clear_screen(void)
{
    uint8_t i, j;
    esp_err_t ret;

    for (i = 0; i < SSD1306_PAGES; i++) {
        ret = LCD_WRITE_CMD(SSD1306_CMD_SET_PAGE_ADDR + i);
        ret |= LCD_WRITE_CMD(SSD1306_LOWER_ADDRESS);
        ret |= LCD_WRITE_CMD(SSD1306_HIGHER_ADDRESS);
        for (j = 0; j < SSD1306_COLUMNS; j++) {
#if CONFIG_MONO_SCREEN_BUFFER
            g_lcd_handle.gram[i][j] = 0;
#endif
            ret |= LCD_WRITE_DATA(0);
        }
        LCD_SSD1306_CHECK(ESP_OK == ret, "Write data failed", ESP_FAIL);
    }
    return ESP_OK;
}

esp_err_t lcd_ssd1306_display_on(void)
{
    esp_err_t ret;
    ret = LCD_WRITE_CMD(OLED_CMD_SET_CHARGE_PUMP); // SET DCDC
    ret |= LCD_WRITE_CMD(0X14); // Enable charge pump during display on
    ret |= LCD_WRITE_CMD(SSD1306_CMD_DISPLAY_ON); // DISPLAY ON
    LCD_SSD1306_CHECK(ESP_OK == ret, "Set display ON failed", ESP_FAIL);
    return ESP_OK;
}

esp_err_t lcd_ssd1306_display_off(void)
{
    esp_err_t ret;
    ret = LCD_WRITE_CMD(OLED_CMD_SET_CHARGE_PUMP); //SET DCDC
    ret |= LCD_WRITE_CMD(0X10); // Disable charge pump
    ret |= LCD_WRITE_CMD(SSD1306_CMD_DISPLAY_OFF); //DISPLAY OFF
    LCD_SSD1306_CHECK(ESP_OK == ret, "Set display OFF failed", ESP_FAIL);
    return ESP_OK;
}

esp_err_t lcd_ssd1306_set_contrast(uint8_t contrast)
{
    esp_err_t ret;
    ret = LCD_WRITE_CMD(0x81);
    ret |= LCD_WRITE_CMD(contrast);
    LCD_SSD1306_CHECK(ESP_OK == ret, "Set contrast failed", ESP_FAIL);
    return ESP_OK;
}

esp_err_t lcd_ssd1306_start_horizontal_scroll(uint8_t dir, uint8_t start, uint8_t stop, uint8_t interval)
{
    LCD_SSD1306_CHECK(start < SSD1306_PAGES, "Start page address invalid", ESP_ERR_INVALID_ARG);
    LCD_SSD1306_CHECK(stop < SSD1306_PAGES, "End page address invalid", ESP_ERR_INVALID_ARG);
    LCD_SSD1306_CHECK(interval < 8, "Time interval invalid", ESP_ERR_INVALID_ARG);

    uint8_t cmd = dir ? SSD1306_CMD_LEFT_HORIZONTAL_SCROLL : SSD1306_CMD_RIGHT_HORIZONTAL_SCROLL;
    esp_err_t ret = ESP_OK;
    ret |= LCD_WRITE_CMD(cmd);
    ret |= LCD_WRITE_CMD(0x00);
    ret |= LCD_WRITE_CMD(start);
    ret |= LCD_WRITE_CMD(interval);
    ret |= LCD_WRITE_CMD(stop);

    ret |= LCD_WRITE_CMD(0x00);
    ret |= LCD_WRITE_CMD(0xff);
    ret |= LCD_WRITE_CMD(SSD1306_CMD_ACTIVATE_SCROLL);
    LCD_SSD1306_CHECK(ESP_OK == ret, "Start horizontal scroll failed", ESP_FAIL);
    return ESP_OK;
}

esp_err_t lcd_ssd1306_start_vertical_scroll(uint8_t start, uint8_t stop)
{
    LCD_SSD1306_CHECK(start < SSD1306_PAGES, "Start page address invalid", ESP_ERR_INVALID_ARG);
    LCD_SSD1306_CHECK(stop < SSD1306_PAGES, "End page address invalid", ESP_ERR_INVALID_ARG);
    esp_err_t ret = ESP_OK;

    ret |= LCD_WRITE_CMD(SSD1306_CMD_SET_VERTICAL_SCROLL_AREA);
    ret |= LCD_WRITE_CMD(start);
    ret |= LCD_WRITE_CMD(stop);
    ret |= LCD_WRITE_CMD(SSD1306_CMD_ACTIVATE_SCROLL);
    LCD_SSD1306_CHECK(ESP_OK == ret, "Start vertical scroll failed", ESP_FAIL);
    return ESP_OK;
}

esp_err_t lcd_ssd1306_start_scroll_diagRight(uint8_t start, uint8_t stop)
{
    esp_err_t ret = ESP_OK;
    ret |= LCD_WRITE_CMD(SSD1306_CMD_SET_VERTICAL_SCROLL_AREA);
    ret |= LCD_WRITE_CMD(0x00);
    ret |= LCD_WRITE_CMD(32);
    ret |= LCD_WRITE_CMD(SSD1306_CMD_VERTICAL_AND_RIGHT_HORIZONTAL_SCROLL);
    ret |= LCD_WRITE_CMD(0x00);
    ret |= LCD_WRITE_CMD(start);
    ret |= LCD_WRITE_CMD(0x00);
    ret |= LCD_WRITE_CMD(stop);
    ret |= LCD_WRITE_CMD(0x01);
    ret |= LCD_WRITE_CMD(SSD1306_CMD_ACTIVATE_SCROLL);
    LCD_SSD1306_CHECK(ESP_OK == ret, "Start diagRight scroll failed", ESP_FAIL);
    return ESP_OK;
}

esp_err_t lcd_ssd1306_stop_scroll(void)
{
    esp_err_t ret = ESP_OK;
    ret |= LCD_WRITE_CMD(SSD1306_CMD_DEACTIVATE_SCROLL);
    LCD_SSD1306_CHECK(ESP_OK == ret, "Stop scroll failed", ESP_FAIL);
    return ESP_OK;
}