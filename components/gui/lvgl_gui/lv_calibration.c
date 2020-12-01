// Copyright 2015-2018 Espressif Systems (Shanghai) PTE LTD
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

/* component Include */
#include <stdio.h>

/* freertos includes */
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"
#include "freertos/event_groups.h"
#include "esp_freertos_hooks.h"

/*sdkconfig Include*/
#include "sdkconfig.h"

/* lvgl includes */
#include "iot_lvgl.h"

/* lvgl calibration includes */
#include "lv_calibration.h"

/* Param Include */
#include "iot_param.h"
#include "nvs_flash.h"

/**********************
 *      MACROS
 **********************/
#if CONFIG_LVGL_LCD_DRIVER_FRAMEBUFFER_MODE
#define TOUCH_CAL_VAL_NAMESPACE "FbLvglParam"
#define TOUCH_CAL_VAL_KEY "FbTouchCalVal"
#elif CONFIG_LVGL_LCD_DRIVER_API_MODE
#define TOUCH_CAL_VAL_NAMESPACE "APILvglParam"
#define TOUCH_CAL_VAL_KEY "APITouchCalVal"
#else
#define TOUCH_CAL_VAL_NAMESPACE "DefLvglParam"
#define TOUCH_CAL_VAL_KEY "DefTouchCalVal"
#endif

#define CALIBRATION_POLL_PERIOD         20      // milliseconds
#define CALIBRATION_MINPRESS_PERIOD     300     // milliseconds
#define CALIBRATION_MAXPRESS_PERIOD     5000    // milliseconds
#define CALIBRATION_ERROR_DELAY			3000    // milliseconds

#define CALIBRATION_CROSS_RADIUS        15
#define CALIBRATION_CROSS_INNERGAP      2

#define GMOUSE_VFLG_CAL_EXTREMES        false   // Use edge to edge calibration
#define GMOUSE_VFLG_CAL_TEST            true    // Test the results of the calibration

// Resolution and Accuracy Settings
#define GMOUSE_PEN_CALIBRATE_ERROR      8       // Maximum error for a calibration to succeed
#define GMOUSE_PEN_CLICK_ERROR          6       // Movement allowed without discarding the CLICK or CLICKCXT event
#define GMOUSE_PEN_MOVE_ERROR           4       // Movement allowed without discarding the MOVE event

#define GMOUSE_FINGER_CALIBRATE_ERROR   14      // Maximum error for a calibration to succeed
#define GMOUSE_FINGER_CLICK_ERROR       18      // Movement allowed without discarding the CLICK or CLICKCXT event
#define GMOUSE_FINGER_MOVE_ERROR        14      // Movement allowed without discarding the MOVE event

/**********************
 *      TYPEDEFS
 **********************/
typedef struct GMouseCalibration {
    float ax;
    float bx;
    float cx;
    float ay;
    float by;
    float cy;
} GMouseCalibration;

/**********************
 *  STATIC VARIABLES
 **********************/
static lv_obj_t *line[12];
static lv_point_t p[24];
static GMouseCalibration caldata;
static bool calibrated = false;

/**********************
 *  STATIC FUNCTIONS
 **********************/
static bool touch_save_calibration(const void *buf, size_t sz)
{
    esp_err_t res = ESP_FAIL;
    res = iot_param_save((const char *)TOUCH_CAL_VAL_NAMESPACE, (const char *)TOUCH_CAL_VAL_KEY, (void *)buf, sz);
    return (res == ESP_OK);
}

static bool touch_load_calibration(void *buf)
{
    esp_err_t res = ESP_FAIL;
    res = iot_param_load((const char *)TOUCH_CAL_VAL_NAMESPACE, (const char *)TOUCH_CAL_VAL_KEY, (void *)buf);
    return (res == ESP_OK);
}

static bool touch_erase_calibration()
{
    esp_err_t res = ESP_FAIL;
    res = iot_param_erase((const char *)TOUCH_CAL_VAL_NAMESPACE, (const char *)TOUCH_CAL_VAL_KEY);
    return (res == ESP_OK);
}

static void CalibrationCrossDraw(const lv_point_t *pp)
{
    static lv_style_t style1;
    lv_style_copy(&style1, &lv_style_transp);

    p[0].x = pp->x - CALIBRATION_CROSS_RADIUS;
    p[0].y = pp->y;
    p[1].x = pp->x - CALIBRATION_CROSS_INNERGAP;
    p[1].y = pp->y;
    line[0] = lv_line_create(lv_scr_act(), NULL);
    lv_line_set_style(line[0], &style1);
    lv_obj_set_pos(line[0], 0, 0);
    lv_line_set_points(line[0], &p[0], 2);

    p[2].x = pp->x + CALIBRATION_CROSS_INNERGAP;
    p[2].y = pp->y;
    p[3].x = pp->x + CALIBRATION_CROSS_RADIUS;
    p[3].y = pp->y;
    line[1] = lv_line_create(lv_scr_act(), NULL);
    lv_line_set_style(line[1], &style1);
    lv_obj_set_pos(line[1], 0, 0);
    lv_line_set_points(line[1], &p[2], 2);

    p[4].x = pp->x;
    p[4].y = pp->y - CALIBRATION_CROSS_RADIUS;
    p[5].x = pp->x;
    p[5].y = pp->y - CALIBRATION_CROSS_INNERGAP;
    line[2] = lv_line_create(lv_scr_act(), NULL);
    lv_line_set_style(line[2], &style1);
    lv_obj_set_pos(line[2], 0, 0);
    lv_line_set_points(line[2], &p[4], 2);

    p[6].x = pp->x;
    p[6].y = pp->y + CALIBRATION_CROSS_INNERGAP;
    p[7].x = pp->x;
    p[7].y = pp->y + CALIBRATION_CROSS_RADIUS;
    line[3] = lv_line_create(lv_scr_act(), NULL);
    lv_line_set_style(line[3], &style1);
    lv_obj_set_pos(line[3], 0, 0);
    lv_line_set_points(line[3], &p[6], 2);

    p[8].x = pp->x - CALIBRATION_CROSS_RADIUS;
    p[8].y = pp->y + CALIBRATION_CROSS_RADIUS;
    p[9].x = pp->x - CALIBRATION_CROSS_RADIUS / 2;
    p[9].y = pp->y + CALIBRATION_CROSS_RADIUS;
    line[4] = lv_line_create(lv_scr_act(), NULL);
    lv_line_set_style(line[4], &style1);
    lv_obj_set_pos(line[4], 0, 0);
    lv_line_set_points(line[4], &p[8], 2);

    p[10].x = pp->x - CALIBRATION_CROSS_RADIUS;
    p[10].y = pp->y + CALIBRATION_CROSS_RADIUS / 2;
    p[11].x = pp->x - CALIBRATION_CROSS_RADIUS;
    p[11].y = pp->y + CALIBRATION_CROSS_RADIUS;
    line[5] = lv_line_create(lv_scr_act(), NULL);
    lv_line_set_style(line[5], &style1);
    lv_obj_set_pos(line[5], 0, 0);
    lv_line_set_points(line[5], &p[10], 2);

    p[12].x = pp->x - CALIBRATION_CROSS_RADIUS;
    p[12].y = pp->y - CALIBRATION_CROSS_RADIUS;
    p[13].x = pp->x - CALIBRATION_CROSS_RADIUS / 2;
    p[13].y = pp->y - CALIBRATION_CROSS_RADIUS;
    line[6] = lv_line_create(lv_scr_act(), NULL);
    lv_line_set_style(line[6], &style1);
    lv_obj_set_pos(line[6], 0, 0);
    lv_line_set_points(line[6], &p[12], 2);

    p[14].x = pp->x - CALIBRATION_CROSS_RADIUS;
    p[14].y = pp->y - CALIBRATION_CROSS_RADIUS / 2;
    p[15].x = pp->x - CALIBRATION_CROSS_RADIUS;
    p[15].y = pp->y - CALIBRATION_CROSS_RADIUS;
    line[7] = lv_line_create(lv_scr_act(), NULL);
    lv_line_set_style(line[7], &style1);
    lv_obj_set_pos(line[7], 0, 0);
    lv_line_set_points(line[7], &p[14], 2);

    p[16].x = pp->x + CALIBRATION_CROSS_RADIUS / 2;
    p[16].y = pp->y + CALIBRATION_CROSS_RADIUS;
    p[17].x = pp->x + CALIBRATION_CROSS_RADIUS;
    p[17].y = pp->y + CALIBRATION_CROSS_RADIUS;
    line[8] = lv_line_create(lv_scr_act(), NULL);
    lv_line_set_style(line[8], &style1);
    lv_obj_set_pos(line[8], 0, 0);
    lv_line_set_points(line[8], &p[16], 2);

    p[18].x = pp->x + CALIBRATION_CROSS_RADIUS;
    p[18].y = pp->y + CALIBRATION_CROSS_RADIUS / 2;
    p[19].x = pp->x + CALIBRATION_CROSS_RADIUS;
    p[19].y = pp->y + CALIBRATION_CROSS_RADIUS;
    line[9] = lv_line_create(lv_scr_act(), NULL);
    lv_line_set_style(line[9], &style1);
    lv_obj_set_pos(line[9], 0, 0);
    lv_line_set_points(line[9], &p[18], 2);

    p[20].x = pp->x + CALIBRATION_CROSS_RADIUS / 2;
    p[20].y = pp->y - CALIBRATION_CROSS_RADIUS;
    p[21].x = pp->x + CALIBRATION_CROSS_RADIUS;
    p[21].y = pp->y - CALIBRATION_CROSS_RADIUS;
    line[10] = lv_line_create(lv_scr_act(), NULL);
    lv_line_set_style(line[10], &style1);
    lv_obj_set_pos(line[10], 0, 0);
    lv_line_set_points(line[10], &p[20], 2);

    p[22].x = pp->x + CALIBRATION_CROSS_RADIUS;
    p[22].y = pp->y - CALIBRATION_CROSS_RADIUS;
    p[23].x = pp->x + CALIBRATION_CROSS_RADIUS;
    p[23].y = pp->y - CALIBRATION_CROSS_RADIUS / 2;
    line[11] = lv_line_create(lv_scr_act(), NULL);
    lv_line_set_style(line[11], &style1);
    lv_obj_set_pos(line[11], 0, 0);
    lv_line_set_points(line[11], &p[22], 2);
}

static void CalibrationCrossClear(const lv_point_t *pp)
{
    for (uint8_t i = 0; i < 12; i++) {
        lv_obj_del(line[i]);
    }
}

static void CalibrationCalculate(const lv_point_t *cross, const lv_point_t *points)
{
    lv_coord_t c0, c1, c2;

    /* Convert all cross points back to GDISP_ROTATE_0 convention
     * before calculating the calibration matrix.
     */
    #ifdef CONFIG_LVGL_DISP_ROTATE_0
        // Work on x values
        c0 = cross[0].x;
        c1 = cross[1].x;
        c2 = cross[2].x;
    #elif defined(CONFIG_LVGL_DISP_ROTATE_90)
        // Work on x values
        c0 = cross[0].y;
        c1 = cross[1].y;
        c2 = cross[2].y;
    #elif defined(CONFIG_LVGL_DISP_ROTATE_180)
        // Work on x values
        c0 = c1 = c2 = LV_HOR_RES - 1;
        c0 -= cross[0].x;
        c1 -= cross[1].x;
        c2 -= cross[2].x;
    #elif defined(CONFIG_LVGL_DISP_ROTATE_270)
        // Work on x values
        c0 = c1 = c2 = LV_VER_RES - 1;
        c0 -= cross[0].y;
        c1 -= cross[1].y;
        c2 -= cross[2].y;
    #endif

    /* Compute all the required determinants */
    float dx;
    
    dx = (float)(points[0].x - points[2].x) * (float)(points[1].y - points[2].y) 
        - (float)(points[1].x - points[2].x) * (float)(points[0].y - points[2].y);

    caldata.ax = ((float)(c0 - c2) * (float)(points[1].y - points[2].y) 
                - (float)(c1 - c2) * (float)(points[0].y - points[2].y)) / dx;
    caldata.bx = ((float)(c1 - c2) * (float)(points[0].x - points[2].x) 
                - (float)(c0 - c2) * (float)(points[1].x - points[2].x)) / dx;
    caldata.cx = (c0 * ((float)points[1].x * (float)points[2].y - (float)points[2].x * (float)points[1].y) 
                - c1 * ((float)points[0].x * (float)points[2].y - (float)points[2].x * (float)points[0].y) 
                + c2 * ((float)points[0].x * (float)points[1].y - (float)points[1].x * (float)points[0].y)) / dx;

    #ifdef CONFIG_LVGL_DISP_ROTATE_0
        // Work on y values
        c0 = cross[0].y;
        c1 = cross[1].y;
        c2 = cross[2].y;
    #elif defined(CONFIG_LVGL_DISP_ROTATE_90)
        // Work on y values
        c0 = c1 = c2 = LV_HOR_RES - 1;
        c0 -= cross[0].x;
        c1 -= cross[1].x;
        c2 -= cross[2].x;
    #elif defined(CONFIG_LVGL_DISP_ROTATE_180)
        // Work on y values
        c0 = c1 = c2 = LV_VER_RES - 1;
        c0 -= cross[0].y;
        c1 -= cross[1].y;
        c2 -= cross[2].y;
    #elif defined(CONFIG_LVGL_DISP_ROTATE_270)
        // Work on y values
        c0 = cross[0].x;
        c1 = cross[1].x;
        c2 = cross[2].x;
    #endif

    caldata.ay = ((float)(c0 - c2) * (float)(points[1].y - points[2].y) 
                - (float)(c1 - c2) * (float)(points[0].y - points[2].y)) / dx;
    caldata.by = ((float)(c1 - c2) * (float)(points[0].x - points[2].x) 
                - (float)(c0 - c2) * (float)(points[1].x - points[2].x)) / dx;
    caldata.cy = (c0 * ((float)points[1].x * (float)points[2].y - (float)points[2].x * (float)points[1].y) 
                - c1 * ((float)points[0].x * (float)points[2].y - (float)points[2].x * (float)points[0].y) 
                + c2 * ((float)points[0].x * (float)points[1].y - (float)points[1].x * (float)points[0].y)) / dx;
}

/**********************
 *   GLOBAL FUNCTIONS
 **********************/
bool lvgl_calibration_transform(lv_point_t *data)
{
    if (calibrated) {
        lv_coord_t x, y;

        x = (lv_coord_t)(caldata.ax * data->x + caldata.bx * data->y + caldata.cx);
        y = (lv_coord_t)(caldata.ay * data->x + caldata.by * data->y + caldata.cy);

        data->x = x;
        data->y = y;
    }
    return calibrated;
}

bool lvgl_calibrate_mouse(lv_indev_drv_t indev_drv, bool recalibrate)
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        // NVS partition was truncated and needs to be erased
        // Retry nvs_flash_init
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK( err );
    vTaskDelay(CALIBRATION_POLL_PERIOD / portTICK_PERIOD_MS);   //Wait until nvs is stable， otherwise will cause exception

    if (!recalibrate && touch_load_calibration(&caldata)) {
        calibrated = true;
        return ESP_OK;
    }
    
    lv_coord_t w, h;
    lv_point_t cross[4];    // The locations of the test points on the display
    lv_point_t points[4];   // The x, y readings obtained from the mouse for each test point
    uint32_t calibrate_error = 100;     // Init , need calibrate

    w = LV_HOR_RES;
    h = LV_VER_RES;

    // Set up our calibration locations
    if (GMOUSE_VFLG_CAL_EXTREMES) {
        cross[0].x = 0;
        cross[0].y = 0;
        cross[1].x = w - 1;
        cross[1].y = 0;
        cross[2].x = w - 1;
        cross[2].y = h - 1;
        cross[3].x = w / 2;
        cross[3].y = h / 2;
    } else {
        cross[0].x = w / 4;
        cross[0].y = h / 4;
        cross[1].x = w - w / 4;
        cross[1].y = h / 4;
        cross[2].x = w - w / 4;
        cross[2].y = h - h / 4;
        cross[3].x = w / 2;
        cross[3].y = h / 2;
    }

    while (calibrate_error) {
        // Set up the calibration display
        lv_obj_t *cont1 = lv_cont_create(lv_scr_act(), NULL);
        lv_obj_set_size(cont1, LV_HOR_RES, LV_VER_RES);
        lv_cont_set_style(cont1, &lv_style_plain_color);

        lv_obj_t *label1 = lv_label_create(lv_scr_act(), NULL);
        lv_label_set_text(label1, "Calibration");
        lv_obj_align(label1, NULL, LV_ALIGN_IN_TOP_MID, 0, 20);

        // Calculate the calibration
        unsigned i, maxpoints, j;
        int32_t px, py;
        lv_indev_data_t data;

        maxpoints = (GMOUSE_VFLG_CAL_TEST) ? 4 : 3;

        // Loop through the calibration points
        for (i = 0; i < maxpoints; i++) {
            // Draw the current calibration point
            CalibrationCrossDraw(&cross[i]);

            // Get a valid "point pressed" average reading
            do {
                // Wait for the mouse to be pressed
                while (!(data.state & LV_INDEV_STATE_PR)) {
                    indev_drv.read(&data);
                    vTaskDelay(CALIBRATION_POLL_PERIOD / portTICK_PERIOD_MS);
                }

                // Sum samples taken every CALIBRATION_POLL_PERIOD milliseconds while the mouse is down
                px = py = j = 0;
                while ((data.state & LV_INDEV_STATE_PR)) {
                    // Limit sampling period to prevent overflow
                    if (j < CALIBRATION_MAXPRESS_PERIOD / CALIBRATION_POLL_PERIOD) {
                        px += data.point.x;
                        py += data.point.y;
                        j++;
                    }
                    indev_drv.read(&data);
                    vTaskDelay(CALIBRATION_POLL_PERIOD / portTICK_PERIOD_MS);
                }

                // Ignore presses less than CALIBRATION_MINPRESS_PERIOD milliseconds
            } while (j < CALIBRATION_MINPRESS_PERIOD / CALIBRATION_POLL_PERIOD);
            points[i].x = px / j;
            points[i].y = py / j;

            // Clear the current calibration point
            CalibrationCrossClear(&cross[i]);
        }

        // Apply 3 point calibration algorithm
        CalibrationCalculate(cross, points);

        /* Verification of correctness of calibration (optional) :
        *  See if the 4th point (Middle of the screen) coincides with the calibrated
        *  result. If point is within +/- Squareroot(ERROR) pixel margin, then successful calibration
        *  Else return the error.
        */
        if (GMOUSE_VFLG_CAL_TEST) {
            lv_coord_t t;
            calibrated = true;

            // Transform the co-ordinates
            lvgl_calibration_transform(&points[3]);

            // Do we need to rotate the reading to match the display
            #ifdef CONFIG_LVGL_DISP_ROTATE_0
                /* Don't rotate */
            #elif defined(CONFIG_LVGL_DISP_ROTATE_90)
                t = points[3].x;
                points[3].x = w - 1 - points[3].y;
                points[3].y = t;
            #elif defined(CONFIG_LVGL_DISP_ROTATE_180)
                points[3].x = w - 1 - points[3].x;
                points[3].y = h - 1 - points[3].y;
            #elif defined(CONFIG_LVGL_DISP_ROTATE_270)
                t = points[3].y;
                points[3].y = h - 1 - points[3].x;
                points[3].x = t;
            #endif

            // Is this accurate enough?
            calibrate_error = (points[3].x - cross[3].x) * (points[3].x - cross[3].x) 
                + (points[3].y - cross[3].y) * (points[3].y - cross[3].y);
            if (calibrate_error > (uint32_t)GMOUSE_FINGER_CALIBRATE_ERROR * (uint32_t)GMOUSE_FINGER_CALIBRATE_ERROR) {
                lv_label_set_text(label1, "Calibration Failed!");
                vTaskDelay(CALIBRATION_ERROR_DELAY / portTICK_PERIOD_MS);
                calibrated = false;
            } else {
                calibrate_error = 0;
                calibrated = true;
            }
        }

        // Save the calibration data (if possible)
        if (!calibrate_error) {
            touch_save_calibration(&caldata, sizeof(GMouseCalibration));
        }

        // Force an initial reading
        indev_drv.read(&data);

        // Clear the screen using the GWIN default background color
        lv_obj_del(label1);
        lv_obj_del(cont1);
        cont1 = lv_cont_create(lv_scr_act(), NULL);
        lv_obj_set_size(cont1, LV_HOR_RES, LV_VER_RES);
        lv_cont_set_style(cont1, &lv_style_scr);
    }
    return calibrated;
}
