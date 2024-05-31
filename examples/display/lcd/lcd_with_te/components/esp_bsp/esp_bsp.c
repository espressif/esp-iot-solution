/*
 * SPDX-FileCopyrightText: 2021-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/i2c.h"
#include "driver/spi_master.h"
#include "esp_timer.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_interface.h"
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "esp_err.h"
#include "esp_check.h"
#include "esp_log.h"
#include "lvgl.h"
#include "esp_rom_gpio.h"
#include "esp_lcd_axs15231b.h"
#include "bsp_err_check.h"

#include "lv_port.h"
#include "display.h"
#include "esp_bsp.h"

static const char *TAG = "example";

#if CONFIG_BSP_LCD_BOARD_QSPI_480_320
static const axs15231b_lcd_init_cmd_t lcd_init_cmds[] = {
    {0xBB, (uint8_t []){0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x5A, 0xA5}, 8, 0},
    {0xA0, (uint8_t []){0xC0, 0x10, 0x00, 0x02, 0x00, 0x00, 0x04, 0x3F, 0x20, 0x05, 0x3F, 0x3F, 0x00, 0x00, 0x00, 0x00, 0x00}, 17, 0},
    {0xA2, (uint8_t []){0x30, 0x3C, 0x24, 0x14, 0xD0, 0x20, 0xFF, 0xE0, 0x40, 0x19, 0x80, 0x80, 0x80, 0x20, 0xf9, 0x10, 0x02, 0xff, 0xff, 0xF0, 0x90, 0x01, 0x32, 0xA0, 0x91, 0xE0, 0x20, 0x7F, 0xFF, 0x00, 0x5A}, 31, 0},
    {0xD0, (uint8_t []){0xE0, 0x40, 0x51, 0x24, 0x08, 0x05, 0x10, 0x01, 0x20, 0x15, 0x42, 0xC2, 0x22, 0x22, 0xAA, 0x03, 0x10, 0x12, 0x60, 0x14, 0x1E, 0x51, 0x15, 0x00, 0x8A, 0x20, 0x00, 0x03, 0x3A, 0x12}, 30, 0},
    {0xA3, (uint8_t []){0xA0, 0x06, 0xAa, 0x00, 0x08, 0x02, 0x0A, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x00, 0x55, 0x55}, 22, 0},
    {0xC1, (uint8_t []){0x31, 0x04, 0x02, 0x02, 0x71, 0x05, 0x24, 0x55, 0x02, 0x00, 0x41, 0x00, 0x53, 0xFF, 0xFF, 0xFF, 0x4F, 0x52, 0x00, 0x4F, 0x52, 0x00, 0x45, 0x3B, 0x0B, 0x02, 0x0d, 0x00, 0xFF, 0x40}, 30, 0},
    {0xC3, (uint8_t []){0x00, 0x00, 0x00, 0x50, 0x03, 0x00, 0x00, 0x00, 0x01, 0x80, 0x01}, 11, 0},
    {0xC4, (uint8_t []){0x00, 0x24, 0x33, 0x80, 0x00, 0xea, 0x64, 0x32, 0xC8, 0x64, 0xC8, 0x32, 0x90, 0x90, 0x11, 0x06, 0xDC, 0xFA, 0x00, 0x00, 0x80, 0xFE, 0x10, 0x10, 0x00, 0x0A, 0x0A, 0x44, 0x50}, 29, 0},
    {0xC5, (uint8_t []){0x18, 0x00, 0x00, 0x03, 0xFE, 0x3A, 0x4A, 0x20, 0x30, 0x10, 0x88, 0xDE, 0x0D, 0x08, 0x0F, 0x0F, 0x01, 0x3A, 0x4A, 0x20, 0x10, 0x10, 0x00}, 23, 0},
    {0xC6, (uint8_t []){0x05, 0x0A, 0x05, 0x0A, 0x00, 0xE0, 0x2E, 0x0B, 0x12, 0x22, 0x12, 0x22, 0x01, 0x03, 0x00, 0x3F, 0x6A, 0x18, 0xC8, 0x22}, 20, 0},
    {0xC7, (uint8_t []){0x50, 0x32, 0x28, 0x00, 0xa2, 0x80, 0x8f, 0x00, 0x80, 0xff, 0x07, 0x11, 0x9c, 0x67, 0xff, 0x24, 0x0c, 0x0d, 0x0e, 0x0f}, 20, 0},
    {0xC9, (uint8_t []){0x33, 0x44, 0x44, 0x01}, 4, 0},
    {0xCF, (uint8_t []){0x2C, 0x1E, 0x88, 0x58, 0x13, 0x18, 0x56, 0x18, 0x1E, 0x68, 0x88, 0x00, 0x65, 0x09, 0x22, 0xC4, 0x0C, 0x77, 0x22, 0x44, 0xAA, 0x55, 0x08, 0x08, 0x12, 0xA0, 0x08}, 27, 0},
    {0xD5, (uint8_t []){0x40, 0x8E, 0x8D, 0x01, 0x35, 0x04, 0x92, 0x74, 0x04, 0x92, 0x74, 0x04, 0x08, 0x6A, 0x04, 0x46, 0x03, 0x03, 0x03, 0x03, 0x82, 0x01, 0x03, 0x00, 0xE0, 0x51, 0xA1, 0x00, 0x00, 0x00}, 30, 0},
    {0xD6, (uint8_t []){0x10, 0x32, 0x54, 0x76, 0x98, 0xBA, 0xDC, 0xFE, 0x93, 0x00, 0x01, 0x83, 0x07, 0x07, 0x00, 0x07, 0x07, 0x00, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x00, 0x84, 0x00, 0x20, 0x01, 0x00}, 30, 0},
    {0xD7, (uint8_t []){0x03, 0x01, 0x0b, 0x09, 0x0f, 0x0d, 0x1E, 0x1F, 0x18, 0x1d, 0x1f, 0x19, 0x40, 0x8E, 0x04, 0x00, 0x20, 0xA0, 0x1F}, 19, 0},
    {0xD8, (uint8_t []){0x02, 0x00, 0x0a, 0x08, 0x0e, 0x0c, 0x1E, 0x1F, 0x18, 0x1d, 0x1f, 0x19}, 12, 0},
    {0xD9, (uint8_t []){0x1F, 0x1F, 0x1F, 0x1F, 0x1F, 0x1F, 0x1F, 0x1F, 0x1F, 0x1F, 0x1F, 0x1F}, 12, 0},
    {0xDD, (uint8_t []){0x1F, 0x1F, 0x1F, 0x1F, 0x1F, 0x1F, 0x1F, 0x1F, 0x1F, 0x1F, 0x1F, 0x1F}, 12, 0},
    {0xDF, (uint8_t []){0x44, 0x73, 0x4B, 0x69, 0x00, 0x0A, 0x02, 0x90}, 8,  0},
    {0xE0, (uint8_t []){0x3B, 0x28, 0x10, 0x16, 0x0c, 0x06, 0x11, 0x28, 0x5c, 0x21, 0x0D, 0x35, 0x13, 0x2C, 0x33, 0x28, 0x0D}, 17, 0},
    {0xE1, (uint8_t []){0x37, 0x28, 0x10, 0x16, 0x0b, 0x06, 0x11, 0x28, 0x5C, 0x21, 0x0D, 0x35, 0x14, 0x2C, 0x33, 0x28, 0x0F}, 17, 0},
    {0xE2, (uint8_t []){0x3B, 0x07, 0x12, 0x18, 0x0E, 0x0D, 0x17, 0x35, 0x44, 0x32, 0x0C, 0x14, 0x14, 0x36, 0x3A, 0x2F, 0x0D}, 17, 0},
    {0xE3, (uint8_t []){0x37, 0x07, 0x12, 0x18, 0x0E, 0x0D, 0x17, 0x35, 0x44, 0x32, 0x0C, 0x14, 0x14, 0x36, 0x32, 0x2F, 0x0F}, 17, 0},
    {0xE4, (uint8_t []){0x3B, 0x07, 0x12, 0x18, 0x0E, 0x0D, 0x17, 0x39, 0x44, 0x2E, 0x0C, 0x14, 0x14, 0x36, 0x3A, 0x2F, 0x0D}, 17, 0},
    {0xE5, (uint8_t []){0x37, 0x07, 0x12, 0x18, 0x0E, 0x0D, 0x17, 0x39, 0x44, 0x2E, 0x0C, 0x14, 0x14, 0x36, 0x3A, 0x2F, 0x0F}, 17, 0},
    {0xA4, (uint8_t []){0x85, 0x85, 0x95, 0x82, 0xAF, 0xAA, 0xAA, 0x80, 0x10, 0x30, 0x40, 0x40, 0x20, 0xFF, 0x60, 0x30}, 16, 0},
    {0xA4, (uint8_t []){0x85, 0x85, 0x95, 0x85}, 4, 0},
    {0xBB, (uint8_t []){0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, 8, 0},
    {0x13, (uint8_t []){0x00}, 0, 0},
    {0x11, (uint8_t []){0x00}, 0, 120},
    {0x2C, (uint8_t []){0x00, 0x00, 0x00, 0x00}, 4, 0},
};
#endif
typedef struct {
    SemaphoreHandle_t te_v_sync_sem;    /*!< Semaphore for vertical synchronization */
    SemaphoreHandle_t te_catch_sem;     /*!< Semaphore for tear catch */
    uint32_t time_Tvdl;                 /*!< tvdl = The display panel is updated from the Frame Memory */
    uint32_t time_Tvdh;                 /*!< tvdh = The display panel is not updated from the Frame Memory */
    uint32_t te_timestamp;              /*!< Tear record timestamp */
    portMUX_TYPE lock;                  /*!< Lock for read/write */
} bsp_lcd_tear_t;

typedef struct {
    SemaphoreHandle_t tp_intr_event;    /*!< Semaphore for tp interrupt */
    lv_disp_rot_t rotate;               /*!< Rotation configuration for the display */
} bsp_touch_int_t;

static lv_disp_t *disp;
static lv_indev_t *disp_indev = NULL;
static esp_lcd_touch_handle_t tp = NULL;   // LCD touch handle
static esp_lcd_panel_handle_t panel_handle = NULL;

static bool i2c_initialized = false;

esp_err_t bsp_i2c_init(void)
{
    /* I2C was initialized before */
    if (i2c_initialized) {
        return ESP_OK;
    }

#if CONFIG_BSP_LCD_BOARD_I80_170_560
    const i2c_config_t i2c_conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = EXAMPLE_PIN_NUM_I80_TOUCH_SDA,
        .sda_pullup_en = GPIO_PULLUP_DISABLE,
        .scl_io_num = EXAMPLE_PIN_NUM_I80_TOUCH_SCL,
        .scl_pullup_en = GPIO_PULLUP_DISABLE,
        .master.clk_speed = BSP_I2C_CLK_SPEED_HZ
    };
#else
    const i2c_config_t i2c_conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = EXAMPLE_PIN_NUM_QSPI_TOUCH_SDA,
        .sda_pullup_en = GPIO_PULLUP_DISABLE,
        .scl_io_num = EXAMPLE_PIN_NUM_QSPI_TOUCH_SCL,
        .scl_pullup_en = GPIO_PULLUP_DISABLE,
        .master.clk_speed = BSP_I2C_CLK_SPEED_HZ
    };
#endif
    BSP_ERROR_CHECK_RETURN_ERR(i2c_param_config(BSP_I2C_NUM, &i2c_conf));
    BSP_ERROR_CHECK_RETURN_ERR(i2c_driver_install(BSP_I2C_NUM, i2c_conf.mode, 0, 0, 0));

    i2c_initialized = true;

    return ESP_OK;
}

esp_err_t bsp_i2c_deinit(void)
{
    BSP_ERROR_CHECK_RETURN_ERR(i2c_driver_delete(BSP_I2C_NUM));
    i2c_initialized = false;
    return ESP_OK;
}

// Bit number used to represent command and parameter
#define LCD_LEDC_CH            1

static esp_err_t bsp_display_brightness_init(void)
{
    // Setup LEDC peripheral for PWM backlight control
    const ledc_channel_config_t LCD_backlight_channel = {
#if CONFIG_BSP_LCD_BOARD_I80_170_560
        .gpio_num = EXAMPLE_PIN_NUM_I80_BL,
#else
        .gpio_num = EXAMPLE_PIN_NUM_QSPI_BL,
#endif
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = LCD_LEDC_CH,
        .intr_type = LEDC_INTR_DISABLE,
        .timer_sel = 1,
        .duty = 0,
        .hpoint = 0
    };
    const ledc_timer_config_t LCD_backlight_timer = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .duty_resolution = LEDC_TIMER_10_BIT,
        .timer_num = 1,
        .freq_hz = 5000,
        .clk_cfg = LEDC_AUTO_CLK
    };

    BSP_ERROR_CHECK_RETURN_ERR(ledc_timer_config(&LCD_backlight_timer));
    BSP_ERROR_CHECK_RETURN_ERR(ledc_channel_config(&LCD_backlight_channel));

    return ESP_OK;
}

esp_err_t bsp_display_brightness_set(int brightness_percent)
{
    if (brightness_percent > 100) {
        brightness_percent = 100;
    }
    if (brightness_percent < 0) {
        brightness_percent = 0;
    }

    ESP_LOGI(TAG, "Setting LCD backlight: %d%%", brightness_percent);
    uint32_t duty_cycle = (1023 * brightness_percent) / 100; // LEDC resolution set to 10bits, thus: 100% = 1023
    BSP_ERROR_CHECK_RETURN_ERR(ledc_set_duty(LEDC_LOW_SPEED_MODE, LCD_LEDC_CH, duty_cycle));
    BSP_ERROR_CHECK_RETURN_ERR(ledc_update_duty(LEDC_LOW_SPEED_MODE, LCD_LEDC_CH));

    return ESP_OK;
}

esp_err_t bsp_display_backlight_off(void)
{
    return bsp_display_brightness_set(0);
}

esp_err_t bsp_display_backlight_on(void)
{
    return bsp_display_brightness_set(100);
}

static bool bsp_display_sync_cb(void *arg)
{
    assert(arg);
    bsp_lcd_tear_t *tear_handle = (bsp_lcd_tear_t *)arg;

    if (tear_handle->te_catch_sem) {
        xSemaphoreGive(tear_handle->te_catch_sem);
    }

    if (tear_handle->te_v_sync_sem) {

        xSemaphoreTake(tear_handle->te_v_sync_sem, portMAX_DELAY);
#if CONFIG_BSP_LCD_BOARD_I80_170_560
        portENTER_CRITICAL(&tear_handle->lock);
        uint32_t tm_out = esp_log_timestamp() - tear_handle->te_timestamp;
        portEXIT_CRITICAL(&tear_handle->lock);
        if (tm_out > tear_handle->time_Tvdh) {
            xSemaphoreTake(tear_handle->te_v_sync_sem, portMAX_DELAY);
        }
#endif
    }
    return true;
}

static void bsp_display_sync_task(void *arg)
{
    assert(arg);
    bsp_lcd_tear_t *tear_handle = (bsp_lcd_tear_t *)arg;

    while (true) {
        if (pdPASS != xSemaphoreTake(tear_handle->te_catch_sem, pdMS_TO_TICKS(tear_handle->time_Tvdl))) {
            xSemaphoreTake(tear_handle->te_v_sync_sem, 0);
        }
    }
    vTaskDelete(NULL);
}

static void bsp_display_tear_interrupt(void *arg)
{
    assert(arg);
    bsp_lcd_tear_t *tear_handle = (bsp_lcd_tear_t *)arg;
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    if (tear_handle->te_v_sync_sem) {
        portENTER_CRITICAL_ISR(&tear_handle->lock);
        tear_handle->te_timestamp = esp_log_timestamp();
        portEXIT_CRITICAL_ISR(&tear_handle->lock);
        xSemaphoreGiveFromISR(tear_handle->te_v_sync_sem, &xHigherPriorityTaskWoken);

        if (xHigherPriorityTaskWoken) {
            portYIELD_FROM_ISR();
        }
    }
}

esp_err_t bsp_display_new(const bsp_display_config_t *config, esp_lcd_panel_handle_t *ret_panel, esp_lcd_panel_io_handle_t *ret_io)
{
    esp_err_t ret = ESP_OK;
    assert(config != NULL && config->max_transfer_sz > 0);

    SemaphoreHandle_t te_catch_sem = NULL;
    SemaphoreHandle_t te_v_sync_sem = NULL;
    bsp_lcd_tear_t *tear_ctx = NULL;

#if CONFIG_BSP_LCD_BOARD_I80_170_560
    gpio_config_t init_gpio_config = {
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = (1ULL << EXAMPLE_PIN_NUM_I80_RD) | (1ULL << EXAMPLE_PIN_NUM_I80_PCLK),
    };
    ESP_ERROR_CHECK(gpio_config(&init_gpio_config));
    gpio_set_level(EXAMPLE_PIN_NUM_I80_RD, 1);
    gpio_set_level(EXAMPLE_PIN_NUM_I80_PCLK, 0);

    ESP_LOGI(TAG, "Initialize Intel 8080 bus");
    esp_lcd_i80_bus_handle_t i80_bus = NULL;
    esp_lcd_i80_bus_config_t bus_config = AXS15231B_PANEL_BUS_I80_CONFIG(
                                              EXAMPLE_PIN_NUM_I80_DC,
                                              EXAMPLE_PIN_NUM_I80_PCLK,
                                              LCD_CLK_SRC_DEFAULT,
                                              EXAMPLE_PIN_NUM_I80_DATA0,
                                              EXAMPLE_PIN_NUM_I80_DATA1,
                                              EXAMPLE_PIN_NUM_I80_DATA2,
                                              EXAMPLE_PIN_NUM_I80_DATA3,
                                              EXAMPLE_PIN_NUM_I80_DATA4,
                                              EXAMPLE_PIN_NUM_I80_DATA5,
                                              EXAMPLE_PIN_NUM_I80_DATA6,
                                              EXAMPLE_PIN_NUM_I80_DATA7,
                                              8,
                                              config->max_transfer_sz);
    ESP_ERROR_CHECK(esp_lcd_new_i80_bus(&bus_config, &i80_bus));

    esp_lcd_panel_io_i80_config_t io_config = {
        .cs_gpio_num = EXAMPLE_PIN_NUM_I80_CS,
        .pclk_hz = EXAMPLE_LCD_I80_CLK,
        .trans_queue_depth = 10,
        .dc_levels = {
            .dc_idle_level = 0,
            .dc_cmd_level = 0,
            .dc_dummy_level = 0,
            .dc_data_level = 1,
        },
        .lcd_cmd_bits = EXAMPLE_LCD_CMD_BITS,
        .lcd_param_bits = EXAMPLE_LCD_PARAM_BITS,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_i80(i80_bus, &io_config, ret_io));

    ESP_LOGI(TAG, "Install LCD driver of axs15231b");
    esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = EXAMPLE_PIN_NUM_I80_RST,
        .flags.reset_active_high = 0,
        .color_space = BSP_LCD_COLOR_SPACE,
        .bits_per_pixel = BSP_LCD_BITS_PER_PIXEL,
    };
#else
    ESP_LOGI(TAG, "Initialize SPI bus");
    const spi_bus_config_t buscfg = AXS15231B_PANEL_BUS_QSPI_CONFIG(
                                        EXAMPLE_PIN_NUM_QSPI_PCLK,
                                        EXAMPLE_PIN_NUM_QSPI_DATA0,
                                        EXAMPLE_PIN_NUM_QSPI_DATA1,
                                        EXAMPLE_PIN_NUM_QSPI_DATA2,
                                        EXAMPLE_PIN_NUM_QSPI_DATA3,
                                        config->max_transfer_sz);
    ESP_ERROR_CHECK(spi_bus_initialize(EXAMPLE_LCD_QSPI_HOST, &buscfg, SPI_DMA_CH_AUTO));

    ESP_LOGI(TAG, "Install panel IO");
    const esp_lcd_panel_io_spi_config_t io_config = AXS15231B_PANEL_IO_QSPI_CONFIG(EXAMPLE_PIN_NUM_QSPI_CS, NULL, NULL);
    // Attach the LCD to the SPI bus
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)EXAMPLE_LCD_QSPI_HOST, &io_config, ret_io));

    ESP_LOGI(TAG, "Install LCD driver of axs15231b");
    const axs15231b_vendor_config_t vendor_config = {
        .init_cmds = lcd_init_cmds,         // Uncomment these line if use custom initialization commands
        .init_cmds_size = sizeof(lcd_init_cmds) / sizeof(lcd_init_cmds[0]),
        .flags = {
            .use_qspi_interface = 1,
        },
    };
    const esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = EXAMPLE_PIN_NUM_QSPI_RST,
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,
        .bits_per_pixel = BSP_LCD_BITS_PER_PIXEL,
        .vendor_config = (void *) &vendor_config,
    };
#endif
    ESP_ERROR_CHECK(esp_lcd_new_panel_axs15231b(*ret_io, &panel_config, ret_panel));

    esp_lcd_panel_reset(*ret_panel);
    esp_lcd_panel_init(*ret_panel);
#if CONFIG_BSP_LCD_BOARD_QSPI_480_320
    esp_lcd_panel_disp_on_off(*ret_panel, true);
#endif

    if (config->tear_cfg.te_gpio_num > 0) {

        tear_ctx = malloc(sizeof(bsp_lcd_tear_t));
        ESP_GOTO_ON_FALSE(tear_ctx, ESP_ERR_NO_MEM, err, TAG, "Not enough memory for tear_ctx allocation!");

        te_v_sync_sem = xSemaphoreCreateCounting(1, 0);
        ESP_GOTO_ON_FALSE(te_v_sync_sem, ESP_ERR_NO_MEM, err, TAG, "Failed to create te_v_sync_sem Semaphore");
        tear_ctx->te_v_sync_sem = te_v_sync_sem;

        te_catch_sem = xSemaphoreCreateCounting(1, 0);
        ESP_GOTO_ON_FALSE(te_catch_sem, ESP_ERR_NO_MEM, err, TAG, "Failed to create te_catch_sem Semaphore");
        tear_ctx->te_catch_sem = te_catch_sem;

        tear_ctx->time_Tvdl = config->tear_cfg.time_Tvdl;
        tear_ctx->time_Tvdh = config->tear_cfg.time_Tvdh;

        tear_ctx->lock.owner = portMUX_FREE_VAL;
        tear_ctx->lock.count = 0;

        const gpio_config_t te_detect_cfg = {
            .intr_type = config->tear_cfg.tear_intr_type,
            .mode = GPIO_MODE_INPUT,
            .pin_bit_mask = BIT64(config->tear_cfg.te_gpio_num),
            .pull_down_en = GPIO_PULLDOWN_DISABLE,
            .pull_up_en = GPIO_PULLUP_ENABLE,
        };

        ESP_ERROR_CHECK(gpio_config(&te_detect_cfg));
        gpio_install_isr_service(0);
        ESP_ERROR_CHECK(gpio_isr_handler_add(config->tear_cfg.te_gpio_num, bsp_display_tear_interrupt, tear_ctx));

        BaseType_t res;
        if (config->tear_cfg.task_affinity < 0) {
            res = xTaskCreate(bsp_display_sync_task, "Tear task", config->tear_cfg.task_stack, tear_ctx, config->tear_cfg.task_priority, NULL);
        } else {
            res = xTaskCreatePinnedToCore(bsp_display_sync_task, "Tear task", config->tear_cfg.task_stack, tear_ctx, config->tear_cfg.task_priority, NULL, config->tear_cfg.task_affinity);
        }
        ESP_GOTO_ON_FALSE(res == pdPASS, ESP_FAIL, err, TAG, "Create Sync task fail!");
    }

    (*ret_panel)->user_data = (void *)tear_ctx;

    return ret;

err:
    if (te_v_sync_sem) {
        vSemaphoreDelete(te_v_sync_sem);
    }
    if (te_catch_sem) {
        vSemaphoreDelete(te_catch_sem);
    }
    if (tear_ctx) {
        free(tear_ctx);
    }
    if (*ret_panel) {
        esp_lcd_panel_del(*ret_panel);
    }
    if (*ret_io) {
        esp_lcd_panel_io_del(*ret_io);
    }
#if CONFIG_BSP_LCD_BOARD_I80_170_560
    esp_lcd_del_i80_bus(i80_bus);
#else
    spi_bus_free(EXAMPLE_LCD_QSPI_HOST);
#endif
    return ret;
}

static lv_disp_t *bsp_display_lcd_init(const bsp_display_cfg_t *cfg)
{
    assert(cfg != NULL);
    esp_lcd_panel_io_handle_t io_handle = NULL;

    uint32_t hres;
    uint32_t vres;

#if CONFIG_BSP_LCD_BOARD_I80_170_560
    /**
    * If the transmission time(t = weight * height * bits_per_pixel / clk / bus) is less than the refresh period (time_Tvdl),
    * adopt a 1x period, and start data transmission at the rising edge.
    */
    hres = EXAMPLE_LCD_I80_H_RES;
    vres = EXAMPLE_LCD_I80_V_RES;
    const bsp_display_config_t bsp_disp_cfg = {
        .max_transfer_sz = hres * vres * sizeof(uint16_t),
        .tear_cfg = BSP_SYNC_TASK_CONFIG(EXAMPLE_PIN_NUM_I80_TE, GPIO_INTR_POSEDGE),
    };
#else
    /**
    * If the transmission time exceeds the refresh period (time_Tvdl), adopt a 2x period,
    * and start data transmission at the falling edge.
    */
    hres = EXAMPLE_LCD_QSPI_H_RES;
    vres = EXAMPLE_LCD_QSPI_V_RES;
    const bsp_display_config_t bsp_disp_cfg = {
        .max_transfer_sz = hres * vres * sizeof(uint16_t),
        .tear_cfg = BSP_SYNC_TASK_CONFIG(EXAMPLE_PIN_NUM_QSPI_TE, GPIO_INTR_NEGEDGE),
    };
#endif
    bsp_display_new(&bsp_disp_cfg, &panel_handle, &io_handle);

    /* Add LCD screen */
    ESP_LOGD(TAG, "Add LCD screen");
    lvgl_port_display_cfg_t disp_cfg = {
        .io_handle = io_handle,
        .panel_handle = panel_handle,
        .buffer_size = cfg->buffer_size,
        .sw_rotate = cfg->rotate,
        .hres = hres,
        .vres = vres,
        .trans_size = hres * vres / 10,
        .draw_wait_cb = bsp_display_sync_cb,
        .flags = {
            .buff_dma = false,
            .buff_spiram = true,
        },
    };

    if (disp_cfg.sw_rotate == LV_DISP_ROT_180 || disp_cfg.sw_rotate == LV_DISP_ROT_NONE) {
        disp_cfg.hres = hres;
        disp_cfg.vres = vres;
    } else {
        disp_cfg.hres = vres;
        disp_cfg.vres = hres;
    }

    return lvgl_port_add_disp(&disp_cfg);
}

static bool bsp_touch_sync_cb(void *arg)
{
    assert(arg);
    bool touch_interrupt = false;
    bsp_touch_int_t *touch_handle = (bsp_touch_int_t *)arg;

    if (touch_handle && touch_handle->tp_intr_event) {
        if (xSemaphoreTake(touch_handle->tp_intr_event, 0) == pdTRUE) {
            touch_interrupt = true;
        }
    } else {
        touch_interrupt = true;
    }

    return touch_interrupt;
}

static void bsp_touch_interrupt_cb(esp_lcd_touch_handle_t tp)
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    bsp_touch_int_t *touch_handle = (bsp_touch_int_t *)tp->config.user_data;

    xSemaphoreGiveFromISR(touch_handle->tp_intr_event, &xHigherPriorityTaskWoken);

    if (xHigherPriorityTaskWoken) {
        portYIELD_FROM_ISR();
    }
}

static void bsp_touch_process_points_cb(esp_lcd_touch_handle_t tp, uint16_t *x, uint16_t *y, uint16_t *strength, uint8_t *point_num, uint8_t max_point_num)
{
    uint16_t tmp;
    bsp_touch_int_t *touch_handle = (bsp_touch_int_t *)tp->config.user_data;

#if CONFIG_BSP_LCD_BOARD_I80_170_560
    for (int i = 0; i < *point_num; i++) {
        if (LV_DISP_ROT_NONE == touch_handle->rotate) {
            tmp = x[i];
            x[i] = y[i];
            y[i] = tp->config.x_max - tmp;
        } else if (LV_DISP_ROT_90 == touch_handle->rotate) {
            x[i] = tp->config.x_max - x[i];
            y[i] = tp->config.y_max - y[i];
        } else if (LV_DISP_ROT_180 == touch_handle->rotate) {
            tmp = x[i];
            x[i] = tp->config.y_max - y[i];
            y[i] = tmp;
        }
    }
#else
    for (int i = 0; i < *point_num; i++) {
        if (LV_DISP_ROT_270 == touch_handle->rotate) {
            tmp = x[i];
            x[i] = tp->config.y_max - y[i];
            y[i] = tmp;
        } else if (LV_DISP_ROT_180 == touch_handle->rotate) {
            tmp = x[i];
            x[i] = tp->config.x_max - x[i];
            y[i] = tp->config.y_max - y[i];
        } else if (LV_DISP_ROT_90 == touch_handle->rotate) {
            tmp = x[i];
            x[i] = y[i];
            y[i] = tp->config.x_max - tmp;
        }
    }
#endif
}

esp_err_t bsp_touch_new(const bsp_display_cfg_t *config, esp_lcd_touch_handle_t *ret_touch)
{
    esp_err_t ret = ESP_OK;

    /* Initialize I2C */
    BSP_ERROR_CHECK_RETURN_ERR(bsp_i2c_init());

    SemaphoreHandle_t tp_intr_event = NULL;
    bsp_touch_int_t *touch_ctx = NULL;

    /* Initialize touch */
    esp_lcd_touch_config_t tp_cfg = {
#if CONFIG_BSP_LCD_BOARD_I80_170_560
        .x_max = EXAMPLE_LCD_I80_V_RES,
        .y_max = EXAMPLE_LCD_I80_H_RES,
        .rst_gpio_num = EXAMPLE_PIN_NUM_I80_TOUCH_RST, // Shared with LCD reset
        .int_gpio_num = EXAMPLE_PIN_NUM_I80_TOUCH_INT,
#else
        .x_max = EXAMPLE_LCD_QSPI_H_RES,
        .y_max = EXAMPLE_LCD_QSPI_V_RES,
        .rst_gpio_num = EXAMPLE_PIN_NUM_QSPI_TOUCH_RST, // Shared with LCD reset
        .int_gpio_num = EXAMPLE_PIN_NUM_QSPI_TOUCH_INT,
#endif
        .process_coordinates = bsp_touch_process_points_cb,
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

    esp_lcd_panel_io_handle_t tp_io_handle = NULL;
    esp_lcd_touch_handle_t tp_handle = NULL;
    const esp_lcd_panel_io_i2c_config_t tp_io_config = ESP_LCD_TOUCH_IO_I2C_AXS15231B_CONFIG();

    ESP_RETURN_ON_ERROR(esp_lcd_new_panel_io_i2c((esp_lcd_i2c_bus_handle_t)BSP_I2C_NUM, &tp_io_config, &tp_io_handle), TAG, "");
    ESP_RETURN_ON_ERROR(esp_lcd_touch_new_i2c_axs15231b(tp_io_handle, &tp_cfg, &tp_handle), TAG, "New axs15231b failed");

    touch_ctx = malloc(sizeof(bsp_touch_int_t));
    ESP_GOTO_ON_FALSE(touch_ctx, ESP_ERR_NO_MEM, err, TAG, "Not enough memory for touch_ctx allocation!");

    if (tp_cfg.int_gpio_num > 0) {

        tp_intr_event = xSemaphoreCreateBinary();
        ESP_GOTO_ON_FALSE(tp_intr_event, ESP_ERR_NO_MEM, err, TAG, "Not enough memory for tp_intr_event allocation!");
        touch_ctx->tp_intr_event = tp_intr_event;
        esp_lcd_touch_register_interrupt_callback_with_data(tp_handle, bsp_touch_interrupt_cb, (void *)touch_ctx);
    } else {
        touch_ctx->tp_intr_event = NULL;
    }
    touch_ctx->rotate = config->rotate;
    tp_handle->config.user_data = touch_ctx;

    *ret_touch = tp_handle;

    return ESP_OK;
err:
    if (tp_intr_event) {
        vSemaphoreDelete(tp_intr_event);
    }
    if (touch_ctx) {
        free(touch_ctx);
    }
    if (tp_handle) {
        esp_lcd_touch_del(tp_handle);
    }
    if (tp_io_handle) {
        esp_lcd_panel_io_del(tp_io_handle);
    }
    return ret;
}

static lv_indev_t *bsp_display_indev_init(const bsp_display_cfg_t *config, lv_disp_t *disp)
{
    BSP_ERROR_CHECK_RETURN_NULL(bsp_touch_new(config, &tp));
    assert(tp);

    /* Add touch input (for selected screen) */
    const lvgl_port_touch_cfg_t touch_cfg = {
        .disp = disp,
        .handle = tp,
        .touch_wait_cb = bsp_touch_sync_cb,
    };

    return lvgl_port_add_touch(&touch_cfg);
}

lv_disp_t *bsp_display_start_with_config(const bsp_display_cfg_t *cfg)
{
    BSP_ERROR_CHECK_RETURN_NULL(lvgl_port_init(&cfg->lvgl_port_cfg));

    BSP_ERROR_CHECK_RETURN_NULL(bsp_display_brightness_init());

    BSP_NULL_CHECK(disp = bsp_display_lcd_init(cfg), NULL);

    BSP_NULL_CHECK(disp_indev = bsp_display_indev_init(cfg, disp), NULL);

    return disp;
}

lv_indev_t *bsp_display_get_input_dev(void)
{
    return disp_indev;
}

bool bsp_display_lock(uint32_t timeout_ms)
{
    return lvgl_port_lock(timeout_ms);
}

void bsp_display_unlock(void)
{
    lvgl_port_unlock();
}
