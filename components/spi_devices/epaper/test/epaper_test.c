/*
 * ESPRSSIF MIT License

 *
 * Copyright (c) 2015 <ESPRESSIF SYSTEMS (SHANGHAI) PTE LTD>
 *
 * Permission is hereby granted for use on ESPRESSIF SYSTEMS ESP8266 only, in which case,
 * it is free of charge, to any person obtaining a copy of this software and associated
 * documentation files (the "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the Software is furnished
 * to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or
 * substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 */

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "esp_log.h"
#include <string.h>
#include "unity.h"

#include "imagedata.h"
#include "epaper.h"
#include "epaper_fonts.h"

// Pin definition
#define MOSI_PIN        23
#define MISO_PIN        19
#define SCK_PIN         18
#define BUSY_PIN        32
#define DC_PIN          26
#define RST_PIN         27
#define CS_PIN          5

// Display resolution
#define EPD_WIDTH       176
#define EPD_HEIGHT      264

// Color inverse. 1 or 0 = set or reset a bit if set a colored pixel
#define IF_INVERT_COLOR 1

void epaper_test()
{
    uint32_t cnt = 0;
    char hum_str[6];
    char tsens_str[6];
    epaper_handle_t device = NULL;

    epaper_conf_t epaper_conf = {
        .busy_pin = BUSY_PIN,
        .cs_pin = CS_PIN,
        .dc_pin = DC_PIN,
        .miso_pin = MISO_PIN,
        .mosi_pin = MOSI_PIN,
        .reset_pin = RST_PIN,
        .sck_pin = SCK_PIN,

        .rst_active_level = 0,
        .dc_lev_data = 1,
        .dc_lev_cmd = 0,

        .clk_freq_hz = 20 * 1000 * 1000,
        .spi_host = HSPI_HOST,

        .width = EPD_WIDTH,
        .height = EPD_HEIGHT,
        .color_inv = 1,
    };

    printf("before epaper init, heap: %d\n", esp_get_free_heap_size());
    device = iot_epaper_create(NULL, &epaper_conf);
    iot_epaper_set_rotate(device, E_PAPER_ROTATE_270);
    /* Display the image buffer */
    iot_epaper_display_frame(device, IMAGE_DATA); /* Sent C array to frame buffer */
    printf("e-Paper Display espressif logo\r\n");
    iot_epaper_clean_paint(device, UNCOLORED);
    iot_epaper_draw_string(device, 190, 0, "@espressif", &epaper_font_12, COLORED);
    iot_epaper_draw_string(device, 10, 30, "ULP&e-Paper Demo ", &epaper_font_16, COLORED);
    iot_epaper_draw_string(device, 15, 90, "Humidity", &epaper_font_16, COLORED);
    iot_epaper_draw_string(device, 15, 140, "TSENS", &epaper_font_16, COLORED);
    memset(hum_str, 0x00, sizeof(hum_str)); /* clear string buffer */
    memset(tsens_str, 0x00, sizeof(tsens_str));
    sprintf(hum_str, "%d %%", (uint8_t) esp_random());
    sprintf(tsens_str, "%d C", (uint8_t) esp_random());

    iot_epaper_draw_string(device, 190, 90, hum_str, &epaper_font_16, COLORED); /* dispaly string, use font16 */
    iot_epaper_draw_string(device, 190, 140, tsens_str, &epaper_font_16, COLORED);
    iot_epaper_draw_horizontal_line(device, 10, 47, 180, COLORED);
    iot_epaper_draw_horizontal_line(device, 10, 120, 240, COLORED); /* draw horizontal line */
    iot_epaper_draw_vertical_line(device, 150, 70, 100, COLORED); /* draw vertical line */
    iot_epaper_draw_rectangle(device, 10, 70, 250, 170, COLORED); /* draw rectangle */
    printf("EPD Display Fresh. CNT:%d\r\n", cnt++);
    /* Display the frame_buffer */
    iot_epaper_display_frame(device, NULL); /* dispaly the frame buffer */
    iot_epaper_delete(device, true);
    printf("after free epaper: heap:%d\n", esp_get_free_heap_size());
}


TEST_CASE("ePaper test", "[epaper][iot]")
{
    epaper_test();
}
