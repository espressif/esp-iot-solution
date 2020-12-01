#ifndef _COM_GUI_LVGL_H
#define _COM_GUI_LVGL_H

#ifdef CONFIG_LVGL_GUI_ENABLE

#ifdef __cplusplus
extern "C"
{
#endif

#include "sdkconfig.h"

#include "lv_conf.h"
#include "lvgl.h"

/* lvgl input device includes */
#include "lvgl_indev_config.h"

/* lvgl display device includes */
#include "lvgl_disp_config.h"

/* lvgl calibration includes */
#include "lv_calibration.h"

/**********************
 * GLOBAL PROTOTYPES
 **********************/

/**
 * @brief Initialize LittlevGL GUI 
 */
void lvgl_init();

#ifdef __cplusplus
}
#endif

#endif /* CONFIG_LVGL_GUI_ENABLE */

#endif /* _COM_GUI_LVGL_H */