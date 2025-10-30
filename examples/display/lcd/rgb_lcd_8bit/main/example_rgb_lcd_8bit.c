/*
 * SPDX-FileCopyrightText: 2023-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include "esp_log.h"
#include "esp_heap_caps.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_io_additions.h"
#include "esp_lcd_st77903_rgb.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lv_demos.h"
#include "esp_lv_adapter.h"

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////// Please update the following configuration according to your LCD spec //////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#define EXAMPLE_LCD_H_RES               (320)
#define EXAMPLE_LCD_V_RES               (480)
#define EXAMPLE_LCD_BIT_PER_PIXEL       (24)
#define EXAMPLE_RGB_BIT_PER_PIXEL       (24)
#define EXAMPLE_RGB_DATA_WIDTH          (8)
#define EXAMPLE_RGB_BOUNCE_BUFFER_SIZE  (EXAMPLE_LCD_H_RES * 20)
#define EXAMPLE_PIN_NUM_LCD_RGB_VSYNC   (GPIO_NUM_3)
#define EXAMPLE_PIN_NUM_LCD_RGB_HSYNC   (GPIO_NUM_46)
#define EXAMPLE_PIN_NUM_LCD_RGB_DE      (GPIO_NUM_17)
#define EXAMPLE_PIN_NUM_LCD_RGB_DISP    (-1)            // -1 if not used
#define EXAMPLE_PIN_NUM_LCD_RGB_PCLK    (GPIO_NUM_9)
#define EXAMPLE_PIN_NUM_LCD_RGB_DATA0   (GPIO_NUM_10)
#define EXAMPLE_PIN_NUM_LCD_RGB_DATA1   (GPIO_NUM_11)
#define EXAMPLE_PIN_NUM_LCD_RGB_DATA2   (GPIO_NUM_12)
#define EXAMPLE_PIN_NUM_LCD_RGB_DATA3   (GPIO_NUM_13)
#define EXAMPLE_PIN_NUM_LCD_RGB_DATA4   (GPIO_NUM_14)
#define EXAMPLE_PIN_NUM_LCD_RGB_DATA5   (GPIO_NUM_21)
#define EXAMPLE_PIN_NUM_LCD_RGB_DATA6   (GPIO_NUM_8)
#define EXAMPLE_PIN_NUM_LCD_RGB_DATA7   (GPIO_NUM_18)
#define EXAMPLE_PIN_NUM_LCD_SPI_CS      (GPIO_NUM_45)
#define EXAMPLE_PIN_NUM_LCD_SPI_SCK     (GPIO_NUM_40)
#define EXAMPLE_PIN_NUM_LCD_SPI_SDO     (GPIO_NUM_42)
#define EXAMPLE_PIN_NUM_LCD_RST         (GPIO_NUM_2)    // -1 if not used
#define EXAMPLE_PIN_NUM_BK_LIGHT        (GPIO_NUM_0)    // -1 if not used
#define EXAMPLE_LCD_BK_LIGHT_ON_LEVEL   (1)
#define EXAMPLE_LCD_BK_LIGHT_OFF_LEVEL  !EXAMPLE_LCD_BK_LIGHT_ON_LEVEL

static const char *TAG = "example";

// static const st77903_lcd_init_cmd_t lcd_init_cmds[] = {
// //  {cmd, { data }, data_size, delay_ms}
//    {0xf0, (uint8_t []){0xc3}, 1, 0},
//    {0xf0, (uint8_t []){0x96}, 1, 0},
//    {0xf0, (uint8_t []){0xa5}, 1, 0},
//     ...
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

    ESP_LOGI(TAG, "Install 3-wire SPI panel IO");
    spi_line_config_t line_config = {
        .cs_io_type = IO_TYPE_GPIO,
        .cs_expander_pin = EXAMPLE_PIN_NUM_LCD_SPI_CS,
        .scl_io_type = IO_TYPE_GPIO,
        .scl_expander_pin = EXAMPLE_PIN_NUM_LCD_SPI_SCK,
        .sda_io_type = IO_TYPE_GPIO,
        .sda_expander_pin = EXAMPLE_PIN_NUM_LCD_SPI_SDO,
        .io_expander = NULL,
    };
    esp_lcd_panel_io_3wire_spi_config_t io_config = ST77903_RGB_PANEL_IO_3WIRE_SPI_CONFIG(line_config, 0);
    esp_lcd_panel_io_handle_t io_handle = NULL;
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_3wire_spi(&io_config, &io_handle));

    ESP_LOGI(TAG, "Install st77903 panel driver");
    esp_lcd_panel_handle_t lcd_handle = NULL;

    // Calculate required frame buffer count based on rotation
    esp_lv_adapter_rotation_t rotation = CONFIG_EXAMPLE_LCD_ROTATION;
    uint8_t num_fbs = esp_lv_adapter_get_required_frame_buffer_count(
                          ESP_LV_ADAPTER_TEAR_AVOID_MODE_DEFAULT_RGB, rotation);

    esp_lcd_rgb_panel_config_t rgb_config = {
        .clk_src = LCD_CLK_SRC_DEFAULT,
        .psram_trans_align = 64,
        .data_width = EXAMPLE_RGB_DATA_WIDTH,
        .bits_per_pixel = EXAMPLE_RGB_BIT_PER_PIXEL,
        .de_gpio_num = EXAMPLE_PIN_NUM_LCD_RGB_DE,
        .pclk_gpio_num = EXAMPLE_PIN_NUM_LCD_RGB_PCLK,
        .vsync_gpio_num = EXAMPLE_PIN_NUM_LCD_RGB_VSYNC,
        .hsync_gpio_num = EXAMPLE_PIN_NUM_LCD_RGB_HSYNC,
        .disp_gpio_num = EXAMPLE_PIN_NUM_LCD_RGB_DISP,
        .data_gpio_nums = {
            EXAMPLE_PIN_NUM_LCD_RGB_DATA0,
            EXAMPLE_PIN_NUM_LCD_RGB_DATA1,
            EXAMPLE_PIN_NUM_LCD_RGB_DATA2,
            EXAMPLE_PIN_NUM_LCD_RGB_DATA3,
            EXAMPLE_PIN_NUM_LCD_RGB_DATA4,
            EXAMPLE_PIN_NUM_LCD_RGB_DATA5,
            EXAMPLE_PIN_NUM_LCD_RGB_DATA6,
            EXAMPLE_PIN_NUM_LCD_RGB_DATA7,
        },
        .timings = ST77903_RGB_320_480_PANEL_48HZ_RGB_TIMING(),
        .flags.fb_in_psram = 1,
        .num_fbs = num_fbs,
        .bounce_buffer_size_px = EXAMPLE_RGB_BOUNCE_BUFFER_SIZE,
    };
    rgb_config.timings.h_res = EXAMPLE_LCD_H_RES;
    rgb_config.timings.v_res = EXAMPLE_LCD_V_RES;
    st77903_vendor_config_t vendor_config = {
        .rgb_config = &rgb_config,
        // .init_cmds = lcd_init_cmds,         // Uncomment these line if use custom initialization commands
        // .init_cmds_size = sizeof(lcd_init_cmds) / sizeof(st77903_lcd_init_cmd_t),
        .flags = {
            .auto_del_panel_io = 0,         /**
                                             * Set to 1 if panel IO is no longer needed after LCD initialization.
                                             * If the panel IO pins are sharing other pins of the RGB interface to save GPIOs,
                                             * Please set it to 1 to release the pins.
                                             */
            .mirror_by_cmd = 1,             // Set to 0 if `auto_del_panel_io` is enabled
        },
    };
    const esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = EXAMPLE_PIN_NUM_LCD_RST,
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,
        .bits_per_pixel = EXAMPLE_LCD_BIT_PER_PIXEL,
        .vendor_config = &vendor_config,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_st77903_rgb(io_handle, &panel_config, &lcd_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_mirror(lcd_handle, true, false));
    ESP_ERROR_CHECK(esp_lcd_panel_reset(lcd_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_init(lcd_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(lcd_handle, true));

    // Initialize LVGL adapter
    ESP_LOGI(TAG, "Initialize LVGL adapter");
    esp_lv_adapter_config_t adapter_config = ESP_LV_ADAPTER_DEFAULT_CONFIG();
    ESP_ERROR_CHECK(esp_lv_adapter_init(&adapter_config));

    // Register display to LVGL adapter
    ESP_LOGI(TAG, "Register display to LVGL adapter");
    esp_lv_adapter_display_config_t display_config = ESP_LV_ADAPTER_DISPLAY_RGB_DEFAULT_CONFIG(
                                                         lcd_handle,
                                                         io_handle,
                                                         EXAMPLE_LCD_H_RES,
                                                         EXAMPLE_LCD_V_RES,
                                                         rotation
                                                     );
    lv_display_t *disp = esp_lv_adapter_register_display(&display_config);
    if (disp == NULL) {
        ESP_LOGE(TAG, "Failed to register display");
        return;
    }

    // Start LVGL adapter task
    ESP_LOGI(TAG, "Start LVGL adapter");
    ESP_ERROR_CHECK(esp_lv_adapter_start());

#if EXAMPLE_PIN_NUM_BK_LIGHT >= 0
    ESP_LOGI(TAG, "Turn on LCD backlight");
    gpio_set_level(EXAMPLE_PIN_NUM_BK_LIGHT, EXAMPLE_LCD_BK_LIGHT_ON_LEVEL);
#endif

    ESP_LOGI(TAG, "Display LVGL demos");
    // Lock the mutex due to the LVGL APIs are not thread-safe
    if (esp_lv_adapter_lock(-1) == ESP_OK) {
        // lv_demo_stress();
        lv_demo_benchmark();
        // lv_demo_music();
        // lv_demo_widgets();

        // Release the mutex
        esp_lv_adapter_unlock();
    }
}
