#ifndef _LVGL_INDEV_CONFIG_H
#define _LVGL_INDEV_CONFIG_H

/* lvgl include */
#include "iot_lvgl.h"

#ifdef __cplusplus
extern "C"
{
#endif

/**
 * @brief Initialize input device
 */
lv_indev_drv_t lvgl_indev_init();

#ifdef __cplusplus
}
#endif

#endif /* _LVGL_INDEV_CONFIG_H */
