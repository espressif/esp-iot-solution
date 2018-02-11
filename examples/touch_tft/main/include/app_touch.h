/*
  * ESPRESSIF MIT License
  *
  * Copyright (c) 2018 <ESPRESSIF SYSTEMS (SHANGHAI) PTE LTD>
  *
  * Permission is hereby granted for use on ESPRESSIF SYSTEMS products only, in which case,
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
#ifndef _IOT_CAMERA_TASK_H_
#define _IOT_CAMERA_TASK_H_

#define WIFI_PASSWORD CONFIG_WIFI_PASSWORD
#define WIFI_SSID     CONFIG_WIFI_SSID
/**
 * CAMERA_PF_RGB565 = 0,       //!< RGB, 2 bytes per pixel
 * CAMERA_PF_YUV422 = 1,       //!< YUYV, 2 bytes per pixel
 * CAMERA_PF_GRAYSCALE = 2,    //!< 1 byte per pixel
 * CAMERA_PF_JPEG = 3,         //!< JPEG compressed
 * CAMERA_PF_RGB555 = 4,       //!< RGB, 2 bytes per pixel
 * CAMERA_PF_RGB444 = 5,       //!< RGB, 2 bytes per pixel
 */
#define CAMERA_PIXEL_FORMAT CAMERA_PF_RGB565

/*
 * CAMERA_FS_QQVGA = 4,     //!< 160x120
 * CAMERA_FS_HQVGA = 7,     //!< 240x160
 * CAMERA_FS_QCIF = 6,      //!< 176x144
 * CAMERA_FS_QVGA = 8,      //!< 320x240
 * CAMERA_FS_VGA = 10,      //!< 640x480
 * CAMERA_FS_SVGA = 11,     //!< 800x600
 */
#define CAMERA_FRAME_SIZE CAMERA_FS_QVGA

#define RGB565_MASK_RED        0xF800
#define RGB565_MASK_GREEN      0x07E0
#define RGB565_MASK_BLUE       0x001F

/**
 * @breif call xSemaphoreTake to receive camera frame number
 */
uint8_t queue_receive();

/**
 * @breif call xSemaphoreGive to send camera frame number
 */
void queue_send(uint8_t frame_num);

uint8_t queue_available(void);

void lcd_init_wifi(void);

void lcd_wifi_connect_complete(void);

void lcd_display_top_right();

void lcd_display_top_left();

void lcd_display_bottom_left();

void lcd_display_bottom_right();

void lcd_change_text_size(int size);

void lcd_display_coordinate(int x, int y);

void lcd_display_point(int x, int y);

void clear_screen();

void lcd_tft_init(void);

void xpt2046_touch_init(void);

#endif
