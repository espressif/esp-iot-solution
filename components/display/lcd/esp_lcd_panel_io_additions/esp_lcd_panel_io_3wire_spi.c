/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>

#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_check.h"
#include "esp_lcd_panel_io_interface.h"

#include "esp_lcd_panel_io_additions.h"

#define LCD_CMD_BYTES_MAX       (sizeof(uint32_t))  // Maximum number of bytes for LCD command
#define LCD_PARAM_BYTES_MAX     (sizeof(uint32_t))  // Maximum number of bytes for LCD parameter

#define DATA_DC_BIT_0           (0)     // DC bit = 0
#define DATA_DC_BIT_1           (1)     // DC bit = 1
#define DATA_NO_DC_BIT          (2)     // No DC bit
#define WRITE_ORDER_LSB_MASK    (0x01)  // Bit mask for LSB first write order
#define WRITE_ORDER_MSB_MASK    (0x80)  // Bit mask for MSB first write order

/**
 * @brief Enumeration of SPI lines
 */
typedef enum {
    CS = 0,
    SCL,
    SDA,
} spi_line_t;

/**
 * @brief Panel IO instance for 3-wire SPI interface
 *
 */
typedef struct {
    esp_lcd_panel_io_t base;                /*!< Base class of generic lcd panel io */
    panel_io_type_t cs_io_type;             /*!< IO type of CS line */
    int cs_io_num;                          /*!< IO used for CS line */
    panel_io_type_t scl_io_type;            /*!< IO type of SCL line */
    int scl_io_num;                         /*!< IO used for SCL line */
    panel_io_type_t sda_io_type;            /*!< IO type of SDA line */
    int sda_io_num;                         /*!< GPIO used for SDA line */
    esp_io_expander_handle_t io_expander;   /*!< IO expander handle, set to NULL if not used */
    uint32_t scl_half_period_us;            /*!< SCL half period in us */
    uint32_t lcd_cmd_bytes: 3;              /*!< Bytes of LCD command (1 ~ 4) */
    uint32_t cmd_dc_bit: 2;                 /*!< DC bit of command */
    uint32_t lcd_param_bytes: 3;            /*!< Bytes of LCD parameter (1 ~ 4) */
    uint32_t param_dc_bit: 2;               /*!< DC bit of parameter */
    uint32_t write_order_mask: 8;           /*!< Bit mask of write order */
    struct {
        uint32_t cs_high_active: 1;         /*!< If this flag is enabled, CS line is high active */
        uint32_t sda_scl_idle_high: 1;      /*!< If this flag is enabled, SDA and SCL line are high when idle */
        uint32_t scl_active_rising_edge: 1; /*!< If this flag is enabled, SCL line is active on rising edge */
        uint32_t del_keep_cs_inactive: 1;   /*!< If this flag is enabled, keep CS line inactive even if panel_io is deleted */
    } flags;
} esp_lcd_panel_io_3wire_spi_t;

static const char *TAG = "lcd_panel.io.3wire_spi";

static esp_err_t panel_io_rx_param(esp_lcd_panel_io_t *io, int lcd_cmd, void *param, size_t param_size);
static esp_err_t panel_io_tx_param(esp_lcd_panel_io_t *io, int lcd_cmd, const void *param, size_t param_size);
static esp_err_t panel_io_tx_color(esp_lcd_panel_io_t *io, int lcd_cmd, const void *color, size_t color_size);
static esp_err_t panel_io_del(esp_lcd_panel_io_t *io);
static esp_err_t panel_io_register_event_callbacks(esp_lcd_panel_io_handle_t io, const esp_lcd_panel_io_callbacks_t *cbs, void *user_ctx);

static esp_err_t set_line_level(esp_lcd_panel_io_3wire_spi_t *panel_io, spi_line_t line, uint32_t level);
static esp_err_t reset_line_io(esp_lcd_panel_io_3wire_spi_t *panel_io, spi_line_t line);
static esp_err_t spi_write_package(esp_lcd_panel_io_3wire_spi_t *panel_io, bool is_cmd, uint32_t data);

esp_err_t esp_lcd_new_panel_io_3wire_spi(const esp_lcd_panel_io_3wire_spi_config_t *io_config, esp_lcd_panel_io_handle_t *ret_io)
{
    ESP_RETURN_ON_FALSE(io_config && ret_io, ESP_ERR_INVALID_ARG, TAG, "Invalid argument");
    ESP_RETURN_ON_FALSE(io_config->expect_clk_speed <= PANEL_IO_3WIRE_SPI_CLK_MAX, ESP_ERR_INVALID_ARG, TAG, "Invalid Clock frequency");
    ESP_RETURN_ON_FALSE(io_config->lcd_cmd_bytes > 0 && io_config->lcd_cmd_bytes <= LCD_CMD_BYTES_MAX, ESP_ERR_INVALID_ARG,
                        TAG, "Invalid LCD command bytes");
    ESP_RETURN_ON_FALSE(io_config->lcd_param_bytes > 0 && io_config->lcd_param_bytes <= LCD_PARAM_BYTES_MAX, ESP_ERR_INVALID_ARG,
                        TAG, "Invalid LCD parameter bytes");

    const spi_line_config_t *line_config = &io_config->line_config;
    ESP_RETURN_ON_FALSE(line_config->io_expander || (line_config->cs_io_type == IO_TYPE_GPIO &&
                        line_config->scl_io_type == IO_TYPE_GPIO && line_config->sda_io_type == IO_TYPE_GPIO),
                        ESP_ERR_INVALID_ARG, TAG, "IO Expander handle is required if any IO is not gpio");

    esp_lcd_panel_io_3wire_spi_t *panel_io = calloc(1, sizeof(esp_lcd_panel_io_3wire_spi_t));
    ESP_RETURN_ON_FALSE(panel_io, ESP_ERR_NO_MEM, TAG, "No memory");

    panel_io->cs_io_type = line_config->cs_io_type;
    panel_io->cs_io_num = line_config->cs_gpio_num;
    panel_io->scl_io_type = line_config->scl_io_type;
    panel_io->scl_io_num = line_config->scl_gpio_num;
    panel_io->sda_io_type = line_config->sda_io_type;
    panel_io->sda_io_num = line_config->sda_gpio_num;
    panel_io->io_expander = line_config->io_expander;
    uint32_t expect_clk_speed = io_config->expect_clk_speed ? io_config->expect_clk_speed : PANEL_IO_3WIRE_SPI_CLK_MAX;
    panel_io->scl_half_period_us = 1000000 / (expect_clk_speed * 2);
    panel_io->lcd_cmd_bytes = io_config->lcd_cmd_bytes;
    panel_io->lcd_param_bytes = io_config->lcd_param_bytes;
    if (io_config->flags.use_dc_bit) {
        panel_io->param_dc_bit = io_config->flags.dc_zero_on_data ? DATA_DC_BIT_0 : DATA_DC_BIT_1;
        panel_io->cmd_dc_bit = io_config->flags.dc_zero_on_data ? DATA_DC_BIT_1 : DATA_DC_BIT_0;
    } else {
        panel_io->param_dc_bit = DATA_NO_DC_BIT;
        panel_io->cmd_dc_bit = DATA_NO_DC_BIT;
    }
    panel_io->write_order_mask = io_config->flags.lsb_first ? WRITE_ORDER_LSB_MASK : WRITE_ORDER_MSB_MASK;
    panel_io->flags.cs_high_active = io_config->flags.cs_high_active;
    panel_io->flags.del_keep_cs_inactive = io_config->flags.del_keep_cs_inactive;
    panel_io->flags.sda_scl_idle_high = io_config->spi_mode & 0x1;
    if (panel_io->flags.sda_scl_idle_high) {
        panel_io->flags.scl_active_rising_edge = (io_config->spi_mode & 0x2) ? 1 : 0;
    } else {
        panel_io->flags.scl_active_rising_edge = (io_config->spi_mode & 0x2) ? 0 : 1;
    }

    panel_io->base.rx_param = panel_io_rx_param;
    panel_io->base.tx_param = panel_io_tx_param;
    panel_io->base.tx_color = panel_io_tx_color;
    panel_io->base.del = panel_io_del;
    panel_io->base.register_event_callbacks = panel_io_register_event_callbacks;

    // Get GPIO mask and IO expander pin mask
    esp_err_t ret = ESP_OK;
    int64_t gpio_mask = 0;
    uint32_t expander_pin_mask = 0;
    if (panel_io->cs_io_type == IO_TYPE_GPIO) {
        gpio_mask |= BIT64(panel_io->cs_io_num);
    } else {
        expander_pin_mask |= panel_io->cs_io_num;
    }
    if (panel_io->scl_io_type == IO_TYPE_GPIO) {
        gpio_mask |= BIT64(panel_io->scl_io_num);
    } else {
        expander_pin_mask |= panel_io->scl_io_num;
    }
    if (panel_io->sda_io_type == IO_TYPE_GPIO) {
        gpio_mask |= BIT64(panel_io->sda_io_num);
    } else {
        expander_pin_mask |= panel_io->sda_io_num;
    }
    // Configure GPIOs
    if (gpio_mask) {
        ESP_GOTO_ON_ERROR(gpio_config(&((gpio_config_t) {
            .pin_bit_mask = gpio_mask,
            .mode = GPIO_MODE_OUTPUT,
            .pull_up_en = GPIO_PULLUP_DISABLE,
            .pull_down_en = GPIO_PULLDOWN_DISABLE,
            .intr_type = GPIO_INTR_DISABLE,
        })), err, TAG, "GPIO config failed");
    }
    // Configure pins of IO expander
    if (expander_pin_mask) {
        ESP_GOTO_ON_ERROR(esp_io_expander_set_dir(panel_io->io_expander, expander_pin_mask, IO_EXPANDER_OUTPUT), err,
                          TAG, "Expander set dir failed");
    }

    // Set CS, SCL and SDA to idle level
    uint32_t cs_idle_level = panel_io->flags.cs_high_active ? 0 : 1;
    uint32_t sda_scl_idle_level = panel_io->flags.sda_scl_idle_high ? 1 : 0;
    ESP_GOTO_ON_ERROR(set_line_level(panel_io, CS, cs_idle_level), err, TAG, "Set CS level failed");
    ESP_GOTO_ON_ERROR(set_line_level(panel_io, SCL, sda_scl_idle_level), err, TAG, "Set SCL level failed");
    ESP_GOTO_ON_ERROR(set_line_level(panel_io, SDA, sda_scl_idle_level), err, TAG, "Set SDA level failed");

    *ret_io = (esp_lcd_panel_io_handle_t)panel_io;
    ESP_LOGI(TAG, "Panel IO create success, version: %d.%d.%d", ESP_LCD_PANEL_IO_ADDITIONS_VER_MAJOR,
             ESP_LCD_PANEL_IO_ADDITIONS_VER_MINOR, ESP_LCD_PANEL_IO_ADDITIONS_VER_PATCH);
    return ESP_OK;

err:
    if (gpio_mask) {
        for (int i = 0; i < 64; i++) {
            if (gpio_mask & BIT64(i)) {
                gpio_reset_pin(i);
            }
        }
    }
    if (expander_pin_mask) {
        esp_io_expander_set_dir(panel_io->io_expander, expander_pin_mask, IO_EXPANDER_INPUT);
    }
    free(panel_io);
    return ret;
}

static esp_err_t panel_io_tx_param(esp_lcd_panel_io_t *io, int lcd_cmd, const void *param, size_t param_size)
{
    esp_lcd_panel_io_3wire_spi_t *panel_io = __containerof(io, esp_lcd_panel_io_3wire_spi_t, base);

    // Send command
    if (lcd_cmd >= 0) {
        ESP_RETURN_ON_ERROR(spi_write_package(panel_io, true, lcd_cmd), TAG, "SPI write package failed");
    }

    // Send parameter
    if (param != NULL && param_size > 0) {
        uint32_t param_data = 0;
        uint32_t param_bytes = panel_io->lcd_param_bytes;
        size_t param_count = param_size / param_bytes;

        // Iteratively get parameter packages and send them one by one
        for (int i = 0; i < param_count; i++) {
            param_data = 0;
            for (int j = 0; j < param_bytes; j++) {
                param_data |= ((uint8_t *)param)[i * param_bytes + j] << (j * 8);
            }
            ESP_RETURN_ON_ERROR(spi_write_package(panel_io, false, param_data), TAG, "SPI write package failed");
        }
    }

    return ESP_OK;
}

static esp_err_t panel_io_del(esp_lcd_panel_io_t *io)
{
    esp_lcd_panel_io_3wire_spi_t *panel_io = __containerof(io, esp_lcd_panel_io_3wire_spi_t, base);

    if (!panel_io->flags.del_keep_cs_inactive) {
        ESP_RETURN_ON_ERROR(reset_line_io(panel_io, CS), TAG, "Reset CS line failed");
    } else {
        ESP_LOGW(TAG, "Delete but keep CS line inactive");
    }
    ESP_RETURN_ON_ERROR(reset_line_io(panel_io, SCL), TAG, "Reset SCL line failed");
    ESP_RETURN_ON_ERROR(reset_line_io(panel_io, SDA), TAG, "Reset SDA line failed");
    free(panel_io);

    return ESP_OK;
}

/**
 * @brief This function is not implemented and only for compatibility
 */
static esp_err_t panel_io_rx_param(esp_lcd_panel_io_t *io, int lcd_cmd, void *param, size_t param_size)
{
    ESP_LOGE(TAG, "Rx param is not supported");

    return ESP_FAIL;
}

/**
 * @brief This function is not implemented and only for compatibility
 */
static esp_err_t panel_io_tx_color(esp_lcd_panel_io_t *io, int lcd_cmd, const void *color, size_t color_size)
{
    ESP_LOGE(TAG, "Tx color is not supported");

    return ESP_FAIL;
}

/**
 * @brief This function is not implemented and only for compatibility
 */
static esp_err_t panel_io_register_event_callbacks(esp_lcd_panel_io_handle_t io, const esp_lcd_panel_io_callbacks_t *cbs, void *user_ctx)
{
    ESP_LOGE(TAG, "Register event callbacks is not supported");

    return ESP_FAIL;
}

/**
 * @brief Set the level of specified line.
 *
 * This function can use GPIO or IO expander according to the type of line
 *
 * @param[in]  panel_io Pointer to panel IO instance
 * @param[in]  line     Target line
 * @param[out] level    Target level, 0 - Low, 1 - High
 *
 * @return
 *      - ESP_OK:              Success
 *      - ESP_ERR_INVALID_ARG: Invalid argument
 *      - Others:              Fail
 */
static esp_err_t set_line_level(esp_lcd_panel_io_3wire_spi_t *panel_io, spi_line_t line, uint32_t level)
{
    panel_io_type_t line_type = IO_TYPE_GPIO;
    int line_io = 0;
    switch (line) {
    case CS:
        line_type = panel_io->cs_io_type;
        line_io = panel_io->cs_io_num;
        break;
    case SCL:
        line_type = panel_io->scl_io_type;
        line_io = panel_io->scl_io_num;
        break;
    case SDA:
        line_type = panel_io->sda_io_type;
        line_io = panel_io->sda_io_num;
        break;
    default:
        break;
    }

    if (line_type == IO_TYPE_GPIO) {
        return gpio_set_level(line_io, level);
    } else {
        return esp_io_expander_set_level(panel_io->io_expander, (esp_io_expander_pin_num_t)line_io, level != 0);
    }
}

/**
 * @brief Reset the IO of specified line
 *
 * This function can use GPIO or IO expander according to the type of line
 *
 * @param[in]  panel_io Pointer to panel IO instance
 * @param[in]  line     Target line
 *
 * @return
 *      - ESP_OK:              Success
 *      - ESP_ERR_INVALID_ARG: Invalid argument
 *      - Others:              Fail
 */
static esp_err_t reset_line_io(esp_lcd_panel_io_3wire_spi_t *panel_io, spi_line_t line)
{
    panel_io_type_t line_type = IO_TYPE_GPIO;
    int line_io = 0;
    switch (line) {
    case CS:
        line_type = panel_io->cs_io_type;
        line_io = panel_io->cs_io_num;
        break;
    case SCL:
        line_type = panel_io->scl_io_type;
        line_io = panel_io->scl_io_num;
        break;
    case SDA:
        line_type = panel_io->sda_io_type;
        line_io = panel_io->sda_io_num;
        break;
    default:
        break;
    }

    if (line_type == IO_TYPE_GPIO) {
        return gpio_reset_pin(line_io);
    } else {
        return esp_io_expander_set_dir(panel_io->io_expander, (esp_io_expander_pin_num_t)line_io, IO_EXPANDER_INPUT);
    }
}

/**
 * @brief Delay for given microseconds
 *
 * @note  This function uses `esp_rom_delay_us()` for delays < 1000us and `vTaskDelay()` for longer delays.
 *
 * @param[in] delay_us Delay time in microseconds
 *
 */
static void delay_us(uint32_t delay_us)
{
    if (delay_us >= 1000) {
        vTaskDelay(pdMS_TO_TICKS(delay_us / 1000));
    } else {
        esp_rom_delay_us(delay_us);
    }
}

/**
 * @brief Write one byte to LCD panel
 *
 * @param[in] panel_io Pointer to panel IO instance
 * @param[in] dc_bit   DC bit
 * @param[in] data     8-bit data to write
 *
 * @return
 *      - ESP_OK:              Success
 *      - ESP_ERR_INVALID_ARG: Invalid argument
 *      - Others:              Fail
 */
static esp_err_t spi_write_byte(esp_lcd_panel_io_3wire_spi_t *panel_io, int dc_bit, uint8_t data)
{
    uint16_t data_temp = data;
    uint8_t data_bits = (dc_bit != DATA_NO_DC_BIT) ? 9 : 8;
    uint16_t write_order_mask = panel_io->write_order_mask;
    uint32_t scl_active_befor_level = panel_io->flags.scl_active_rising_edge ? 0 : 1;
    uint32_t scl_active_after_level = !scl_active_befor_level;
    uint32_t scl_half_period_us = panel_io->scl_half_period_us;

    for (uint8_t i = 0; i < data_bits; i++) {
        // Send DC bit first
        if (data_bits == 9 && i == 0) {
            ESP_RETURN_ON_ERROR(set_line_level(panel_io, SDA, dc_bit), TAG, "Set SDA level failed");
        } else { // Then send data bit
            // SDA set to data bit
            ESP_RETURN_ON_ERROR(set_line_level(panel_io, SDA, data_temp & write_order_mask), TAG, "Set SDA level failed");
            // Get next bit
            data_temp = (write_order_mask == WRITE_ORDER_LSB_MASK) ? data_temp >> 1 : data_temp << 1;
        }
        // Generate SCL active edge
        ESP_RETURN_ON_ERROR(set_line_level(panel_io, SCL, scl_active_befor_level), TAG, "Set SCL level failed");
        delay_us(scl_half_period_us);
        ESP_RETURN_ON_ERROR(set_line_level(panel_io, SCL, scl_active_after_level), TAG, "Set SCL level failed");
        delay_us(scl_half_period_us);
    }

    return ESP_OK;
}

/**
 * @brief Write a package of data to LCD panel in big-endian order
 *
 * @param[in] panel_io Pointer to panel IO instance
 * @param[in] is_cmd   True for command, false for data
 * @param[in] data     Data to write, with
 *
 * @return
 *      - ESP_OK:              Success
 *      - ESP_ERR_INVALID_ARG: Invalid argument
 *      - Others:              Fail
 */
static esp_err_t spi_write_package(esp_lcd_panel_io_3wire_spi_t *panel_io, bool is_cmd, uint32_t data)
{
    uint32_t data_bytes = is_cmd ? panel_io->lcd_cmd_bytes : panel_io->lcd_param_bytes;
    uint32_t cs_idle_level = panel_io->flags.cs_high_active ? 0 : 1;
    uint32_t sda_scl_idle_level = panel_io->flags.sda_scl_idle_high ? 1 : 0;
    uint32_t scl_active_befor_level = panel_io->flags.scl_active_rising_edge ? 0 : 1;
    uint32_t time_us = panel_io->scl_half_period_us;
    // Swap command bytes order due to different endianness
    uint32_t swap_data = SPI_SWAP_DATA_TX(data, data_bytes * 8);
    int data_dc_bit = is_cmd ? panel_io->cmd_dc_bit : panel_io->param_dc_bit;

    // CS active
    ESP_RETURN_ON_ERROR(set_line_level(panel_io, CS, !cs_idle_level), TAG, "Set CS level failed");
    delay_us(time_us);
    ESP_RETURN_ON_ERROR(set_line_level(panel_io, SCL, scl_active_befor_level), TAG, "Set SCL level failed");
    // Send data byte by byte
    for (int i = 0; i < data_bytes; i++) {
        // Only set DC bit for the first byte
        if (i == 0) {
            ESP_RETURN_ON_ERROR(spi_write_byte(panel_io, data_dc_bit, swap_data & 0xff), TAG, "SPI write byte failed");
        } else {
            ESP_RETURN_ON_ERROR(spi_write_byte(panel_io, DATA_NO_DC_BIT, swap_data & 0xff), TAG, "SPI write byte failed");
        }
        swap_data >>= 8;
    }
    ESP_RETURN_ON_ERROR(set_line_level(panel_io, SCL, sda_scl_idle_level), TAG, "Set SCL level failed");
    ESP_RETURN_ON_ERROR(set_line_level(panel_io, SDA, sda_scl_idle_level), TAG, "Set SDA level failed");
    delay_us(time_us);
    // CS inactive
    ESP_RETURN_ON_ERROR(set_line_level(panel_io, CS, cs_idle_level), TAG, "Set CS level failed");
    delay_us(time_us);

    return ESP_OK;
}
