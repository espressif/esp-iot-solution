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

/* component includes */
#include <stdio.h>

/* freertos includes */
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"
#include "freertos/event_groups.h"
#include "esp_freertos_hooks.h"
#include "board.h"
#include "lvgl_gui.h"
#include "esp_log.h"

#include "lv_examples/src/lv_demo_printer/lv_demo_printer.h"
#include "lv_examples/src/lv_demo_widgets/lv_demo_widgets.h"
#include "lv_examples/src/lv_ex_get_started/lv_ex_get_started.h"
#include "lv_examples/src/lv_demo_benchmark/lv_demo_benchmark.h"
#include "lv_examples/src/lv_demo_keypad_encoder/lv_demo_keypad_encoder.h"
#include "lv_examples/src/lv_demo_stress/lv_demo_stress.h"
#include "lv_examples/src/lv_ex_style/lv_ex_style.h"

static const char *TAG = "example_lvgl";

void app_main()
{
    iot_board_init();
    spi_bus_handle_t spi2_bus = iot_board_get_handle(BOARD_SPI2_ID);

    scr_driver_t lcd_drv;
    touch_panel_driver_t touch_drv;
    scr_interface_spi_config_t spi_lcd_cfg = {
        .spi_bus = spi2_bus,
        .pin_num_cs = BOARD_LCD_SPI_CS_PIN,
        .pin_num_dc = BOARD_LCD_SPI_DC_PIN,
        .clk_freq = BOARD_LCD_SPI_CLOCK_FREQ,
        .swap_data = true,
    };

    scr_interface_driver_t *iface_drv;
    scr_interface_create(SCREEN_IFACE_SPI, &spi_lcd_cfg, &iface_drv);

    scr_controller_config_t lcd_cfg = {
        .interface_drv = iface_drv,
        .pin_num_rst = 18,
        .pin_num_bckl = 23,
        .rst_active_level = 0,
        .bckl_active_level = 1,
        .offset_hor = 0,
        .offset_ver = 0,
        .width = 240,
        .height = 320,
        .rotate = SCR_DIR_TBLR,
    };
    scr_find_driver(SCREEN_CONTROLLER_ILI9341, &lcd_drv);
    lcd_drv.init(&lcd_cfg);

    touch_panel_config_t touch_cfg = {
        .interface_spi = {
            .spi_bus = spi2_bus,
            .pin_num_cs = BOARD_TOUCH_SPI_CS_PIN,
            .clk_freq = 10000000,
        },
        .interface_type = TOUCH_PANEL_IFACE_SPI,
        .pin_num_int = -1,
        .direction = TOUCH_DIR_TBLR,
        .width = 240,
        .height = 320,
    };
    touch_panel_find_driver(TOUCH_PANEL_CONTROLLER_XPT2046, &touch_drv);
    touch_drv.init(&touch_cfg);
    touch_drv.calibration_run(&lcd_drv, false);

    /* Initialize LittlevGL GUI */
    lvgl_init(&lcd_drv, &touch_drv);

#ifdef CONFIG_LV_DEMO_BENCHMARK
    lv_demo_benchmark();
#elif defined CONFIG_LV_DEMO_PRINTER
    lv_demo_printer();
#elif defined CONFIG_LV_DEMO_WIDGETS
    lv_demo_widgets();
#elif defined CONFIG_LV_EX_GET_STARTED
    lv_ex_get_started_1();
#elif defined CONFIG_LV_DEMO_STRESS
    lv_demo_stress();
#elif defined CONFIG_LV_EX_STYLE
    lv_ex_style_1();
#endif

    ESP_LOGI(TAG, "[APP] IDF version: %s", esp_get_idf_version());
    ESP_LOGI(TAG, "[APP] Free memory: %d bytes", esp_get_free_heap_size());
}
