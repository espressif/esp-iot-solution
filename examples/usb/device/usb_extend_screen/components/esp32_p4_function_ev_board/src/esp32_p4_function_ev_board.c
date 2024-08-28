/*
 * SPDX-FileCopyrightText: 2022-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <string.h>
#include <stddef.h>
#include <time.h>
#include <sys/time.h>

#include "esp_ldo_regulator.h"
#include "esp_codec_dev_defaults.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_spiffs.h"
#include "esp_vfs_fat.h"
#include "driver/i2c_master.h"
#include "driver/i2s_std.h"
#include "driver/gpio.h"
#include "sdmmc_cmd.h"

#include "bsp/display.h"
#include "bsp_lvgl_port.h"
#include "bsp/touch.h"
#include "bsp/esp32_p4_function_ev_board.h"
#include "bsp_err_check.h"
#include "driver/ledc.h"

#define BSP_ES7210_CODEC_ADDR   (0x82)

/* Can be used for `i2s_std_gpio_config_t` and/or `i2s_std_config_t` initialization */
#define BSP_I2S_GPIO_CFG       \
    {                          \
        .mclk = BSP_I2S_MCLK,  \
        .bclk = BSP_I2S_SCLK,  \
        .ws = BSP_I2S_LCLK,    \
        .dout = BSP_I2S_DOUT,  \
        .din = BSP_I2S_DSIN,   \
        .invert_flags = {      \
            .mclk_inv = false, \
            .bclk_inv = false, \
            .ws_inv = false,   \
        },                     \
    }

/* This configuration is used by default in `bsp_audio_init()` */
#define BSP_I2S_DUPLEX_MONO_CFG(_sample_rate)                                                         \
    {                                                                                                 \
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(_sample_rate),                                          \
        .slot_cfg = I2S_STD_PHILIP_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO), \
        .gpio_cfg = BSP_I2S_GPIO_CFG,                                                                 \
    }

#define USE_I2S_RX_CHAN     (1)

static const char *TAG = "P4-FUNCTION-EV-BOARD";

static bool i2c_initialized = false;
static i2c_master_bus_handle_t i2c_bus_handle;

static const audio_codec_data_if_t *i2s_data_if = NULL;  /* Codec data interface */
static i2s_chan_handle_t i2s_tx_chan = NULL;
#if USE_I2S_RX_CHAN
static i2s_chan_handle_t i2s_rx_chan = NULL;
#endif

static lv_indev_t *disp_indev = NULL;
static int lcd_brightness = 0;

static sdmmc_card_t *bsp_sdcard = NULL;    // Global SD card handler

/**************************************************************************************************
 *
 * I2C Function
 *
 **************************************************************************************************/
esp_err_t bsp_i2c_init(void)
{
    /* I2C was initialized before */
    if (i2c_initialized) {
        return ESP_OK;
    }

    i2c_master_bus_config_t i2c_mst_config = {
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .i2c_port = BSP_I2C_NUM,
        .scl_io_num = BSP_I2C_SCL,
        .sda_io_num = BSP_I2C_SDA,
        .flags.enable_internal_pullup = false,  // no pull-up
    };
    BSP_ERROR_CHECK_RETURN_ERR(i2c_new_master_bus(&i2c_mst_config, &i2c_bus_handle));

    i2c_initialized = true;

    return ESP_OK;
}

esp_err_t bsp_i2c_deinit(void)
{
    BSP_ERROR_CHECK_RETURN_ERR(i2c_del_master_bus(i2c_bus_handle));
    return ESP_OK;
}

esp_err_t bsp_get_i2c_bus_handle(i2c_master_bus_handle_t *handle)
{
    if (handle == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    *handle = i2c_bus_handle;
    return ESP_OK;
}

/**************************************************************************************************
 *
 * SPIFFS Function
 *
 **************************************************************************************************/
esp_err_t bsp_spiffs_mount(void)
{
    esp_vfs_spiffs_conf_t conf = {
        .base_path = CONFIG_BSP_SPIFFS_MOUNT_POINT,
        .partition_label = CONFIG_BSP_SPIFFS_PARTITION_LABEL,
        .max_files = CONFIG_BSP_SPIFFS_MAX_FILES,
#ifdef CONFIG_BSP_SPIFFS_FORMAT_ON_MOUNT_FAIL
        .format_if_mount_failed = true,
#else
        .format_if_mount_failed = false,
#endif
    };

    esp_err_t ret_val = esp_vfs_spiffs_register(&conf);

    BSP_ERROR_CHECK_RETURN_ERR(ret_val);

    size_t total = 0, used = 0;
    ret_val = esp_spiffs_info(conf.partition_label, &total, &used);
    if (ret_val != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get SPIFFS partition information (%s)", esp_err_to_name(ret_val));
    } else {
        ESP_LOGI(TAG, "Partition size: total: %d, used: %d", total, used);
    }

    return ret_val;
}

esp_err_t bsp_spiffs_unmount(void)
{
    return esp_vfs_spiffs_unregister(CONFIG_BSP_SPIFFS_PARTITION_LABEL);
}

/**************************************************************************************************
 *
 * SD card Function
 *
 **************************************************************************************************/
sdmmc_card_t *bsp_sdcard_mount(void)
{
    esp_err_t ret = ESP_OK;

    bsp_ldo_power_on();

    const esp_vfs_fat_sdmmc_mount_config_t mount_config = {
#ifdef CONFIG_BSP_SD_FORMAT_ON_MOUNT_FAIL
        .format_if_mount_failed = true,
#else
        .format_if_mount_failed = false,
#endif
        .max_files = 5,
        .allocation_unit_size = 64 * 1024
    };

#if CONFIG_BSP_SD_HOST_SPI
    sdmmc_host_t host = SDSPI_HOST_DEFAULT();
    spi_bus_config_t bus_cfg = {
        .mosi_io_num = BSP_SD_MOSI,
        .miso_io_num = BSP_SD_MISO,
        .sclk_io_num = BSP_SD_CLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 4000,
    };
    BSP_ERROR_CHECK_RETURN_NULL(spi_bus_initialize(host.slot, &bus_cfg, SDSPI_DEFAULT_DMA));

    sdspi_device_config_t slot_config = SDSPI_DEVICE_CONFIG_DEFAULT();
    slot_config.gpio_cs = BSP_SD_CS;
    slot_config.host_id = host.slot;

    ret = esp_vfs_fat_sdspi_mount(BSP_SD_MOUNT_POINT, &host, &slot_config, &mount_config, &bsp_sdcard);
#elif CONFIG_BSP_SD_HOST_SDMMC
    sdmmc_host_t host = SDMMC_HOST_DEFAULT();
    host.slot = SDMMC_HOST_SLOT_0;
    host.max_freq_khz = SDMMC_FREQ_HIGHSPEED;
    sdmmc_slot_config_t slot_config = SDMMC_SLOT_CONFIG_DEFAULT();
    slot_config.width = 4;
    slot_config.cmd = BSP_SD_CMD;
    slot_config.clk = BSP_SD_CLK;
    slot_config.d0 = BSP_SD_D0;
    slot_config.d1 = BSP_SD_D1;
    slot_config.d2 = BSP_SD_D2;
    slot_config.d3 = BSP_SD_D3;
    slot_config.d4 = 0;
    slot_config.d5 = 0;
    slot_config.d6 = 0;
    slot_config.d7 = 0;

    ret = esp_vfs_fat_sdmmc_mount(BSP_SD_MOUNT_POINT, &host, &slot_config, &mount_config, &bsp_sdcard);
#endif

    if (ret != ESP_OK) {
        if (ret == ESP_FAIL) {
            ESP_LOGE(TAG, "Failed to mount filesystem. "
                     "If you want the card to be formatted, set the CONFIG_EXAMPLE_FORMAT_IF_MOUNT_FAILED menuconfig option.");
        } else {
            ESP_LOGE(TAG, "Failed to initialize the card (%s). "
                     "Make sure SD card lines have pull-up resistors in place.", esp_err_to_name(ret));
        }
        return NULL;
    }
    ESP_LOGI(TAG, "Filesystem mounted");

    sdmmc_card_print_info(stdout, bsp_sdcard);

    return bsp_sdcard;
}

esp_err_t bsp_sdcard_unmount(void)
{
    return esp_vfs_fat_sdcard_unmount(BSP_SD_MOUNT_POINT, bsp_sdcard);
}

/**************************************************************************************************
 *
 * I2S Audio Function
 *
 **************************************************************************************************/
#define EXAMPLE_BUFF_SIZE               2048

static void i2s_example_write_task(void *args)
{
    uint8_t *w_buf = (uint8_t *)calloc(1, EXAMPLE_BUFF_SIZE);
    assert(w_buf); // Check if w_buf allocation success

    /* Assign w_buf */
    for (int i = 0; i < EXAMPLE_BUFF_SIZE; i += 8) {
        w_buf[i]     = 0x12;
        w_buf[i + 1] = 0x34;
        w_buf[i + 2] = 0x56;
        w_buf[i + 3] = 0x78;
        w_buf[i + 4] = 0x9A;
        w_buf[i + 5] = 0xBC;
        w_buf[i + 6] = 0xDE;
        w_buf[i + 7] = 0xF0;
    }

    size_t w_bytes = EXAMPLE_BUFF_SIZE;

    // /* (Optional) Preload the data before enabling the TX channel, so that the valid data can be transmitted immediately */
    // while (w_bytes == EXAMPLE_BUFF_SIZE) {
    //     /* Here we load the target buffer repeatedly, until all the DMA buffers are preloaded */
    //     ESP_ERROR_CHECK(i2s_channel_preload_data(tx_chan, w_buf, EXAMPLE_BUFF_SIZE, &w_bytes));
    // }

    /* Enable the TX channel */
    // ESP_ERROR_CHECK(i2s_channel_enable(i2s_tx_chan));
    while (1) {
        /* Write i2s data */
        if (i2s_channel_write(i2s_tx_chan, w_buf, EXAMPLE_BUFF_SIZE, &w_bytes, 1000) == ESP_OK) {
            printf("Write Task: i2s write %d bytes\n", w_bytes);
        } else {
            printf("Write Task: i2s write failed\n");
        }
        vTaskDelay(pdMS_TO_TICKS(200));
    }
    free(w_buf);
    vTaskDelete(NULL);
}

esp_err_t bsp_audio_init(const i2s_std_config_t *i2s_config)
{
    esp_err_t ret = ESP_FAIL;
#if USE_I2S_RX_CHAN
    if (i2s_tx_chan && i2s_rx_chan) {
#else
    if (i2s_tx_chan) {
#endif
        /* Audio was initialized before */
        return ESP_OK;
    }

    /* Setup I2S peripheral */
    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(CONFIG_BSP_I2S_NUM, I2S_ROLE_MASTER);
    chan_cfg.auto_clear = true; // Auto clear the legacy data in the DMA buffer
#if USE_I2S_RX_CHAN
    BSP_ERROR_CHECK_RETURN_ERR(i2s_new_channel(&chan_cfg, &i2s_tx_chan, &i2s_rx_chan));
#else
    BSP_ERROR_CHECK_RETURN_ERR(i2s_new_channel(&chan_cfg, &i2s_tx_chan, NULL));
#endif

    /* Setup I2S channels */
    i2s_std_config_t std_cfg_default = BSP_I2S_DUPLEX_MONO_CFG(22050);
    std_cfg_default.clk_cfg.mclk_multiple = I2S_MCLK_MULTIPLE_384;
    const i2s_std_config_t *p_i2s_cfg = &std_cfg_default;
    if (i2s_config != NULL) {
        p_i2s_cfg = i2s_config;
    }

    if (i2s_tx_chan != NULL) {
        ESP_GOTO_ON_ERROR(i2s_channel_init_std_mode(i2s_tx_chan, p_i2s_cfg), err, TAG, "I2S channel initialization failed");
        ESP_GOTO_ON_ERROR(i2s_channel_enable(i2s_tx_chan), err, TAG, "I2S enabling failed");
    }
#if USE_I2S_RX_CHAN
    if (i2s_rx_chan != NULL) {
        ESP_GOTO_ON_ERROR(i2s_channel_init_std_mode(i2s_rx_chan, p_i2s_cfg), err, TAG, "I2S channel initialization failed");
        ESP_GOTO_ON_ERROR(i2s_channel_enable(i2s_rx_chan), err, TAG, "I2S enabling failed");
    }
#endif

    audio_codec_i2s_cfg_t i2s_cfg = {
        .port = CONFIG_BSP_I2S_NUM,
        .tx_handle = i2s_tx_chan,
#if USE_I2S_RX_CHAN
        .rx_handle = i2s_rx_chan,
#else
        .rx_handle = NULL,
#endif
    };
    i2s_data_if = audio_codec_new_i2s_data(&i2s_cfg);
    BSP_NULL_CHECK_GOTO(i2s_data_if, err);

    // xTaskCreate(i2s_example_write_task, "i2s_example_write_task", 4096, NULL, 5, NULL);

    return ESP_OK;

err:
    if (i2s_tx_chan) {
        i2s_del_channel(i2s_tx_chan);
    }
#if USE_I2S_RX_CHAN
    if (i2s_rx_chan) {
        i2s_del_channel(i2s_rx_chan);
    }
#endif

    return ret;
}

esp_codec_dev_handle_t bsp_audio_codec_speaker_init(void)
{
    if (i2s_data_if == NULL) {
        /* Initialize I2C */
        BSP_ERROR_CHECK_RETURN_NULL(bsp_i2c_init());
        /* Configure I2S peripheral and Power Amplifier */
        BSP_ERROR_CHECK_RETURN_NULL(bsp_audio_init(NULL));
    }
    assert(i2s_data_if);

    const audio_codec_gpio_if_t *gpio_if = audio_codec_new_gpio();

    audio_codec_i2c_cfg_t i2c_cfg = {
        .port = BSP_I2C_NUM,
        .addr = ES8311_CODEC_DEFAULT_ADDR,
        .bus_handle = i2c_bus_handle,
    };
    const audio_codec_ctrl_if_t *i2c_ctrl_if = audio_codec_new_i2c_ctrl(&i2c_cfg);
    BSP_NULL_CHECK(i2c_ctrl_if, NULL);

    esp_codec_dev_hw_gain_t gain = {
        .pa_voltage = 5.0,
        .codec_dac_voltage = 3.3,
    };

    es8311_codec_cfg_t es8311_cfg = {
        .ctrl_if = i2c_ctrl_if,
        .gpio_if = gpio_if,
        .codec_mode = ESP_CODEC_DEV_TYPE_OUT,
        .pa_pin = BSP_POWER_AMP_IO,
        .pa_reverted = false,
        .master_mode = false,
        .use_mclk = true,
        .digital_mic = false,
        .invert_mclk = false,
        .invert_sclk = false,
        .hw_gain = gain,
    };
    const audio_codec_if_t *es8311_dev = es8311_codec_new(&es8311_cfg);
    BSP_NULL_CHECK(es8311_dev, NULL);

    esp_codec_dev_cfg_t codec_dev_cfg = {
        .dev_type = ESP_CODEC_DEV_TYPE_IN_OUT,
        .codec_if = es8311_dev,
        .data_if = i2s_data_if,
    };
    return esp_codec_dev_new(&codec_dev_cfg);
}

esp_codec_dev_handle_t bsp_audio_codec_microphone_init(void)
{
    if (i2s_data_if == NULL) {
        /* Initialize I2C */
        BSP_ERROR_CHECK_RETURN_NULL(bsp_i2c_init());
        /* Configure I2S peripheral and Power Amplifier */
        BSP_ERROR_CHECK_RETURN_NULL(bsp_audio_init(NULL));
    }
    assert(i2s_data_if);

    const audio_codec_gpio_if_t *gpio_if = audio_codec_new_gpio();

    audio_codec_i2c_cfg_t i2c_cfg = {
        .port = BSP_I2C_NUM,
        .addr = ES8311_CODEC_DEFAULT_ADDR,
        .bus_handle = i2c_bus_handle,
    };
    const audio_codec_ctrl_if_t *i2c_ctrl_if = audio_codec_new_i2c_ctrl(&i2c_cfg);
    BSP_NULL_CHECK(i2c_ctrl_if, NULL);

    esp_codec_dev_hw_gain_t gain = {
        .pa_voltage = 5.0,
        .codec_dac_voltage = 3.3,
    };

    es8311_codec_cfg_t es8311_cfg = {
        .ctrl_if = i2c_ctrl_if,
        .gpio_if = gpio_if,
        .codec_mode = ESP_CODEC_DEV_WORK_MODE_BOTH,
        .pa_pin = BSP_POWER_AMP_IO,
        .pa_reverted = false,
        .master_mode = false,
        .use_mclk = true,
        .digital_mic = false,
        .invert_mclk = false,
        .invert_sclk = false,
        .hw_gain = gain,
    };

    const audio_codec_if_t *es8311_dev = es8311_codec_new(&es8311_cfg);
    BSP_NULL_CHECK(es8311_dev, NULL);

    esp_codec_dev_cfg_t codec_es8311_dev_cfg = {
        .dev_type = ESP_CODEC_DEV_TYPE_IN,
        .codec_if = es8311_dev,
        .data_if = i2s_data_if,
    };
    return esp_codec_dev_new(&codec_es8311_dev_cfg);
}

/**********************************************************************************************************
 *
 * Display Function
 *
 **********************************************************************************************************/
esp_err_t bsp_display_brightness_init(void)
{
    if (BSP_LCD_BACKLIGHT < 0) {
        ESP_LOGW(TAG, "LCD backlight control is not supported on this board");
        return ESP_ERR_NOT_SUPPORTED;
    }

    // Setup LEDC peripheral for PWM backlight control
    const ledc_channel_config_t LCD_backlight_channel = {
        .gpio_num = BSP_LCD_BACKLIGHT,
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
        .freq_hz = 1000,
        .clk_cfg = LEDC_AUTO_CLK
    };

    BSP_ERROR_CHECK_RETURN_ERR(ledc_timer_config(&LCD_backlight_timer));
    BSP_ERROR_CHECK_RETURN_ERR(ledc_channel_config(&LCD_backlight_channel));

    return ESP_OK;
}

esp_err_t bsp_display_brightness_set(int brightness_percent)
{
    if (BSP_LCD_BACKLIGHT < 0) {
        ESP_LOGW(TAG, "LCD backlight control is not supported on this board");
        return ESP_ERR_NOT_SUPPORTED;
    }

    if (brightness_percent == lcd_brightness) {
        return ESP_OK;
    }

    if (brightness_percent > BSP_LCD_BACKLIGHT_BRIGHTNESS_MAX) {
        brightness_percent = BSP_LCD_BACKLIGHT_BRIGHTNESS_MAX;
    }
    if (brightness_percent < BSP_LCD_BACKLIGHT_BRIGHTNESS_MIN) {
        brightness_percent = BSP_LCD_BACKLIGHT_BRIGHTNESS_MIN;
    }

    ESP_LOGI(TAG, "Setting LCD backlight: %d%%", brightness_percent);
    uint32_t duty_cycle = (1023 * brightness_percent) / 100; // LEDC resolution set to 10bits, thus: 100% = 1023
    BSP_ERROR_CHECK_RETURN_ERR(ledc_set_duty(LEDC_LOW_SPEED_MODE, LCD_LEDC_CH, duty_cycle));
    BSP_ERROR_CHECK_RETURN_ERR(ledc_update_duty(LEDC_LOW_SPEED_MODE, LCD_LEDC_CH));
    lcd_brightness = brightness_percent;

    return ESP_OK;
}

int bsp_display_brightness_get(void)
{
    if (BSP_LCD_BACKLIGHT < 0) {
        ESP_LOGW(TAG, "LCD backlight control is not supported on this board");
        return 100;
    }

    return lcd_brightness;
}

esp_err_t bsp_display_backlight_off(void)
{
    return bsp_display_brightness_set(0);
}

esp_err_t bsp_display_backlight_on(void)
{
    return bsp_display_brightness_set(BSP_LCD_BACKLIGHT_BRIGHTNESS_MAX);
}

lv_disp_t *bsp_display_start(void)
{
    return bsp_display_start_with_config(NULL);
}

lv_disp_t *bsp_display_start_with_config(const bsp_display_cfg_t *cfg)
{
    (void)cfg;
    bsp_display_config_t disp_config = {
        .dpi_fb_buf_num = LVGL_PORT_LCD_DPI_BUFFER_NUMS,
    };
    lv_disp_t *disp = NULL;

    esp_lcd_panel_handle_t lcd = NULL;           // LCD panel handle
    esp_lcd_touch_handle_t tp = NULL;            // LCD touch panel handle

    BSP_ERROR_CHECK_RETURN_NULL(bsp_display_new(&disp_config, &lcd, NULL));
    bsp_touch_new(NULL, &tp);
    BSP_ERROR_CHECK_RETURN_NULL(bsp_lvgl_port_init(lcd, tp, &disp, &disp_indev));
    bsp_display_brightness_init();

    return disp;
}

lv_indev_t *bsp_display_get_input_dev(void)
{
    return disp_indev;
}

bool bsp_display_lock(uint32_t timeout_ms)
{
    return bsp_lvgl_port_lock(timeout_ms);
}

void bsp_display_unlock(void)
{
    bsp_lvgl_port_unlock();
}

void bsp_ldo_power_on(void)
{
    static bool is_ldo_powered = false;

    if (is_ldo_powered) {
        return;
    }
    is_ldo_powered = true;

    esp_ldo_channel_handle_t ldo_mipi_phy = NULL;
    esp_ldo_channel_config_t ldo_mipi_phy_config = {
        .chan_id = BSP_LDO_MIPI_CHAN,
        .voltage_mv = BSP_LDO_MIPI_VOLTAGE_MV,
    };
    ESP_ERROR_CHECK(esp_ldo_acquire_channel(&ldo_mipi_phy_config, &ldo_mipi_phy));

    esp_ldo_channel_config_t ldo_sd_probe_config = {
        .chan_id = BSP_LDO_PROBE_SD_CHAN,
        .voltage_mv = BSP_LDO_PROBE_SD_VOLTAGE_MV,
    };
    ESP_ERROR_CHECK(esp_ldo_acquire_channel(&ldo_sd_probe_config, &ldo_mipi_phy));
}
