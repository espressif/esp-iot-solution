/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
// #include "driver/spi_master.h"
#include "esp_log.h"
#include "epaper.h"
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
    spi_host_device_t spi_host_id;
    bool bus_initialized;
    bool device_added;
    bool isr_handler_added;
    bool busy_intr_enabled;
} epaper_panel_t;

#define EPAPER_BUSY_TIMEOUT_MS  3000

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

static void log_busy_diag(const char *phase, int start_lvl, int saw_high, int saw_low, int end_lvl)
{
    ESP_LOGW(TAG,
             "BUSY diag [%s]: pin=%d start=%d saw_high=%d saw_low=%d end=%d "
             "(expect busy=1 then idle=0)",
             phase ? phase : "?", CONFIG_EPAPER_PIN_NUM_BUSY,
             start_lvl, saw_high, saw_low, end_lvl);
    if (start_lvl == 0 && end_lvl == 0 && !saw_high) {
        ESP_LOGW(TAG, "BUSY stuck LOW: pin floating/GND, wrong GPIO, or panel not powered/SPI dead");
    } else if (start_lvl == 1 && end_lvl == 1 && !saw_low) {
        ESP_LOGW(TAG, "BUSY stuck HIGH: panel hung, wrong polarity, or BUSY not connected (pull-up)");
    } else if (saw_high && saw_low) {
        ESP_LOGW(TAG, "BUSY did toggle but completion IRQ missed — check NEGEDGE/ISR");
    }
}

static esp_err_t wait_epaper_idle(void)
{
    int count = 0;
    const int max_count = EPAPER_BUSY_TIMEOUT_MS / 10;
    const int start_lvl = EPAPER_BUSY();
    int saw_high = (start_lvl != 0);
    int saw_low = (start_lvl == 0);
    while (count++ < max_count) {
        int lvl = EPAPER_BUSY();
        if (lvl != 0) {
            saw_high = 1;
        } else {
            saw_low = 1;
            return ESP_OK;
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    log_busy_diag("poll_idle", start_lvl, saw_high, saw_low, EPAPER_BUSY());
    ESP_LOGE(TAG, "Epaper busy timeout");
    return ESP_ERR_TIMEOUT;
}

static esp_err_t wait_busy_sem(epaper_panel_t *panel)
{
    if (panel->busy_state == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    const int start_lvl = EPAPER_BUSY();
    int saw_high = (start_lvl != 0);
    int saw_low = (start_lvl == 0);
    const TickType_t start = xTaskGetTickCount();
    const TickType_t timeout_ticks = pdMS_TO_TICKS(EPAPER_BUSY_TIMEOUT_MS);

    /* IRQ path (NEGEDGE) plus poll sampling so logs can classify wiring faults. */
    while ((xTaskGetTickCount() - start) < timeout_ticks) {
        if (xSemaphoreTake(panel->busy_state, pdMS_TO_TICKS(50)) == pdTRUE) {
            ESP_LOGI(TAG, "BUSY IRQ ok (pin=%d level_now=%d)",
                     CONFIG_EPAPER_PIN_NUM_BUSY, EPAPER_BUSY());
            return ESP_OK;
        }
        int lvl = EPAPER_BUSY();
        if (lvl != 0) {
            saw_high = 1;
        } else {
            saw_low = 1;
            /* Already idle: accept poll completion even if edge IRQ was missed. */
            if (saw_high) {
                ESP_LOGI(TAG, "BUSY poll ok after busy (pin=%d)", CONFIG_EPAPER_PIN_NUM_BUSY);
                return ESP_OK;
            }
        }
    }

    log_busy_diag("sem_wait", start_lvl, saw_high, saw_low, EPAPER_BUSY());
    ESP_LOGE(TAG, "Epaper BUSY semaphore timeout");
    return ESP_ERR_TIMEOUT;
}

static esp_err_t epaper_update(epaper_panel_t *panel)
{
    esp_err_t ret = epaper_write_command(panel->spi_handle, EPAPER_UPDATE_CTRL_CMD, false);
    if (ret != ESP_OK) {
        return ret;
    }
    const uint8_t data = EPAPER_UPDATE_SEQUENCE_DATA;
    ret = epaper_write_data(panel->spi_handle, &data, 1);
    if (ret != ESP_OK) {
        return ret;
    }
    ret = epaper_write_command(panel->spi_handle, EPAPER_UPDATE_SEQUENCE_ACTIVATE_CMD, false);
    if (ret != ESP_OK) {
        return ret;
    }
    return wait_busy_sem(panel);
}

static esp_err_t epaper_busy_intr_enable(epaper_panel_t *panel)
{
    if (panel == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    while (panel->busy_state != NULL &&
            xSemaphoreTake(panel->busy_state, 0) == pdTRUE) {
        /* drain stale BUSY edges from power transitions */
    }
    esp_err_t ret = gpio_intr_enable((gpio_num_t)CONFIG_EPAPER_PIN_NUM_BUSY);
    if (ret == ESP_OK) {
        panel->busy_intr_enabled = true;
    }
    return ret;
}

esp_err_t epaper_busy_intr_disable(epaper_handle_t epaper_handle)
{
    if (epaper_handle == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    epaper_panel_t *panel = (epaper_panel_t *)epaper_handle;
    esp_err_t ret = gpio_intr_disable((gpio_num_t)CONFIG_EPAPER_PIN_NUM_BUSY);
    if (ret == ESP_OK || ret == ESP_ERR_INVALID_STATE) {
        panel->busy_intr_enabled = false;
        return ESP_OK;
    }
    return ret;
}

static esp_err_t epaper_send_frame(epaper_panel_t *panel, const uint8_t *frame, size_t len)
{
    if (panel == NULL || frame == NULL || len != EPAPER_SCREEN_BUFFER_SIZE) {
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t ret = epaper_busy_intr_enable(panel);
    if (ret != ESP_OK) {
        return ret;
    }

    ret = epaper_write_command(panel->spi_handle, 0x24, false);
    if (ret != ESP_OK) {
        (void)epaper_busy_intr_disable((epaper_handle_t)panel);
        ESP_LOGE(TAG, "Failed to write command 0x24");
        return ret;
    }

    ret = epaper_write_data(panel->spi_handle, frame, (int)len);
    if (ret != ESP_OK) {
        (void)epaper_busy_intr_disable((epaper_handle_t)panel);
        ESP_LOGE(TAG, "Failed to write framebuffer");
        return ret;
    }

    ret = epaper_update(panel);
    (void)epaper_busy_intr_disable((epaper_handle_t)panel);
    return ret;
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

esp_err_t epaper_clear_display(epaper_handle_t epaper_handle, uint16_t color)
{
    if (epaper_handle == NULL) {
        ESP_LOGE(TAG, "epaper panel not created yet, please create first.");
        return ESP_FAIL;
    }
    epaper_panel_t *panel = (epaper_panel_t *) epaper_handle;
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

/**
 * @brief Get placeholder character data for missing characters
 * @param size Font size
 * @param index Byte index within the character data
 * @return Placeholder byte data (0xFF for solid rectangle)
 */
static uint8_t get_placeholder_byte(uint16_t size, uint16_t index)
{
    (void)size;
    (void)index;
    // Return 0xFF to draw a solid rectangle as placeholder
    return 0xFF;
}

void epaper_print_char(epaper_handle_t epaper_handle, uint16_t x, uint16_t y, uint16_t chr, uint16_t size, uint16_t color)
{
    uint16_t i, m, temp, size2, chr1;
    uint16_t x0, y0;
    bool use_placeholder = false;
    x += 1, y += 1, x0 = x, y0 = y;
    if (size == 8) {
        size2 = 6;
    } else {
        size2 = (size / 8 + ((size % 8) ? 1 : 0)) * (size / 2);
    }
    chr1 = chr - ' ';

    // Boundary check for ASCII character index
    if (size == 8) {
        if (chr1 >= ASC2_0806_SIZE) {
            use_placeholder = true;
            ESP_LOGW(TAG, "Character 0x%02X (index %d) out of range for size %d, using placeholder", chr, chr1, size);
        }
    } else if (size == 12) {
        if (chr1 >= ASC2_1206_SIZE) {
            use_placeholder = true;
            ESP_LOGW(TAG, "Character 0x%02X (index %d) out of range for size %d, using placeholder", chr, chr1, size);
        }
    } else if (size == 16) {
        if (chr1 >= ASC2_1608_SIZE) {
            use_placeholder = true;
            ESP_LOGW(TAG, "Character 0x%02X (index %d) out of range for size %d, using placeholder", chr, chr1, size);
        }
    } else if (size == 24) {
        if (chr1 >= ASC2_2412_SIZE) {
            use_placeholder = true;
            ESP_LOGW(TAG, "Character 0x%02X (index %d) out of range for size %d, using placeholder", chr, chr1, size);
        }
    } else if (size == 36) {
        if (chr1 >= ASC2_3618_SIZE) {
            use_placeholder = true;
            ESP_LOGW(TAG, "Character 0x%02X (index %d) out of range for size %d, using placeholder", chr, chr1, size);
        }
    } else if (size == 80) {
        if (chr1 >= ASC2_8040_SIZE) {
            use_placeholder = true;
            ESP_LOGW(TAG, "Character 0x%02X (index %d) out of range for size %d, using placeholder", chr, chr1, size);
        }
    } else {
        return;
    }

    for (i = 0; i < size2; i++) {
        if (use_placeholder) {
            // Use placeholder data (solid rectangle)
            temp = get_placeholder_byte(size, i);
        } else {
            // Use normal font data with boundary check
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
    printf("print_string: %s, size: %d, color: %d\n", chr, size, color);
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
    bool use_placeholder = false;
    x += 1, y += 1, x0 = x, y0 = y;

    // Boundary check for Chinese character index
    if (size == 16) {
        if (num >= HZK1_SIZE) {
            use_placeholder = true;
            ESP_LOGW(TAG, "Chinese character index %d out of range for size %d (max %d), using placeholder", num, size, HZK1_SIZE - 1);
        }
    } else if (size == 24) {
        if (num >= HZK2_SIZE) {
            use_placeholder = true;
            ESP_LOGW(TAG, "Chinese character index %d out of range for size %d (max %d), using placeholder", num, size, HZK2_SIZE - 1);
        }
    } else if (size == 32) {
        if (num >= HZK3_SIZE) {
            use_placeholder = true;
            ESP_LOGW(TAG, "Chinese character index %d out of range for size %d (max %d), using placeholder", num, size, HZK3_SIZE - 1);
        }
    } else if (size == 64) {
        if (num >= HZK4_SIZE) {
            use_placeholder = true;
            ESP_LOGW(TAG, "Chinese character index %d out of range for size %d (max %d), using placeholder", num, size, HZK4_SIZE - 1);
        }
    } else {
        return;
    }

    for (i = 0; i < size3; i++) {
        if (use_placeholder) {
            // Use placeholder data (solid rectangle)
            temp = get_placeholder_byte(size, i);
        } else {
            // Use normal font data
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
    printf("epaper_display_picture: x: %d, y: %d, x_size: %d, y_size: %d, color: %d\n", x, y, x_size, y_size, color);
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

esp_err_t epaper_refresh(epaper_handle_t epaper_handle)
{
    if (epaper_handle == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    epaper_panel_t *panel = (epaper_panel_t *)epaper_handle;
    if (panel->screen_buffer == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    return epaper_send_frame(panel, panel->screen_buffer, EPAPER_SCREEN_BUFFER_SIZE);
}

esp_err_t epaper_refresh_frame(epaper_handle_t epaper_handle, const uint8_t *frame, size_t len)
{
    if (epaper_handle == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    return epaper_send_frame((epaper_panel_t *)epaper_handle, frame, len);
}

esp_err_t epaper_panel_init(epaper_handle_t epaper_handle)
{
    if (epaper_handle == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    epaper_panel_t *panel = (epaper_panel_t *)epaper_handle;
    if (!panel->device_added || panel->spi_handle == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    esp_err_t ret = epaper_busy_intr_enable(panel);
    if (ret != ESP_OK) {
        return ret;
    }

    gpio_set_level((gpio_num_t)CONFIG_EPAPER_PIN_NUM_RST, 0);
    vTaskDelay(pdMS_TO_TICKS(20));
    gpio_set_level((gpio_num_t)CONFIG_EPAPER_PIN_NUM_RST, 1);
    vTaskDelay(pdMS_TO_TICKS(20));

    panel->init_cmds = ssd1680_init_cmds;
    int cmd = 0;
    while (panel->init_cmds[cmd].databytes != 0xff) {
        ret = epaper_write_command(panel->spi_handle, panel->init_cmds[cmd].cmd, false);
        if (ret != ESP_OK) {
            (void)epaper_busy_intr_disable(epaper_handle);
            return ret;
        }
        uint8_t nbytes = panel->init_cmds[cmd].databytes & 0x7F;
        if (nbytes > 0) {
            ret = epaper_write_data(panel->spi_handle, panel->init_cmds[cmd].data, nbytes);
            if (ret != ESP_OK) {
                (void)epaper_busy_intr_disable(epaper_handle);
                return ret;
            }
        }
        if (panel->init_cmds[cmd].databytes & 0x80) {
            ret = wait_epaper_idle();
            if (ret != ESP_OK) {
                (void)epaper_busy_intr_disable(epaper_handle);
                return ret;
            }
        }
        cmd++;
    }

    /* Clear any pending edge after init; keep IRQ disabled until refresh. */
    (void)epaper_busy_intr_disable(epaper_handle);
    return ESP_OK;
}

esp_err_t epaper_create(spi_host_device_t spi_host_id, epaper_handle_t *out_handle)
{
    if (out_handle == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    *out_handle = NULL;

    epaper_panel_t *panel = (epaper_panel_t *)calloc(1, sizeof(epaper_panel_t));
    if (panel == NULL) {
        ESP_LOGE(TAG, "Failed to allocate memory for epaper_panel_t.");
        return ESP_ERR_NO_MEM;
    }

    panel->spi_host_id = spi_host_id;
    panel->busy_state = xSemaphoreCreateBinary();
    if (panel->busy_state == NULL) {
        free(panel);
        return ESP_ERR_NO_MEM;
    }

    panel->screen_buffer = (uint8_t *)calloc(EPAPER_SCREEN_BUFFER_SIZE, sizeof(uint8_t));
    if (panel->screen_buffer == NULL) {
        vSemaphoreDelete(panel->busy_state);
        free(panel);
        return ESP_ERR_NO_MEM;
    }

    panel->config.buscfg.miso_io_num = -1;
    panel->config.buscfg.mosi_io_num = CONFIG_EPAPER_PIN_NUM_MOSI;
    panel->config.buscfg.sclk_io_num = CONFIG_EPAPER_PIN_NUM_SCLK;
    panel->config.buscfg.quadwp_io_num = -1;
    panel->config.buscfg.quadhd_io_num = -1;
    panel->config.buscfg.max_transfer_sz = EPAPER_SCREEN_BUFFER_SIZE + 8;

    panel->config.devcfg.clock_speed_hz = 10 * 1000 * 1000;
    panel->config.devcfg.mode = 0;
    panel->config.devcfg.spics_io_num = CONFIG_EPAPER_PIN_NUM_CS;
    panel->config.devcfg.queue_size = 7;
    panel->config.devcfg.pre_cb = lcd_spi_pre_transfer_callback;

    esp_err_t ret = spi_bus_initialize(spi_host_id, &panel->config.buscfg, SPI_DMA_CH_AUTO);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "spi_bus_initialize failed: %s", esp_err_to_name(ret));
        goto fail;
    }
    panel->bus_initialized = true;

    ret = spi_bus_add_device(spi_host_id, &panel->config.devcfg, &panel->spi_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "spi_bus_add_device failed: %s", esp_err_to_name(ret));
        goto fail;
    }
    panel->device_added = true;

    gpio_config_t out_conf = {
        .pin_bit_mask = (1ULL << CONFIG_EPAPER_PIN_NUM_DC) | (1ULL << CONFIG_EPAPER_PIN_NUM_RST),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ret = gpio_config(&out_conf);
    if (ret != ESP_OK) {
        goto fail;
    }

    gpio_config_t busy_conf = {
        .pin_bit_mask = (1ULL << CONFIG_EPAPER_PIN_NUM_BUSY),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_NEGEDGE,
    };
    ret = gpio_config(&busy_conf);
    if (ret != ESP_OK) {
        goto fail;
    }
    ret = gpio_install_isr_service(ESP_INTR_FLAG_IRAM);
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "gpio_install_isr_service failed: %s", esp_err_to_name(ret));
        goto fail;
    }

    ret = gpio_isr_handler_add((gpio_num_t)CONFIG_EPAPER_PIN_NUM_BUSY,
                               gpio_isr_handler, (void *)panel->busy_state);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "gpio_isr_handler_add failed: %s", esp_err_to_name(ret));
        goto fail;
    }
    panel->isr_handler_added = true;
    (void)gpio_intr_disable((gpio_num_t)CONFIG_EPAPER_PIN_NUM_BUSY);
    panel->busy_intr_enabled = false;

    ret = create_new_epaper_image(panel, EPAPER_W, EPAPER_H, 0, WHITE);
    if (ret != ESP_OK) {
        goto fail;
    }
    ret = epaper_clear_display((epaper_handle_t)panel, WHITE);
    if (ret != ESP_OK) {
        goto fail;
    }

    *out_handle = (epaper_handle_t)panel;
    return ESP_OK;

fail: {
        epaper_handle_t tmp = (epaper_handle_t)panel;
        (void)epaper_delete(&tmp);
    }
    return (ret == ESP_OK) ? ESP_FAIL : ret;
}

esp_err_t epaper_delete(epaper_handle_t *epaper_handle)
{
    if (epaper_handle == NULL || *epaper_handle == NULL) {
        return ESP_OK;
    }

    epaper_panel_t *panel = (epaper_panel_t *)(*epaper_handle);

    if (panel->busy_intr_enabled || panel->isr_handler_added) {
        (void)gpio_intr_disable((gpio_num_t)CONFIG_EPAPER_PIN_NUM_BUSY);
        panel->busy_intr_enabled = false;
    }
    if (panel->isr_handler_added) {
        (void)gpio_isr_handler_remove((gpio_num_t)CONFIG_EPAPER_PIN_NUM_BUSY);
        panel->isr_handler_added = false;
    }
    if (panel->device_added) {
        (void)spi_bus_remove_device(panel->spi_handle);
        panel->device_added = false;
        panel->spi_handle = NULL;
    }
    if (panel->bus_initialized) {
        (void)spi_bus_free(panel->spi_host_id);
        panel->bus_initialized = false;
    }
    if (panel->busy_state != NULL) {
        vSemaphoreDelete(panel->busy_state);
        panel->busy_state = NULL;
    }
    if (panel->screen_buffer != NULL) {
        free(panel->screen_buffer);
        panel->screen_buffer = NULL;
    }

    free(panel);
    *epaper_handle = NULL;
    return ESP_OK;
}
