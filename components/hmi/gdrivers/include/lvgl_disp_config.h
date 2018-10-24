#ifndef _LVGL_LCD_CONFIG_H
#define _LVGL_LCD_CONFIG_H

/* lvgl include */
#include "iot_lvgl.h"

#ifdef __cplusplus
extern "C"
{
#endif

/**
 * @brief Initialize display
 */
lv_disp_drv_t lvgl_lcd_display_init();

#ifdef __cplusplus
}
#endif

#endif /* _LVGL_LCD_CONFIG_H */