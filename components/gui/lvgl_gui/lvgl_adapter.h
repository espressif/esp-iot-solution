#ifndef _LVGL_LCD_ADAPTER_H
#define _LVGL_LCD_ADAPTER_H

#include "lvgl.h"
#include "esp_err.h"
#include "iot_touch.h"
#include "iot_lcd.h"

#ifdef __cplusplus
extern "C"
{
#endif


/**
 * @brief Initiliaze display device for lvgl
 * @note  This function only registers the dispaly driver into lvgl, and does not initiliaze the display driver.
 *        'init()' function in struct lcd_driver_fun_t must be called before.
 * 
 * @param driver Pointer to display driver
 * @return
 *  - ESP_OK: Success
 *  - ESP_ERR_NO_MEM: Cannot allocate memory
 *  - ESP_ERR_INVALID_ARG: Invalid pointer of driver
 */
esp_err_t lvgl_display_init(lcd_driver_fun_t *driver);

/**
 * @brief Initiliaze touch device for lvgl
 * @note  This function only registers the touch driver into lvgl, and does not initiliaze the touch driver.
 *        'init()' function in struct touch_driver_fun_t must be called before.
 * 
 * @param driver Pointer to display driver
 * @return
 *  - ESP_OK: Success
 *  - ESP_ERR_INVALID_ARG: Invalid pointer of driver
 */
esp_err_t lvgl_indev_init(touch_driver_fun_t *driver);

#ifdef __cplusplus
}
#endif

#endif /* _LVGL_LCD_ADAPTER_H */