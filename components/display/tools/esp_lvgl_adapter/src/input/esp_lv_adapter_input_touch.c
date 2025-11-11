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

/* Maximum touch points to read */
#define MAX_TOUCH_POINTS        1

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
} esp_lv_adapter_touch_ctx_t;

/* Forward declarations */
static void lvgl_touch_read(lv_indev_t *indev_drv, lv_indev_data_t *data);
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

    /* Check if interrupt mode is supported */
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
        esp_lcd_touch_point_data_t touch_data[MAX_TOUCH_POINTS] = {0};
        uint8_t count = 0;

        /* Read touch data from hardware */
        esp_lcd_touch_read_data(touch_ctx->handle);

        /* Get touch coordinates */
        esp_err_t ret = esp_lcd_touch_get_data(
                            touch_ctx->handle,
                            touch_data,
                            &count,
                            MAX_TOUCH_POINTS
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
        esp_lcd_touch_point_data_t touch_data[MAX_TOUCH_POINTS] = {0};
        uint8_t count = 0;

        /* Read touch data from hardware */
        esp_lcd_touch_read_data(touch_ctx->handle);

        /* Get touch coordinates */
        esp_err_t ret = esp_lcd_touch_get_data(
                            touch_ctx->handle,
                            touch_data,
                            &count,
                            MAX_TOUCH_POINTS
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
