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

/* FreeRTOS includes */
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "lvgl_gui.h"

static const char *TAG = "lvgl_gui";

#define ESP_GOTO_ON_FALSE(a, err_code, goto_tag, log_tag, format, ...) do {                                \
        if (unlikely(!(a))) {                                                                              \
            ESP_LOGE(log_tag, "%s(%d): " format, __FUNCTION__, __LINE__ __VA_OPT__(,) __VA_ARGS__);        \
            ret = err_code;                                                                                \
            goto goto_tag;                                                                                 \
        }                                                                                                  \
    } while (0)

/* Creates a semaphore to handle concurrent call to lvgl stuff
 * If you wish to call *any* lvgl function from other threads/tasks
 * you should lock on the very same semaphore! */
static SemaphoreHandle_t xGuiSemaphore = NULL;
static TaskHandle_t g_lvgl_task_handle;

#define LVGL_TICK_MS 5

static scr_driver_t lcd_obj;
static touch_panel_driver_t touch_obj;

static uint16_t g_screen_width = 240;
static uint16_t g_screen_height = 320;

/*Write the internal buffer (VDB) to the display. 'lv_flush_ready()' has to be called when finished*/
static void ex_disp_flush(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *color_map)
{
    lcd_obj.draw_bitmap(area->x1, area->y1, (uint16_t)(area->x2 - area->x1 + 1), (uint16_t)(area->y2 - area->y1 + 1), (uint16_t *)color_map);

    /* IMPORTANT!!!
     * Inform the graphics library that you are ready with the flushing*/
    lv_disp_flush_ready(drv);
}

#define DISP_BUF_SIZE  (g_screen_width * 64)
#define SIZE_TO_PIXEL(v) ((v) / sizeof(lv_color_t))
#define PIXEL_TO_SIZE(v) ((v) * sizeof(lv_color_t))
#define BUFFER_NUMBER (2)

static esp_err_t lvgl_display_init(scr_driver_t *driver)
{
    if (NULL == driver) {
        ESP_LOGE(TAG, "Pointer of lcd driver is invalid");
        return ESP_ERR_INVALID_ARG;
    }

    lcd_obj = *driver;
    scr_info_t info;
    lcd_obj.get_info(&info);
    g_screen_width = info.width;
    g_screen_height = info.height;

    lv_disp_drv_t disp_drv;      /*Descriptor of a display driver*/
    lv_disp_drv_init(&disp_drv); /*Basic initialization*/
    disp_drv.hor_res = g_screen_width;
    disp_drv.ver_res = g_screen_height;

    disp_drv.flush_cb = ex_disp_flush; /*Used in buffered mode (LV_VDB_SIZE != 0  in lv_conf.h)*/

    size_t free_size = heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    size_t remain_size = CONFIG_LVGL_MEM_REMAIN_SIZE * 1024; /**< Remain for other functions */
    size_t alloc_pixel = DISP_BUF_SIZE;
    if (((BUFFER_NUMBER * PIXEL_TO_SIZE(alloc_pixel)) + remain_size) > free_size) {
        if ((remain_size > free_size) || (free_size / 2 > alloc_pixel)) {
            ESP_LOGW(TAG, "Free size = %d Bytes, Initial remain size = %d Bytes", free_size, remain_size);
            // If we can't leave 60k of spare memory, just leave half of whatever is left.
            remain_size = free_size / 2;
            ESP_LOGW(TAG, "Final remain size = %d Bytes", remain_size);
        }
        size_t allow_size = (free_size - remain_size) & 0xfffffffc;
        alloc_pixel = SIZE_TO_PIXEL(allow_size / BUFFER_NUMBER);
        if (alloc_pixel > DISP_BUF_SIZE) {
            // If half the remaining memory is more than we originally planned to allocate, just stick with the original plan.
            alloc_pixel = DISP_BUF_SIZE;
        }
        else {
            ESP_LOGW(TAG, "Exceeded max free size, force shrink to %u Byte", allow_size);
        }
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
    touch_panel_points_t points;
    touch_obj.read_point_data(&points);
    // please be sure that your touch driver every time return old (last clcked) value.
    if (TOUCH_EVT_PRESS == points.event) {
        int32_t x = points.curx[0];
        int32_t y = points.cury[0];
        data->point.x = x;
        data->point.y = y;
        data->state = LV_INDEV_STATE_PR;
    }
    return false;
}

/* Input device interface，Initialize your touchpad */
static esp_err_t lvgl_indev_init(touch_panel_driver_t *driver)
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

static void lv_tick_timercb(void *timer)
{
    lv_tick_inc(LVGL_TICK_MS);
}

static void gui_task(void *args)
{
    ESP_LOGI(TAG, "Start to run LVGL");
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(10));

        /* Try to take the semaphore, call lvgl related function on success */
        if (pdTRUE == xSemaphoreTake(xGuiSemaphore, portMAX_DELAY)) {
            lv_task_handler();
            xSemaphoreGive(xGuiSemaphore);
        }
    }
}

esp_err_t lvgl_init(scr_driver_t *lcd_drv, touch_panel_driver_t *touch_drv)
{
    esp_err_t ret;
    esp_timer_handle_t tick_timer = NULL;

    /* Initialize LittlevGL */
    lv_init();

    /* Display interface */
    ret = lvgl_display_init(lcd_drv);
    ESP_GOTO_ON_FALSE(ESP_OK == ret, ESP_FAIL, err, TAG, "lvgl display initialize failed");

#if defined(CONFIG_LVGL_DRIVER_TOUCH_SCREEN_ENABLE)
    if (NULL != touch_drv) {
        /* Input device interface */
        ret = lvgl_indev_init(touch_drv);
        ESP_GOTO_ON_FALSE(ESP_OK == ret, ESP_FAIL, err, TAG, "lvgl indev initialize failed");
    }
#endif

    esp_timer_create_args_t timer_conf = {
        .callback = lv_tick_timercb,
        .name     = "lv_tick_timer"
    };
    ret = esp_timer_create(&timer_conf, &tick_timer);
    ESP_GOTO_ON_FALSE(ESP_OK == ret, ESP_FAIL, err, TAG, "lvgl tick timer initialize failed");

    xGuiSemaphore = xSemaphoreCreateMutex();
    ESP_GOTO_ON_FALSE(NULL != xGuiSemaphore, ESP_FAIL, err, TAG, "Create mutex for LVGL failed");

#if CONFIG_FREERTOS_UNICORE || CONFIG_LVGL_TASK_CORE_AFFINITY_CPU0
    int err = xTaskCreatePinnedToCore(gui_task, "lv gui", 1024 * CONFIG_LVGL_TASK_MEM_SIZE, NULL, CONFIG_LVGL_TASK_PRIORITY, &g_lvgl_task_handle, 0);
#elif CONFIG_LVGL_TASK_CORE_AFFINITY_CPU1
    int err = xTaskCreatePinnedToCore(gui_task, "lv gui", 1024 * CONFIG_LVGL_TASK_MEM_SIZE, NULL, CONFIG_LVGL_TASK_PRIORITY, &g_lvgl_task_handle, 1);
#elif CONFIG_LVGL_TASK_CORE_AFFINITY_NONE
    int err = xTaskCreatePinnedToCore(gui_task, "lv gui", 1024 * CONFIG_LVGL_TASK_MEM_SIZE, NULL, CONFIG_LVGL_TASK_PRIORITY, &g_lvgl_task_handle, tskNO_AFFINITY);
#endif
    ESP_GOTO_ON_FALSE(pdPASS == err, ESP_FAIL, err, TAG, "Create task for LVGL failed");

    esp_timer_start_periodic(tick_timer, LVGL_TICK_MS * 1000U);
    return ESP_OK;
err:
    if (xGuiSemaphore) {
        vSemaphoreDelete(xGuiSemaphore);
    }
    if (tick_timer) {
        esp_timer_delete(tick_timer);
    }

    return ESP_FAIL;
}

void lvgl_acquire(void)
{
    TaskHandle_t task = xTaskGetCurrentTaskHandle();
    if (g_lvgl_task_handle != task) {
        xSemaphoreTake(xGuiSemaphore, portMAX_DELAY);
    }
}

void lvgl_release(void)
{
    TaskHandle_t task = xTaskGetCurrentTaskHandle();
    if (g_lvgl_task_handle != task) {
        xSemaphoreGive(xGuiSemaphore);
    }
}
