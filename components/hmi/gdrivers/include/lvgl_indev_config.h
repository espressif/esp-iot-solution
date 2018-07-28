#ifndef _LVGL_INDEV_CONFIG_H
#define _LVGL_INDEV_CONFIG_H

#if CONFIG_LVGL_USE_CUSTOM_DRIVER

/* lvgl include */
#include "iot_lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Input device interface 
   Initialize your touchpad */
void lvgl_indev_init();

#ifdef __cplusplus
}
#endif

#endif

#endif /* _LVGL_INDEV_CONFIG_H */
