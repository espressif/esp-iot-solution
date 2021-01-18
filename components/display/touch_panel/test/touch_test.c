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
#include "freertos/FreeRTOS.h"
#include "screen_driver.h"
#include "touch_panel.h"
#include "basic_painter.h"
#include "unity.h"

static const char *TAG = "TOUCH LCD TEST";

static touch_panel_driver_t touch;
static scr_driver_t lcd;

TEST_CASE("Touch XPT2046 test", "[touch_panel][iot]")
{
    spi_config_t spi_cfg = {
        .miso_io_num = 27,
        .mosi_io_num = 21,
        .sclk_io_num = 22,
        .max_transfer_sz = 320 * 480,
    };
    spi_bus_handle_t spi_bus = spi_bus_create(2, &spi_cfg);
    TEST_ASSERT_NOT_NULL(spi_bus);

    scr_interface_spi_config_t spi_lcd_cfg = {
        .spi_bus = spi_bus,
        .pin_num_cs = 5,
        .pin_num_dc = 19,
        .clk_freq = 20000000,
        .swap_data = true,
    };

    scr_interface_driver_t *interface_drv;
    TEST_ASSERT(ESP_OK == scr_interface_create(SCREEN_IFACE_SPI, &spi_lcd_cfg, &interface_drv));

    scr_controller_config_t lcd_cfg = {0};
    lcd_cfg.interface_drv = interface_drv,
    lcd_cfg.pin_num_rst = 18,
    lcd_cfg.pin_num_bckl = 23,
    lcd_cfg.rst_active_level = 0,
    lcd_cfg.bckl_active_level = 1,
    lcd_cfg.width = 240;
    lcd_cfg.height = 320;
    lcd_cfg.rotate = SCR_DIR_LRBT;
    TEST_ASSERT(ESP_OK == scr_find_driver(SCREEN_CONTROLLER_ILI9341, &lcd));
    lcd.init(&lcd_cfg);

    touch_panel_config_t touch_cfg = {
        .interface_spi = {
            .spi_bus = spi_bus,
            .pin_num_cs = 32,
            .clk_freq = 10000000,
        },
        .interface_type = TOUCH_PANEL_IFACE_SPI,
        .pin_num_int = 33,
        .direction = TOUCH_DIR_LRTB,
        .width = 240,
        .height = 320,
    };
    TEST_ASSERT(ESP_OK == touch_panel_find_driver(TOUCH_PANEL_CONTROLLER_XPT2046, &touch));
    TEST_ASSERT(ESP_OK == touch.init(&touch_cfg));
    TEST_ASSERT(ESP_ERR_INVALID_STATE == touch.init(&touch_cfg));
    touch.calibration_run(&lcd, true);

    painter_init(&lcd);
    painter_clear(COLOR_WHITE);

    while (1) {
        touch_panel_points_t touch_info;
        touch.read_point_data(&touch_info);
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

    touch.deinit();
    lcd.deinit();
    scr_interface_delete(interface_drv);
    spi_bus_delete(&spi_bus);
}

TEST_CASE("Touch NS2016 test", "[touch_panel][iot]")
{
    spi_config_t spi_cfg = {
        .miso_io_num = -1,
        .mosi_io_num = 38,
        .sclk_io_num = 39,
        .max_transfer_sz = 32,
    };
    spi_bus_handle_t spi_bus = spi_bus_create(2, &spi_cfg);
    TEST_ASSERT_NOT_NULL(spi_bus);

    scr_interface_spi_config_t spi_lcd_cfg = {
        .spi_bus = spi_bus,
        .pin_num_cs = 37,
        .pin_num_dc = 41,
        .clk_freq = 40000000,
        .swap_data = true,
    };

    scr_interface_driver_t *interface_drv;
    TEST_ASSERT(ESP_OK == scr_interface_create(SCREEN_IFACE_SPI, &spi_lcd_cfg, &interface_drv));

    scr_controller_config_t lcd_cfg = {0};
    lcd_cfg.interface_drv = interface_drv,
    lcd_cfg.pin_num_rst = -1,
    lcd_cfg.pin_num_bckl = -1,
    lcd_cfg.rst_active_level = 0,
    lcd_cfg.bckl_active_level = 1,
    lcd_cfg.width = 240;
    lcd_cfg.height = 320;
    lcd_cfg.rotate = SCR_DIR_LRTB;
    TEST_ASSERT(ESP_OK == scr_find_driver(SCREEN_CONTROLLER_ILI9341, &lcd));
    lcd.init(&lcd_cfg);

    i2c_config_t i2c_conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = 35,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_io_num = 36,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = 100000,
    };
    i2c_bus_handle_t i2c_bus = i2c_bus_create(I2C_NUM_0, &i2c_conf);

    touch_panel_config_t touch_cfg = {
        .interface_i2c = {
            .i2c_bus = i2c_bus,
            .clk_freq = 100000,
            .i2c_addr = 0x48,
        },
        .interface_type = TOUCH_PANEL_IFACE_I2C,
        .pin_num_int = 33,
        .direction = TOUCH_DIR_LRTB,
        .width = 240,
        .height = 320,
    };
    TEST_ASSERT(ESP_OK == touch_panel_find_driver(TOUCH_PANEL_CONTROLLER_NS2016, &touch));
    TEST_ASSERT(ESP_OK == touch.init(&touch_cfg));
    TEST_ASSERT(ESP_ERR_INVALID_STATE == touch.init(&touch_cfg));
    touch.calibration_run(&lcd, true);

    painter_init(&lcd);
    painter_clear(COLOR_WHITE);

    while (1) {
        touch_panel_points_t touch_info;
        touch.read_point_data(&touch_info);
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
    touch.deinit();
    lcd.deinit();
    scr_interface_delete(interface_drv);
    spi_bus_delete(&spi_bus);
}

TEST_CASE("Touch FT5x06 test", "[touch_panel][iot]")
{
    i2s_lcd_config_t i2s_lcd_cfg = {
        .data_width  = 16,
        .pin_data_num = {
            16, 15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1
        },
        .pin_num_cs = -1,
        .pin_num_wr = 34,
        .pin_num_rs = 33,
        .clk_freq = 20000000,
        .i2s_port = I2S_NUM_0,
        .buffer_size = 32000,
        .swap_data = false,
    };

    scr_interface_driver_t *interface_drv;
    TEST_ASSERT(ESP_OK == scr_interface_create(SCREEN_IFACE_8080, &i2s_lcd_cfg, &interface_drv));
    scr_controller_config_t lcd_cfg = {0};
    lcd_cfg.interface_drv = interface_drv,
    lcd_cfg.pin_num_rst = -1,
    lcd_cfg.pin_num_bckl = 21,
    lcd_cfg.rst_active_level = 0,
    lcd_cfg.bckl_active_level = 1,
    lcd_cfg.width = 480;
    lcd_cfg.height = 800;
    lcd_cfg.rotate = SCR_DIR_LRTB;
    TEST_ASSERT(ESP_OK == scr_find_driver(SCREEN_CONTROLLER_RM68120, &lcd));
    lcd.init(&lcd_cfg);

    i2c_config_t i2c_conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = 35,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_io_num = 36,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = 100000,
    };
    i2c_bus_handle_t i2c_bus = i2c_bus_create(I2C_NUM_0, &i2c_conf);

    touch_panel_config_t touch_cfg = {
        .interface_i2c = {
            .i2c_bus = i2c_bus,
            .clk_freq = 100000,
            .i2c_addr = 0x38,
        },
        .interface_type = TOUCH_PANEL_IFACE_I2C,
        .pin_num_int = -1,
        .direction = TOUCH_DIR_LRTB,
        .width = 800,
        .height = 480,
    };
    TEST_ASSERT(ESP_OK == touch_panel_find_driver(TOUCH_PANEL_CONTROLLER_FT5X06, &touch));
    TEST_ASSERT(ESP_OK == touch.init(&touch_cfg));
    TEST_ASSERT(ESP_ERR_INVALID_STATE == touch.init(&touch_cfg));
    touch.calibration_run(&lcd, true);

    painter_init(&lcd);
    painter_clear(COLOR_WHITE);

    while (1) {
        touch_panel_points_t touch_info;
        touch.read_point_data(&touch_info);
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
    touch.deinit();
    lcd.deinit();
    scr_interface_delete(interface_drv);
}
