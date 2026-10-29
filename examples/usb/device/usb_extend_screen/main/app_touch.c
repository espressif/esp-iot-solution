/*
 * SPDX-FileCopyrightText: 2024-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <sys/param.h>

#include "esp_log.h"
#include "esp_check.h"
#include "esp_board_manager_includes.h"
#include "esp_lcd_touch.h"
#include "app_lcd.h"
#include "app_usb.h"
#include "usb_descriptors.h"

static const char *TAG = "app_touch";

static esp_lcd_touch_handle_t tp = NULL;
static uint16_t s_touch_width;
static uint16_t s_touch_height;
static uint16_t s_display_width;
static uint16_t s_display_height;

static uint16_t app_touch_scale(uint16_t coordinate, uint16_t source_max, uint16_t target_max)
{
    if (source_max == 0 || source_max == target_max) {
        return MIN(coordinate, target_max);
    }
    return MIN((uint32_t)coordinate * target_max / source_max, target_max);
}

static void app_touch_task(void *arg)
{
    uint8_t touchpad_cnt = 0;
    bool send_press = false;
    while (1) {
        if (esp_lcd_touch_read_data(tp) != ESP_OK) {
            vTaskDelay(pdMS_TO_TICKS(20));
            continue;
        }
        esp_lcd_touch_point_data_t touch_points[CONFIG_ESP_LCD_TOUCH_MAX_POINTS] = {0};
        if (esp_lcd_touch_get_data(tp, touch_points, &touchpad_cnt,
                                   CONFIG_ESP_LCD_TOUCH_MAX_POINTS) != ESP_OK) {
            vTaskDelay(pdMS_TO_TICKS(20));
            continue;
        }
        hid_report_t report = {0};
        if (touchpad_cnt > 0) {
            report.report_id = REPORT_ID_TOUCH;
            int i = 0;
            for (i = 0; i < touchpad_cnt; i++) {
                report.touch_report.data[i].index = touch_points[i].track_id;
                report.touch_report.data[i].press_down = 1;
                report.touch_report.data[i].x = app_touch_scale(touch_points[i].x, s_touch_width,
                                                                s_display_width);
                report.touch_report.data[i].y = app_touch_scale(touch_points[i].y, s_touch_height,
                                                                s_display_height);
                report.touch_report.data[i].width = touch_points[i].strength;
                report.touch_report.data[i].height = touch_points[i].strength;
                /*!< >= LOG_LEVEL_DEBUG */
#if CONFIG_LOG_DEFAULT_LEVEL >= 4
                /*!< For debug */
                printf("(%d: %d, %d. %d) ", touch_points[i].track_id, touch_points[i].x, touch_points[i].y, touch_points[i].strength);
#endif
            }
#if CONFIG_LOG_DEFAULT_LEVEL >= 4
            /*!< For debug */
            printf("\n");
#endif
            ESP_LOGD(TAG, "touchpad cnt: %d\n", touchpad_cnt);
            report.touch_report.cnt = touchpad_cnt;
#if CFG_TUD_HID
            tinyusb_hid_keyboard_report(report);
#endif
            send_press = true;
        } else if (send_press) {
            send_press = false;
            report.report_id = REPORT_ID_TOUCH;
#if CFG_TUD_HID
            tinyusb_hid_keyboard_report(report);
#endif
            ESP_LOGD(TAG, "send release %d", touchpad_cnt);
        }
        // Reading from the GT911 at a time shorter than this may result in false reports.
        vTaskDelay(20 / portTICK_PERIOD_MS);
    }
}

esp_err_t app_touch_init(void)
{
#if !CONFIG_ESP_BOARD_DEV_LCD_TOUCH_SUPPORT
    ESP_LOGE(TAG, "Selected Board Manager board has no lcd_touch device");
    return ESP_ERR_NOT_SUPPORTED;
#else
    ESP_RETURN_ON_FALSE(esp_board_manager_check_name(ESP_BOARD_DEVICE_NAME_LCD_TOUCH),
                        ESP_ERR_NOT_FOUND, TAG, "selected board has no lcd_touch device");
    ESP_RETURN_ON_ERROR(esp_board_manager_init_device_by_name(ESP_BOARD_DEVICE_NAME_LCD_TOUCH),
                        TAG, "initialize lcd_touch failed");

    dev_lcd_touch_handles_t *touch = NULL;
    ESP_RETURN_ON_ERROR(esp_board_manager_get_device_handle(ESP_BOARD_DEVICE_NAME_LCD_TOUCH,
                                                            (void **)&touch),
                        TAG, "get lcd_touch handle failed");
    ESP_RETURN_ON_FALSE(touch && touch->touch_handle, ESP_ERR_INVALID_STATE, TAG,
                        "lcd_touch returned an invalid handle");
    tp = touch->touch_handle;

    dev_lcd_touch_config_t *touch_config = NULL;
    ESP_RETURN_ON_ERROR(esp_board_manager_get_device_config(ESP_BOARD_DEVICE_NAME_LCD_TOUCH,
                                                            (void **)&touch_config),
                        TAG, "get lcd_touch config failed");
    s_touch_width = touch_config->touch_config.x_max;
    s_touch_height = touch_config->touch_config.y_max;
    ESP_RETURN_ON_ERROR(app_lcd_get_resolution(&s_display_width, &s_display_height),
                        TAG, "get display resolution failed");

    BaseType_t task_created = xTaskCreate(app_touch_task, "app_touch_task", 4096, NULL,
                                          CONFIG_TOUCH_TASK_PRIORITY, NULL);
    ESP_RETURN_ON_FALSE(task_created == pdPASS, ESP_ERR_NO_MEM, TAG, "create touch task failed");
    return ESP_OK;
#endif
}
