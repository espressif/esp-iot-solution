// Copyright 2015-2016 Espressif Systems (Shanghai) PTE LTD
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
#include <stdio.h>

#include <string.h>
#include <stdlib.h>
#include <math.h>
#include "driver/gpio.h"
#include "driver/spi_master.h"

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/xtensa_api.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/ringbuf.h"
#include "epaper.h"
#include "esp_log.h"

static const char* TAG = "epaper";

#define EPAPER_CS_SETUP_NS      55
#define EPAPER_CS_HOLD_NS       60
#define EPAPER_1S_NS            1000000000
#define EPAPER_QUE_SIZE_DEFAULT 10

const unsigned char lut_vcom_dc[] = { 0x00, 0x00, 0x00, 0x0F, 0x0F, 0x00, 0x00,
        0x05, 0x00, 0x32, 0x32, 0x00, 0x00, 0x02, 0x00, 0x0F, 0x0F, 0x00, 0x00,
        0x05, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00 };
//R21H
const unsigned char lut_ww[] = { 0x50, 0x0F, 0x0F, 0x00, 0x00, 0x05, 0x60, 0x32,
        0x32, 0x00, 0x00, 0x02, 0xA0, 0x0F, 0x0F, 0x00, 0x00, 0x05, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
//R22H    r
const unsigned char lut_bw[] = { 0x50, 0x0F, 0x0F, 0x00, 0x00, 0x05, 0x60, 0x32,
        0x32, 0x00, 0x00, 0x02, 0xA0, 0x0F, 0x0F, 0x00, 0x00, 0x05, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
//R24H    b
const unsigned char lut_bb[] = { 0xA0, 0x0F, 0x0F, 0x00, 0x00, 0x05, 0x60, 0x32,
        0x32, 0x00, 0x00, 0x02, 0x50, 0x0F, 0x0F, 0x00, 0x00, 0x05, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
//R23H    w
const unsigned char lut_wb[] = { 0xA0, 0x0F, 0x0F, 0x00, 0x00, 0x05, 0x60, 0x32,
        0x32, 0x00, 0x00, 0x02, 0x50, 0x0F, 0x0F, 0x00, 0x00, 0x05, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };

static portMUX_TYPE epaper_spinlock = portMUX_INITIALIZER_UNLOCKED;
#define EPAPER_ENTER_CRITICAL(mux)    portENTER_CRITICAL(mux)
#define EPAPER_EXIT_CRITICAL(mux)     portEXIT_CRITICAL(mux)

// LCD data/command
typedef struct {
    uint8_t dc_io;
    uint8_t dc_level;
} epaper_dc_t;

typedef struct {
    spi_device_handle_t bus;
    epaper_conf_t pin;       /* EPD properties */
    epaper_paint_t paint;   /* Paint properties */
    epaper_dc_t dc;
    xSemaphoreHandle spi_mux;
} epaper_dev_t;

/*This function is called (in irq context!) just before a transmission starts.
It will set the D/C line to the value indicated in the user field */
static void iot_epaper_pre_transfer_callback(spi_transaction_t *t)
{
    epaper_dc_t *dc = (epaper_dc_t *) t->user;
    gpio_set_level((int)dc->dc_io, (int)dc->dc_level);
}

static esp_err_t _iot_epaper_spi_send(spi_device_handle_t spi, spi_transaction_t* t)
{
    return spi_device_transmit(spi, t);
}

void iot_epaper_send(spi_device_handle_t spi, const uint8_t *data, int len, epaper_dc_t *dc)
{
    esp_err_t ret;
    if (len == 0) {
        return;    //no need to send anything
    }
    spi_transaction_t t = {
        .length = len * 8,              // Len is in bytes, transaction length is in bits.
        .tx_buffer = data,              // Data
        .user = (void *) dc,            // D/C needs to be set to 1
    };
    ret = _iot_epaper_spi_send(spi, &t);
    assert(ret == ESP_OK);              // Should have had no issues.
}

static void iot_epaper_send_command(epaper_handle_t dev, unsigned char command)
{
    epaper_dev_t* device = (epaper_dev_t*) dev;
    device->dc.dc_io = device->pin.dc_pin;
    device->dc.dc_level = device->pin.dc_lev_cmd;
    iot_epaper_send(device->bus, &command, 1, &device->dc);
}

static void iot_epaper_send_byte(epaper_handle_t dev, const uint8_t data)
{
    epaper_dev_t* device = (epaper_dev_t*) dev;
    device->dc.dc_io = device->pin.dc_pin;
    device->dc.dc_level = device->pin.dc_lev_data;
    iot_epaper_send(device->bus, &data, 1, &device->dc);
}

static void iot_epaper_send_data(epaper_handle_t dev, const uint8_t *data, int length)
{
    epaper_dev_t* device = (epaper_dev_t*) dev;
    device->dc.dc_io = device->pin.dc_pin;
    device->dc.dc_level = device->pin.dc_lev_data;
    // This SPI slave only support slow write mode
    // We can just send data byte by byte.
    int idx = 0;
    while(idx < length) {
        iot_epaper_send_byte(dev, data[idx++]);
    }
}

static void iot_epaper_paint_init(epaper_handle_t dev, unsigned char* image, int width, int height)
{
    epaper_dev_t* device = (epaper_dev_t*) dev;
    device->paint.rotate = E_PAPER_ROTATE_0;
    device->paint.image = image;
    /* 1 byte = 8 pixels, so the width should be the multiple of 8 */
    device->paint.width = width % 8 ? width + 8 - (width % 8) : width;
    device->paint.height = height;
}

static void iot_epaper_gpio_init(epaper_conf_t * pin)
{
    gpio_pad_select_gpio(pin->reset_pin);
    gpio_set_direction(pin->reset_pin, GPIO_MODE_OUTPUT);
    gpio_set_level(pin->reset_pin, pin->rst_active_level);
    gpio_pad_select_gpio(pin->dc_pin);
    gpio_set_direction(pin->dc_pin, GPIO_MODE_OUTPUT);
    gpio_set_level(pin->dc_pin, 1);
    ets_delay_us(10000);
    gpio_set_level(pin->dc_pin, 0);
    gpio_pad_select_gpio(pin->busy_pin);
    gpio_set_direction(pin->busy_pin, GPIO_MODE_INPUT);
    gpio_set_pull_mode(pin->busy_pin, GPIO_PULLUP_ONLY);
}

static esp_err_t iot_epaper_spi_init(epaper_handle_t dev, spi_device_handle_t *e_spi, epaper_conf_t *pin)
{
    esp_err_t ret;
    spi_bus_config_t buscfg = {
        .miso_io_num = -1,                   // set SPI MISO pin
        .mosi_io_num = pin->mosi_pin,        // set SPI MOSI pin
        .sclk_io_num = pin->sck_pin,         // set SPI CLK pin
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 0,                // max transfer size is 4k bytes
    };
    spi_device_interface_config_t devcfg = {
        .clock_speed_hz = pin->clk_freq_hz, // SPI clock is 40 MHz
        .mode = 0,                        // SPI mode 0
        .spics_io_num = pin->cs_pin,               // we will use external CS pin
        .cs_ena_pretrans = EPAPER_CS_SETUP_NS / (EPAPER_1S_NS / (pin->clk_freq_hz)) + 2,
        .cs_ena_posttrans = EPAPER_CS_HOLD_NS / (EPAPER_1S_NS / (pin->clk_freq_hz)) + 2,
        .queue_size = EPAPER_QUE_SIZE_DEFAULT,                //
        .flags = (SPI_DEVICE_HALFDUPLEX | SPI_DEVICE_3WIRE), // ALWAYS SET to HALF DUPLEX MODE for display spi !!
        .pre_cb = iot_epaper_pre_transfer_callback,  //Specify pre-transfer callback to handle D/C line
    };
    //Initialize the SPI bus
    ret = spi_bus_initialize(pin->spi_host, &buscfg, 1);
    assert(ret == ESP_OK);
    //Attach the EPD to the SPI bus
    ret = spi_bus_add_device(pin->spi_host, &devcfg, e_spi);
    assert(ret == ESP_OK);
    return ret;
}

static void iot_epaper_set_lut(epaper_handle_t dev)
{
    epaper_dev_t* device = (epaper_dev_t*) dev;
    xSemaphoreTakeRecursive(device->spi_mux, portMAX_DELAY);
    iot_epaper_send_command(dev, E_PAPER_LUT_FOR_VCOM);             //vcom
    iot_epaper_send_data(dev, lut_vcom_dc, sizeof(lut_vcom_dc));
    iot_epaper_send_command(dev, E_PAPER_LUT_WHITE_TO_WHITE);       //ww --
    iot_epaper_send_data(dev, lut_ww, sizeof(lut_ww));
    iot_epaper_send_command(dev, E_PAPER_LUT_BLACK_TO_WHITE);       //bw r
    iot_epaper_send_data(dev, lut_bw, sizeof(lut_bw));
    iot_epaper_send_command(dev, E_PAPER_LUT_WHITE_TO_BLACK);       //wb w
    iot_epaper_send_data(dev, lut_bb, sizeof(lut_bb));
    iot_epaper_send_command(dev, E_PAPER_LUT_BLACK_TO_BLACK);       //bb b
    iot_epaper_send_data(dev, lut_wb, sizeof(lut_wb));
    xSemaphoreGiveRecursive(device->spi_mux);
}

static void iot_epaper_epd_init(epaper_handle_t dev)
{
    epaper_dev_t* device = (epaper_dev_t*) dev;
    xSemaphoreTakeRecursive(device->spi_mux, portMAX_DELAY);
    /* EPD hardware init start */
    iot_epaper_reset(dev);
    iot_epaper_send_command(dev, E_PAPER_POWER_SETTING);
    iot_epaper_send_byte(dev, 0x03);                  // VDS_EN, VDG_EN
    iot_epaper_send_byte(dev, 0x00);                  // VCOM_HV, VGHL_LV[1], VGHL_LV[0]
    iot_epaper_send_byte(dev, 0x2b);                  // VDH
    iot_epaper_send_byte(dev, 0x2b);                  // VDL
    iot_epaper_send_byte(dev, 0x09);                  // VDHR
    iot_epaper_send_command(dev, E_PAPER_BOOSTER_SOFT_START);
    iot_epaper_send_byte(dev, 0x07);
    iot_epaper_send_byte(dev, 0x07);
    iot_epaper_send_byte(dev, 0x17);
    // Power optimization
    iot_epaper_send_command(dev, 0xF8);
    iot_epaper_send_byte(dev, 0x60);
    iot_epaper_send_byte(dev, 0xA5);
    // Power optimization
    iot_epaper_send_command(dev, 0xF8);
    iot_epaper_send_byte(dev, 0x89);
    iot_epaper_send_byte(dev, 0xA5);
    // Power optimization
    iot_epaper_send_command(dev, 0xF8);
    iot_epaper_send_byte(dev, 0x90);
    iot_epaper_send_byte(dev, 0x00);
    // Power optimization
    iot_epaper_send_command(dev, 0xF8);
    iot_epaper_send_byte(dev, 0x93);
    iot_epaper_send_byte(dev, 0x2A);
    // Power optimization
    iot_epaper_send_command(dev, 0xF8);
    iot_epaper_send_byte(dev, 0xA0);
    iot_epaper_send_byte(dev, 0xA5);
    // Power optimization
    iot_epaper_send_command(dev, 0xF8);
    iot_epaper_send_byte(dev, 0xA1);
    iot_epaper_send_byte(dev, 0x00);
    // Power optimization
    iot_epaper_send_command(dev, 0xF8);
    iot_epaper_send_byte(dev, 0x73);
    iot_epaper_send_byte(dev, 0x41);
    iot_epaper_send_command(dev, E_PAPER_PARTIAL_DISPLAY_REFRESH);
    iot_epaper_send_byte(dev, 0x00);
    iot_epaper_send_command(dev, E_PAPER_POWER_ON);
    iot_epaper_wait_idle(dev);
    iot_epaper_send_command(dev, E_PAPER_PANEL_SETTING);
    iot_epaper_send_byte(dev, 0xAF);            //KW-BF   KWR-AF    BWROTP 0f
    iot_epaper_send_command(dev, E_PAPER_PLL_CONTROL);
    iot_epaper_send_byte(dev, 0x3A);          //3A 100HZ   29 150Hz 39 200HZ    31 171HZ
    iot_epaper_send_command(dev, E_PAPER_VCM_DC_SETTING_REGISTER);
    iot_epaper_send_byte(dev, 0x12);
    iot_epaper_set_lut(dev);
    xSemaphoreGiveRecursive(device->spi_mux);
    /* EPD hardware init end */
}

epaper_handle_t iot_epaper_create(spi_device_handle_t bus, epaper_conf_t *epconf)
{
    epaper_dev_t* dev = (epaper_dev_t*) calloc(1, sizeof(epaper_dev_t));
    dev->spi_mux = xSemaphoreCreateRecursiveMutex();
    uint8_t* frame_buf = (unsigned char*) heap_caps_malloc(
            (epconf->width * epconf->height / 8), MALLOC_CAP_8BIT);
    if (frame_buf == NULL) {
        ESP_LOGE(TAG, "frame_buffer malloc fail\r\n");
        return NULL;
    }
    iot_epaper_gpio_init(epconf);
    ESP_LOGD(TAG, "gpio init ok");
    if (bus) {
        dev->bus = bus;
    } else {
        iot_epaper_spi_init(dev, &dev->bus, epconf);
        ESP_LOGD(TAG, "spi init ok");
    }
    dev->pin = *epconf;
    iot_epaper_epd_init(dev);
    iot_epaper_paint_init(dev, frame_buf, epconf->width, epconf->height);
    return (epaper_handle_t) dev;
}

esp_err_t iot_epaper_delete(epaper_handle_t dev, bool del_bus)
{
    epaper_dev_t* device = (epaper_dev_t*) dev;
    iot_epaper_send_command(dev, E_PAPER_POWER_OFF);
    spi_bus_remove_device(device->bus);
    if (del_bus) {
        spi_bus_free(device->pin.spi_host);
    }
    vSemaphoreDelete(device->spi_mux);
    if (device->paint.image) {
        free(device->paint.image);
        device->paint.image = NULL;
    }
    free(device);
    return ESP_OK;
}

int iot_epaper_get_width(epaper_handle_t dev)
{
    epaper_dev_t* device = (epaper_dev_t*) dev;
    return device->paint.width;
}

void iot_epaper_set_width(epaper_handle_t dev, int width)
{
    epaper_dev_t* device = (epaper_dev_t*) dev;
    xSemaphoreTakeRecursive(device->spi_mux, portMAX_DELAY);
    device->paint.width = width % 8 ? width + 8 - (width % 8) : width;
    xSemaphoreGiveRecursive(device->spi_mux);

}

int iot_epaper_get_height(epaper_handle_t dev)
{
    epaper_dev_t* device = (epaper_dev_t*) dev;
    return device->paint.height;
}

void iot_epaper_set_height(epaper_handle_t dev, int height)
{
    epaper_dev_t* device = (epaper_dev_t*) dev;
    xSemaphoreTakeRecursive(device->spi_mux, portMAX_DELAY);
    device->paint.height = height;
    xSemaphoreGiveRecursive(device->spi_mux);
}

int iot_epaper_get_rotate(epaper_handle_t dev)
{
    epaper_dev_t* device = (epaper_dev_t*) dev;
    return device->paint.rotate;
}

void iot_epaper_set_rotate(epaper_handle_t dev, int rotate)
{
    epaper_dev_t* device = (epaper_dev_t*) dev;
    xSemaphoreTakeRecursive(device->spi_mux, portMAX_DELAY);
    device->paint.rotate = rotate;
    xSemaphoreGiveRecursive(device->spi_mux);
}

/**
 *  @brief: Getters and Setters
 */
unsigned char* iot_epaper_get_image(epaper_handle_t dev)
{
    epaper_dev_t* device = (epaper_dev_t*) dev;
    return device->paint.image;
}

/**
 *  @brief: this draws a pixel by absolute coordinates.
 *          this function won't be affected by the rotate parameter.
 */
static void iot_epaper_draw_absolute_pixel(epaper_handle_t dev, int x, int y, int colored)
{
    epaper_dev_t* device = (epaper_dev_t*) dev;
    if (x < 0 || x >= device->paint.width || y < 0 || y >= device->paint.height) {
        return;
    }
    EPAPER_ENTER_CRITICAL(&epaper_spinlock);
    if (device->pin.color_inv) {
        if (colored) {
            device->paint.image[(x + y * device->paint.width) / 8] |= 0x80 >> (x % 8);
        } else {
            device->paint.image[(x + y * device->paint.width) / 8] &= ~(0x80 >> (x % 8));
        }
    } else {
        if (colored) {
            device->paint.image[(x + y * device->paint.width) / 8] &= ~(0x80 >> (x % 8));
        } else {
            device->paint.image[(x + y * device->paint.width) / 8] |= 0x80 >> (x % 8);
        }
    }
    EPAPER_EXIT_CRITICAL(&epaper_spinlock);
}

void iot_epaper_clean_paint(epaper_handle_t dev, int colored)
{
    epaper_dev_t* device = (epaper_dev_t*) dev;
    xSemaphoreTakeRecursive(device->spi_mux, portMAX_DELAY);
    for (int x = 0; x < device->paint.width; x++) {
        for (int y = 0; y < device->paint.height; y++) {
            iot_epaper_draw_absolute_pixel(dev, x, y, colored);
        }
    }
    xSemaphoreGiveRecursive(device->spi_mux);
}

/**
 *  @brief: this displays a string on the frame buffer but not refresh
 */
void iot_epaper_draw_string(epaper_handle_t dev, int x, int y, const char* text, epaper_font_t* font, int colored)
{
    const char* p_text = text;
    unsigned int counter = 0;
    int refcolumn = x;
    epaper_dev_t* device = (epaper_dev_t*) dev;
    xSemaphoreTakeRecursive(device->spi_mux, portMAX_DELAY);
    /* Send the string character by character on EPD */
    while (*p_text != 0) {
        /* Display one character on EPD */
        iot_epaper_draw_char(dev, refcolumn, y, *p_text, font, colored);
        /* Decrement the column position by 16 */
        refcolumn += font->width;
        /* Point on the next character */
        p_text++;
        counter++;
    }
    xSemaphoreGiveRecursive(device->spi_mux);
}

/**
 *  @brief: this draws a pixel by the coordinates
 */
void iot_epaper_draw_pixel(epaper_handle_t dev, int x, int y, int colored)
{
    int point_temp;
    epaper_dev_t* device = (epaper_dev_t*) dev;
    if (device->paint.rotate == E_PAPER_ROTATE_0) {
        if (x < 0 || x >= device->paint.width || y < 0 || y >= device->paint.height) {
            return;
        }
        iot_epaper_draw_absolute_pixel(dev, x, y, colored);
    } else if (device->paint.rotate == E_PAPER_ROTATE_90) {
        if (x < 0 || x >= device->paint.height || y < 0 || y >= device->paint.width) {
            return;
        }
        point_temp = x;
        x = device->paint.width - y;
        y = point_temp;
        iot_epaper_draw_absolute_pixel(dev, x, y, colored);
    } else if (device->paint.rotate == E_PAPER_ROTATE_180) {
        if (x < 0 || x >= device->paint.width || y < 0 || y >= device->paint.height) {
            return;
        }
        x = device->paint.width - x;
        y = device->paint.height - y;
        iot_epaper_draw_absolute_pixel(dev, x, y, colored);
    } else if (device->paint.rotate == E_PAPER_ROTATE_270) {
        if (x < 0 || x >= device->paint.height || y < 0 || y >= device->paint.width) {
            return;
        }
        point_temp = x;
        x = y;
        y = device->paint.height - point_temp;
        iot_epaper_draw_absolute_pixel(dev, x, y, colored);
    }
}

/**
 *  @brief: this draws a charactor on the frame buffer but not refresh
 */
void iot_epaper_draw_char(epaper_handle_t dev, int x, int y, char ascii_char, epaper_font_t* font, int colored)
{
    int i, j;
    unsigned int char_offset = (ascii_char - ' ') * font->height * (font->width / 8 + (font->width % 8 ? 1 : 0));
    const unsigned char* ptr = &font->font_table[char_offset];
    epaper_dev_t* device = (epaper_dev_t*) dev;
    xSemaphoreTakeRecursive(device->spi_mux, portMAX_DELAY);
    for (j = 0; j < font->height; j++) {
        for (i = 0; i < font->width; i++) {
            if (*ptr & (0x80 >> (i % 8))) {
                iot_epaper_draw_pixel(dev, x + i, y + j, colored);
            }
            if (i % 8 == 7) {
                ptr++;
            }
        }
        if (font->width % 8 != 0) {
            ptr++;
        }
    }
    xSemaphoreGiveRecursive(device->spi_mux);
}

/**
 *  @brief: this draws a line on the frame buffer
 */
void iot_epaper_draw_line(epaper_handle_t dev, int x0, int y0, int x1, int y1,
        int colored)
{
    /* Bresenham algorithm */
    int dx = x1 - x0 >= 0 ? x1 - x0 : x0 - x1;
    int sx = x0 < x1 ? 1 : -1;
    int dy = y1 - y0 <= 0 ? y1 - y0 : y0 - y1;
    int sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;
    epaper_dev_t* device = (epaper_dev_t*) dev;
    xSemaphoreTakeRecursive(device->spi_mux, portMAX_DELAY);
    while ((x0 != x1) && (y0 != y1)) {
        iot_epaper_draw_pixel(dev, x0, y0, colored);
        if (2 * err >= dy) {
            err += dy;
            x0 += sx;
        }
        if (2 * err <= dx) {
            err += dx;
            y0 += sy;
        }
    }
    xSemaphoreGiveRecursive(device->spi_mux);
}

/**
 *  @brief: this draws a horizontal line on the frame buffer
 */
void iot_epaper_draw_horizontal_line(epaper_handle_t dev, int x, int y, int width, int colored)
{
    int i;
    epaper_dev_t* device = (epaper_dev_t*) dev;
    xSemaphoreTakeRecursive(device->spi_mux, portMAX_DELAY);
    for (i = x; i < x + width; i++) {
        iot_epaper_draw_pixel(dev, i, y, colored);
    }
    xSemaphoreGiveRecursive(device->spi_mux);
}

/**
 *  @brief: this draws a vertical line on the frame buffer
 */
void iot_epaper_draw_vertical_line(epaper_handle_t dev, int x, int y, int height, int colored)
{
    int i;
    epaper_dev_t* device = (epaper_dev_t*) dev;
    xSemaphoreTakeRecursive(device->spi_mux, portMAX_DELAY);
    for (i = y; i < y + height; i++) {
        iot_epaper_draw_pixel(dev, x, i, colored);
    }
    xSemaphoreGiveRecursive(device->spi_mux);
}

/**
 *  @brief: this draws a rectangle
 */
void iot_epaper_draw_rectangle(epaper_handle_t dev, int x0, int y0, int x1, int y1, int colored)
{
    int min_x, min_y, max_x, max_y;
    min_x = x1 > x0 ? x0 : x1;
    max_x = x1 > x0 ? x1 : x0;
    min_y = y1 > y0 ? y0 : y1;
    max_y = y1 > y0 ? y1 : y0;
    epaper_dev_t* device = (epaper_dev_t*) dev;
    xSemaphoreTakeRecursive(device->spi_mux, portMAX_DELAY);
    iot_epaper_draw_horizontal_line(dev, min_x, min_y, max_x - min_x + 1, colored);
    iot_epaper_draw_horizontal_line(dev, min_x, max_y, max_x - min_x + 1, colored);
    iot_epaper_draw_vertical_line(dev, min_x, min_y, max_y - min_y + 1, colored);
    iot_epaper_draw_vertical_line(dev, max_x, min_y, max_y - min_y + 1, colored);
    xSemaphoreGiveRecursive(device->spi_mux);
}

/**
 *  @brief: this draws a filled rectangle
 */
void iot_epaper_draw_filled_rectangle(epaper_handle_t dev, int x0, int y0, int x1, int y1, int colored)
{
    int min_x, min_y, max_x, max_y;
    int i;
    min_x = x1 > x0 ? x0 : x1;
    max_x = x1 > x0 ? x1 : x0;
    min_y = y1 > y0 ? y0 : y1;
    max_y = y1 > y0 ? y1 : y0;
    epaper_dev_t* device = (epaper_dev_t*) dev;
    xSemaphoreTakeRecursive(device->spi_mux, portMAX_DELAY);
    for (i = min_x; i <= max_x; i++) {
        iot_epaper_draw_vertical_line(dev, i, min_y, max_y - min_y + 1, colored);
    }
    xSemaphoreGiveRecursive(device->spi_mux);
}

/**
 *  @brief: this draws a circle
 */
void iot_epaper_draw_circle(epaper_handle_t dev, int x, int y, int radius,
        int colored)
{
    /* Bresenham algorithm */
    int x_pos = -radius;
    int y_pos = 0;
    int err = 2 - 2 * radius;
    int e2;
    epaper_dev_t* device = (epaper_dev_t*) dev;
    xSemaphoreTakeRecursive(device->spi_mux, portMAX_DELAY);
    do {
        iot_epaper_draw_pixel(dev, x - x_pos, y + y_pos, colored);
        iot_epaper_draw_pixel(dev, x + x_pos, y + y_pos, colored);
        iot_epaper_draw_pixel(dev, x + x_pos, y - y_pos, colored);
        iot_epaper_draw_pixel(dev, x - x_pos, y - y_pos, colored);
        e2 = err;
        if (e2 <= y_pos) {
            err += ++y_pos * 2 + 1;
            if (-x_pos == y_pos && e2 <= x_pos) {
                e2 = 0;
            }
        }
        if (e2 > x_pos) {
            err += ++x_pos * 2 + 1;
        }
    } while (x_pos <= 0);
    xSemaphoreGiveRecursive(device->spi_mux);
}

/**
 *  @brief: this draws a filled circle
 */
void iot_epaper_draw_filled_circle(epaper_handle_t dev, int x, int y, int radius, int colored)
{
    /* Bresenham algorithm */
    int x_pos = -radius;
    int y_pos = 0;
    int err = 2 - 2 * radius;
    int e2;
    epaper_dev_t* device = (epaper_dev_t*) dev;
    xSemaphoreTakeRecursive(device->spi_mux, portMAX_DELAY);
    do {
        iot_epaper_draw_pixel(dev, x - x_pos, y + y_pos, colored);
        iot_epaper_draw_pixel(dev, x + x_pos, y + y_pos, colored);
        iot_epaper_draw_pixel(dev, x + x_pos, y - y_pos, colored);
        iot_epaper_draw_pixel(dev, x - x_pos, y - y_pos, colored);
        iot_epaper_draw_horizontal_line(dev, x + x_pos, y + y_pos, 2 * (-x_pos) + 1, colored);
        iot_epaper_draw_horizontal_line(dev, x + x_pos, y - y_pos, 2 * (-x_pos) + 1, colored);
        e2 = err;
        if (e2 <= y_pos) {
            err += ++y_pos * 2 + 1;
            if (-x_pos == y_pos && e2 <= x_pos) {
                e2 = 0;
            }
        }
        if (e2 > x_pos) {
            err += ++x_pos * 2 + 1;
        }
    } while (x_pos <= 0);
    xSemaphoreGiveRecursive(device->spi_mux);
}

void iot_epaper_wait_idle(epaper_handle_t dev)
{
    epaper_dev_t* device = (epaper_dev_t*) dev;
    while (gpio_get_level((gpio_num_t) device->pin.busy_pin) == 0) {      //0: busy, 1: idle
        vTaskDelay(10 / portTICK_RATE_MS);
    }
}

void iot_epaper_reset(epaper_handle_t dev)
{
    epaper_dev_t* device = (epaper_dev_t*) dev;
    xSemaphoreTakeRecursive(device->spi_mux, portMAX_DELAY);
    gpio_set_level((gpio_num_t) device->pin.reset_pin, (~(device->pin.rst_active_level)) & 0x1);
    ets_delay_us(5);
    gpio_set_level((gpio_num_t) device->pin.reset_pin, (device->pin.rst_active_level) & 0x1);             //module reset
    ets_delay_us(5);
    gpio_set_level((gpio_num_t) device->pin.reset_pin, (~(device->pin.rst_active_level)) & 0x1);
    iot_epaper_wait_idle(dev);
    xSemaphoreGiveRecursive(device->spi_mux);
}

void iot_epaper_display_frame(epaper_handle_t dev, const unsigned char* frame_buffer)
{
    epaper_dev_t* device = (epaper_dev_t*) dev;
    if (frame_buffer == NULL) {
        frame_buffer = device->paint.image;
    }
    xSemaphoreTakeRecursive(device->spi_mux, portMAX_DELAY);
    if (frame_buffer != NULL) {
        iot_epaper_send_command(dev, E_PAPER_DATA_START_TRANSMISSION_2);
        iot_epaper_send_data(dev, frame_buffer, device->paint.width / 8 * device->paint.height);
        iot_epaper_send_command(dev, E_PAPER_DISPLAY_REFRESH);
        iot_epaper_wait_idle(dev);
    }
    xSemaphoreGiveRecursive(device->spi_mux);
}

void iot_epaper_sleep(epaper_handle_t dev)
{
    epaper_dev_t* device = (epaper_dev_t*) dev;
    xSemaphoreTakeRecursive(device->spi_mux, portMAX_DELAY);
    iot_epaper_send_command(dev, E_PAPER_DEEP_SLEEP);
    iot_epaper_send_byte(dev, 0xa5);
    xSemaphoreGiveRecursive(device->spi_mux);
}

