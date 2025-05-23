/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include "esp_err.h"
#include "esp_log.h"
#include "esp_check.h"
#include "driver/i2c.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lvgl_port.h"

#include "esp_lcd_touch_tt21100.h"

#include "unity.h"

/* LCD size */
#define EXAMPLE_LCD_H_RES   (320)
#define EXAMPLE_LCD_V_RES   (240)

/* LCD settings */
#define EXAMPLE_LCD_SPI_NUM         (SPI3_HOST)
#define EXAMPLE_LCD_PIXEL_CLK_HZ    (40 * 1000 * 1000)
#define EXAMPLE_LCD_CMD_BITS        (8)
#define EXAMPLE_LCD_PARAM_BITS      (8)
#define EXAMPLE_LCD_COLOR_SPACE     (ESP_LCD_COLOR_SPACE_BGR)
#define EXAMPLE_LCD_BITS_PER_PIXEL  (16)
#define EXAMPLE_LCD_DRAW_BUFF_DOUBLE (1)
#define EXAMPLE_LCD_DRAW_BUFF_HEIGHT (50)
#define EXAMPLE_LCD_BL_ON_LEVEL     (1)

/* LCD pins */
#define EXAMPLE_LCD_GPIO_SCLK       (GPIO_NUM_7)
#define EXAMPLE_LCD_GPIO_MOSI       (GPIO_NUM_6)
#define EXAMPLE_LCD_GPIO_RST        (GPIO_NUM_48)
#define EXAMPLE_LCD_GPIO_DC         (GPIO_NUM_4)
#define EXAMPLE_LCD_GPIO_CS         (GPIO_NUM_5)
#define EXAMPLE_LCD_GPIO_BL         (GPIO_NUM_45)

/* Touch settings */
#define EXAMPLE_TOUCH_I2C_NUM       (0)
#define EXAMPLE_TOUCH_I2C_CLK_HZ    (400000)

/* LCD touch pins */
#define EXAMPLE_TOUCH_I2C_SCL       (GPIO_NUM_18)
#define EXAMPLE_TOUCH_I2C_SDA       (GPIO_NUM_8)
#define EXAMPLE_TOUCH_GPIO_INT      (GPIO_NUM_3)

static char *TAG = "test";

/* LCD IO and panel */
static esp_lcd_panel_io_handle_t lcd_io = NULL;
static esp_lcd_panel_handle_t lcd_panel = NULL;
static esp_lcd_panel_io_handle_t tp_io_handle = NULL;
static esp_lcd_touch_handle_t touch_handle = NULL;

/* LVGL display and touch */
static lv_display_t *lvgl_disp = NULL;
static lv_indev_t *lvgl_touch_indev = NULL;

static esp_err_t app_lcd_init(void)
{
    esp_err_t ret = ESP_OK;

    /* LCD backlight */
    gpio_config_t bk_gpio_config = {
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = 1ULL << EXAMPLE_LCD_GPIO_BL
    };
    ESP_ERROR_CHECK(gpio_config(&bk_gpio_config));

    /* LCD initialization */
    ESP_LOGD(TAG, "Initialize SPI bus");
    const spi_bus_config_t buscfg = {
        .sclk_io_num = EXAMPLE_LCD_GPIO_SCLK,
        .mosi_io_num = EXAMPLE_LCD_GPIO_MOSI,
        .miso_io_num = GPIO_NUM_NC,
        .quadwp_io_num = GPIO_NUM_NC,
        .quadhd_io_num = GPIO_NUM_NC,
        .max_transfer_sz = EXAMPLE_LCD_H_RES * EXAMPLE_LCD_DRAW_BUFF_HEIGHT * sizeof(uint16_t),
    };
    ESP_RETURN_ON_ERROR(spi_bus_initialize(EXAMPLE_LCD_SPI_NUM, &buscfg, SPI_DMA_CH_AUTO), TAG, "SPI init failed");

    ESP_LOGD(TAG, "Install panel IO");
    const esp_lcd_panel_io_spi_config_t io_config = {
        .dc_gpio_num = EXAMPLE_LCD_GPIO_DC,
        .cs_gpio_num = EXAMPLE_LCD_GPIO_CS,
        .pclk_hz = EXAMPLE_LCD_PIXEL_CLK_HZ,
        .lcd_cmd_bits = EXAMPLE_LCD_CMD_BITS,
        .lcd_param_bits = EXAMPLE_LCD_PARAM_BITS,
        .spi_mode = 0,
        .trans_queue_depth = 10,
    };
    ESP_GOTO_ON_ERROR(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)EXAMPLE_LCD_SPI_NUM, &io_config, &lcd_io), err, TAG, "New panel IO failed");

    ESP_LOGD(TAG, "Install LCD driver");
    const esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = EXAMPLE_LCD_GPIO_RST,
        .color_space = EXAMPLE_LCD_COLOR_SPACE,
        .bits_per_pixel = EXAMPLE_LCD_BITS_PER_PIXEL,
    };
    ESP_GOTO_ON_ERROR(esp_lcd_new_panel_st7789(lcd_io, &panel_config, &lcd_panel), err, TAG, "New panel failed");

    esp_lcd_panel_reset(lcd_panel);
    esp_lcd_panel_init(lcd_panel);
    esp_lcd_panel_mirror(lcd_panel, true, true);
    esp_lcd_panel_disp_on_off(lcd_panel, true);

    /* LCD backlight on */
    ESP_ERROR_CHECK(gpio_set_level(EXAMPLE_LCD_GPIO_BL, EXAMPLE_LCD_BL_ON_LEVEL));

    return ret;

err:
    if (lcd_panel) {
        esp_lcd_panel_del(lcd_panel);
    }
    if (lcd_io) {
        esp_lcd_panel_io_del(lcd_io);
    }
    spi_bus_free(EXAMPLE_LCD_SPI_NUM);
    return ret;
}

static esp_err_t app_lcd_deinit(void)
{
    ESP_RETURN_ON_ERROR(esp_lcd_panel_del(lcd_panel), TAG, "LCD panel deinit failed");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_io_del(lcd_io), TAG, "LCD IO deinit failed");
    ESP_RETURN_ON_ERROR(spi_bus_free(EXAMPLE_LCD_SPI_NUM), TAG, "SPI BUS free failed");
    ESP_RETURN_ON_ERROR(gpio_reset_pin(EXAMPLE_LCD_GPIO_BL), TAG, "Reset BL pin failed");
    return ESP_OK;
}

static esp_err_t app_touch_init(void)
{
    /* Initialize I2C */
    const i2c_config_t i2c_conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = EXAMPLE_TOUCH_I2C_SDA,
        .sda_pullup_en = GPIO_PULLUP_DISABLE,
        .scl_io_num = EXAMPLE_TOUCH_I2C_SCL,
        .scl_pullup_en = GPIO_PULLUP_DISABLE,
        .master.clk_speed = EXAMPLE_TOUCH_I2C_CLK_HZ
    };
    ESP_RETURN_ON_ERROR(i2c_param_config(EXAMPLE_TOUCH_I2C_NUM, &i2c_conf), TAG, "I2C configuration failed");
    ESP_RETURN_ON_ERROR(i2c_driver_install(EXAMPLE_TOUCH_I2C_NUM, i2c_conf.mode, 0, 0, 0), TAG, "I2C initialization failed");

    /* Initialize touch HW */
    const esp_lcd_touch_config_t tp_cfg = {
        .x_max = EXAMPLE_LCD_H_RES,
        .y_max = EXAMPLE_LCD_V_RES,
        .rst_gpio_num = GPIO_NUM_NC, // Shared with LCD reset
        .int_gpio_num = EXAMPLE_TOUCH_GPIO_INT,
        .levels = {
            .reset = 0,
            .interrupt = 0,
        },
        .flags = {
            .swap_xy = 0,
            .mirror_x = 1,
            .mirror_y = 0,
        },
    };
    const esp_lcd_panel_io_i2c_config_t tp_io_config = ESP_LCD_TOUCH_IO_I2C_TT21100_CONFIG();
    ESP_RETURN_ON_ERROR(esp_lcd_new_panel_io_i2c((esp_lcd_i2c_bus_handle_t)EXAMPLE_TOUCH_I2C_NUM, &tp_io_config, &tp_io_handle), TAG, "");
    return esp_lcd_touch_new_i2c_tt21100(tp_io_handle, &tp_cfg, &touch_handle);
}

static esp_err_t app_touch_deinit(void)
{
    ESP_RETURN_ON_ERROR(esp_lcd_touch_del(touch_handle), TAG, "Touch deinit failed");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_io_del(tp_io_handle), TAG, "Touch IO deinit failed");
    ESP_RETURN_ON_ERROR(i2c_driver_delete(EXAMPLE_TOUCH_I2C_NUM), TAG, "I2C deinit failed");
    return ESP_OK;
}

static esp_err_t app_lvgl_init(void)
{
    /* Initialize LVGL */
    const lvgl_port_cfg_t lvgl_cfg = {
        .task_priority = 4,         /* LVGL task priority */
        .task_stack = 4096,         /* LVGL task stack size */
        .task_affinity = -1,        /* LVGL task pinned to core (-1 is no affinity) */
        .task_max_sleep_ms = 500,   /* Maximum sleep in LVGL task */
        .timer_period_ms = 5        /* LVGL timer tick period in ms */
    };
    ESP_RETURN_ON_ERROR(lvgl_port_init(&lvgl_cfg), TAG, "LVGL port initialization failed");

    /* Add LCD screen */
    ESP_LOGD(TAG, "Add LCD screen");
    const lvgl_port_display_cfg_t disp_cfg = {
        .io_handle = lcd_io,
        .panel_handle = lcd_panel,
        .buffer_size = EXAMPLE_LCD_H_RES * EXAMPLE_LCD_DRAW_BUFF_HEIGHT * sizeof(uint16_t),
        .double_buffer = EXAMPLE_LCD_DRAW_BUFF_DOUBLE,
        .hres = EXAMPLE_LCD_H_RES,
        .vres = EXAMPLE_LCD_V_RES,
        .monochrome = false,
        /* Rotation values must be same as used in esp_lcd for initial settings of the screen */
        .rotation = {
            .swap_xy = false,
            .mirror_x = true,
            .mirror_y = true,
        },
        .flags = {
            .buff_dma = true,
#if LVGL_VERSION_MAJOR >= 9
            .swap_bytes = true,
#endif
        }
    };
    lvgl_disp = lvgl_port_add_disp(&disp_cfg);

    /* Add touch input (for selected screen) */
    const lvgl_port_touch_cfg_t touch_cfg = {
        .disp = lvgl_disp,
        .handle = touch_handle,
    };
    lvgl_touch_indev = lvgl_port_add_touch(&touch_cfg);

    return ESP_OK;
}

static esp_err_t app_lvgl_deinit(void)
{
    ESP_RETURN_ON_ERROR(lvgl_port_remove_touch(lvgl_touch_indev), TAG, "LVGL touch removing failed");
    gpio_uninstall_isr_service();

    ESP_RETURN_ON_ERROR(lvgl_port_remove_disp(lvgl_disp), TAG, "LVGL disp removing failed");
    ESP_RETURN_ON_ERROR(lvgl_port_deinit(), TAG, "LVGL deinit failed");

    return ESP_OK;
}

static void _app_button_cb(lv_event_t *e)
{
    lv_disp_rotation_t rotation = lv_disp_get_rotation(lvgl_disp);
    rotation++;
    if (rotation > LV_DISPLAY_ROTATION_270) {
        rotation = LV_DISPLAY_ROTATION_0;
    }

    /* LCD HW rotation */
    lv_disp_set_rotation(lvgl_disp, rotation);
}

static void app_main_display(void)
{
    lv_obj_t *scr = lv_scr_act();

    /* Task lock */
    lvgl_port_lock(0);

    /* Your LVGL objects code here .... */

    /* Label */
    lv_obj_t *label = lv_label_create(scr);
    lv_obj_set_width(label, EXAMPLE_LCD_H_RES);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
#if LVGL_VERSION_MAJOR == 8
    lv_label_set_recolor(label, true);
    lv_label_set_text(label, "#FF0000 "LV_SYMBOL_BELL" Hello world Espressif and LVGL "LV_SYMBOL_BELL"#\n#FF9400 "LV_SYMBOL_WARNING" For simpler initialization, use BSP "LV_SYMBOL_WARNING" #");
#else
    lv_label_set_text(label, LV_SYMBOL_BELL" Hello world Espressif and LVGL "LV_SYMBOL_BELL"\n "LV_SYMBOL_WARNING" For simpler initialization, use BSP "LV_SYMBOL_WARNING);
#endif
    lv_obj_align(label, LV_ALIGN_CENTER, 0, -30);

    /* Button */
    lv_obj_t *btn = lv_btn_create(scr);
    label = lv_label_create(btn);
    lv_label_set_text_static(label, "Rotate screen");
    lv_obj_align(btn, LV_ALIGN_BOTTOM_MID, 0, -30);
    lv_obj_add_event_cb(btn, _app_button_cb, LV_EVENT_CLICKED, NULL);

    /* Task unlock */
    lvgl_port_unlock();
}

// Some resources are lazy allocated in the LCD driver, the threadhold is left for that case
#define TEST_MEMORY_LEAK_THRESHOLD (50)

static void check_leak(size_t start_free, size_t end_free, const char *type)
{
    ssize_t delta = start_free - end_free;
    printf("MALLOC_CAP_%s: Before %u bytes free, After %u bytes free (delta %d)\n", type, start_free, end_free, delta);
    TEST_ASSERT_GREATER_OR_EQUAL_MESSAGE(delta, TEST_MEMORY_LEAK_THRESHOLD, "memory leak");
}

TEST_CASE("Main test LVGL port", "[lvgl port]")
{
    size_t start_freemem_8bit = heap_caps_get_free_size(MALLOC_CAP_8BIT);
    size_t start_freemem_32bit = heap_caps_get_free_size(MALLOC_CAP_32BIT);

    ESP_LOGI(TAG, "Initialize LCD.");

    /* LCD HW initialization */
    TEST_ASSERT_EQUAL(app_lcd_init(), ESP_OK);

    /* Touch initialization */
    TEST_ASSERT_EQUAL(app_touch_init(), ESP_OK);

    size_t start_lvgl_freemem_8bit = heap_caps_get_free_size(MALLOC_CAP_8BIT);
    size_t start_lvgl_freemem_32bit = heap_caps_get_free_size(MALLOC_CAP_32BIT);

    ESP_LOGI(TAG, "Initialize LVGL.");

    /* LVGL initialization */
    TEST_ASSERT_EQUAL(app_lvgl_init(), ESP_OK);

    /* Show LVGL objects */
    app_main_display();

    vTaskDelay(5000 / portTICK_PERIOD_MS);

    /* LVGL deinit */
    TEST_ASSERT_EQUAL(app_lvgl_deinit(), ESP_OK);

    /* When using LVGL8, it takes some time to release all memory */
    vTaskDelay(1000 / portTICK_PERIOD_MS);

    ESP_LOGI(TAG, "LVGL deinitialized.");

    size_t end_lvgl_freemem_8bit = heap_caps_get_free_size(MALLOC_CAP_8BIT);
    size_t end_lvgl_freemem_32bit = heap_caps_get_free_size(MALLOC_CAP_32BIT);
    check_leak(start_lvgl_freemem_8bit, end_lvgl_freemem_8bit, "8BIT LVGL");
    check_leak(start_lvgl_freemem_32bit, end_lvgl_freemem_32bit, "32BIT LVGL");

    /* Touch deinit */
    TEST_ASSERT_EQUAL(app_touch_deinit(), ESP_OK);

    /* LCD deinit */
    TEST_ASSERT_EQUAL(app_lcd_deinit(), ESP_OK);

    vTaskDelay(1000 / portTICK_PERIOD_MS);

    ESP_LOGI(TAG, "LCD deinitilized.");

    size_t end_freemem_8bit = heap_caps_get_free_size(MALLOC_CAP_8BIT);
    size_t end_freemem_32bit = heap_caps_get_free_size(MALLOC_CAP_32BIT);
    check_leak(start_freemem_8bit, end_freemem_8bit, "8BIT");
    check_leak(start_freemem_32bit, end_freemem_32bit, "32BIT");

}

void app_main(void)
{
    printf("TEST ESP LVGL port\n\r");
    unity_run_menu();
}
