/*
 * SPDX-FileCopyrightText: 2023-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/i2c.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_lcd_st77903_qspi.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_touch.h"
#include "esp_lcd_touch_cst816s.h"
#include "lvgl_port.h"
#include "lv_demos.h"

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////// Please update the following configuration according to your LCD spec //////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#define EXAMPLE_LCD_H_RES                   (400)
#define EXAMPLE_LCD_V_RES                   (400)
#define EXAMPLE_LCD_BIT_PER_PIXEL           (16)
#define EXAMPLE_LCD_HOST                    (SPI2_HOST)
#define EXAMPLE_LCD_FB_BUF_NUM              (LVGL_PORT_LCD_QSPI_BUFFER_NUMS)
#define EXAMPLE_PIN_NUM_LCD_CS              (GPIO_NUM_12)
#define EXAMPLE_PIN_NUM_LCD_SCK             (GPIO_NUM_10)
#define EXAMPLE_PIN_NUM_LCD_D0              (GPIO_NUM_13)
#define EXAMPLE_PIN_NUM_LCD_D1              (GPIO_NUM_11)
#define EXAMPLE_PIN_NUM_LCD_D2              (GPIO_NUM_14)
#define EXAMPLE_PIN_NUM_LCD_D3              (GPIO_NUM_9)
#define EXAMPLE_PIN_NUM_BK_LIGHT            (-1)
#define EXAMPLE_PIN_NUM_LCD_RST             (GPIO_NUM_47)
#define EXAMPLE_LCD_BK_LIGHT_ON_LEVEL       (1)
#define EXAMPLE_LCD_BK_LIGHT_OFF_LEVEL      (!EXAMPLE_LCD_BK_LIGHT_ON_LEVEL)

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////// Please update the following configuration according to your touch spec ////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#if CONFIG_EXAMPLE_LCD_TOUCH_CONTROLLER_CST816S
#define TOUCH_HOST                      (I2C_NUM_0)
#define EXAMPLE_PIN_NUM_TOUCH_SCL       (GPIO_NUM_45)
#define EXAMPLE_PIN_NUM_TOUCH_SDA       (GPIO_NUM_48)
#define EXAMPLE_PIN_NUM_TOUCH_RST       (-1)                   // -1 if not used
#define EXAMPLE_PIN_NUM_TOUCH_INT       (GPIO_NUM_21)          // -1 if not used
#endif

static const char *TAG = "example";

// static const st77903_lcd_init_cmd_t lcd_init_cmds[] = {
// //  {cmd, { data }, data_size, delay_ms}
//     {0xFF, (uint8_t []){0x77, 0x01, 0x00, 0x00, 0x10}, 5, 0},
//     {0xC0, (uint8_t []){0x3B, 0x00}, 2, 0},
//     {0xC1, (uint8_t []){0x0D, 0x02}, 2, 0},
//     {0xC2, (uint8_t []){0x31, 0x05}, 2, 0},
//   ...
// };

void app_main()
{
#if EXAMPLE_PIN_NUM_BK_LIGHT >= 0
    ESP_LOGI(TAG, "Turn off LCD backlight");
    gpio_config_t bk_gpio_config = {
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = 1ULL << EXAMPLE_PIN_NUM_BK_LIGHT
    };
    ESP_ERROR_CHECK(gpio_config(&bk_gpio_config));
#endif

    esp_lcd_panel_handle_t lcd_handle = NULL;
    ESP_LOGI(TAG, "Install ST77903 panel driver");
    st77903_qspi_config_t qspi_config = ST77903_QSPI_CONFIG_DEFAULT(EXAMPLE_LCD_HOST,
                                                                    EXAMPLE_PIN_NUM_LCD_CS,
                                                                    EXAMPLE_PIN_NUM_LCD_SCK,
                                                                    EXAMPLE_PIN_NUM_LCD_D0,
                                                                    EXAMPLE_PIN_NUM_LCD_D1,
                                                                    EXAMPLE_PIN_NUM_LCD_D2,
                                                                    EXAMPLE_PIN_NUM_LCD_D3,
                                                                    EXAMPLE_LCD_FB_BUF_NUM,
                                                                    EXAMPLE_LCD_H_RES,
                                                                    EXAMPLE_LCD_V_RES);
    st77903_vendor_config_t vendor_config = {
        .qspi_config = &qspi_config,
        // .init_cmds = lcd_init_cmds,      // Uncomment these line if use custom initialization commands
        // .init_cmds_size = sizeof(lcd_init_cmds) / sizeof(lcd_init_cmds[0]),
        .flags = {
            .mirror_by_cmd = 1,
        },
    };
    const esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = EXAMPLE_PIN_NUM_LCD_RST,
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,
        .bits_per_pixel = EXAMPLE_LCD_BIT_PER_PIXEL,
        .vendor_config = &vendor_config,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_st77903_qspi(&panel_config, &lcd_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_reset(lcd_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_mirror(lcd_handle, true, false));
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(lcd_handle, true));
    ESP_ERROR_CHECK(esp_lcd_panel_init(lcd_handle));

    esp_lcd_touch_handle_t tp_handle = NULL;
#if CONFIG_EXAMPLE_LCD_TOUCH_CONTROLLER_CST816S
    ESP_LOGI(TAG, "Initialize I2C bus");
    const i2c_config_t i2c_conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = EXAMPLE_PIN_NUM_TOUCH_SDA,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_io_num = EXAMPLE_PIN_NUM_TOUCH_SCL,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = 400 * 1000,
    };
    ESP_ERROR_CHECK(i2c_param_config(TOUCH_HOST, &i2c_conf));
    ESP_ERROR_CHECK(i2c_driver_install(TOUCH_HOST, i2c_conf.mode, 0, 0, 0));

    esp_lcd_panel_io_handle_t tp_io_handle = NULL;
    const esp_lcd_panel_io_i2c_config_t tp_io_config = ESP_LCD_TOUCH_IO_I2C_CST816S_CONFIG();

    // Attach the TOUCH to the I2C bus
    ESP_LOGI(TAG, "Initialize I2C panel IO");
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_i2c((esp_lcd_i2c_bus_handle_t)TOUCH_HOST, &tp_io_config, &tp_io_handle));

    ESP_LOGI(TAG, "Initialize touch controller CST816S");
    esp_lcd_touch_config_t tp_cfg = {
        .x_max = EXAMPLE_LCD_H_RES,
        .y_max = EXAMPLE_LCD_V_RES,
        .rst_gpio_num = EXAMPLE_PIN_NUM_TOUCH_RST,
        .int_gpio_num = EXAMPLE_PIN_NUM_TOUCH_INT,
        .levels = {
            .reset = 0,
            .interrupt = 0,
        },
        .flags = {
            .swap_xy = 0,
            .mirror_x = 0,
            .mirror_y = 0,
        },
    };
    if (tp_cfg.int_gpio_num != GPIO_NUM_NC) {
        init_touch_isr_mux();
        tp_cfg.interrupt_callback = lvgl_port_touch_isr_cb;
    }
    ESP_ERROR_CHECK(esp_lcd_touch_new_i2c_cst816s(tp_io_handle, &tp_cfg, &tp_handle));
#endif // CONFIG_EXAMPLE_LCD_TOUCH_CONTROLLER_CST816S

    ESP_ERROR_CHECK(lvgl_port_init(lcd_handle, tp_handle));

#if EXAMPLE_PIN_NUM_BK_LIGHT >= 0
    ESP_LOGI(TAG, "Turn on LCD backlight");
    gpio_set_level(EXAMPLE_PIN_NUM_BK_LIGHT, EXAMPLE_LCD_BK_LIGHT_ON_LEVEL);
#endif

    ESP_LOGI(TAG, "Display LVGL demos");
    // Lock the mutex due to the LVGL APIs are not thread-safe
    if (lvgl_port_lock(-1)) {
        //lv_demo_stress();
        // lv_demo_benchmark();
        lv_demo_music();

        // Release the mutex
        lvgl_port_unlock();
    }
}
