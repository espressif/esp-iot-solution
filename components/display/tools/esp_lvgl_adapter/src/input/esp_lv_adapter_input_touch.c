/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file esp_lv_adapter_input_touch.c
 * @brief Touch input device implementation for LVGL adapter
 */

#include <stdlib.h>
#include "driver/gpio.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_check.h"
#include "esp_lv_adapter_input.h"
#include "adapter_internal.h"
#include "display_manager.h"

/* Tag for logging */
static const char *TAG = "esp_lvgl:touch";

/* Default scale factor when not specified */
#define DEFAULT_SCALE_FACTOR    1.0f

/* Maximum touch points to read in LVGL v8 path */
#define MAX_TOUCH_POINTS_V8     1
/* LVGL gesture internals store finger id as int8_t, valid runtime IDs are 0..127 */
#define MAX_LV_GESTURE_TOUCH_ID 127

/**
 * @brief ISR context wrapper to protect original user_data
 *
 * This structure wraps the touch context pointer to avoid overwriting
 * the original user_data in esp_lcd_touch_handle_t->config.user_data
 */
typedef struct {
    void *touch_ctx;
    void *original_user_data;
    volatile bool unregistering;
} esp_lv_adapter_touch_isr_ctx_t;

/*****************************************************************************
 *                         LVGL v9 Implementation                            *
 *****************************************************************************/

#if LVGL_VERSION_MAJOR >= 9

#if LV_USE_GESTURE_RECOGNITION
#define MAX_TOUCH_POINTS_V9     CONFIG_ESP_LCD_TOUCH_MAX_POINTS
#else
#define MAX_TOUCH_POINTS_V9     1
#endif

/**
 * @brief Touch device context for LVGL v9
 */
typedef struct {
    esp_lcd_touch_handle_t handle;      /*!< Touch device handle */
    lv_indev_t *indev;                  /*!< LVGL input device */
    struct {
        float x;                        /*!< X-axis scale factor */
        float y;                        /*!< Y-axis scale factor */
    } scale;                            /*!< Touch coordinate scaling */
    bool with_irq;                      /*!< Whether interrupt mode is enabled */
    SemaphoreHandle_t touch_sem;        /*!< Binary semaphore for touch event signaling (IRQ mode) */
    esp_lv_adapter_touch_isr_ctx_t *isr_ctx;  /*!< ISR context wrapper to protect user_data */
    lv_point_t last_point;              /*!< Last touch point coordinates */
    lv_indev_state_t last_state;        /*!< Last touch state */
#if LV_USE_GESTURE_RECOGNITION
    uint8_t prev_hw_track_id[MAX_TOUCH_POINTS_V9]; /*!< HW track IDs from previous read */
    uint8_t prev_lv_id[MAX_TOUCH_POINTS_V9];       /*!< LVGL touch IDs mapped from previous HW tracks */
    lv_point_t prev_point[MAX_TOUCH_POINTS_V9];    /*!< Last known points for previous active tracks */
    uint8_t prev_count;                             /*!< Number of active tracks in previous read */
    uint8_t next_lv_id;                             /*!< Next LVGL touch ID allocator */
    bool primary_valid;                             /*!< Whether primary track is valid */
    uint8_t primary_hw_track_id;                    /*!< Primary HW track used for pointer event */
#endif
} esp_lv_adapter_touch_ctx_t;

/* Forward declarations */
static void lvgl_touch_read(lv_indev_t *indev_drv, lv_indev_data_t *data);
static void lvgl_touch_isr(esp_lcd_touch_handle_t tp);
#if LV_USE_GESTURE_RECOGNITION
static bool contains_id(const uint8_t *ids, uint8_t count, uint8_t id);
static bool is_valid_lv_id(uint8_t id);
static bool is_lv_id_reserved(const esp_lv_adapter_touch_ctx_t *touch_ctx, const uint8_t *curr_ids,
                              uint8_t curr_count, uint8_t id);
static uint8_t alloc_lv_id(esp_lv_adapter_touch_ctx_t *touch_ctx, uint8_t *next_id,
                           const uint8_t *curr_ids, uint8_t curr_count);
static uint8_t find_prev_lv_id(const esp_lv_adapter_touch_ctx_t *touch_ctx, uint8_t hw_track_id, bool *mapped);
static uint8_t resolve_lv_id(esp_lv_adapter_touch_ctx_t *touch_ctx, uint8_t hw_track_id,
                             const uint8_t *curr_lv_ids, uint8_t curr_count);
static void append_released_touches(const esp_lv_adapter_touch_ctx_t *touch_ctx,
                                    const uint8_t *curr_hw_ids, uint8_t curr_count,
                                    lv_indev_touch_data_t *gesture_touches,
                                    uint16_t *gesture_touch_count, uint32_t ts);
static void flush_released_touches_on_error(const esp_lv_adapter_touch_ctx_t *touch_ctx,
                                            lv_indev_t *indev_drv, lv_indev_data_t *data);
static void save_current_tracks(esp_lv_adapter_touch_ctx_t *touch_ctx,
                                const uint8_t *curr_hw_ids, const uint8_t *curr_lv_ids,
                                const lv_point_t *curr_points, uint8_t curr_count);
static void update_primary_pointer(esp_lv_adapter_touch_ctx_t *touch_ctx,
                                   const uint8_t *curr_hw_ids, const lv_point_t *curr_points,
                                   uint8_t curr_count, bool primary_found);
static void reset_gesture_state(esp_lv_adapter_touch_ctx_t *touch_ctx);
#else
static bool s_gesture_disabled_logged = false;
#endif

lv_indev_t *esp_lv_adapter_register_touch(const esp_lv_adapter_touch_config_t *config)
{
    /* Validate parameters */
    if (!config || !config->disp || !config->handle) {
        ESP_LOGE(TAG, "Invalid configuration parameters");
        return NULL;
    }

    /* Allocate touch context */
    esp_lv_adapter_touch_ctx_t *touch_ctx = calloc(1, sizeof(esp_lv_adapter_touch_ctx_t));
    if (!touch_ctx) {
        ESP_LOGE(TAG, "Failed to allocate touch context");
        return NULL;
    }

    /* Initialize basic context */
    touch_ctx->handle = config->handle;
    touch_ctx->scale.x = config->scale.x ? config->scale.x : DEFAULT_SCALE_FACTOR;
    touch_ctx->scale.y = config->scale.y ? config->scale.y : DEFAULT_SCALE_FACTOR;
    touch_ctx->last_point.x = 0;
    touch_ctx->last_point.y = 0;
    touch_ctx->last_state = LV_INDEV_STATE_RELEASED;
    touch_ctx->touch_sem = NULL;
    touch_ctx->isr_ctx = NULL;
#if LV_USE_GESTURE_RECOGNITION
    touch_ctx->prev_count = 0;
    touch_ctx->next_lv_id = 0;
    touch_ctx->primary_valid = false;
#endif

    /* Check if interrupt mode is supported */
    bool with_irq = touch_ctx->handle->config.int_gpio_num != GPIO_NUM_NC;
    touch_ctx->with_irq = with_irq;

#if !LV_USE_GESTURE_RECOGNITION
    if (!s_gesture_disabled_logged) {
        ESP_LOGW(TAG, "LV_USE_GESTURE_RECOGNITION is disabled; only single-point pointer events are available");
        s_gesture_disabled_logged = true;
    }
#endif

    if (with_irq) {
        touch_ctx->touch_sem = xSemaphoreCreateBinary();
        if (!touch_ctx->touch_sem) {
            ESP_LOGE(TAG, "Failed to create touch semaphore");
            free(touch_ctx);
            return NULL;
        }

        xSemaphoreGive(touch_ctx->touch_sem);

        touch_ctx->isr_ctx = calloc(1, sizeof(esp_lv_adapter_touch_isr_ctx_t));
        if (!touch_ctx->isr_ctx) {
            ESP_LOGE(TAG, "Failed to allocate ISR context wrapper");
            vSemaphoreDelete(touch_ctx->touch_sem);
            free(touch_ctx);
            return NULL;
        }

        touch_ctx->isr_ctx->original_user_data = touch_ctx->handle->config.user_data;
        touch_ctx->isr_ctx->touch_ctx = touch_ctx;
        touch_ctx->isr_ctx->unregistering = false;

        esp_err_t err = esp_lcd_touch_register_interrupt_callback_with_data(
                            touch_ctx->handle,
                            lvgl_touch_isr,
                            touch_ctx->isr_ctx
                        );
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to register interrupt callback: 0x%x", err);
            free(touch_ctx->isr_ctx);
            vSemaphoreDelete(touch_ctx->touch_sem);
            free(touch_ctx);
            return NULL;
        }

        ESP_LOGD(TAG, "Touch interrupt mode enabled on GPIO %d", touch_ctx->handle->config.int_gpio_num);
    }

    if (esp_lv_adapter_lock((uint32_t) -1) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to acquire LVGL lock");
        goto cleanup_on_error;
    }

    lv_indev_t *indev = lv_indev_create();
    if (!indev) {
        ESP_LOGE(TAG, "Failed to create LVGL input device");
        esp_lv_adapter_unlock();
        goto cleanup_on_error;
    }

    /* Configure input device */
    lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(indev, lvgl_touch_read);
    lv_indev_set_disp(indev, config->disp);
    lv_indev_set_driver_data(indev, touch_ctx);
    touch_ctx->indev = indev;

    esp_lv_adapter_unlock();

    /* Register input device for unified management */
    esp_err_t ret = esp_lv_adapter_register_input_device(indev, ESP_LV_ADAPTER_INPUT_TYPE_TOUCH, touch_ctx);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to register input device for management: 0x%x", ret);
        /* Continue anyway, device will still work but won't be auto-cleaned */
    }

    /* Associate with display for sleep mgmt */
    esp_lv_adapter_display_node_t *display_node = display_manager_get_node(config->disp);
    if (display_node && display_node->sleep.input_count < 8) {
        display_node->sleep.associated_inputs[display_node->sleep.input_count++] = indev;
    }

    ESP_LOGI(TAG, "Touch input device registered successfully (IRQ mode: %s)", with_irq ? "enabled" : "disabled");
    return indev;

cleanup_on_error:
    if (with_irq) {
        if (touch_ctx->isr_ctx) {
            /* Restore original user_data before cleanup */
            touch_ctx->handle->config.user_data = touch_ctx->isr_ctx->original_user_data;
            esp_lcd_touch_register_interrupt_callback(touch_ctx->handle, NULL);
            free(touch_ctx->isr_ctx);
        }
        if (touch_ctx->touch_sem) {
            vSemaphoreDelete(touch_ctx->touch_sem);
        }
    }
    free(touch_ctx);
    return NULL;
}

esp_err_t esp_lv_adapter_unregister_touch(lv_indev_t *touch)
{
    ESP_RETURN_ON_FALSE(touch, ESP_ERR_INVALID_ARG, TAG, "Touch input handle cannot be NULL");

    esp_lv_adapter_touch_ctx_t *touch_ctx = (esp_lv_adapter_touch_ctx_t *)lv_indev_get_driver_data(touch);
    ESP_RETURN_ON_FALSE(touch_ctx, ESP_ERR_INVALID_STATE, TAG, "Touch context is NULL");

    if (touch_ctx->with_irq) {
        if (touch_ctx->handle->config.int_gpio_num != GPIO_NUM_NC) {
            gpio_intr_disable(touch_ctx->handle->config.int_gpio_num);
            ESP_LOGD(TAG, "Disabled GPIO interrupt on pin %d", touch_ctx->handle->config.int_gpio_num);
        }

        esp_lcd_touch_register_interrupt_callback(touch_ctx->handle, NULL);
        ESP_LOGD(TAG, "Unregistered touch interrupt callback");

        if (touch_ctx->isr_ctx) {
            touch_ctx->isr_ctx->unregistering = true;
            __sync_synchronize();
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }

    esp_lv_adapter_unregister_input_device(touch);

    /* Step 5: Acquire LVGL lock and delete input device */
    esp_err_t ret = esp_lv_adapter_lock((uint32_t) -1);
    ESP_RETURN_ON_ERROR(ret, TAG, "Failed to acquire LVGL lock: 0x%x", ret);

    lv_indev_delete(touch);
    esp_lv_adapter_unlock();

    if (touch_ctx->with_irq) {
        if (touch_ctx->isr_ctx) {
            touch_ctx->handle->config.user_data = touch_ctx->isr_ctx->original_user_data;
            free(touch_ctx->isr_ctx);
            ESP_LOGD(TAG, "Restored original user_data and freed ISR context");
        }

        if (touch_ctx->touch_sem) {
            vSemaphoreDelete(touch_ctx->touch_sem);
            ESP_LOGD(TAG, "Deleted touch semaphore");
        }
    }

    free(touch_ctx);

    ESP_LOGI(TAG, "Touch input device unregistered successfully");
    return ESP_OK;
}

static void lvgl_touch_read(lv_indev_t *indev_drv, lv_indev_data_t *data)
{
    esp_lv_adapter_touch_ctx_t *touch_ctx = (esp_lv_adapter_touch_ctx_t *)lv_indev_get_driver_data(indev_drv);
    if (!touch_ctx || !touch_ctx->handle) {
        data->state = LV_INDEV_STATE_RELEASED;
        return;
    }

    /* Determine if we should read touch data */
    bool should_read = true;
    if (touch_ctx->with_irq) {
        /* In IRQ mode, only read if semaphore is available (signaled by ISR) */
        should_read = (xSemaphoreTake(touch_ctx->touch_sem, 0) == pdTRUE);
    }

    if (should_read) {
        esp_lcd_touch_point_data_t touch_data[MAX_TOUCH_POINTS_V9] = {0};
        uint8_t count = 0;

        /* Read touch data from hardware */
        esp_lcd_touch_read_data(touch_ctx->handle);

        /* Get touch coordinates */
        esp_err_t ret = esp_lcd_touch_get_data(
                            touch_ctx->handle,
                            touch_data,
                            &count,
                            MAX_TOUCH_POINTS_V9
                        );

#if LV_USE_GESTURE_RECOGNITION
        if (ret == ESP_OK) {
            lv_indev_touch_data_t gesture_touches[MAX_TOUCH_POINTS_V9 * 2] = {0};
            uint8_t curr_hw_ids[MAX_TOUCH_POINTS_V9] = {0};
            uint8_t curr_lv_ids[MAX_TOUCH_POINTS_V9] = {0};
            lv_point_t curr_points[MAX_TOUCH_POINTS_V9] = {0};
            uint8_t curr_count = count > MAX_TOUCH_POINTS_V9 ? MAX_TOUCH_POINTS_V9 : count;
            uint16_t gesture_touch_count = 0;
            uint32_t ts = lv_tick_get();
            bool primary_found = false;

            for (uint8_t i = 0; i < curr_count; i++) {
                uint8_t hw_track_id = touch_data[i].track_id;
                uint8_t lv_id = resolve_lv_id(touch_ctx, hw_track_id, curr_lv_ids, i);

                curr_hw_ids[i] = hw_track_id;
                curr_lv_ids[i] = lv_id;
                curr_points[i].x = (lv_coord_t)(touch_ctx->scale.x * touch_data[i].x);
                curr_points[i].y = (lv_coord_t)(touch_ctx->scale.y * touch_data[i].y);

                gesture_touches[gesture_touch_count].id = lv_id;
                gesture_touches[gesture_touch_count].point = curr_points[i];
                gesture_touches[gesture_touch_count].state = LV_INDEV_STATE_PRESSED;
                gesture_touches[gesture_touch_count].timestamp = ts;
                gesture_touch_count++;

                if (touch_ctx->primary_valid && touch_ctx->primary_hw_track_id == hw_track_id) {
                    touch_ctx->last_point = curr_points[i];
                    primary_found = true;
                }
            }

            append_released_touches(touch_ctx, curr_hw_ids, curr_count, gesture_touches, &gesture_touch_count, ts);

            if (gesture_touch_count > 0) {
                lv_indev_gesture_recognizers_update(indev_drv, gesture_touches, gesture_touch_count);
                lv_indev_gesture_recognizers_set_data(indev_drv, data);
            }

            save_current_tracks(touch_ctx, curr_hw_ids, curr_lv_ids, curr_points, curr_count);
            update_primary_pointer(touch_ctx, curr_hw_ids, curr_points, curr_count, primary_found);
        } else {
            flush_released_touches_on_error(touch_ctx, indev_drv, data);
            reset_gesture_state(touch_ctx);
        }
#else
        /* Single-point pointer path */
        if (ret == ESP_OK && count > 0) {
            touch_ctx->last_point.x = (lv_coord_t)(touch_ctx->scale.x * touch_data[0].x);
            touch_ctx->last_point.y = (lv_coord_t)(touch_ctx->scale.y * touch_data[0].y);
            touch_ctx->last_state = LV_INDEV_STATE_PRESSED;
        } else {
            touch_ctx->last_state = LV_INDEV_STATE_RELEASED;
        }
#endif
    }

    /* Return last known state */
    data->point.x = touch_ctx->last_point.x;
    data->point.y = touch_ctx->last_point.y;
    data->state = touch_ctx->last_state;
}

/**
 * @brief Touch interrupt service routine
 *
 * Called when touch interrupt is triggered (IRQ mode only).
 * Signals the semaphore to notify the read callback that touch data is available.
 */
static void IRAM_ATTR lvgl_touch_isr(esp_lcd_touch_handle_t tp)
{
    esp_lv_adapter_touch_isr_ctx_t *isr_ctx = (esp_lv_adapter_touch_isr_ctx_t *)tp->config.user_data;
    if (!isr_ctx || !isr_ctx->touch_ctx) {
        return;
    }

    if (isr_ctx->unregistering) {
        return;
    }

    esp_lv_adapter_touch_ctx_t *touch_ctx = (esp_lv_adapter_touch_ctx_t *)isr_ctx->touch_ctx;
    if (!touch_ctx->touch_sem) {
        return;
    }

    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    xSemaphoreGiveFromISR(touch_ctx->touch_sem, &xHigherPriorityTaskWoken);

    if (xHigherPriorityTaskWoken == pdTRUE) {
        portYIELD_FROM_ISR();
    }
}

#if LV_USE_GESTURE_RECOGNITION
static bool contains_id(const uint8_t *ids, uint8_t count, uint8_t id)
{
    for (uint8_t i = 0; i < count; i++) {
        if (ids[i] == id) {
            return true;
        }
    }

    return false;
}

static bool is_valid_lv_id(uint8_t id)
{
    return id <= MAX_LV_GESTURE_TOUCH_ID;
}

static bool is_lv_id_reserved(const esp_lv_adapter_touch_ctx_t *touch_ctx, const uint8_t *curr_ids,
                              uint8_t curr_count, uint8_t id)
{
    return contains_id(curr_ids, curr_count, id) ||
           contains_id(touch_ctx->prev_lv_id, touch_ctx->prev_count, id);
}

static uint8_t alloc_lv_id(esp_lv_adapter_touch_ctx_t *touch_ctx, uint8_t *next_id,
                           const uint8_t *curr_ids, uint8_t curr_count)
{
    for (uint16_t tries = 0; tries <= MAX_LV_GESTURE_TOUCH_ID; tries++) {
        uint8_t id = *next_id;
        *next_id = (uint8_t)((*next_id + 1) & MAX_LV_GESTURE_TOUCH_ID);
        if (!is_lv_id_reserved(touch_ctx, curr_ids, curr_count, id)) {
            return id;
        }
    }

    return 0;
}

static uint8_t find_prev_lv_id(const esp_lv_adapter_touch_ctx_t *touch_ctx, uint8_t hw_track_id, bool *mapped)
{
    *mapped = false;
    for (uint8_t i = 0; i < touch_ctx->prev_count && i < MAX_TOUCH_POINTS_V9; i++) {
        if (touch_ctx->prev_hw_track_id[i] == hw_track_id) {
            *mapped = true;
            return touch_ctx->prev_lv_id[i];
        }
    }
    return 0;
}

static uint8_t resolve_lv_id(esp_lv_adapter_touch_ctx_t *touch_ctx, uint8_t hw_track_id,
                             const uint8_t *curr_lv_ids, uint8_t curr_count)
{
    bool mapped = false;
    uint8_t lv_id = find_prev_lv_id(touch_ctx, hw_track_id, &mapped);
    if (!mapped || !is_valid_lv_id(lv_id) || contains_id(curr_lv_ids, curr_count, lv_id)) {
        lv_id = alloc_lv_id(touch_ctx, &touch_ctx->next_lv_id, curr_lv_ids, curr_count);
    }
    return lv_id;
}

static void append_released_touches(const esp_lv_adapter_touch_ctx_t *touch_ctx,
                                    const uint8_t *curr_hw_ids, uint8_t curr_count,
                                    lv_indev_touch_data_t *gesture_touches,
                                    uint16_t *gesture_touch_count, uint32_t ts)
{
    for (uint8_t i = 0; i < touch_ctx->prev_count && i < MAX_TOUCH_POINTS_V9; i++) {
        uint8_t prev_hw_id = touch_ctx->prev_hw_track_id[i];
        if (!contains_id(curr_hw_ids, curr_count, prev_hw_id)) {
            gesture_touches[*gesture_touch_count].id = touch_ctx->prev_lv_id[i];
            gesture_touches[*gesture_touch_count].point = touch_ctx->prev_point[i];
            gesture_touches[*gesture_touch_count].state = LV_INDEV_STATE_RELEASED;
            gesture_touches[*gesture_touch_count].timestamp = ts;
            (*gesture_touch_count)++;
        }
    }
}

static void flush_released_touches_on_error(const esp_lv_adapter_touch_ctx_t *touch_ctx,
                                            lv_indev_t *indev_drv, lv_indev_data_t *data)
{
    if (touch_ctx->prev_count == 0) {
        return;
    }

    lv_indev_touch_data_t gesture_touches[MAX_TOUCH_POINTS_V9] = {0};
    uint8_t curr_hw_ids[MAX_TOUCH_POINTS_V9] = {0};
    uint16_t gesture_touch_count = 0;
    uint32_t ts = lv_tick_get();

    append_released_touches(touch_ctx, curr_hw_ids, 0, gesture_touches, &gesture_touch_count, ts);
    if (gesture_touch_count > 0) {
        lv_indev_gesture_recognizers_update(indev_drv, gesture_touches, gesture_touch_count);
        lv_indev_gesture_recognizers_set_data(indev_drv, data);
    }
}

static void save_current_tracks(esp_lv_adapter_touch_ctx_t *touch_ctx,
                                const uint8_t *curr_hw_ids, const uint8_t *curr_lv_ids,
                                const lv_point_t *curr_points, uint8_t curr_count)
{
    curr_count = curr_count > MAX_TOUCH_POINTS_V9 ? MAX_TOUCH_POINTS_V9 : curr_count;
    touch_ctx->prev_count = curr_count;
    for (uint8_t i = 0; i < curr_count; i++) {
        touch_ctx->prev_hw_track_id[i] = curr_hw_ids[i];
        touch_ctx->prev_lv_id[i] = curr_lv_ids[i];
        touch_ctx->prev_point[i] = curr_points[i];
    }
}

static void update_primary_pointer(esp_lv_adapter_touch_ctx_t *touch_ctx,
                                   const uint8_t *curr_hw_ids, const lv_point_t *curr_points,
                                   uint8_t curr_count, bool primary_found)
{
    if (curr_count == 0) {
        touch_ctx->primary_valid = false;
        touch_ctx->last_state = LV_INDEV_STATE_RELEASED;
        return;
    }

    if (!primary_found) {
        touch_ctx->primary_hw_track_id = curr_hw_ids[0];
        touch_ctx->last_point = curr_points[0];
    }
    touch_ctx->primary_valid = true;
    touch_ctx->last_state = LV_INDEV_STATE_PRESSED;
}

static void reset_gesture_state(esp_lv_adapter_touch_ctx_t *touch_ctx)
{
    touch_ctx->primary_valid = false;
    touch_ctx->prev_count = 0;
    touch_ctx->last_state = LV_INDEV_STATE_RELEASED;
}
#endif

/*****************************************************************************
 *                         LVGL v8 Implementation                            *
 *****************************************************************************/

#else

/**
 * @brief Touch device context for LVGL v8
 */
typedef struct {
    esp_lcd_touch_handle_t handle;      /*!< Touch device handle */
    lv_indev_drv_t indev_drv;           /*!< LVGL v8 input device driver */
    lv_indev_t *indev;                  /*!< LVGL input device */
    struct {
        float x;                        /*!< X-axis scale factor */
        float y;                        /*!< Y-axis scale factor */
    } scale;                            /*!< Touch coordinate scaling */
    bool with_irq;                      /*!< Whether interrupt mode is enabled */
    SemaphoreHandle_t touch_sem;        /*!< Binary semaphore for touch event signaling (IRQ mode) */
    esp_lv_adapter_touch_isr_ctx_t *isr_ctx;  /*!< ISR context wrapper to protect user_data */
    lv_point_t last_point;              /*!< Last touch point coordinates */
    lv_indev_state_t last_state;        /*!< Last touch state */
} esp_lv_adapter_touch_ctx_t;

/* Forward declarations */
static void lvgl_touch_read(lv_indev_drv_t *indev_drv, lv_indev_data_t *data);
static void lvgl_touch_isr(esp_lcd_touch_handle_t tp);

lv_indev_t *esp_lv_adapter_register_touch(const esp_lv_adapter_touch_config_t *config)
{
    /* Validate parameters */
    if (!config || !config->disp || !config->handle) {
        ESP_LOGE(TAG, "Invalid configuration parameters");
        return NULL;
    }

    /* Allocate touch context */
    esp_lv_adapter_touch_ctx_t *touch_ctx = calloc(1, sizeof(esp_lv_adapter_touch_ctx_t));
    if (!touch_ctx) {
        ESP_LOGE(TAG, "Failed to allocate touch context");
        return NULL;
    }

    /* Initialize basic context */
    touch_ctx->handle = config->handle;
    touch_ctx->scale.x = config->scale.x ? config->scale.x : DEFAULT_SCALE_FACTOR;
    touch_ctx->scale.y = config->scale.y ? config->scale.y : DEFAULT_SCALE_FACTOR;
    touch_ctx->last_point.x = 0;
    touch_ctx->last_point.y = 0;
    touch_ctx->last_state = LV_INDEV_STATE_RELEASED;
    touch_ctx->touch_sem = NULL;
    touch_ctx->isr_ctx = NULL;

    bool with_irq = touch_ctx->handle->config.int_gpio_num != GPIO_NUM_NC;
    touch_ctx->with_irq = with_irq;

    if (with_irq) {
        touch_ctx->touch_sem = xSemaphoreCreateBinary();
        if (!touch_ctx->touch_sem) {
            ESP_LOGE(TAG, "Failed to create touch semaphore");
            free(touch_ctx);
            return NULL;
        }

        xSemaphoreGive(touch_ctx->touch_sem);

        touch_ctx->isr_ctx = calloc(1, sizeof(esp_lv_adapter_touch_isr_ctx_t));
        if (!touch_ctx->isr_ctx) {
            ESP_LOGE(TAG, "Failed to allocate ISR context wrapper");
            vSemaphoreDelete(touch_ctx->touch_sem);
            free(touch_ctx);
            return NULL;
        }

        touch_ctx->isr_ctx->original_user_data = touch_ctx->handle->config.user_data;
        touch_ctx->isr_ctx->touch_ctx = touch_ctx;
        touch_ctx->isr_ctx->unregistering = false;

        esp_err_t err = esp_lcd_touch_register_interrupt_callback_with_data(
                            touch_ctx->handle,
                            lvgl_touch_isr,
                            touch_ctx->isr_ctx
                        );
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to register interrupt callback: 0x%x", err);
            free(touch_ctx->isr_ctx);
            vSemaphoreDelete(touch_ctx->touch_sem);
            free(touch_ctx);
            return NULL;
        }

        ESP_LOGD(TAG, "Touch interrupt mode enabled on GPIO %d", touch_ctx->handle->config.int_gpio_num);
    }

    if (esp_lv_adapter_lock((uint32_t) -1) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to acquire LVGL lock");
        goto cleanup_on_error;
    }

    lv_indev_drv_init(&touch_ctx->indev_drv);
    touch_ctx->indev_drv.type = LV_INDEV_TYPE_POINTER;
    touch_ctx->indev_drv.disp = config->disp;
    touch_ctx->indev_drv.read_cb = lvgl_touch_read;
    touch_ctx->indev_drv.user_data = touch_ctx;

    /* Register input device driver */
    lv_indev_t *indev = lv_indev_drv_register(&touch_ctx->indev_drv);
    if (!indev) {
        ESP_LOGE(TAG, "Failed to register LVGL input device driver");
        esp_lv_adapter_unlock();
        goto cleanup_on_error;
    }

    touch_ctx->indev = indev;
    esp_lv_adapter_unlock();

    /* Register input device for unified management */
    esp_err_t ret = esp_lv_adapter_register_input_device(indev, ESP_LV_ADAPTER_INPUT_TYPE_TOUCH, touch_ctx);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to register input device for management: 0x%x", ret);
        /* Continue anyway, device will still work but won't be auto-cleaned */
    }

    ESP_LOGI(TAG, "Touch input device registered successfully (IRQ mode: %s)", with_irq ? "enabled" : "disabled");
    return indev;

cleanup_on_error:
    if (with_irq) {
        if (touch_ctx->isr_ctx) {
            /* Restore original user_data before cleanup */
            touch_ctx->handle->config.user_data = touch_ctx->isr_ctx->original_user_data;
            esp_lcd_touch_register_interrupt_callback(touch_ctx->handle, NULL);
            free(touch_ctx->isr_ctx);
        }
        if (touch_ctx->touch_sem) {
            vSemaphoreDelete(touch_ctx->touch_sem);
        }
    }
    free(touch_ctx);
    return NULL;
}

esp_err_t esp_lv_adapter_unregister_touch(lv_indev_t *touch)
{
    ESP_RETURN_ON_FALSE(touch, ESP_ERR_INVALID_ARG, TAG, "Touch input handle cannot be NULL");

    lv_indev_drv_t *indev_drv = touch->driver;
    ESP_RETURN_ON_FALSE(indev_drv, ESP_ERR_INVALID_STATE, TAG, "Touch driver not initialized");

    esp_lv_adapter_touch_ctx_t *touch_ctx = (esp_lv_adapter_touch_ctx_t *)indev_drv->user_data;
    ESP_RETURN_ON_FALSE(touch_ctx, ESP_ERR_INVALID_STATE, TAG, "Touch context is NULL");

    if (touch_ctx->with_irq) {
        if (touch_ctx->handle->config.int_gpio_num != GPIO_NUM_NC) {
            gpio_intr_disable(touch_ctx->handle->config.int_gpio_num);
            ESP_LOGD(TAG, "Disabled GPIO interrupt on pin %d", touch_ctx->handle->config.int_gpio_num);
        }

        esp_lcd_touch_register_interrupt_callback(touch_ctx->handle, NULL);
        ESP_LOGD(TAG, "Unregistered touch interrupt callback");

        if (touch_ctx->isr_ctx) {
            touch_ctx->isr_ctx->unregistering = true;
            __sync_synchronize();
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }

    esp_lv_adapter_unregister_input_device(touch);
    esp_err_t ret = esp_lv_adapter_lock((uint32_t) -1);
    ESP_RETURN_ON_ERROR(ret, TAG, "Failed to acquire LVGL lock: 0x%x", ret);

    lv_indev_delete(touch);
    esp_lv_adapter_unlock();

    if (touch_ctx->with_irq) {
        if (touch_ctx->isr_ctx) {
            touch_ctx->handle->config.user_data = touch_ctx->isr_ctx->original_user_data;
            free(touch_ctx->isr_ctx);
            ESP_LOGD(TAG, "Restored original user_data and freed ISR context");
        }

        if (touch_ctx->touch_sem) {
            vSemaphoreDelete(touch_ctx->touch_sem);
            ESP_LOGD(TAG, "Deleted touch semaphore");
        }
    }

    free(touch_ctx);

    ESP_LOGI(TAG, "Touch input device unregistered successfully");
    return ESP_OK;
}

static void lvgl_touch_read(lv_indev_drv_t *indev_drv, lv_indev_data_t *data)
{
    esp_lv_adapter_touch_ctx_t *touch_ctx = (esp_lv_adapter_touch_ctx_t *)indev_drv->user_data;
    if (!touch_ctx || !touch_ctx->handle) {
        data->state = LV_INDEV_STATE_RELEASED;
        data->point.x = 0;
        data->point.y = 0;
        return;
    }

    /* Determine if we should read touch data */
    bool should_read = true;
    if (touch_ctx->with_irq) {
        /* In IRQ mode, only read if semaphore is available (signaled by ISR) */
        should_read = (xSemaphoreTake(touch_ctx->touch_sem, 0) == pdTRUE);
    }

    if (should_read) {
        esp_lcd_touch_point_data_t touch_data[MAX_TOUCH_POINTS_V8] = {0};
        uint8_t count = 0;

        /* Read touch data from hardware */
        esp_lcd_touch_read_data(touch_ctx->handle);

        /* Get touch coordinates */
        esp_err_t ret = esp_lcd_touch_get_data(
                            touch_ctx->handle,
                            touch_data,
                            &count,
                            MAX_TOUCH_POINTS_V8
                        );

        /* Update last state */
        if (ret == ESP_OK && count > 0) {
            touch_ctx->last_point.x = (lv_coord_t)(touch_ctx->scale.x * touch_data[0].x);
            touch_ctx->last_point.y = (lv_coord_t)(touch_ctx->scale.y * touch_data[0].y);
            touch_ctx->last_state = LV_INDEV_STATE_PRESSED;
        } else {
            touch_ctx->last_state = LV_INDEV_STATE_RELEASED;
        }
    }

    /* Return last known state */
    data->point.x = touch_ctx->last_point.x;
    data->point.y = touch_ctx->last_point.y;
    data->state = touch_ctx->last_state;
}

static void IRAM_ATTR lvgl_touch_isr(esp_lcd_touch_handle_t tp)
{
    esp_lv_adapter_touch_isr_ctx_t *isr_ctx = (esp_lv_adapter_touch_isr_ctx_t *)tp->config.user_data;
    if (!isr_ctx || !isr_ctx->touch_ctx) {
        return;
    }

    if (isr_ctx->unregistering) {
        return;
    }

    esp_lv_adapter_touch_ctx_t *touch_ctx = (esp_lv_adapter_touch_ctx_t *)isr_ctx->touch_ctx;
    if (!touch_ctx->touch_sem) {
        return;
    }

    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    xSemaphoreGiveFromISR(touch_ctx->touch_sem, &xHigherPriorityTaskWoken);

    if (xHigherPriorityTaskWoken == pdTRUE) {
        portYIELD_FROM_ISR();
    }
}

#endif /* LVGL_VERSION_MAJOR >= 9 */
