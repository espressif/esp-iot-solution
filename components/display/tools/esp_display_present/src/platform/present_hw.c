/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "esp_display_present_hw.h"

#include <string.h>

#include "sdkconfig.h"
#include "esp_check.h"
#include "esp_log.h"
#include "soc/soc_caps.h"

#if CONFIG_SOC_DMA2D_SUPPORTED
#if defined(ESP_ASYNC_COLOR_CONVERT_AVAILABLE)
#include "esp_async_color_convert.h"
#elif defined(ESP_ASYNC_FBCPY_AVAILABLE)
#include "esp_async_fbcpy.h"
#endif
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#endif

#if CONFIG_SOC_DMA2D_SUPPORTED
static const char *TAG = "display_present:hw";

static esp_display_present_hw_resource_t s_hw_resource = {0};
static bool s_hw_resource_initialized = false;

#if CONFIG_SOC_DMA2D_SUPPORTED
static inline esp_err_t present_dma2d_install(void **out_handle)
{
#if defined(ESP_ASYNC_COLOR_CONVERT_AVAILABLE)
    async_color_convert_config_t cfg = {
        .dma_burst_size = 128,
    };
    return esp_async_color_convert_install_dma2d(&cfg, (async_color_convert_handle_t *)out_handle);
#elif defined(ESP_ASYNC_FBCPY_AVAILABLE)
    esp_async_fbcpy_config_t cfg = {};
    return esp_async_fbcpy_install(&cfg, (esp_async_fbcpy_handle_t *)out_handle);
#else
    return ESP_ERR_NOT_SUPPORTED;
#endif
}

static inline void present_dma2d_uninstall(void *handle)
{
#if defined(ESP_ASYNC_COLOR_CONVERT_AVAILABLE)
    esp_async_color_convert_uninstall((async_color_convert_handle_t)handle);
#elif defined(ESP_ASYNC_FBCPY_AVAILABLE)
    esp_async_fbcpy_uninstall((esp_async_fbcpy_handle_t)handle);
#endif
}
#endif /* CONFIG_SOC_DMA2D_SUPPORTED */

esp_display_present_hw_resource_t *esp_display_present_hw_resource_acquire(int max_pending_trans_num)
{
    if (!s_hw_resource_initialized) {
        ESP_LOGI(TAG, "Initializing hardware resources");

#if CONFIG_SOC_DMA2D_SUPPORTED
        esp_err_t ret = present_dma2d_install(&s_hw_resource.fbcpy_handle);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to install DMA2D (ret=%d)", ret);
            return NULL;
        }

        s_hw_resource.dma2d_mutex = xSemaphoreCreateMutex();
        s_hw_resource.dma2d_done_sem = xSemaphoreCreateBinary();

        if (!s_hw_resource.dma2d_mutex || !s_hw_resource.dma2d_done_sem) {
            ESP_LOGE(TAG, "Failed to create DMA2D semaphores");
            if (s_hw_resource.dma2d_mutex) {
                vSemaphoreDelete((SemaphoreHandle_t)s_hw_resource.dma2d_mutex);
            }
            if (s_hw_resource.dma2d_done_sem) {
                vSemaphoreDelete((SemaphoreHandle_t)s_hw_resource.dma2d_done_sem);
            }
            present_dma2d_uninstall(s_hw_resource.fbcpy_handle);
            memset(&s_hw_resource, 0, sizeof(s_hw_resource));
            return NULL;
        }
#endif

        (void)max_pending_trans_num;

        s_hw_resource_initialized = true;
        ESP_LOGI(TAG, "Hardware resources initialized successfully");
    }

    return &s_hw_resource;
}

esp_display_present_hw_resource_t *esp_display_present_hw_resource_peek(void)
{
    return s_hw_resource_initialized ? &s_hw_resource : NULL;
}

esp_err_t esp_display_present_hw_resource_release(void)
{
    if (!s_hw_resource_initialized) {
        return ESP_OK;
    }

    SemaphoreHandle_t mutex =
        (SemaphoreHandle_t)s_hw_resource.dma2d_mutex;
    if (mutex == NULL || xSemaphoreTake(mutex, portMAX_DELAY) != pdTRUE) {
        return ESP_ERR_INVALID_STATE;
    }
    present_dma2d_uninstall(s_hw_resource.fbcpy_handle);
    if (s_hw_resource.dma2d_done_sem != NULL) {
        vSemaphoreDelete(
            (SemaphoreHandle_t)s_hw_resource.dma2d_done_sem);
    }
    s_hw_resource.dma2d_done_sem = NULL;
    s_hw_resource.fbcpy_handle = NULL;
    s_hw_resource_initialized = false;
    (void)xSemaphoreGive(mutex);
    vSemaphoreDelete(mutex);
    s_hw_resource.dma2d_mutex = NULL;
    return ESP_OK;
}

#if CONFIG_SOC_DMA2D_SUPPORTED
#if defined(ESP_ASYNC_COLOR_CONVERT_AVAILABLE)
static bool IRAM_ATTR present_dma2d_done_callback(async_color_convert_handle_t mcp,
                                                  async_color_convert_event_data_t *event_data,
                                                  void *cb_args)
{
    BaseType_t high_task_woken = pdFALSE;
    (void)mcp;
    (void)event_data;
    (void)cb_args;

    xSemaphoreGiveFromISR((SemaphoreHandle_t)s_hw_resource.dma2d_done_sem, &high_task_woken);

    return (high_task_woken == pdTRUE);
}
#elif defined(ESP_ASYNC_FBCPY_AVAILABLE)
static bool IRAM_ATTR present_dma2d_done_callback(esp_async_fbcpy_handle_t mcp,
                                                  esp_async_fbcpy_event_data_t *event_data,
                                                  void *cb_args)
{
    BaseType_t high_task_woken = pdFALSE;
    (void)mcp;
    (void)event_data;
    (void)cb_args;

    xSemaphoreGiveFromISR((SemaphoreHandle_t)s_hw_resource.dma2d_done_sem, &high_task_woken);

    return (high_task_woken == pdTRUE);
}
#endif
#endif /* CONFIG_SOC_DMA2D_SUPPORTED */

esp_err_t esp_display_present_dma2d_copy_sync(void *trans_desc, uint32_t timeout_ms)
{
#if CONFIG_SOC_DMA2D_SUPPORTED
    esp_err_t ret = ESP_OK;
    esp_display_present_hw_resource_t *hw = esp_display_present_hw_resource_peek();
    ESP_RETURN_ON_FALSE(hw, ESP_ERR_INVALID_STATE, TAG, "DMA2D resource not initialized");

    ESP_GOTO_ON_FALSE(xSemaphoreTake((SemaphoreHandle_t)hw->dma2d_mutex,
                                     pdMS_TO_TICKS(timeout_ms)) == pdTRUE,
                      ESP_ERR_TIMEOUT, out, TAG, "Acquire DMA2D mutex timeout");

    xSemaphoreTake((SemaphoreHandle_t)hw->dma2d_done_sem, 0);

#if defined(ESP_ASYNC_COLOR_CONVERT_AVAILABLE)
    ret = esp_async_color_convert(hw->fbcpy_handle,
                                  (const async_color_convert_request_t *)trans_desc,
                                  present_dma2d_done_callback,
                                  NULL);
#elif defined(ESP_ASYNC_FBCPY_AVAILABLE)
    ret = esp_async_fbcpy(hw->fbcpy_handle,
                          trans_desc,
                          present_dma2d_done_callback,
                          NULL);
#else
    ret = ESP_ERR_NOT_SUPPORTED;
#endif
    ESP_GOTO_ON_ERROR(ret, release_mutex, TAG, "DMA2D transfer start failed (%d)", ret);

    ESP_GOTO_ON_FALSE(xSemaphoreTake((SemaphoreHandle_t)hw->dma2d_done_sem,
                                     pdMS_TO_TICKS(timeout_ms)) == pdTRUE,
                      ESP_ERR_TIMEOUT, release_mutex, TAG, "DMA2D transfer timeout");

release_mutex:
    xSemaphoreGive((SemaphoreHandle_t)hw->dma2d_mutex);

out:
    return ret;
#else
    (void)trans_desc;
    (void)timeout_ms;
    return ESP_ERR_NOT_SUPPORTED;
#endif
}

#else

esp_display_present_hw_resource_t *esp_display_present_hw_resource_acquire(int max_pending_trans_num)
{
    (void)max_pending_trans_num;
    return NULL;
}

esp_display_present_hw_resource_t *esp_display_present_hw_resource_peek(void)
{
    return NULL;
}

esp_err_t esp_display_present_hw_resource_release(void)
{
    return ESP_OK;
}

esp_err_t esp_display_present_dma2d_copy_sync(void *trans_desc, uint32_t timeout_ms)
{
    (void)trans_desc;
    (void)timeout_ms;
    return ESP_ERR_NOT_SUPPORTED;
}

#endif /* CONFIG_SOC_DMA2D_SUPPORTED */
