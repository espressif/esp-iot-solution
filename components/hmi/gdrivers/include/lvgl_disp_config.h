#ifndef _LVGL_LCD_CONFIG_H
#define _LVGL_LCD_CONFIG_H

#define LCD_COLORSPACE_R5G6B5
#define TYPE_UCOLOR r5g6b5_t

#if CONFIG_LVGL_USE_CUSTOM_DRIVER

/* lvgl include */
#include "iot_lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif
	
/* Display interface 
   Initialize your display*/
void lvgl_lcd_display_init();

#ifdef __cplusplus
}
#endif

#endif

#endif /* _LVGL_LCD_CONFIG_H */