// Copyright 2015-2020 Espressif Systems (Shanghai) PTE LTD
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


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "freertos/semphr.h"

#include "lvgl_adapter.h"
#include "sdkconfig.h"

static const char *TAG = "lvgl adapter";

static lcd_driver_fun_t lcd_obj;
static touch_driver_fun_t touch_obj;

/*Write the internal buffer (VDB) to the display. 'lv_flush_ready()' has to be called when finished*/
static void ex_disp_flush(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *color_map)
{
    lcd_obj.draw_bitmap(area->x1, area->y1, (uint16_t)(area->x2 - area->x1 + 1), (uint16_t)(area->y2 - area->y1 + 1), (uint16_t *)color_map);

    /* IMPORTANT!!!
     * Inform the graphics library that you are ready with the flushing*/
    lv_disp_flush_ready(drv);
}

#define DISP_BUF_SIZE  (LV_HOR_RES_MAX * 64)
#define SIZE_TO_PIXEL(v) ((v) / sizeof(lv_color_t))
#define PIXEL_TO_SIZE(v) ((v) * sizeof(lv_color_t))
#define BUFFER_NUMBER (2)

esp_err_t lvgl_display_init(lcd_driver_fun_t *driver)
{
    if (NULL == driver) {
        ESP_LOGE(TAG, "Pointer of lcd driver is invalid");
        return ESP_ERR_INVALID_ARG;
    }

    lcd_obj = *driver;

    lv_disp_drv_t disp_drv;      /*Descriptor of a display driver*/
    lv_disp_drv_init(&disp_drv); /*Basic initialization*/

    disp_drv.flush_cb = ex_disp_flush; /*Used in buffered mode (LV_VDB_SIZE != 0  in lv_conf.h)*/

    size_t free_size = heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    const size_t remain_size = 60 * 1024; /**< Remain for other functions */
    size_t alloc_pixel = DISP_BUF_SIZE;
    if (((BUFFER_NUMBER * PIXEL_TO_SIZE(alloc_pixel)) + remain_size) > free_size) {
        size_t allow_size = (free_size - remain_size) & 0xfffffffc;
        alloc_pixel = SIZE_TO_PIXEL(allow_size / BUFFER_NUMBER);
        ESP_LOGW(TAG, "Exceeded max free size, force shrink to %u Byte", allow_size);
    }

    lv_color_t *buf1 = heap_caps_malloc(PIXEL_TO_SIZE(alloc_pixel), MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    if (NULL == buf1) {
        ESP_LOGE(TAG, "Display buffer memory not enough");
        return ESP_ERR_NO_MEM;
    }
#if (BUFFER_NUMBER == 2)
    lv_color_t *buf2 = heap_caps_malloc(PIXEL_TO_SIZE(alloc_pixel), MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    if (NULL == buf2) {
        heap_caps_free(buf1);
        ESP_LOGE(TAG, "Display buffer memory not enough");
        return ESP_ERR_NO_MEM;
    }
#endif

    ESP_LOGI(TAG, "Alloc memory total size: %u Byte", BUFFER_NUMBER * PIXEL_TO_SIZE(alloc_pixel));

    static lv_disp_buf_t disp_buf;

#if (BUFFER_NUMBER == 2)
    lv_disp_buf_init(&disp_buf, buf1, buf2, alloc_pixel);
#else
    lv_disp_buf_init(&disp_buf, buf1, NULL, alloc_pixel);
#endif

    disp_drv.buffer = &disp_buf;

    /* Finally register the driver */
    lv_disp_drv_register(&disp_drv);
    return ESP_OK;
}

/*Function pointer to read data. Return 'true' if there is still data to be read (buffered)*/
static bool ex_tp_read(struct _lv_indev_drv_t *indev_drv, lv_indev_data_t *data)
{
    data->state = LV_INDEV_STATE_REL;
    // please be sure that your touch driver every time return old (last clcked) value.
    if (touch_obj.is_pressed()) {
        touch_info_t info;
        touch_obj.read_info(&info);
        int32_t x = info.curx[0];
        int32_t y = info.cury[0];
        data->point.x = x;
        data->point.y = y;
        data->state = LV_INDEV_STATE_PR;
    }
    return false;
}

/* Input device interface，Initialize your touchpad */
esp_err_t lvgl_indev_init(touch_driver_fun_t *driver)
{
    if (NULL == driver) {
        ESP_LOGE(TAG, "Pointer of touch driver is invalid");
        return ESP_ERR_INVALID_ARG;
    }

    touch_obj = *driver;

    lv_indev_drv_t indev_drv;               /*Descriptor of an input device driver*/
    lv_indev_drv_init(&indev_drv);          /*Basic initialization*/

    indev_drv.type = LV_INDEV_TYPE_POINTER; /*The touchpad is pointer type device*/
    indev_drv.read_cb = ex_tp_read;            /*Library ready your touchpad via this function*/

    lv_indev_drv_register(&indev_drv);      /*Finally register the driver*/
    return ESP_OK;
}