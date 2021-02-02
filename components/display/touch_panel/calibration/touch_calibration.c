// Copyright 2020 Espressif Systems (Shanghai) Co. Ltd.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
#include "esp_log.h"
#include "math.h"
#include "param_save.h"
#include "nvs_flash.h"
#include "screen_driver.h"
#include "basic_painter.h"

static const char* TAG = "Touch calibration";

#define CALIBRATION_CHECK(a, str, ret)  if(!(a)) {                                   \
        ESP_LOGE(TAG,"%s:%d (%s):%s", __FILE__, __LINE__, __FUNCTION__, str);   \
        return (ret);                                                           \
    }

#define GMOUSE_FINGER_CALIBRATE_ERROR   CONFIG_TOUCH_PANEL_MAX_CALIBRATE_ERROR
#define TOUCH_CAL_VAL_NAMESPACE "DefLvglParam"
#define TOUCH_CAL_VAL_KEY "DefTouchCalVal"

typedef struct{
    int32_t x;
    int32_t y;
}point_t;

typedef struct {
    float ax;
    float bx;
    float cx;
    float ay;
    float by;
    float cy;
}Calibration_t;

static Calibration_t g_caldata;
static bool g_calibrated = false;
static scr_driver_t g_lcd;
static int (*g_touch_is_pressed)(void) = NULL;
static esp_err_t (*g_touch_read_rawdata)(uint16_t *x, uint16_t *y) = NULL;

static esp_err_t touch_save_calibration(const void *buf, size_t sz)
{
    esp_err_t res = ESP_FAIL;
    res = param_save((const char *)TOUCH_CAL_VAL_NAMESPACE, (const char *)TOUCH_CAL_VAL_KEY, (void *)buf, sz);
    return res;
}

static esp_err_t touch_load_calibration(void *buf)
{
    esp_err_t res = ESP_FAIL;
    res = param_load((const char *)TOUCH_CAL_VAL_NAMESPACE, (const char *)TOUCH_CAL_VAL_KEY, (void *)buf);
    return res;
}

static void _draw_touch_Point(uint16_t x, uint16_t y, uint16_t color)
{
    painter_draw_line(x - 14, y, x + 15, y, color); //horizontal line
    painter_draw_line(x, y - 14, x, y + 15, color); //vertical line
    g_lcd.draw_pixel(x + 1, y + 1, color);
    g_lcd.draw_pixel(x - 1, y + 1, color);
    g_lcd.draw_pixel(x + 1, y - 1, color);
    g_lcd.draw_pixel(x - 1, y - 1, color);
    painter_draw_circle(x, y, 8, color); //circular
}

static void _get_point(int8_t index, point_t *cross, point_t *point)
{
    uint32_t time_out = 10000; // ms

    if (0 != index) {
        _draw_touch_Point(cross[index-1].x, cross[index-1].y, COLOR_WHITE);
    }
    _draw_touch_Point(cross[index].x, cross[index].y, COLOR_RED);

    vTaskDelay(1000 / portTICK_PERIOD_MS);
    while (!g_touch_is_pressed()) {
        vTaskDelay(50 / portTICK_RATE_MS);
        time_out -= 50;
        if (0 == time_out)
        {
            point->x = point->y = 0;
            /** TODO: How to deal with it when the user does not touch the screen for a long time
             */
            // break;
        }
        
    };
    uint16_t tx, ty;
    g_touch_read_rawdata(&tx, &ty);
    point->x = tx;
    point->y = ty;
    ESP_LOGI(TAG, "[%d] X:%d Y:%d", index, point->x, point->y);
}

/**
 * @brief transform raw data of touch panel to pixel coordinate value of screen
 * 
 * @param x Value of X axis direction
 * @param y Value of Y axis direction
 * @return esp_err_t 
 */
esp_err_t touch_calibration_transform(int32_t *x, int32_t *y)
{
    CALIBRATION_CHECK((NULL != x) && (NULL != y), "Coordinate pointer invalid", ESP_ERR_INVALID_ARG);
    CALIBRATION_CHECK(true == g_calibrated, "Touch is uncalibrated", ESP_ERR_INVALID_STATE);

    int32_t _x, _y;
    _x = (int32_t)(g_caldata.ax * (*x) + g_caldata.bx * (*y) + g_caldata.cx);
    _y = (int32_t)(g_caldata.ay * (*x) + g_caldata.by * (*y) + g_caldata.cy);
    (*x) = _x >= 0 ? _x : 0;
    (*y) = _y >= 0 ? _y : 0;

    return ESP_OK;
}

/**
 *  Algorithm reference https://d1.ourdev.cn/bbs_upload782111/files_13/ourdev_427934.pdf
 */
static void calibration_calculate(const point_t *cross, const point_t *points)
{
    int32_t c0, c1, c2;

    // Work on x values
    c0 = cross[0].x;
    c1 = cross[1].x;
    c2 = cross[2].x;


    /* Compute all the required determinants */
    float dx;
    
    dx = (float)(points[0].x - points[2].x) * (float)(points[1].y - points[2].y) 
        - (float)(points[1].x - points[2].x) * (float)(points[0].y - points[2].y);

    g_caldata.ax = ((float)(c0 - c2) * (float)(points[1].y - points[2].y) 
                - (float)(c1 - c2) * (float)(points[0].y - points[2].y)) / dx;
    g_caldata.bx = ((float)(c1 - c2) * (float)(points[0].x - points[2].x) 
                - (float)(c0 - c2) * (float)(points[1].x - points[2].x)) / dx;
    g_caldata.cx = (c0 * ((float)points[1].x * (float)points[2].y - (float)points[2].x * (float)points[1].y) 
                - c1 * ((float)points[0].x * (float)points[2].y - (float)points[2].x * (float)points[0].y) 
                + c2 * ((float)points[0].x * (float)points[1].y - (float)points[1].x * (float)points[0].y)) / dx;

  
    // Work on y values
    c0 = cross[0].y;
    c1 = cross[1].y;
    c2 = cross[2].y;
 

    g_caldata.ay = ((float)(c0 - c2) * (float)(points[1].y - points[2].y) 
                - (float)(c1 - c2) * (float)(points[0].y - points[2].y)) / dx;
    g_caldata.by = ((float)(c1 - c2) * (float)(points[0].x - points[2].x) 
                - (float)(c0 - c2) * (float)(points[1].x - points[2].x)) / dx;
    g_caldata.cy = (c0 * ((float)points[1].x * (float)points[2].y - (float)points[2].x * (float)points[1].y) 
                - c1 * ((float)points[0].x * (float)points[2].y - (float)points[2].x * (float)points[0].y) 
                + c2 * ((float)points[0].x * (float)points[1].y - (float)points[1].x * (float)points[0].y)) / dx;
}

static void show_prompt_with_dir(uint16_t x, uint16_t y, const char *str, const font_t *font, uint16_t color, scr_dir_t dir)
{
    g_lcd.set_direction(dir);
    uint16_t c = painter_get_point_color();
    painter_set_point_color(color);
    painter_draw_string(x, y, str, font, color);
    painter_set_point_color(c);
    /** Aftershow prompt, Set screen to original direction */
    g_lcd.set_direction(SCR_DIR_LRTB);
}

/**
 * @brief Start run touch panel calibration
 * 
 * @param screen LCD driver for display prompts
 * @param func_is_pressed pointer of function
 * @param func_read_rawdata pointer of function
 * @param recalibrate Is calibration mandatory
 * @return esp_err_t 
 */
esp_err_t touch_calibration_run(const scr_driver_t *screen,
                                int (*func_is_pressed)(void),
                                esp_err_t (*func_read_rawdata)(uint16_t *x, uint16_t *y),
                                bool recalibrate)
{
    CALIBRATION_CHECK(NULL != screen, "Screen driver invalid", ESP_ERR_INVALID_ARG);
    CALIBRATION_CHECK(NULL != func_is_pressed, "Func pointer func_is_pressed invalid", ESP_ERR_INVALID_ARG);
    CALIBRATION_CHECK(NULL != func_read_rawdata, "Func pointer func_read_rawdata invalid", ESP_ERR_INVALID_ARG);
    
    esp_err_t ret = ESP_OK;

    g_touch_is_pressed = func_is_pressed;
    g_touch_read_rawdata = func_read_rawdata;
    g_lcd = *screen;
    ret = painter_init(&g_lcd);
    CALIBRATION_CHECK(ESP_OK == ret, "Initial failed", ESP_FAIL);

    ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        // NVS partition was truncated and needs to be erased
        // Retry nvs_flash_init
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK( ret );
    vTaskDelay(100 / portTICK_PERIOD_MS);   //Wait until nvs is stable， otherwise will cause exception

    if ((false == recalibrate) && (ESP_OK == touch_load_calibration(&g_caldata))) {
        g_calibrated = true;
        return ESP_OK;
    }

    /**
     * Restore screen display rotate to original for corresponding raw data of touch panel. 
     * So read current direction of screen and backup it.
     */
    scr_info_t lcd_info;
    g_lcd.get_info(&lcd_info);
    scr_dir_t old_dir = lcd_info.dir;

    uint8_t index = 0;
    uint32_t w = lcd_info.width;
    uint32_t h = lcd_info.height;
    point_t cross[4];
    point_t points[4];
    uint32_t calibrate_error = 100;

    /**
     * |----------------------|
     * |  0                 1 |
     * |                      |
     * |           3          |
     * |                      |
     * |                    2 |
     * |----------------------|
     */
    cross[0].x = w / 6;
    cross[0].y = h / 6;
    cross[1].x = w - w / 6;
    cross[1].y = h / 6;
    cross[2].x = w - w / 6;
    cross[2].y = h - h / 6;
    cross[3].x = w / 2;
    cross[3].y = h / 2;

    while (calibrate_error)
    {
        painter_clear(COLOR_WHITE);
        show_prompt_with_dir(0, 0, "Please press the center of the circle in turn", &Font12, COLOR_BLUE, old_dir);

        _get_point(index, cross, &points[index]);
        index++;
        _get_point(index, cross, &points[index]);
        index++;
        _get_point(index, cross, &points[index]);
        index++;
        _get_point(index, cross, &points[index]);
        index++;

        // Apply 3 point calibration algorithm
        calibration_calculate(cross, points);

        g_calibrated = true;
        // Converted to screen pixel coordinates to test for correct calibration
        touch_calibration_transform(&points[3].x, &points[3].y);
        // Is this accurate enough?
        calibrate_error = (points[3].x - cross[3].x) * (points[3].x - cross[3].x) 
            + (points[3].y - cross[3].y) * (points[3].y - cross[3].y);
        if (calibrate_error > (uint32_t)GMOUSE_FINGER_CALIBRATE_ERROR * (uint32_t)GMOUSE_FINGER_CALIBRATE_ERROR) {
            show_prompt_with_dir(10, h/2, "Calibration Failed!", &Font16, COLOR_RED, old_dir);
            ESP_LOGW(TAG, "Touch Calibration failed! Error=%d", calibrate_error);
            g_calibrated = false;
            index = 0;
            vTaskDelay(2000 / portTICK_PERIOD_MS);
            ESP_LOGW(TAG, "Retry to calibration");
        } else {
            calibrate_error = 0;
            ESP_LOGI(TAG, "/ XL = (%f)X + (%f)Y + (%f)", g_caldata.ax, g_caldata.bx, g_caldata.cx);
            ESP_LOGI(TAG, "\\ YL = (%f)X + (%f)Y + (%f)", g_caldata.ay, g_caldata.by, g_caldata.cy);
            show_prompt_with_dir(30, h/2, "Successful", &Font16, COLOR_BLUE, old_dir);
            touch_save_calibration(&g_caldata, sizeof(Calibration_t));
            vTaskDelay(2000 / portTICK_PERIOD_MS);
        }
    }
    ESP_LOGI(TAG, "Touch Calibration successful");
    painter_clear(COLOR_WHITE);
    g_lcd.set_direction(old_dir);
    return ESP_OK;
}
