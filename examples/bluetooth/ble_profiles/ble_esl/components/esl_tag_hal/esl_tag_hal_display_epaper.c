/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "sdkconfig.h"

#if CONFIG_ESL_TAG_DISPLAY_BACKEND_EPAPER

#include "esl_tag_hal.h"

#include <string.h>
#include <inttypes.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "driver/gpio.h"
#include "esp_check.h"
#include "esp_log.h"
#include "epaper.h"

static const char *TAG = "tag_hal_epd";

#define DISPLAY_QUEUE_DEPTH           2
#define PANEL_FAIL_DEGRADE_THRESHOLD  3
#define EPAPER_POWER_STABLE_MS        300

typedef enum {
    DISP_STATE_UNINITIALIZED = 0,
    DISP_STATE_READY,
    DISP_STATE_DEGRADED,
} display_backend_state_t;

typedef enum {
    DISP_REQ_SHOW = 0,
    DISP_REQ_REFRESH,
} display_req_type_t;

typedef struct {
    display_req_type_t type;
    uint8_t display_index;
    uint8_t image_index;
} display_req_t;

static SemaphoreHandle_t s_state_mutex;
static QueueHandle_t s_display_queue;
static TaskHandle_t s_display_task;
static uint8_t *s_snapshot;
static epaper_handle_t s_epaper;
static display_backend_state_t s_state = DISP_STATE_UNINITIALIZED;
static uint8_t s_consecutive_panel_failures;
static bool s_has_last_displayed_image;
static uint8_t s_last_displayed_image_index;
static bool s_pwr_gpio_ready;
static bool s_degraded_logged;

esp_err_t esl_tag_display_epaper_init(void);

static void display_queue_drain(void)
{
    if (s_display_queue == NULL) {
        return;
    }
    display_req_t drop;
    while (xQueueReceive(s_display_queue, &drop, 0) == pdTRUE) {
    }
}

static void power_set_active(bool active)
{
    if (!s_pwr_gpio_ready) {
        return;
    }
#if CONFIG_ESL_TAG_EPAPER_PWR_ACTIVE_HIGH
    /* Active-high: high = on (matches esp_esl_demo H4 during refresh). */
    gpio_set_level((gpio_num_t)CONFIG_ESL_TAG_EPAPER_PWR_GPIO, active ? 1 : 0);
#else
    /* Active-low: low = on. */
    gpio_set_level((gpio_num_t)CONFIG_ESL_TAG_EPAPER_PWR_GPIO, active ? 0 : 1);
#endif
}

static esp_err_t power_gpio_init(void)
{
    gpio_config_t conf = {
        .pin_bit_mask = 1ULL << CONFIG_ESL_TAG_EPAPER_PWR_GPIO,
                             .mode = GPIO_MODE_OUTPUT,
                             .pull_up_en = GPIO_PULLUP_DISABLE,
                             .pull_down_en = GPIO_PULLDOWN_DISABLE,
                             .intr_type = GPIO_INTR_DISABLE,
    };
    esp_err_t ret = gpio_config(&conf);
    if (ret != ESP_OK) {
        return ret;
    }
    s_pwr_gpio_ready = true;
    power_set_active(false);
    return ESP_OK;
}

static void mark_degraded_locked(const char *reason)
{
    s_state = DISP_STATE_DEGRADED;
    display_queue_drain();
    if (!s_degraded_logged) {
        ESP_LOGE(TAG, "Display backend DEGRADED: %s", reason ? reason : "unknown");
        s_degraded_logged = true;
    }
}

static esp_err_t ensure_transport_locked(void)
{
    if (s_state == DISP_STATE_DEGRADED) {
        return ESP_ERR_INVALID_STATE;
    }
    if (s_epaper != NULL && s_state == DISP_STATE_READY) {
        return ESP_OK;
    }

    esp_err_t ret = power_gpio_init();
    if (ret != ESP_OK) {
        mark_degraded_locked("power GPIO init failed");
        return ret;
    }

    ret = epaper_create((spi_host_device_t)CONFIG_ESL_TAG_EPAPER_SPI_HOST, &s_epaper);
    if (ret != ESP_OK) {
        s_epaper = NULL;
        mark_degraded_locked("epaper_create failed");
        return ret;
    }

    s_state = DISP_STATE_READY;
    s_consecutive_panel_failures = 0;
    ESP_LOGI(TAG, "E-paper transport ready (SPI host %d)", CONFIG_ESL_TAG_EPAPER_SPI_HOST);
    return ESP_OK;
}

static esp_err_t run_panel_session(uint8_t image_index)
{
    size_t snap_len = 0;
    esp_err_t ret = ble_esl_image_snapshot(image_index, s_snapshot,
                                           EPAPER_SCREEN_BUFFER_SIZE, &snap_len);
    if (ret != ESP_OK) {
        /* Software/input failure: do not count toward hardware degrade. */
        ESP_LOGW(TAG, "image snapshot failed index=%u: %s", image_index, esp_err_to_name(ret));
        return ret;
    }
    if (snap_len != EPAPER_SCREEN_BUFFER_SIZE) {
        ESP_LOGW(TAG, "image size %" PRIu32 " != %d", (uint32_t)snap_len, EPAPER_SCREEN_BUFFER_SIZE);
        return ESP_ERR_INVALID_SIZE;
    }

    power_set_active(true);
    vTaskDelay(pdMS_TO_TICKS(EPAPER_POWER_STABLE_MS));
    ESP_LOGI(TAG,
             "Panel session start: PWR_GPIO=%d level=%d (%s) BUSY_GPIO=%d level=%d "
             "(1=busy 0=idle) SPI MOSI=%d SCLK=%d CS=%d DC=%d RST=%d",
             CONFIG_ESL_TAG_EPAPER_PWR_GPIO,
             gpio_get_level((gpio_num_t)CONFIG_ESL_TAG_EPAPER_PWR_GPIO),
#if CONFIG_ESL_TAG_EPAPER_PWR_ACTIVE_HIGH
             "1=on",
#else
             "0=on",
#endif
             CONFIG_EPAPER_PIN_NUM_BUSY,
             gpio_get_level((gpio_num_t)CONFIG_EPAPER_PIN_NUM_BUSY),
             CONFIG_EPAPER_PIN_NUM_MOSI, CONFIG_EPAPER_PIN_NUM_SCLK,
             CONFIG_EPAPER_PIN_NUM_CS, CONFIG_EPAPER_PIN_NUM_DC,
             CONFIG_EPAPER_PIN_NUM_RST);

    ret = epaper_panel_init(s_epaper);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "panel_init failed BUSY=%d: %s",
                 gpio_get_level((gpio_num_t)CONFIG_EPAPER_PIN_NUM_BUSY),
                 esp_err_to_name(ret));
        goto panel_fail;
    }
    ESP_LOGI(TAG, "panel_init ok BUSY=%d",
             gpio_get_level((gpio_num_t)CONFIG_EPAPER_PIN_NUM_BUSY));

    ret = epaper_refresh_frame(s_epaper, s_snapshot, snap_len);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "refresh_frame failed BUSY=%d: %s",
                 gpio_get_level((gpio_num_t)CONFIG_EPAPER_PIN_NUM_BUSY),
                 esp_err_to_name(ret));
        goto panel_fail;
    }

    (void)epaper_busy_intr_disable(s_epaper);
    power_set_active(false);

    xSemaphoreTake(s_state_mutex, portMAX_DELAY);
    s_consecutive_panel_failures = 0;
    s_has_last_displayed_image = true;
    s_last_displayed_image_index = image_index;
    xSemaphoreGive(s_state_mutex);
    ESP_LOGI(TAG, "Displayed image %u (%u bytes)", image_index, (unsigned)snap_len);
    return ESP_OK;

panel_fail:
    (void)epaper_busy_intr_disable(s_epaper);
    power_set_active(false);

    xSemaphoreTake(s_state_mutex, portMAX_DELAY);
    if (s_state == DISP_STATE_READY) {
        s_consecutive_panel_failures++;
        ESP_LOGW(TAG, "Panel session failed (%u/%u): %s",
                 s_consecutive_panel_failures, PANEL_FAIL_DEGRADE_THRESHOLD,
                 esp_err_to_name(ret));
        if (s_consecutive_panel_failures >= PANEL_FAIL_DEGRADE_THRESHOLD) {
            mark_degraded_locked("consecutive panel session failures");
        }
    }
    xSemaphoreGive(s_state_mutex);
    return ret;
}

static void display_task(void *arg)
{
    (void)arg;
    display_req_t req;

    for (;;) {
        if (xQueueReceive(s_display_queue, &req, portMAX_DELAY) != pdTRUE) {
            continue;
        }

        xSemaphoreTake(s_state_mutex, portMAX_DELAY);
        if (s_state == DISP_STATE_DEGRADED) {
            xSemaphoreGive(s_state_mutex);
            continue;
        }
        esp_err_t res = ensure_transport_locked();
        xSemaphoreGive(s_state_mutex);
        if (res != ESP_OK) {
            continue;
        }

        uint8_t image_index = req.image_index;
        if (req.type == DISP_REQ_REFRESH) {
            xSemaphoreTake(s_state_mutex, portMAX_DELAY);
            if (!s_has_last_displayed_image) {
                xSemaphoreGive(s_state_mutex);
                ESP_LOGW(TAG, "Refresh ignored: no previously displayed image");
                continue;
            }
            image_index = s_last_displayed_image_index;
            xSemaphoreGive(s_state_mutex);
        }

        (void)run_panel_session(image_index);
    }
}

esp_err_t esl_tag_display_epaper_init(void)
{
    if (s_state_mutex != NULL) {
        return ESP_OK;
    }

    s_state_mutex = xSemaphoreCreateMutex();
    if (s_state_mutex == NULL) {
        return ESP_ERR_NO_MEM;
    }

    s_snapshot = (uint8_t *)calloc(1, EPAPER_SCREEN_BUFFER_SIZE);
    if (s_snapshot == NULL) {
        vSemaphoreDelete(s_state_mutex);
        s_state_mutex = NULL;
        return ESP_ERR_NO_MEM;
    }

    s_display_queue = xQueueCreate(DISPLAY_QUEUE_DEPTH, sizeof(display_req_t));
    if (s_display_queue == NULL) {
        free(s_snapshot);
        s_snapshot = NULL;
        vSemaphoreDelete(s_state_mutex);
        s_state_mutex = NULL;
        return ESP_ERR_NO_MEM;
    }

    BaseType_t ok = xTaskCreate(display_task, "esl_epd", 4096, NULL, 5, &s_display_task);
    if (ok != pdPASS) {
        vQueueDelete(s_display_queue);
        s_display_queue = NULL;
        free(s_snapshot);
        s_snapshot = NULL;
        vSemaphoreDelete(s_state_mutex);
        s_state_mutex = NULL;
        return ESP_ERR_NO_MEM;
    }

    s_state = DISP_STATE_UNINITIALIZED;
    ESP_LOGI(TAG, "E-paper display backend initialized (lazy transport create)");
    return ESP_OK;
}

static esp_err_t enqueue_request(display_req_type_t type, uint8_t display_index, uint8_t image_index)
{
    if (s_state_mutex == NULL || s_display_queue == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    xSemaphoreTake(s_state_mutex, portMAX_DELAY);
    if (s_state == DISP_STATE_DEGRADED) {
        xSemaphoreGive(s_state_mutex);
        return ESP_ERR_INVALID_STATE;
    }
    if (type == DISP_REQ_REFRESH && !s_has_last_displayed_image) {
        xSemaphoreGive(s_state_mutex);
        return ESP_ERR_INVALID_STATE;
    }
    xSemaphoreGive(s_state_mutex);

    display_req_t req = {
        .type = type,
        .display_index = display_index,
        .image_index = image_index,
    };
    if (xQueueSend(s_display_queue, &req, 0) != pdTRUE) {
        if (xQueueSend(s_display_queue, &req, pdMS_TO_TICKS(20)) != pdTRUE) {
            ESP_LOGE(TAG, "display queue full type=%d", (int)type);
            return ESP_ERR_TIMEOUT;
        }
    }
    return ESP_OK;
}

esp_err_t esl_tag_display_show(uint8_t display_index, uint8_t image_index)
{
    if (display_index != 0) {
        return ESP_ERR_INVALID_ARG;
    }
    return enqueue_request(DISP_REQ_SHOW, display_index, image_index);
}

esp_err_t esl_tag_display_refresh(uint8_t display_index)
{
    if (display_index != 0) {
        return ESP_ERR_INVALID_ARG;
    }
    return enqueue_request(DISP_REQ_REFRESH, display_index, 0);
}

#else /* !CONFIG_ESL_TAG_DISPLAY_BACKEND_EPAPER */

/* Mock builds compile this translation unit empty on purpose. */
typedef int esl_tag_hal_display_epaper_mock_tu_t;

#endif /* CONFIG_ESL_TAG_DISPLAY_BACKEND_EPAPER */
