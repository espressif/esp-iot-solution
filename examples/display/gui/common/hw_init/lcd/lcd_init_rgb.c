/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include "hw_init.h"

#if CONFIG_EXAMPLE_LCD_INTERFACE_RGB

#include "esp_check.h"
#include "esp_idf_version.h"
#include "esp_lcd_panel_rgb.h"
#include "driver/gpio.h"
#if CONFIG_EXAMPLE_LCD_S3_EV_GC9503
#include "driver/i2c_master.h"
#include "esp_lcd_gc9503.h"
#include "esp_lcd_panel_io_additions.h"
#include "esp_io_expander_tca9554.h"
#endif

static const char *TAG = "hw_lcd_init";

#if CONFIG_IDF_TARGET_ESP32S3
#define HW_LCD_PIXEL_CLOCK_HZ                   (18 * 1000 * 1000)
#else
#define HW_LCD_PIXEL_CLOCK_HZ                   (26 * 1000 * 1000)
#endif

#define HW_LCD_HSYNC                            (1)
#define HW_LCD_HBP                              (40)
#define HW_LCD_HFP                              (20)
#define HW_LCD_VSYNC                            (1)
#define HW_LCD_VBP                              (10)
#define HW_LCD_VFP                              (5)

/* Match each target's PSRAM DMA burst limit for external frame buffers. */
#if CONFIG_IDF_TARGET_ESP32P4 || CONFIG_IDF_TARGET_ESP32S31
#define HW_LCD_DMA_BURST_SIZE                   (128)
#else
#define HW_LCD_DMA_BURST_SIZE                   (64)
#endif

#if CONFIG_IDF_TARGET_ESP32S31
#define HW_LCD_RGB_VSYNC                        (GPIO_NUM_45)
#define HW_LCD_RGB_HSYNC                        (GPIO_NUM_44)
#define HW_LCD_RGB_DE                           (GPIO_NUM_43)
#define HW_LCD_RGB_PCLK                         (GPIO_NUM_40)
#define HW_LCD_RGB_DISP                         (GPIO_NUM_NC)
#define HW_LCD_RGB_DATA0                        (GPIO_NUM_8)
#define HW_LCD_RGB_DATA1                        (GPIO_NUM_9)
#define HW_LCD_RGB_DATA2                        (GPIO_NUM_10)
#define HW_LCD_RGB_DATA3                        (GPIO_NUM_11)
#define HW_LCD_RGB_DATA4                        (GPIO_NUM_12)
#define HW_LCD_RGB_DATA5                        (GPIO_NUM_13)
#define HW_LCD_RGB_DATA6                        (GPIO_NUM_14)
#define HW_LCD_RGB_DATA7                        (GPIO_NUM_15)
#define HW_LCD_RGB_DATA8                        (GPIO_NUM_16)
#define HW_LCD_RGB_DATA9                        (GPIO_NUM_17)
#define HW_LCD_RGB_DATA10                       (GPIO_NUM_18)
#define HW_LCD_RGB_DATA11                       (GPIO_NUM_19)
#define HW_LCD_RGB_DATA12                       (GPIO_NUM_33)
#define HW_LCD_RGB_DATA13                       (GPIO_NUM_34)
#define HW_LCD_RGB_DATA14                       (GPIO_NUM_35)
#define HW_LCD_RGB_DATA15                       (GPIO_NUM_36)
#else
#define HW_LCD_RGB_VSYNC                        (GPIO_NUM_3)
#define HW_LCD_RGB_HSYNC                        (GPIO_NUM_46)
#define HW_LCD_RGB_DE                           (GPIO_NUM_17)
#define HW_LCD_RGB_PCLK                         (GPIO_NUM_9)
#define HW_LCD_RGB_DISP                         (GPIO_NUM_NC)
#define HW_LCD_RGB_DATA0                        (GPIO_NUM_10)
#define HW_LCD_RGB_DATA1                        (GPIO_NUM_11)
#define HW_LCD_RGB_DATA2                        (GPIO_NUM_12)
#define HW_LCD_RGB_DATA3                        (GPIO_NUM_13)
#define HW_LCD_RGB_DATA4                        (GPIO_NUM_14)
#define HW_LCD_RGB_DATA5                        (GPIO_NUM_21)
#define HW_LCD_RGB_DATA6                        (GPIO_NUM_8)
#define HW_LCD_RGB_DATA7                        (GPIO_NUM_18)
#define HW_LCD_RGB_DATA8                        (GPIO_NUM_45)
#define HW_LCD_RGB_DATA9                        (GPIO_NUM_38)
#define HW_LCD_RGB_DATA10                       (GPIO_NUM_39)
#define HW_LCD_RGB_DATA11                       (GPIO_NUM_40)
#define HW_LCD_RGB_DATA12                       (GPIO_NUM_41)
#define HW_LCD_RGB_DATA13                       (GPIO_NUM_42)
#define HW_LCD_RGB_DATA14                       (GPIO_NUM_2)
#define HW_LCD_RGB_DATA15                       (GPIO_NUM_1)
#endif

#define HW_LCD_DATA_WIDTH                       (16)
#define HW_LCD_BIT_PER_PIXEL                    (16)
#define HW_LCD_BOUNCE_BUFFER_HEIGHT             (20)

static esp_lcd_panel_handle_t s_panel_handle;
#if CONFIG_EXAMPLE_LCD_S3_EV_GC9503
static i2c_master_bus_handle_t s_i2c_bus;
static esp_io_expander_handle_t s_expander;
#endif

esp_err_t hw_lcd_init(esp_lcd_panel_handle_t *panel_handle, esp_lcd_panel_io_handle_t *io_handle, uint8_t frame_buffer_count, hw_rotation_t rotation)
{
#if CONFIG_EXAMPLE_LCD_S3_EV_GC9503
    if (frame_buffer_count < 1 || frame_buffer_count > 3) {
        return ESP_ERR_INVALID_ARG;
    }
    (void)rotation;
    ESP_LOGI(TAG, "Initialize GC9503 480x480 RGB panel");
    gpio_config_t pins = {
        .pin_bit_mask = 1ULL << 3,
                             .mode = GPIO_MODE_OUTPUT,
                             .pull_up_en = true,
    };
    ESP_RETURN_ON_ERROR(gpio_config(&pins), TAG, "VSYNC config");
    ESP_RETURN_ON_ERROR(gpio_set_level(3, 1), TAG, "VSYNC high");
    i2c_master_bus_config_t i2c = {
        .i2c_port = 0,
        .sda_io_num = 47,
        .scl_io_num = 48,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    ESP_RETURN_ON_ERROR(i2c_new_master_bus(&i2c, &s_i2c_bus), TAG, "I2C");
    ESP_RETURN_ON_ERROR(esp_io_expander_new_i2c_tca9554(s_i2c_bus, 0x20, &s_expander), TAG, "expander");
    spi_line_config_t lines = {
        .cs_io_type = IO_TYPE_EXPANDER,
        .cs_expander_pin = IO_EXPANDER_PIN_NUM_1,
        .scl_io_type = IO_TYPE_EXPANDER,
        .scl_expander_pin = IO_EXPANDER_PIN_NUM_2,
        .sda_io_type = IO_TYPE_EXPANDER,
        .sda_expander_pin = IO_EXPANDER_PIN_NUM_3,
        .io_expander = s_expander,
    };
    esp_lcd_panel_io_3wire_spi_config_t io_cfg = GC9503_PANEL_IO_3WIRE_SPI_CONFIG(lines, 0);
    esp_lcd_panel_io_handle_t panel_io = NULL;
    ESP_RETURN_ON_ERROR(esp_lcd_new_panel_io_3wire_spi(&io_cfg, &panel_io), TAG, "panel IO");
    esp_lcd_rgb_panel_config_t rgb = {
        .clk_src = LCD_CLK_SRC_PLL160M,
        .dma_burst_size = 64,
        .data_width = 16,
        .in_color_format = LCD_COLOR_FMT_RGB565,
        .de_gpio_num = 17,
        .pclk_gpio_num = 9,
        .vsync_gpio_num = 3,
        .hsync_gpio_num = 46,
        .disp_gpio_num = -1,
        .data_gpio_nums = {10, 11, 12, 13, 14, 21, 8, 18, 45, 38, 39, 40, 41, 42, 2, 1},
        .timings = GC9503_480_480_PANEL_60HZ_RGB_TIMING(),
        .flags.fb_in_psram = true,
        .num_fbs = frame_buffer_count,
        .bounce_buffer_size_px = HW_LCD_H_RES * 10,
    };
    gc9503_vendor_config_t vendor = {
        .rgb_config = &rgb,
        .flags.auto_del_panel_io = true,
    };
    esp_lcd_panel_dev_config_t device = {
        .reset_gpio_num = -1,
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,
        .bits_per_pixel = 18,
        .vendor_config = &vendor,
    };
    ESP_RETURN_ON_ERROR(esp_lcd_new_panel_gc9503(panel_io, &device, &s_panel_handle), TAG, "GC9503");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_init(s_panel_handle), TAG, "panel init");
    *panel_handle = s_panel_handle;
    if (io_handle) {
        *io_handle = NULL;
    }
    return ESP_OK;
#else
    if (frame_buffer_count < 1 || frame_buffer_count > 3) {
        return ESP_ERR_INVALID_ARG;
    }
    (void)rotation;
    ESP_LOGI(TAG, "Initialize RGB panel");
    esp_lcd_rgb_panel_config_t panel_conf = {
        .clk_src = LCD_CLK_SRC_DEFAULT,
        .dma_burst_size = HW_LCD_DMA_BURST_SIZE,
        .data_width = HW_LCD_DATA_WIDTH,
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(6, 0, 0)
        .in_color_format = LCD_COLOR_FMT_RGB565,
#else
        .bits_per_pixel = HW_LCD_BIT_PER_PIXEL,
#endif
        .de_gpio_num = HW_LCD_RGB_DE,
        .pclk_gpio_num = HW_LCD_RGB_PCLK,
        .vsync_gpio_num = HW_LCD_RGB_VSYNC,
        .hsync_gpio_num = HW_LCD_RGB_HSYNC,
        .disp_gpio_num = HW_LCD_RGB_DISP,
        .data_gpio_nums = {
            HW_LCD_RGB_DATA0,
            HW_LCD_RGB_DATA1,
            HW_LCD_RGB_DATA2,
            HW_LCD_RGB_DATA3,
            HW_LCD_RGB_DATA4,
            HW_LCD_RGB_DATA5,
            HW_LCD_RGB_DATA6,
            HW_LCD_RGB_DATA7,
            HW_LCD_RGB_DATA8,
            HW_LCD_RGB_DATA9,
            HW_LCD_RGB_DATA10,
            HW_LCD_RGB_DATA11,
            HW_LCD_RGB_DATA12,
            HW_LCD_RGB_DATA13,
            HW_LCD_RGB_DATA14,
            HW_LCD_RGB_DATA15,
        },
        .timings = {
            .pclk_hz = HW_LCD_PIXEL_CLOCK_HZ,
            .h_res = HW_LCD_H_RES,
            .v_res = HW_LCD_V_RES,
            .hsync_back_porch = HW_LCD_HBP,
            .hsync_front_porch = HW_LCD_HFP,
            .hsync_pulse_width = HW_LCD_HSYNC,
            .vsync_back_porch = HW_LCD_VBP,
            .vsync_front_porch = HW_LCD_VFP,
            .vsync_pulse_width = HW_LCD_VSYNC,
            .flags = {
                .pclk_active_neg = true,
            },
        },
        .flags.fb_in_psram = 1,
        .num_fbs = frame_buffer_count,
#if CONFIG_IDF_TARGET_ESP32S3
        .bounce_buffer_size_px = HW_LCD_H_RES * HW_LCD_BOUNCE_BUFFER_HEIGHT,
#endif
    };
    ESP_ERROR_CHECK(esp_lcd_new_rgb_panel(&panel_conf, &s_panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_reset(s_panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_init(s_panel_handle));

    *panel_handle = s_panel_handle;
    if (io_handle) {
        *io_handle = NULL;
    }

    return ESP_OK;
#endif
}

esp_err_t hw_lcd_deinit(void)
{
    if (s_panel_handle) {
        esp_lcd_panel_disp_on_off(s_panel_handle, false);
        esp_lcd_panel_del(s_panel_handle);
        s_panel_handle = NULL;
    }
#if CONFIG_EXAMPLE_LCD_S3_EV_GC9503
    if (s_expander) {
        esp_io_expander_del(s_expander);
        s_expander = NULL;
    }
    if (s_i2c_bus) {
        i2c_del_master_bus(s_i2c_bus);
        s_i2c_bus = NULL;
    }
#endif

    return ESP_OK;
}

int hw_lcd_get_te_gpio(void)
{
    return GPIO_NUM_NC;
}

uint32_t hw_lcd_get_bus_freq_hz(void)
{
    return 0;
}

uint8_t hw_lcd_get_bus_data_lines(void)
{
    return HW_LCD_DATA_WIDTH;
}

uint8_t hw_lcd_get_bits_per_pixel(void)
{
    return HW_LCD_BIT_PER_PIXEL;
}

#endif
