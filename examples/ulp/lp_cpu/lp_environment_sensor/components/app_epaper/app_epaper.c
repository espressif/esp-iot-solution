/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "driver/spi_master.h"
#include "esp_log.h"
#include "app_epaper.h"
#include "esp_ths_font.h"
#include "esp_ths_image.h"

static const char *TAG = "app_epaper";

typedef struct {
    epaper_config_t config;
    const epaper_init_cmd_t *init_cmds;
    epaper_paint_t paint;
    spi_device_handle_t spi_handle;
    SemaphoreHandle_t busy_state;
    uint8_t *screen_buffer;
} epaper_panel_t;

/* Place data into DRAM. Constant data gets placed into DROM by default, which is not accessible by DMA. */
DRAM_ATTR static const epaper_init_cmd_t ssd1680_init_cmds[] = {
    /* SWRESET */
    {0x12, {0}, 0x80},
    /* Driver output control */
    {0x01, {0x27, 0x01, 0x01}, 3},
    /* data entry mode */
    {0x11, {0x01}, 1},
    /* set Ram-X address start/end position */
    {0x44, {0x00, 0x0F}, 2},
    /* set Ram-Y address start/end position */
    {0x45, {0x27, 0x01, 0x00, 0x00}, 4},
    /* BorderWavefrom */
    {0x3C, {0x05}, 1},
    /* Display update control */
    {0x21, {0x00, 0x80}, 2},
    /* Read built-in temperature sensor */
    {0x18, {0x80}, 1},
    /* set RAM x address count to 0 */
    {0x4E, {0x00}, 1},

    /* set RAM y address count to 0X199 */
    {0x4F, {0x27, 0x01}, 2},
    {0, {0}, 0xff}
};

static void lcd_spi_pre_transfer_callback(spi_transaction_t *t)
{
    int dc = (int)t->user;
    gpio_set_level(CONFIG_EPAPER_PIN_NUM_DC, dc);
}

/* Send a command to the epaper. Uses spi_device_polling_transmit, which waits
 * until the transfer is complete.
 *
 * Since command transactions are usually small, they are handled in polling
 * mode for higher speed. The overhead of interrupt transactions is more than
 * just waiting for the transaction to complete.
 */
static esp_err_t epaper_write_command(spi_device_handle_t spi, const uint8_t cmd, bool keep_cs_active)
{
    esp_err_t ret;
    spi_transaction_t t = {0};
    memset(&t, 0, sizeof(t));       //Zero out the transaction
    t.length = 8;                   //Command is 8 bits
    t.tx_buffer = &cmd;             //The data is the cmd itself
    t.user = (void*)0;              //D/C needs to be set to 0
    if (keep_cs_active) {
        t.flags = SPI_TRANS_CS_KEEP_ACTIVE;   //Keep CS active after data transfer
    }
    ret = spi_device_polling_transmit(spi, &t); //Transmit!
    return ret;
}

/* Send data to the epaper. Uses spi_device_polling_transmit, which waits until the
 * transfer is complete.
 *
 * Since data transactions are usually small, they are handled in polling
 * mode for higher speed. The overhead of interrupt transactions is more than
 * just waiting for the transaction to complete.
 */
static esp_err_t epaper_write_data(spi_device_handle_t spi, const uint8_t *data, int len)
{
    esp_err_t ret = ESP_OK;
    spi_transaction_t t;
    if (len == 0) {
        return ret;    //no need to send anything
    }
    memset(&t, 0, sizeof(t));       //Zero out the transaction
    t.length = len * 8;             //Len is in bytes, transaction length is in bits.
    t.tx_buffer = data;             //Data
    t.user = (void*)1;              //D/C needs to be set to 1
    ret = spi_device_polling_transmit(spi, &t); //Transmit!
    return ret;
}

static void IRAM_ATTR gpio_isr_handler(void* arg)
{
    SemaphoreHandle_t busy_state = (SemaphoreHandle_t)arg;
    BaseType_t need_yeild = pdFALSE;
    xSemaphoreGiveFromISR(busy_state, &need_yeild);
    if (need_yeild) {
        portYIELD_FROM_ISR();
    }
}

static void wait_epaper_idle()
{
    while (1) {
        if (EPAPER_BUSY() == 0) {
            break;
        }
    }
}

static void epaper_update(epaper_panel_t *panel)
{
    epaper_write_command(panel->spi_handle, EPAPER_UPDATE_CTRL_CMD, false);
    const uint8_t data = EPAPER_UPDATE_SEQUENCE_DATA;
    epaper_write_data(panel->spi_handle, &data, 1);
    epaper_write_command(panel->spi_handle, EPAPER_UPDATE_SEQUENCE_ACTIVATE_CMD, false);
    xSemaphoreTake(panel->busy_state, portMAX_DELAY);
}

static esp_err_t create_new_epaper_image(epaper_panel_t *panel, uint16_t width, uint16_t height, uint16_t rotate, uint16_t color)
{
    panel->paint.image = panel->screen_buffer;
    panel->paint.width_memory = width;
    panel->paint.height_memory = height;
    panel->paint.color = color;
    panel->paint.width_byte = (width % 8 == 0) ? (width / 8) : (width / 8 + 1);
    panel->paint.height_byte = height;
    panel->paint.rotate = rotate;
    if (rotate == ROTATE_0 || rotate == ROTATE_180) {
        panel->paint.width = width;
        panel->paint.height = height;
    } else {
        panel->paint.width = height;
        panel->paint.height = width;
    }
    return ESP_OK;
}

static esp_err_t paint_set_pixel(epaper_handle_t epaper_handle, uint16_t x_point, uint16_t y_point, uint16_t color)
{
    if (epaper_handle == NULL) {
        ESP_LOGE(TAG, "epaper panel not created yet, please create first.");
        return ESP_FAIL;
    }
    epaper_panel_t *panel = (epaper_panel_t *) epaper_handle;
    uint16_t X = 0;
    uint16_t Y = 0;
    uint32_t addr = 0;
    uint8_t rdata = 0;
    switch (panel->paint.rotate) {
    case 0:

        X = panel->paint.width_memory - y_point - 1;
        Y = x_point;
        break;
    case 90:
        X = panel->paint.width_memory - x_point - 1;
        Y = panel->paint.height_memory - y_point - 1;
        break;
    case 180:
        X = y_point;
        Y = panel->paint.height_memory - x_point - 1;
        break;
    case 270:
        X = x_point;
        Y = y_point;
        break;
    default:
        break;
    }
    addr = X / 8 + Y * panel->paint.width_byte;
    rdata = panel->paint.image[addr];
    if (color == BLACK) {
        panel->paint.image[addr] = rdata & ~(0x80 >> (X % 8));
    } else {
        panel->paint.image[addr] = rdata | (0x80 >> (X % 8));
    }
    return ESP_OK;
}

static esp_err_t epaper_clear_display(epaper_panel_t *panel, uint16_t color)
{
    uint16_t X = 0;
    uint16_t Y = 0;
    uint32_t addr = 0;
    for (Y = 0; Y < panel->paint.height_byte; Y++) {
        for (X = 0; X < panel->paint.width_byte; X++) {
            /* 8 pixel =  1 byte */
            addr = X + Y * panel->paint.width_byte;
            panel->paint.image[addr] = color;
        }
    }
    return ESP_OK;
}

void epaper_draw_point(epaper_handle_t epaper_handle, uint16_t x_point, uint16_t y_point, uint16_t color)
{
    esp_err_t ret = paint_set_pixel(epaper_handle, x_point - 1, y_point - 1, color);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "draw point failed");
    }
}

void epaper_draw_line(epaper_handle_t epaper_handle, uint16_t x_start, uint16_t y_start, uint16_t x_end, uint16_t y_end, uint16_t color)
{
    uint16_t x_point, y_point;
    int dx, dy;
    int x_add_way, y_add_way;
    int epaper_sp;
    char dotted_len;
    x_point = x_start;
    y_point = y_start;
    dx = (int)x_end - (int)x_start >= 0 ? x_end - x_start : x_start - x_end;
    dy = (int)y_end - (int)y_start <= 0 ? y_end - y_start : y_start - y_end;

    x_add_way = x_start < x_end ? 1 : -1;
    y_add_way = y_start < y_end ? 1 : -1;

    epaper_sp = dx + dy;
    dotted_len = 0;

    for (;;) {
        dotted_len++;
        epaper_draw_point(epaper_handle, x_point, y_point, color);
        if (2 * epaper_sp >= dy) {
            if (x_point == x_end) {
                break;
            }
            epaper_sp += dy;
            x_point += x_add_way;
        }
        if (2 * epaper_sp <= dx) {
            if (y_point == y_end) {
                break;
            }
            epaper_sp += dx;
            y_point += y_add_way;
        }
    }
}

void epaper_draw_rectangle(epaper_handle_t epaper_handle, uint16_t x_start, uint16_t y_start, uint16_t x_end, uint16_t y_end, uint16_t color, uint8_t mode)
{
    uint16_t i;
    if (mode) {
        for (i = y_start; i < y_end; i++) {
            epaper_draw_line(epaper_handle, x_start, i, x_end, i, color);
        }
    } else {
        epaper_draw_line(epaper_handle, x_start, y_start, x_end, y_start, color);
        epaper_draw_line(epaper_handle, x_start, y_start, x_start, y_end, color);
        epaper_draw_line(epaper_handle, x_end, y_end, x_end, y_start, color);
        epaper_draw_line(epaper_handle, x_end, y_end, x_start, y_end, color);
    }
}

void epaper_print_char(epaper_handle_t epaper_handle, uint16_t x, uint16_t y, uint16_t chr, uint16_t size, uint16_t color)
{
    uint16_t i, m, temp, size2, chr1;
    uint16_t x0, y0;
    x += 1, y += 1, x0 = x, y0 = y;
    if (size == 8) {
        size2 = 6;
    } else {
        size2 = (size / 8 + ((size % 8) ? 1 : 0)) * (size / 2);
    }
    chr1 = chr - ' ';
    for (i = 0; i < size2; i++) {
        if (size == 8) {
            temp = asc2_0806[chr1][i];
        } else if (size == 12) {
            temp = asc2_1206[chr1][i];
        } else if (size == 16) {
            temp = asc2_1608[chr1][i];
        } else if (size == 24) {
            temp = asc2_2412[chr1][i];
        } else if (size == 36) {
            temp = asc2_3618[chr1][i];
        } else if (size == 80) {
            temp = asc2_8040[chr1][i];
        } else {
            return;
        }
        for (m = 0; m < 8; m++) {
            if (temp & 0x01) {
                epaper_draw_point(epaper_handle, x, y, color);
            } else {
                epaper_draw_point(epaper_handle, x, y, !color);
            }
            temp >>= 1;
            y++;
        }
        x++;
        if ((size != 8) && ((x - x0) == size / 2)) {
            x = x0;
            y0 = y0 + 8;
        }
        y = y0;
    }
}

void epaper_print_string(epaper_handle_t epaper_handle, uint16_t x, uint16_t y, char *chr, uint16_t size, uint16_t color)
{
    while (*chr != '\0') {

        epaper_print_char(epaper_handle, x, y, *chr, size, color);
        chr++;
        x += size / 2;
    }
}

static uint32_t epaper_pow(uint16_t m, uint16_t n)
{
    uint32_t result = 1;
    while (n--) {
        result *= m;
    }
    return result;
}

void epaper_print_number(epaper_handle_t epaper_handle, uint16_t x, uint16_t y, uint32_t num, uint16_t len, uint16_t size, uint16_t color)
{
    uint8_t t, temp, m = 0;
    if (size == 8) {
        m = 2;
    }
    for (t = 0; t < len; t++) {
        temp = (num / epaper_pow(10, len - t - 1)) % 10;
        if (temp == 0) {
            epaper_print_char(epaper_handle, x + (size / 2 + m) * t, y, '0', size, color);
        } else {
            epaper_print_char(epaper_handle, x + (size / 2 + m) * t, y, temp + '0', size, color);
        }
    }
}

void epaper_print_chinese(epaper_handle_t epaper_handle, uint16_t x, uint16_t y, uint16_t num, uint16_t size, uint16_t color)
{
    uint16_t m, temp;
    uint16_t x0, y0;
    uint16_t i, size3 = (size / 8 + ((size % 8) ? 1 : 0)) * size;
    x += 1, y += 1, x0 = x, y0 = y;
    for (i = 0; i < size3; i++) {
        if (size == 16) {
            temp = Hzk1[num][i];
        } else if (size == 24) {
            temp = Hzk2[num][i];
        } else if (size == 32) {
            temp = Hzk3[num][i];
        } else if (size == 64) {
            temp = Hzk4[num][i];
        } else {
            return;
        }
        for (m = 0; m < 8; m++) {
            if (temp & 0x01) {
                epaper_draw_point(epaper_handle, x, y, color);
            } else {
                epaper_draw_point(epaper_handle, x, y, !color);
            }
            temp >>= 1;
            y++;
        }
        x++;
        if ((x - x0) == size) {
            x = x0;
            y0 = y0 + 8;
        }
        y = y0;
    }
}

void epaper_display_picture(epaper_handle_t epaper_handle, uint16_t x, uint16_t y, uint16_t x_size, uint16_t y_size, const uint8_t bmp[], uint16_t color)
{
    uint16_t j = 0;
    uint16_t i, n, temp, m;
    uint16_t x0, y0;
    x += 1, y += 1, x0 = x, y0 = y;
    y_size = y_size / 8 + ((y_size % 8) ? 1 : 0);
    for (n = 0; n < y_size; n++) {
        for (i = 0; i < x_size; i++) {
            temp = bmp[j];
            j++;
            for (m = 0; m < 8; m++) {
                if (temp & 0x01) {
                    epaper_draw_point(epaper_handle, x, y, !color);
                } else {
                    epaper_draw_point(epaper_handle, x, y, color);
                }
                temp >>= 1;
                y++;
            }
            x++;
            if ((x - x0) == x_size) {
                x = x0;
                y0 = y0 + 8;
            }
            y = y0;
        }
    }
}

esp_err_t epaper_refresh(epaper_handle_t epaper_handle, uint16_t width)
{
    esp_err_t ret = ESP_OK;
    epaper_panel_t *panel = (epaper_panel_t *) epaper_handle;
    unsigned int height, i, j;
    uint32_t k = 0;
    height = 16;
    ret = epaper_write_command(panel->spi_handle, 0x24, false);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to write command 0x24");
        return ret;
    }
    for (j = 0; j < height; j++) {
        for (i = 0; i < width; i++) {
            epaper_write_data(panel->spi_handle, panel->screen_buffer + k, 1);
            k++;
        }
    }
    epaper_update(panel);
    return ret;
}

epaper_handle_t epaper_create(spi_host_device_t spi_host_id)
{
    esp_err_t ret;
    epaper_panel_t *epaper_panel = (epaper_panel_t *) calloc(1, sizeof(epaper_panel_t));
    if (epaper_panel == NULL) {
        ESP_LOGE(TAG, "Failed to allocate memory for epaper_panel_t.");
        return NULL;
    }
    epaper_panel->busy_state = xSemaphoreCreateBinary();
    epaper_panel->config.buscfg.miso_io_num = -1;
    epaper_panel->config.buscfg.miso_io_num = -1;
    epaper_panel->config.buscfg.mosi_io_num = CONFIG_EPAPER_PIN_NUM_MOSI;
    epaper_panel->config.buscfg.sclk_io_num = CONFIG_EPAPER_PIN_NUM_SCLK;
    epaper_panel->config.buscfg.quadwp_io_num = -1;
    epaper_panel->config.buscfg.quadhd_io_num = -1;
    epaper_panel->config.buscfg.max_transfer_sz = PARALLEL_LINES * 320 * 2 + 8;

    epaper_panel->config.devcfg.clock_speed_hz = 26 * 1000 * 1000;          //Clock out at 10 MHz
    epaper_panel->config.devcfg.mode = 0;                                   //SPI mode 0
    epaper_panel->config.devcfg.spics_io_num = CONFIG_EPAPER_PIN_NUM_CS;    //CS pin
    epaper_panel->config.devcfg.queue_size = 7;                             //We want to be able to queue 7 transactions at a time
    epaper_panel->config.devcfg.pre_cb = lcd_spi_pre_transfer_callback;     //Specify pre-transfer callback to handle D/C line

    /* Initialize the SPI bus */
    ret = spi_bus_initialize(spi_host_id, &epaper_panel->config.buscfg, SPI_DMA_CH_AUTO);
    ESP_ERROR_CHECK(ret);
    /* Attach the epaper to the SPI bus */
    ret = spi_bus_add_device(spi_host_id, &epaper_panel->config.devcfg, &epaper_panel->spi_handle);
    ESP_ERROR_CHECK(ret);

    int cmd = 0;
    /* Initialize non-SPI GPIOs */
    epaper_panel->config.io_conf.pin_bit_mask = ((1ULL << CONFIG_EPAPER_PIN_NUM_DC) | (1ULL << CONFIG_EPAPER_PIN_NUM_RST));
    epaper_panel->config.io_conf.mode = GPIO_MODE_OUTPUT;
    epaper_panel->config.io_conf.pull_up_en = true;
    gpio_config(&epaper_panel->config.io_conf);

    epaper_panel->config.io_conf.intr_type = GPIO_INTR_NEGEDGE;
    epaper_panel->config.io_conf.pin_bit_mask = (1ULL << CONFIG_EPAPER_PIN_NUM_BUSY);
    epaper_panel->config.io_conf.mode = GPIO_MODE_INPUT;
    epaper_panel->config.io_conf.pull_up_en = GPIO_PULLUP_ENABLE;
    epaper_panel->config.io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    gpio_config(&epaper_panel->config.io_conf);

    gpio_install_isr_service(ESP_INTR_FLAG_IRAM);
    gpio_isr_handler_add(CONFIG_EPAPER_PIN_NUM_BUSY, gpio_isr_handler, (void*)epaper_panel->busy_state);

    if (epaper_panel->busy_state == NULL) {
        ESP_LOGE(TAG, "Failed to create busy_state semaphore.");
    }
    /* Reset the display */
    gpio_set_level(CONFIG_EPAPER_PIN_NUM_RST, 0);
    vTaskDelay(20 / portTICK_PERIOD_MS);
    gpio_set_level(CONFIG_EPAPER_PIN_NUM_RST, 1);
    vTaskDelay(20 / portTICK_PERIOD_MS);

    epaper_panel->init_cmds = ssd1680_init_cmds;
    /* Send all the commands */
    while (epaper_panel->init_cmds[cmd].databytes != 0xff) {
        epaper_write_command(epaper_panel->spi_handle, epaper_panel->init_cmds[cmd].cmd, false);
        epaper_write_data(epaper_panel->spi_handle, epaper_panel->init_cmds[cmd].data, epaper_panel->init_cmds[cmd].databytes);
        if (epaper_panel->init_cmds[cmd].databytes & 0x80) {
            wait_epaper_idle();
        }
        cmd++;
    }
    xSemaphoreTake(epaper_panel->busy_state, portMAX_DELAY);

    epaper_panel->screen_buffer = (uint8_t *) calloc(EPAPER_SCREEN_BUFFER_SIZE, sizeof(uint8_t));
    if (epaper_panel->screen_buffer == NULL) {
        ESP_LOGE(TAG, "%s calloc failed", __func__);
    }

    ret = create_new_epaper_image(epaper_panel, EPAPER_W, EPAPER_H, 0, WHITE);
    ESP_ERROR_CHECK(ret);

    ret = epaper_clear_display(epaper_panel, WHITE);

    ESP_ERROR_CHECK(ret);
    return (epaper_handle_t)epaper_panel;
}

esp_err_t epaper_delete(epaper_handle_t *epaper_handle)
{
    esp_err_t ret = ESP_OK;
    if (*epaper_handle == NULL) {
        return ESP_OK;
    }

    epaper_panel_t *panel = (epaper_panel_t *)(*epaper_handle);
    ret = spi_bus_remove_device(panel->spi_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to remove SPI device");
        return ret;
    }
    free(panel);
    *epaper_handle = NULL;
    return ESP_OK;
}

esp_err_t epaper_display(epaper_handle_t paper, epaper_display_data_t display_data)
{
    esp_err_t ret;
    int temp_decimal_part = (display_data.temp - (int)display_data.temp) * 10 + 0.5;
    int hum_decimal_part = (display_data.hum - (int)display_data.hum) * 10 + 0.5;

    epaper_print_number(paper, 10, 16, display_data.temp, 2, 80, BLACK);
    epaper_print_string(paper, 89, 48, ".", 24, BLACK);
    epaper_print_number(paper, 89 + 8, 51, temp_decimal_part, 1, 24, BLACK);
    epaper_print_string(paper, 90, 23, "o", 8, BLACK);
    epaper_print_string(paper, 96, 20, "C", 24, BLACK);
    epaper_print_string(paper, 25, 3, "Temperature", 12, BLACK);
    epaper_draw_line(paper, 1, 18, 119, 18, BLACK);

    epaper_print_number(paper, 18, 88, display_data.hum, 2, 36, BLACK);
    epaper_print_string(paper, 54, 98, ".", 24, BLACK);
    epaper_print_number(paper, 60, 88, hum_decimal_part, 1, 36, BLACK);
    epaper_print_string(paper, 80, 88, "%", 36, BLACK);
    epaper_draw_rectangle(paper, 1, 1, 120, 75, BLACK, 0);
    epaper_draw_rectangle(paper, 1, 77, 120, 128, BLACK, 0);
    epaper_print_string(paper, 30, 78, "Humidity", 12, BLACK);
    epaper_draw_line(paper, 1, 92, 119, 92, BLACK);

#if CONFIG_REGION_INTERNATIONAL
    if (strstr(display_data.text, "Sunny") != NULL) {
        epaper_print_string(paper, 145, 5, "Sunny", 16, BLACK);
        epaper_display_picture(paper, 141, 25, 48, 48, SUNNY, WHITE);
    } else if (strstr(display_data.text, "Partly Cloudy") != NULL) {
        epaper_print_string(paper, 128, 5, "Partly Cloudy", 12, BLACK);
        epaper_display_picture(paper, 141, 25, 48, 48, PARTLY_CLOUDY, WHITE);
    } else if (strstr(display_data.text, "Cloudy") != NULL) {
        epaper_print_string(paper, 140, 5, "Cloudy", 16, BLACK);
        epaper_display_picture(paper, 141, 25, 48, 48, CLOUDY, WHITE);
    } else if (strstr(display_data.text, "Snowy") != NULL) {
        epaper_print_string(paper, 145, 5, "Snowy", 16, BLACK);
        epaper_display_picture(paper, 141, 25, 48, 48, SNOWY, WHITE);
    } else if (strstr(display_data.text, "Rain") != NULL) {
        epaper_print_string(paper, 150, 5, "Rain", 16, BLACK);
        epaper_display_picture(paper, 141, 25, 48, 48, RAIN, WHITE);
    } else if (strstr(display_data.text, "Haze") != NULL) {
        epaper_print_string(paper, 150, 5, "Haze", 16, BLACK);
        epaper_display_picture(paper, 141, 25, 48, 48, HAZE, WHITE);
    } else {
        epaper_print_string(paper, 140, 5, "No Wifi", 16, BLACK);
        epaper_display_picture(paper, 141, 25, 48, 48, NO_WEATHER, WHITE);
    }
#else
    if (strstr(display_data.text, "晴") != NULL) {
        epaper_print_string(paper, 145, 5, "Sunny", 16, BLACK);
        epaper_display_picture(paper, 141, 25, 48, 48, SUNNY, WHITE);
    } else if (strstr(display_data.text, "多云") != NULL) {
        epaper_print_string(paper, 128, 5, "Partly Cloudy", 12, BLACK);
        epaper_display_picture(paper, 141, 25, 48, 48, PARTLY_CLOUDY, WHITE);
    } else if (strstr(display_data.text, "阴") != NULL) {
        epaper_print_string(paper, 140, 5, "Cloudy", 16, BLACK);
        epaper_display_picture(paper, 141, 25, 48, 48, CLOUDY, WHITE);
    } else if (strstr(display_data.text, "雪") != NULL) {
        epaper_print_string(paper, 145, 5, "Snowy", 16, BLACK);
        epaper_display_picture(paper, 141, 25, 48, 48, SNOWY, WHITE);
    } else if (strstr(display_data.text, "雨") != NULL) {
        epaper_print_string(paper, 150, 5, "Rain", 16, BLACK);
        epaper_display_picture(paper, 141, 25, 48, 48, RAIN, WHITE);
    } else if (strstr(display_data.text, "霾") != NULL) {
        epaper_print_string(paper, 150, 5, "Haze", 16, BLACK);
        epaper_display_picture(paper, 141, 25, 48, 48, HAZE, WHITE);
    } else {
        epaper_print_string(paper, 140, 5, "No Wifi", 16, BLACK);
        epaper_display_picture(paper, 141, 25, 48, 48, NO_WEATHER, WHITE);
    }
#endif

    char high_temp[10];
    char low_temp[10];
    sprintf(high_temp, "H:%d", display_data.max_temp);
    sprintf(low_temp, " L:%d", display_data.min_temp);
    char temp_str[12] = "";
    strcat(temp_str, high_temp);
    strcat(temp_str, low_temp);

    int city_len = strlen(display_data.city);
    int district_len = strlen(display_data.district);
    epaper_print_string(paper, 132, 75, temp_str, 16, BLACK);
    epaper_print_string(paper, 166 - (city_len * 4), 92, display_data.city, 16, BLACK);
    epaper_print_string(paper, 166 - (district_len * 4), 108, display_data.district, 16, BLACK);

    epaper_draw_rectangle(paper, 122, 1, 210, 128, BLACK, 0);

    epaper_print_string(paper, 240, 3, "Wind", 12, BLACK);
    epaper_draw_line(paper, 213, 18, 295, 18, BLACK);

    char wind_speed[15] = "Speed level:";
    wind_speed[strlen(wind_speed)] = display_data.wind_class[0];
    wind_speed[strlen(wind_speed) + 1] = '\0';
    epaper_print_string(paper, 214, 19, wind_speed, 12, BLACK);

#if CONFIG_REGION_INTERNATIONAL
    int wind_dir_len = strlen(display_data.wind_dir);
    epaper_print_string(paper, 254 - (wind_dir_len * 3), 32, display_data.wind_dir, 12, BLACK);
#else
    char wind_direction[15] = {0};
    if (strstr(display_data.wind_dir, "西北风") != NULL) {
        strcat(wind_direction, "Northwest");
    } else if (strstr(display_data.wind_dir, "东北风") != NULL) {
        strcat(wind_direction, "Northeast");
    } else if (strstr(display_data.wind_dir, "西南风") != NULL) {
        strcat(wind_direction, "Southwest");
    } else if (strstr(display_data.wind_dir, "东南风") != NULL) {
        strcat(wind_direction, "Southeast");
    } else if (strstr(display_data.wind_dir, "北风") != NULL) {
        strcat(wind_direction, "North");
    } else if (strstr(display_data.wind_dir, "南风") != NULL) {
        strcat(wind_direction, "South");
    } else if (strstr(display_data.wind_dir, "西风") != NULL) {
        strcat(wind_direction, "West");
    } else if (strstr(display_data.wind_dir, "东风") != NULL) {
        strcat(wind_direction, "East");
    } else {
        strcat(wind_direction, "No data");
    }
    int wind_dir_len = strlen(wind_direction);
    epaper_print_string(paper, 254 - (wind_dir_len * 3), 32, wind_direction, 12, BLACK);
#endif

    epaper_draw_rectangle(paper, 212, 1, 296, 46, BLACK, 0);

#if CONFIG_REGION_INTERNATIONAL
    int week_len = strlen(display_data.week);
    epaper_print_string(paper, 254 - (week_len * 4), 48, display_data.week, 16, BLACK);
#else
    char week_str[15] = {0};
    if (strstr(display_data.week, "一") != NULL) {
        strcat(week_str, "Monday");
    } else if (strstr(display_data.week, "二") != NULL) {
        strcat(week_str, "Tuesday");
    } else if (strstr(display_data.week, "三") != NULL) {
        strcat(week_str, "Wednesday");
    } else if (strstr(display_data.week, "四") != NULL) {
        strcat(week_str, "Thursday");
    } else if (strstr(display_data.week, "五") != NULL) {
        strcat(week_str, "Friday");
    } else if (strstr(display_data.week, "六") != NULL) {
        strcat(week_str, "Saturday");
    } else {
        strcat(week_str, "Sunday");
    }
    int week_len = strlen(week_str);
    epaper_print_string(paper, 254 - (week_len * 4), 48, week_str, 16, BLACK);
#endif

    char uptime_str[12];
    uint8_t index = 0;
    for (int i = 0; i < 8; i++) {
        uptime_str[index] = display_data.uptime[i];
        index++;
        if (index == 4) {
            uptime_str[index] = '/';
            index++;
        }
        if (index == 7) {
            uptime_str[index] = '/';
            index++;
        }
        if (index == 10) {
            uptime_str[index] = '\0';
        }
    }
    epaper_print_string(paper, 225, 66, uptime_str, 12, BLACK);
    epaper_display_picture(paper, 236, 85, 36, 36, ESP_LOGO, WHITE);
    epaper_draw_rectangle(paper, 212, 48, 296, 128, BLACK, 0);

    ret = epaper_refresh(paper, EPAPER_H);

    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "epaper refresh failed");
    }
    return ret;
}
