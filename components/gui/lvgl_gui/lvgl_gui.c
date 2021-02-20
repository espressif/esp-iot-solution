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
#include "freertos/timers.h"
#include "freertos/semphr.h"
#include "esp_log.h"
/* LVGL includes */
#include "lvgl_gui.h"

static const char *TAG = "lvgl_gui";

typedef struct {
    scr_driver_t *lcd_drv;
    touch_panel_driver_t *touch_drv;
} lvgl_drv_t;

// wait for execute lv_task_handler and lv_tick_inc to avoid some widget don't refresh.
#define LVGL_TICK_MS 1

static void lv_tick_timercb(void *timer)
{
    /* Initialize a Timer for 1 ms period and
     * in its interrupt call
     * lv_tick_inc(1); */
    lv_tick_inc(LVGL_TICK_MS);
}

/* Creates a semaphore to handle concurrent call to lvgl stuff
 * If you wish to call *any* lvgl function from other threads/tasks
 * you should lock on the very same semaphore! */
static SemaphoreHandle_t xGuiSemaphore = NULL;
static void gui_task(void *args)
{
    esp_err_t ret;
    lvgl_drv_t *lvgl_driver = (lvgl_drv_t *)args;

    /* Initialize LittlevGL */
    lv_init();

    /* Display interface */
    ret = lvgl_display_init(lvgl_driver->lcd_drv);
    if (ESP_OK != ret) {
        ESP_LOGE(TAG, "lvgl display initialize failed");
        // lv_deinit(); /**< lvgl should be deinit here, but it seems lvgl doesn't support it */
        vTaskDelete(NULL);
    }

#if defined(CONFIG_LVGL_DRIVER_TOUCH_SCREEN_ENABLE)
    if (NULL != lvgl_driver->touch_drv) {
        /* Input device interface */
        ret = lvgl_indev_init(lvgl_driver->touch_drv);
        if (ESP_OK != ret) {
            ESP_LOGE(TAG, "lvgl indev initialize failed");
            // lv_deinit(); /**< lvgl should be deinit here, but it seems lvgl doesn't support it */
            vTaskDelete(NULL);
        }
    }
#endif

    esp_timer_create_args_t timer_conf = {
        .callback = lv_tick_timercb,
        .name     = "lv_tick_timer"
    };
    esp_timer_handle_t g_wifi_connect_timer = NULL;
    esp_timer_create(&timer_conf, &g_wifi_connect_timer);
    esp_timer_start_periodic(g_wifi_connect_timer, LVGL_TICK_MS * 1000U);

    xGuiSemaphore = xSemaphoreCreateMutex();
    ESP_LOGI(TAG, "Start to run LVGL");

    while (1) {
        /* Delay 1 tick (assumes FreeRTOS tick is 10ms */
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
    /* If you want to use a task to create the graphic, you NEED to create a Pinned task
      * Otherwise there can be problem such as memory corruption and so on.
      * NOTE: When not using Wi-Fi nor Bluetooth you can pin the gui_task to core 0 */
    esp_chip_info_t chip_info;
    esp_chip_info(&chip_info);
    ESP_LOGI(TAG, "GUI Run at %s Pinned to Core%d", CONFIG_IDF_TARGET, chip_info.cores - 1);

    static lvgl_drv_t lvgl_driver;
    lvgl_driver.lcd_drv = lcd_drv;
    lvgl_driver.touch_drv = touch_drv;

    xTaskCreatePinnedToCore(gui_task, "lv gui", 1024 * 8, &lvgl_driver, 5, NULL, chip_info.cores - 1);

    uint16_t timeout = 20;
    while (NULL == xGuiSemaphore) {
        vTaskDelay(pdMS_TO_TICKS(100));
        timeout--;
        if (0 == timeout) {
            ESP_LOGW(TAG, "GUI Task Start Timeout");
            return ESP_ERR_TIMEOUT;
        }
    }
    return ESP_OK;
}