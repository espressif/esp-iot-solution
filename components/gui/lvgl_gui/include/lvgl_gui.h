#ifndef _COM_GUI_LVGL_H
#define _COM_GUI_LVGL_H

#include "lv_conf.h"
#include "lvgl.h"
#include "touch_panel.h"
#include "screen_driver.h"

#ifdef __cplusplus
extern "C"
{
#endif

/**
 * @brief Initialize lvgl and register driver to lvgl
 * 
 * @note  This function will create a task to run the lvgl handler. The task will always Pinned on the APP_CPU
 * 
 * @param lcd_drv Pointer of lcd driver
 * @param touch_drv  Pointer of touch driver. If you don't have a touch panel, set it to NULL
 * 
 * @return
 *     - ESP_OK Success
 *     - ESP_ERR_TIMEOUT Operation timeout
 */
esp_err_t lvgl_init(scr_driver_t *lcd_drv, touch_panel_driver_t *touch_drv);

/**
 * @brief Acquire lock for LVGL
 * 
 * @note If you wish to call any lvgl function from other threads/tasks you should lock on the very same semaphore
 * 
 */
void lvgl_acquire(void);

/**
 * @brief Release lock for LVGL
 * 
 */
void lvgl_release(void);

#ifdef __cplusplus
}
#endif

#endif /* _COM_GUI_LVGL_H */
