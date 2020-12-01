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

#ifndef _IOT_APP_TOUCH_TFT_H_
#define _IOT_APP_TOUCH_TFT_H_

#include "iot_xpt2046.h"

void lcd_display_top_right();

void lcd_display_top_left();

void lcd_display_bottom_left();

void lcd_display_bottom_right();

void lcd_change_text_size(int size);

void lcd_display_coordinate(int x, int y);

void lcd_display_point(int x, int y);

void clear_screen();

void lcd_tft_init(void);

position get_screen_position(position pos);

void xpt2046_touch_init(void);

#endif
