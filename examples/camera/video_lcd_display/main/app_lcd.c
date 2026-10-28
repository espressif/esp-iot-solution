/*
 * SPDX-FileCopyrightText: 2024-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */
#include "freertos/FreeRTOS.h"
#include "freertos/idf_additions.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "esp_attr.h"
#include "esp_idf_version.h"
#include "esp_heap_caps.h"
#include "esp_ldo_regulator.h"
#include "esp_lcd_mipi_dsi.h"
#include "esp_lcd_ek79007.h"
#include "driver/ppa.h"
#include "app_lcd.h"

#define EXAMPLE_LDO_MIPI_CHAN        3
#define EXAMPLE_LDO_MIPI_VOLTAGE_MV  2500
#define EXAMPLE_PIN_NUM_LCD_RST      27
#define EXAMPLE_LCD_FRAME_TIMEOUT_MS 1000

static struct {
    SemaphoreHandle_t frame_done;
    ppa_client_handle_t ppa;
    void *buffers[EXAMPLE_LCD_SHARED_BUF_NUM];
    portMUX_TYPE publish_lock;
    uint32_t frame_done_count;
    uint32_t submitted_at;
    int callback_core;
    uint8_t display_index;
    uint8_t draw_index;
    uint8_t retiring_index;
    bool shared;
    bool pending;
} lcd = {.publish_lock = portMUX_INITIALIZER_UNLOCKED, .draw_index = 1};
static esp_ldo_channel_handle_t mipi_ldo;

static bool IRAM_ATTR lcd_frame_done(esp_lcd_panel_handle_t panel,
                                     esp_lcd_dpi_panel_event_data_t *event, void *user_data)
{
    BaseType_t task_woken = pdFALSE;
    __atomic_store_n(&lcd.callback_core, xPortGetCoreID(), __ATOMIC_RELAXED);
    __atomic_fetch_add(&lcd.frame_done_count, 1, __ATOMIC_RELEASE);
    xSemaphoreGiveFromISR((SemaphoreHandle_t)user_data, &task_woken);
    return task_woken == pdTRUE;
}

static esp_err_t wait_for_retirement(void)
{
    if (!lcd.pending) {
        return ESP_OK;
    }
    const TickType_t budget = pdMS_TO_TICKS(EXAMPLE_LCD_FRAME_TIMEOUT_MS);
    const TickType_t start = xTaskGetTickCount();
    while (__atomic_load_n(&lcd.frame_done_count, __ATOMIC_ACQUIRE) == lcd.submitted_at) {
        TickType_t elapsed = xTaskGetTickCount() - start;
        if (elapsed >= budget || xSemaphoreTake(lcd.frame_done, budget - elapsed) != pdTRUE) {
            return ESP_ERR_TIMEOUT;
        }
    }
    lcd.draw_index = lcd.retiring_index;
    lcd.pending = false;
    return ESP_OK;
}

static esp_err_t publish_buffer(esp_lcd_panel_handle_t panel, uint8_t index)
{
    /* The producer shares the DMA interrupt core. Keep publication and its
     * sequence snapshot indivisible, so one later callback retires the old FB.
     * Native draw_bitmap only synchronizes cache and selects this driver FB. */
    portENTER_CRITICAL(&lcd.publish_lock);
    esp_err_t ret = esp_lcd_panel_draw_bitmap(panel, 0, 0, EXAMPLE_LCD_H_RES,
                                              EXAMPLE_LCD_V_RES, lcd.buffers[index]);
    lcd.submitted_at = __atomic_load_n(&lcd.frame_done_count, __ATOMIC_ACQUIRE);
    portEXIT_CRITICAL(&lcd.publish_lock);
    if (ret == ESP_OK) {
        lcd.retiring_index = lcd.display_index;
        lcd.display_index = index;
        lcd.pending = true;
    }
    return ret;
}

esp_err_t app_lcd_init(esp_lcd_panel_handle_t *panel_handle, bool shared, int *draw_core)
{
    if (!panel_handle || !draw_core) {
        return ESP_ERR_INVALID_ARG;
    }
    if (lcd.frame_done) {
        return ESP_ERR_INVALID_STATE;
    }
    lcd.frame_done = xSemaphoreCreateBinaryWithCaps(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    if (!lcd.frame_done) {
        return ESP_ERR_NO_MEM;
    }

    const esp_ldo_channel_config_t ldo_config = {
        .chan_id = EXAMPLE_LDO_MIPI_CHAN,
        .voltage_mv = EXAMPLE_LDO_MIPI_VOLTAGE_MV,
    };
    ESP_ERROR_CHECK(esp_ldo_acquire_channel(&ldo_config, &mipi_ldo));

    esp_lcd_dsi_bus_handle_t dsi_bus;
    esp_lcd_dsi_bus_config_t bus_config = EK79007_PANEL_BUS_DSI_2CH_CONFIG();
    ESP_ERROR_CHECK(esp_lcd_new_dsi_bus(&bus_config, &dsi_bus));
    esp_lcd_panel_io_handle_t panel_io;
    esp_lcd_dbi_io_config_t dbi_config = EK79007_PANEL_IO_DBI_CONFIG();
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_dbi(dsi_bus, &dbi_config, &panel_io));

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(6, 0, 0)
    esp_lcd_dpi_panel_config_t dpi_config = EK79007_1024_600_PANEL_60HZ_CONFIG_CF(
                                                EXAMPLE_LCD_BIT_PER_PIXEL == 16 ? LCD_COLOR_FMT_RGB565 : LCD_COLOR_FMT_RGB888);
#else
    esp_lcd_dpi_panel_config_t dpi_config = EK79007_1024_600_PANEL_60HZ_CONFIG(
                                                EXAMPLE_LCD_BIT_PER_PIXEL == 16 ? LCD_COLOR_PIXEL_FORMAT_RGB565 : LCD_COLOR_PIXEL_FORMAT_RGB888);
#endif
    lcd.shared = shared;
    dpi_config.num_fbs = shared ? EXAMPLE_LCD_SHARED_BUF_NUM : EXAMPLE_LCD_COPY_BUF_NUM;
    ek79007_vendor_config_t vendor_config = {
        .mipi_config = {
            .dsi_bus = dsi_bus,
            .dpi_config = &dpi_config,
        },
    };
    const esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = EXAMPLE_PIN_NUM_LCD_RST,
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,
        .bits_per_pixel = EXAMPLE_LCD_BIT_PER_PIXEL,
        .vendor_config = &vendor_config,
    };
    esp_lcd_panel_handle_t display;
    ESP_ERROR_CHECK(esp_lcd_new_panel_ek79007(panel_io, &panel_config, &display));
    ESP_ERROR_CHECK(esp_lcd_panel_reset(display));
    if (shared) {
        ESP_ERROR_CHECK(esp_lcd_dpi_panel_get_frame_buffer(display, EXAMPLE_LCD_SHARED_BUF_NUM,
                                                           &lcd.buffers[0], &lcd.buffers[1], &lcd.buffers[2]));
    } else {
        ESP_ERROR_CHECK(esp_lcd_dpi_panel_get_frame_buffer(display, EXAMPLE_LCD_COPY_BUF_NUM,
                                                           &lcd.buffers[0], &lcd.buffers[1]));
        const ppa_client_config_t ppa_config = {.oper_type = PPA_OPERATION_SRM};
        ESP_ERROR_CHECK(ppa_register_client(&ppa_config, &lcd.ppa));
    }
    const esp_lcd_dpi_panel_event_callbacks_t callbacks = {
        .on_frame_buf_complete = lcd_frame_done,
    };
    ESP_ERROR_CHECK(esp_lcd_dpi_panel_register_event_callbacks(display, &callbacks, lcd.frame_done));
    ESP_ERROR_CHECK(esp_lcd_panel_init(display));
    if (xSemaphoreTake(lcd.frame_done, pdMS_TO_TICKS(EXAMPLE_LCD_FRAME_TIMEOUT_MS)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }
    (void)__atomic_load_n(&lcd.frame_done_count, __ATOMIC_ACQUIRE);
    *draw_core = __atomic_load_n(&lcd.callback_core, __ATOMIC_RELAXED);
    *panel_handle = display;
    return ESP_OK;
}

esp_err_t app_lcd_draw_bitmap(esp_lcd_panel_handle_t panel, int width, int height, const void *buffer, int *retired_index)
{
    if (!lcd.frame_done) {
        return ESP_ERR_INVALID_STATE;
    }
    if (!panel || !buffer || !retired_index || width <= 0 || height <= 0) {
        return ESP_ERR_INVALID_ARG;
    }
    if (xPortGetCoreID() != __atomic_load_n(&lcd.callback_core, __ATOMIC_RELAXED)) {
        return ESP_ERR_INVALID_STATE;
    }
    *retired_index = -1;
    esp_err_t ret = wait_for_retirement();
    if (ret != ESP_OK) {
        return ret;
    }
    if (lcd.shared) {
        if (width != EXAMPLE_LCD_H_RES || height != EXAMPLE_LCD_V_RES) {
            return ESP_ERR_INVALID_SIZE;
        }
        uint8_t index = 0;
        while (index < EXAMPLE_LCD_SHARED_BUF_NUM && lcd.buffers[index] != buffer) {
            ++index;
        }
        if (index == EXAMPLE_LCD_SHARED_BUF_NUM || index == lcd.display_index) {
            return ESP_ERR_INVALID_ARG;
        }
        ret = publish_buffer(panel, index);
        if (ret == ESP_OK) {
            ret = wait_for_retirement();
        }
        if (ret == ESP_OK) {
            *retired_index = lcd.retiring_index;
        }
        return ret;
    }
    const uint32_t draw_width = width > EXAMPLE_LCD_H_RES ? EXAMPLE_LCD_H_RES : width;
    const uint32_t draw_height = height > EXAMPLE_LCD_V_RES ? EXAMPLE_LCD_V_RES : height;
    const ppa_srm_oper_config_t copy = {
        .in = {
            .buffer = buffer,
            .pic_w = width,
            .pic_h = height,
            .block_w = draw_width,
            .block_h = draw_height,
            .block_offset_x = (width - draw_width) / 2,
            .block_offset_y = (height - draw_height) / 2,
            .srm_cm = EXAMPLE_LCD_BIT_PER_PIXEL == 16 ? PPA_SRM_COLOR_MODE_RGB565 : PPA_SRM_COLOR_MODE_RGB888,
        },
        .out = {
            .buffer = lcd.buffers[lcd.draw_index],
            .buffer_size = EXAMPLE_LCD_BUF_SIZE,
            .pic_w = EXAMPLE_LCD_H_RES,
            .pic_h = EXAMPLE_LCD_V_RES,
            .srm_cm = EXAMPLE_LCD_BIT_PER_PIXEL == 16 ? PPA_SRM_COLOR_MODE_RGB565 : PPA_SRM_COLOR_MODE_RGB888,
        },
        .rotation_angle = PPA_SRM_ROTATION_ANGLE_0,
        .scale_x = 1,
        .scale_y = 1,
        .mode = PPA_TRANS_MODE_BLOCKING,
    };
    ret = ppa_do_scale_rotate_mirror(lcd.ppa, &copy);
    if (ret != ESP_OK) {
        return ret;
    }
    return publish_buffer(panel, lcd.draw_index);
}
