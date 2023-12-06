/*
 * SPDX-FileCopyrightText: 2022-2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "esp_log.h"
#include "xpt2046.h"
#include "ft5x06.h"

static const char *TAG = "touch panel driver";

#define TOUCH_CHECK(a, str, ret)  if(!(a)) {                                      \
        ESP_LOGE(TAG,"%s:%d (%s):%s", __FILE__, __LINE__, __FUNCTION__, str);   \
        return (ret);                                                                   \
    }

#ifdef CONFIG_TOUCH_DRIVER_XPT2046
extern touch_panel_driver_t xpt2046_default_driver;
#endif
#ifdef CONFIG_TOUCH_DRIVER_FT5X06
extern touch_panel_driver_t ft5x06_default_driver;
#endif
#ifdef CONFIG_TOUCH_DRIVER_NS2016
extern touch_panel_driver_t ns2016_default_driver;
#endif

esp_err_t touch_panel_find_driver(touch_panel_controller_t controller, touch_panel_driver_t *out_driver)
{
    TOUCH_CHECK(NULL != out_driver, "Pointer of Touch driver is invalid", ESP_ERR_INVALID_ARG);

    esp_err_t ret = ESP_OK;
    switch (controller) {
#ifdef CONFIG_TOUCH_DRIVER_FT5X06
    case TOUCH_PANEL_CONTROLLER_FT5X06:
        *out_driver = ft5x06_default_driver;
        break;
#endif
#ifdef CONFIG_TOUCH_DRIVER_XPT2046
    case TOUCH_PANEL_CONTROLLER_XPT2046:
        *out_driver = xpt2046_default_driver;
        break;
#endif
#ifdef CONFIG_TOUCH_DRIVER_NS2016
    case TOUCH_PANEL_CONTROLLER_NS2016:
        *out_driver = ns2016_default_driver;
        break;
#endif
    default:
        ESP_LOGE(TAG, "Not support touch panel controller, it may not be enabled in menuconfig");
        ret = ESP_ERR_NOT_FOUND;
        break;
    }

    return ret;
}
