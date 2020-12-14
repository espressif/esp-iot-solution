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
#include "freertos/FreeRTOS.h"
#include "screen_driver.h"
#include "touch_panel.h"
#include "basic_painter.h"
#include "unity.h"

static const char *TAG = "TOUCH LCD TEST";

static touch_driver_fun_t touch;
static scr_driver_fun_t lcd;

TEST_CASE("Touch XPT2016 test", "[touch_panel][iot]")
{
    iface_spi_config_t spi_lcd_cfg = {
        .pin_num_miso = 27,
        .pin_num_mosi = 21,
        .pin_num_clk = 22,
        .pin_num_cs = 5,
        .pin_num_dc = 19,
        .clk_freq = 40000000,
        .spi_host = 2,
        .dma_chan = 2,
        .init_spi_bus = 1,
        .swap_data = true,
    };

    scr_iface_driver_fun_t *iface_drv;
    TEST_ASSERT(ESP_OK == scr_iface_create(SCREEN_IFACE_SPI, &spi_lcd_cfg, &iface_drv));

    scr_controller_config_t lcd_cfg = {0};
    lcd_cfg.iface_drv = iface_drv,
    lcd_cfg.pin_num_rst = 18,
    lcd_cfg.pin_num_bckl = 23,
    lcd_cfg.rst_active_level = 0,
    lcd_cfg.bckl_active_level = 1,
    lcd_cfg.width = 240;
    lcd_cfg.height = 320;
    lcd_cfg.rotate = SCR_DIR_LRBT;
    TEST_ASSERT(ESP_OK == scr_init(SCREEN_CONTROLLER_ILI9341, &lcd_cfg, &lcd));

    touch_panel_config_t touch_cfg = {
        .iface_spi = {
            .pin_num_miso = 27,
            .pin_num_mosi = 21,
            .pin_num_clk = 22,
            .pin_num_cs = 32,
            .clk_freq = 10000000,
            .spi_host = 2,
            .dma_chan = 2,
            .init_spi_bus = false,
        },
        .iface_type = TOUCH_IFACE_SPI,
        .pin_num_int = 33,
        .direction = TOUCH_DIR_LRTB,
        .width = 240,
        .height = 320,
    };
    TEST_ASSERT(ESP_OK == touch_panel_init(TOUCH_CONTROLLER_XPT2046, &touch_cfg, &touch));
    touch.calibration_run(&lcd, true);

    painter_init(&lcd);
    painter_clear(COLOR_WHITE);

    while (1) {
        touch_info_t touch_info;
        touch.read_info(&touch_info);
        for (int i = 0; i < touch_info.point_num; i++) {
            ESP_LOGI(TAG, "touch point %d: (%d, %d)", i, touch_info.curx[i], touch_info.cury[i]);
        }

        if (TOUCH_EVT_PRESS == touch_info.event) {
            int32_t x = touch_info.curx[0];
            int32_t y = touch_info.cury[0];
            ESP_LOGI(TAG, "Draw point at (%d, %d)", x, y);
            lcd.draw_pixel(x, y, COLOR_RED);
            lcd.draw_pixel(x + 1, y, COLOR_RED);
            lcd.draw_pixel(x, y + 1, COLOR_RED);
            lcd.draw_pixel(x + 1, y + 1, COLOR_RED);

        } else {
            vTaskDelay(50 / portTICK_RATE_MS);
        }
    }

    touch.deinit(false);
    scr_deinit(&lcd);
    scr_iface_delete(iface_drv);
}

TEST_CASE("Touch NS2016 test", "[touch_panel][iot]")
{
    iface_spi_config_t spi_lcd_cfg = {
        .pin_num_miso = 27,
        .pin_num_mosi = 21,
        .pin_num_clk = 22,
        .pin_num_cs = 5,
        .pin_num_dc = 19,
        .clk_freq = 40000000,
        .spi_host = 2,
        .dma_chan = 2,
        .init_spi_bus = 1,
        .swap_data = true,
    };

    scr_iface_driver_fun_t *iface_drv;
    TEST_ASSERT(ESP_OK == scr_iface_create(SCREEN_IFACE_SPI, &spi_lcd_cfg, &iface_drv));

    scr_controller_config_t lcd_cfg = {0};
    lcd_cfg.iface_drv = iface_drv,
    lcd_cfg.pin_num_rst = 18,
    lcd_cfg.pin_num_bckl = 23,
    lcd_cfg.rst_active_level = 0,
    lcd_cfg.bckl_active_level = 1,
    lcd_cfg.width = 240;
    lcd_cfg.height = 320;
    lcd_cfg.rotate = SCR_DIR_LRTB;
    TEST_ASSERT(ESP_OK == scr_init(SCREEN_CONTROLLER_ILI9341, &lcd_cfg, &lcd));

    touch_panel_config_t touch_cfg = {
        .iface_i2c = {
            .pin_num_sda = 12,
            .pin_num_scl = 32,
            .clk_freq = 100000,
            .i2c_port = 0,
            .i2c_addr = 0x48,
        },
        .iface_type = TOUCH_IFACE_I2C,
        .pin_num_int = 33,
        .direction = TOUCH_DIR_LRTB,
        .width = 240,
        .height = 320,
    };
    TEST_ASSERT(ESP_OK == touch_panel_init(TOUCH_CONTROLLER_NS2016, &touch_cfg, &touch));
    touch.calibration_run(&lcd, true);

    painter_init(&lcd);
    painter_clear(COLOR_WHITE);

    while (1) {
        touch_info_t touch_info;
        touch.read_info(&touch_info);
        for (int i = 0; i < touch_info.point_num; i++) {
            ESP_LOGI(TAG, "touch point %d: (%d, %d)", i, touch_info.curx[i], touch_info.cury[i]);
        }

        if (TOUCH_EVT_PRESS == touch_info.event) {
            int32_t x = touch_info.curx[0];
            int32_t y = touch_info.cury[0];
            ESP_LOGI(TAG, "Draw point at (%d, %d)", x, y);
            lcd.draw_pixel(x, y, COLOR_RED);
            lcd.draw_pixel(x + 1, y, COLOR_RED);
            lcd.draw_pixel(x, y + 1, COLOR_RED);
            lcd.draw_pixel(x + 1, y + 1, COLOR_RED);

        } else {
            vTaskDelay(50 / portTICK_RATE_MS);
        }
    }
    touch.deinit(false);
    scr_deinit(&lcd);
    scr_iface_delete(iface_drv);
}

TEST_CASE("Touch FT5x06 test", "[touch_panel][iot]")
{
    i2s_lcd_config_t i2s_lcd_cfg = {
        .data_width  = 16,
        .pin_data_num = {
            16, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15,
        },
        .pin_num_cs = 45,
        .pin_num_wr = 34,
        .pin_num_rs = 33,
        .clk_freq = 20000000,
        .i2s_port = I2S_NUM_0,
        .buffer_size = 32000,
        .swap_data = false,
    };

    scr_iface_driver_fun_t *iface_drv;
    TEST_ASSERT(ESP_OK == scr_iface_create(SCREEN_IFACE_8080, &i2s_lcd_cfg, &iface_drv));
    scr_controller_config_t lcd_cfg = {0};
    lcd_cfg.iface_drv = iface_drv,
    lcd_cfg.pin_num_rst = 0,
    lcd_cfg.pin_num_bckl = -1,
    lcd_cfg.rst_active_level = 0,
    lcd_cfg.bckl_active_level = 1,
    lcd_cfg.width = 480;
    lcd_cfg.height = 854;
    lcd_cfg.rotate = SCR_DIR_LRTB;
    TEST_ASSERT(ESP_OK == scr_init(SCREEN_CONTROLLER_ILI9806, &lcd_cfg, &lcd));

    touch_panel_config_t touch_cfg = {
        .iface_i2c = {
            .pin_num_sda = 19,
            .pin_num_scl = 18,
            .clk_freq = 100000,
            .i2c_port = 0,
            .i2c_addr = 0x38,
        },
        .iface_type = TOUCH_IFACE_I2C,
        .pin_num_int = -1,
        .direction = TOUCH_DIR_TBRL,
        .width = 854,
        .height = 480,
    };
    TEST_ASSERT(ESP_OK == touch_panel_init(TOUCH_CONTROLLER_FT5X06, &touch_cfg, &touch));
    touch.calibration_run(&lcd, true);

    painter_init(&lcd);
    painter_clear(COLOR_WHITE);

    while (1) {
        touch_info_t touch_info;
        touch.read_info(&touch_info);
        for (int i = 0; i < touch_info.point_num; i++) {
            ESP_LOGI(TAG, "touch point %d: (%d, %d)", i, touch_info.curx[i], touch_info.cury[i]);
        }

        if (TOUCH_EVT_PRESS == touch_info.event) {
            int32_t x = touch_info.curx[0];
            int32_t y = touch_info.cury[0];
            ESP_LOGI(TAG, "Draw point at (%d, %d)", x, y);
            lcd.draw_pixel(x, y, COLOR_RED);
            lcd.draw_pixel(x + 1, y, COLOR_RED);
            lcd.draw_pixel(x, y + 1, COLOR_RED);
            lcd.draw_pixel(x + 1, y + 1, COLOR_RED);

        } else {
            vTaskDelay(50 / portTICK_RATE_MS);
        }
    }
    touch.deinit(false);
    scr_deinit(&lcd);
    scr_iface_delete(iface_drv);
}
