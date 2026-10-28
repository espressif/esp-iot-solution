/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stddef.h>
#include "esp_err.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"

#define PARALLEL_LINES  16

#define EPAPER_BUSY() gpio_get_level((gpio_num_t)CONFIG_EPAPER_PIN_NUM_BUSY)

#define EPAPER_W 128
#define EPAPER_H 296
#define EPAPER_SCREEN_BUFFER_SIZE (EPAPER_W * EPAPER_H / 8)

typedef struct {
    spi_bus_config_t buscfg;
    spi_device_interface_config_t devcfg;
    gpio_config_t io_conf;
} epaper_config_t;

typedef struct {
    uint8_t cmd;
    uint8_t data[16];
    uint8_t databytes; /* No of data in data; bit 7 = delay after set; 0xFF = end of cmds. */
} epaper_init_cmd_t;

typedef struct {
    uint8_t *image;
    uint16_t width;
    uint16_t height;
    uint16_t width_memory;
    uint16_t height_memory;
    uint16_t color;
    uint16_t rotate;
    uint16_t width_byte;
    uint16_t height_byte;
} epaper_paint_t;

typedef void *epaper_handle_t;

#define EPAPER_UPDATE_CTRL_CMD                 0x22
#define EPAPER_UPDATE_SEQUENCE_DATA            0xF7
#define EPAPER_UPDATE_SEQUENCE_ACTIVATE_CMD    0x20

#define ROTATE_0    0
#define ROTATE_90   90
#define ROTATE_180  180
#define ROTATE_270  270

#define WHITE 0xFF
#define BLACK 0x00

void epaper_draw_point(epaper_handle_t epaper_handle, uint16_t x_point, uint16_t y_point, uint16_t color);
void epaper_draw_line(epaper_handle_t epaper_handle, uint16_t x_start, uint16_t y_start, uint16_t x_end, uint16_t y_end, uint16_t color);
void epaper_draw_rectangle(epaper_handle_t epaper_handle, uint16_t x_start, uint16_t y_start, uint16_t x_end, uint16_t y_end, uint16_t color, uint8_t mode);
void epaper_print_char(epaper_handle_t epaper_handle, uint16_t x, uint16_t y, uint16_t chr, uint16_t size, uint16_t color);
void epaper_print_string(epaper_handle_t epaper_handle, uint16_t x, uint16_t y, char *chr, uint16_t size, uint16_t color);
void epaper_print_number(epaper_handle_t epaper_handle, uint16_t x, uint16_t y, uint32_t num, uint16_t len, uint16_t size, uint16_t color);
void epaper_print_chinese(epaper_handle_t epaper_handle, uint16_t x, uint16_t y, uint16_t num, uint16_t size, uint16_t color);
void epaper_display_picture(epaper_handle_t epaper_handle, uint16_t x, uint16_t y, uint16_t x_size, uint16_t y_size, const uint8_t bmp[], uint16_t color);

esp_err_t epaper_clear_display(epaper_handle_t epaper_handle, uint16_t color);

/**
 * @brief Create transport resources (SPI/GPIO/ISR/framebuffer) once.
 *
 * Does not assume the panel stays powered. Call epaper_panel_init() after each
 * power-on before refreshing.
 */
esp_err_t epaper_create(spi_host_device_t spi_host_id, epaper_handle_t *out_handle);

/**
 * @brief Replay full SSD1680 init after panel power-on (hard reset + init cmds).
 */
esp_err_t epaper_panel_init(epaper_handle_t epaper_handle);

/**
 * @brief Refresh using the internal paint framebuffer.
 */
esp_err_t epaper_refresh(epaper_handle_t epaper_handle);

/**
 * @brief Refresh using an external raw framebuffer (no copy into paint buffer).
 *
 * @param frame Caller-owned buffer of exactly EPAPER_SCREEN_BUFFER_SIZE bytes.
 *              Must remain valid until this call returns.
 */
esp_err_t epaper_refresh_frame(epaper_handle_t epaper_handle, const uint8_t *frame, size_t len);

/**
 * @brief Disable BUSY GPIO interrupt after a powered panel session.
 */
esp_err_t epaper_busy_intr_disable(epaper_handle_t epaper_handle);

/**
 * @brief Final transport teardown (not used between normal refresh sessions).
 */
esp_err_t epaper_delete(epaper_handle_t *epaper_handle);

#ifdef __cplusplus
}
#endif
