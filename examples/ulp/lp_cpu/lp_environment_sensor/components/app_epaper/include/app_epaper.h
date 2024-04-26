/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#ifdef __cplusplus
extern "C"
{
#endif

#include "driver/gpio.h"
#include "driver/spi_master.h"

#define PARALLEL_LINES  16

#define EPAPER_BUSY() gpio_get_level(CONFIG_EPAPER_PIN_NUM_BUSY)

#define EPAPER_W 128
#define EPAPER_H 296
#define EPAPER_SCREEN_BUFFER_SIZE (EPAPER_W * EPAPER_H / 8)

typedef struct {
    spi_bus_config_t buscfg;
    spi_device_interface_config_t devcfg;
    gpio_config_t io_conf;
} epaper_config_t;

/**
 * @brief e-paper initialization commands.
 *
 */
typedef struct {
    uint8_t cmd;
    uint8_t data[16];
    uint8_t databytes; //No of data in data; bit 7 = delay after set; 0xFF = end of cmds.
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

typedef struct {
    char *city;
    char *district;
    float temp;
    float hum;
    char *text;
    char *week;
    int8_t max_temp;
    int8_t min_temp;
    char *wind_class;
    char *wind_dir;
    char *uptime;
} epaper_display_data_t;

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

/**
 * @brief Draw a point on the e-paper display
 *
 * @param[in] epaper_handle The e-paper handle
 * @param[in] x_point X-coordinate of the point
 * @param[in] y_point Y-coordinate of the point
 */
void epaper_draw_point(epaper_handle_t epaper_handle, uint16_t x_point, uint16_t y_point, uint16_t color);

/**
 * @brief Draw a line on the e-paper display
 *
 * This function draws a line between the specified start and end points on an e-paper display with
 * the specified color.
 *
 * @param[in] epaper_handle The e-paper handle
 * @param[in] x_start X-coordinate of the start point
 * @param[in] y_start Y-coordinate of the start point
 * @param[in] x_end X-coordinate of the end point
 * @param[in] y_end Y-coordinate of the end point
 * @param[in] color Color of the line
 */
void epaper_draw_line(epaper_handle_t epaper_handle, uint16_t x_start, uint16_t y_start, uint16_t x_end, uint16_t y_end, uint16_t color);

/**
 * @brief Draw a rectangle on the e-paper display
 *
 * This function draws a rectangle on an e-paper display with the specified color and drawing mode.
 *
 * @param[in] epaper_handle The e-paper handle
 * @param[in] x_start X-coordinate of the top-left corner of the rectangle
 * @param[in] y_start Y-coordinate of the top-left corner of the rectangle
 * @param[in] x_end X-coordinate of the bottom-right corner of the rectangle
 * @param[in] y_end Y-coordinate of the bottom-right corner of the rectangle
 * @param[in] color Color of the rectangle
 * @param[in] mode Drawing mode of the rectangle (1:filled or 0:outline)
 */
void epaper_draw_rectangle(epaper_handle_t epaper_handle, uint16_t x_start, uint16_t y_start, uint16_t x_end, uint16_t y_end, uint16_t color, uint8_t mode);

/**
 * @brief Print a character on the e-paper display
 *
 * @param[in] epaper_handle The e-paper handle
 * @param[in] x X-coordinate of the top-left corner of the character
 * @param[in] y Y-coordinate of the top-left corner of the character
 * @param[in] chr Unicode code point of the character to be printed
 * @param[in] size Size of the character to be printed
 * @param[in] color Color of the character
 */
void epaper_print_char(epaper_handle_t epaper_handle, uint16_t x, uint16_t y, uint16_t chr, uint16_t size, uint16_t color);

/**
 * @brief Print a string on the e-paper display
 *
 * @param[in] epaper_handle The e-paper handle
 * @param[in] x X-coordinate of the top-left corner of the string
 * @param[in] y Y-coordinate of the top-left corner of the string
 * @param[in] chr Pointer to the null-terminated string to be printed
 * @param[in] size Size of the characters in the string to be printed
 * @param[in] color Color of the string
 */
void epaper_print_string(epaper_handle_t epaper_handle, uint16_t x, uint16_t y, char *chr, uint16_t size, uint16_t color);

/**
 * @brief Print a number on the e-paper display
 *
 * @param[in] epaper_handle The e-paper handle
 * @param[in] x X-coordinate of the top-left corner of the number
 * @param[in] y Y-coordinate of the top-left corner of the number
 * @param[in] num Unsigned 32-bit integer to be printed
 * @param[in] len Maximum number of number to be printed
 * @param[in] size Size of the number to be printed
 * @param[in] color Color of the number
 */
void epaper_print_number(epaper_handle_t epaper_handle, uint16_t x, uint16_t y, uint32_t num, uint16_t len, uint16_t size, uint16_t color);

/**
 * @brief Print a Chinese character on the e-paper display
 *
 * @param[in] epaper_handle The e-paper handle
 * @param[in] x X-coordinate of the top-left corner of the character
 * @param[in] y Y-coordinate of the top-left corner of the character
 * @param[in] num Unicode code point of the Chinese character to be printed
 * @param[in] size Size of the character to be printed
 * @param[in] color Color of the character
 */
void epaper_print_chinese(epaper_handle_t epaper_handle, uint16_t x, uint16_t y, uint16_t num, uint16_t size, uint16_t color);

/**
 * @brief Print a picture on the e-paper display
 *
 * @param[in] epaper_handle The e-paper handle
 * @param[in] x X-coordinate of the top-left corner of the picture
 * @param[in] y Y-coordinate of the top-left corner of the picture
 * @param[in] x_size Width of the picture
 * @param[in] y_size Height of the picture
 * @param[in] bmp Pointer to the bitmap data of the picture
 * @param[in] color Color of the picture
 */
void epaper_display_picture(epaper_handle_t epaper_handle, uint16_t x, uint16_t y, uint16_t x_size, uint16_t y_size, const uint8_t bmp[], uint16_t color);

/**
 * @brief Refreshes the e-paper display with new image data
 *
 * This function refreshes the e-paper display with new image data specified by the image parameter.
 *
 * @param[in] epaper_handle The e-paper handle
 * @param[in] width Width of the image data
 */
esp_err_t epaper_refresh(epaper_handle_t epaper_handle, uint16_t width);

/**
 * @brief Creates a new e-paper display instance
 *
 * This function initializes and creates a new e-paper display instance using the specified SPI host ID
 *
 * @param[in] spi_host_id The SPI host ID to be used for communication
 *
 * @return The handle to the newly created e-paper display instance,
 *         or NULL if the creation failed.
 */
epaper_handle_t epaper_create(spi_host_device_t spi_host_id);

/**
 * @brief Deletes the specified e-paper display instance
 *
 * This function releases the resources associated with the specified e-paper display instance.
 *
 * @param[in] epaper_handle Pointer to the handle of the e-paper display instance
 *                          to be deleted.
 *
 * @return  - ESP_OK if the e-paper display instance is successfully deleted.
 *          - An error code if there was an error deleting the instance.
 */
esp_err_t epaper_delete(epaper_handle_t *epaper_handle);

/**
 * @brief Displays the data on the e-paper
 *
 * @param[in] epaper_handle The e-paper handle
 * @param[in] display_data  The structure containing the data to be displayed.
 *                          It includes various parameters such as city name,
 *                          temperature, humidity, weather text, etc
 */
esp_err_t epaper_display(epaper_handle_t paper, epaper_display_data_t display_data);

#ifdef __cplusplus
}
#endif
