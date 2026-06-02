/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include "cJSON.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "usb/uac_host.h"
#include "app_web.h"
#include "app_uac_manager.h"

#define APP_UAC_MAX_ALT_NUM              8
#define APP_UAC_EVENT_QUEUE_LEN          12
#define APP_UAC_MIC_FRAME_MS             50
#define APP_UAC_MIC_MAX_FRAME_BYTES      49152
#define APP_UAC_RX_BUFFER_SIZE           16*1024
#define APP_UAC_RX_THRESHOLD             APP_UAC_RX_BUFFER_SIZE/6
#define APP_UAC_TX_BUFFER_SIZE           96*1024
#define APP_UAC_TX_THRESHOLD             (APP_UAC_TX_BUFFER_SIZE/2)
#define APP_UAC_PCM_MAX_PAYLOAD          4096
#define APP_UAC_SPK_REFILL_BYTES         (APP_UAC_TX_BUFFER_SIZE - APP_UAC_TX_THRESHOLD)
#define APP_UAC_SPK_REFILL_MIN_SLOW_MS   80

typedef enum {
    APP_UAC_EVENT_DRIVER = 0,
    APP_UAC_EVENT_DEVICE,
} app_uac_event_group_t;

typedef struct {
    app_uac_event_group_t group;
    union {
        struct {
            uint8_t addr;
            uint8_t iface_num;
            uac_host_driver_event_t event;
        } driver;
        struct {
            uac_host_device_handle_t handle;
            uac_host_device_event_t event;
        } device;
    };
} app_uac_event_t;

typedef struct {
    bool connected;
    bool opened;
    bool started;
    bool enabled;
    bool mute;
    bool volume_supported;
    bool mute_supported;
    uint8_t addr;
    uint8_t iface_num;
    uint8_t selected_alt;
    uint8_t volume;
    uint32_t selected_sample_freq;
    uac_host_device_handle_t handle;
    uac_host_dev_info_t info;
    uint8_t alt_num;
    uac_host_dev_alt_param_t alt[APP_UAC_MAX_ALT_NUM];
} app_uac_stream_ctx_t;

static const char *TAG = "app_uac";
static QueueHandle_t s_event_queue;
static SemaphoreHandle_t s_lock;
static app_uac_stream_ctx_t s_mic;
static app_uac_stream_ctx_t s_spk;
static uint32_t s_spk_write_count;
static uint32_t s_spk_defer_count;
static uint32_t s_spk_refill_budget_outstanding;
static uint32_t s_spk_refill_budget_seq;
static uint32_t s_spk_refill_budget_packets;
static int64_t s_spk_refill_budget_grant_us;
static bool s_spk_waiting_first_packet;
static uint8_t *s_mic_rx_buf;
static size_t s_mic_rx_buf_size;
static uint8_t *s_mic_frame_buf;
static size_t s_mic_frame_buf_size;
static size_t s_mic_frame_len;

static esp_err_t mic_buffers_reserve(void);

static app_uac_stream_ctx_t *stream_ctx(app_uac_stream_id_t stream)
{
    return stream == APP_UAC_STREAM_MIC ? &s_mic : &s_spk;
}

static const char *stream_name(app_uac_stream_id_t stream)
{
    return stream == APP_UAC_STREAM_MIC ? "mic" : "spk";
}

static app_uac_stream_id_t stream_from_handle(uac_host_device_handle_t handle)
{
    if (s_mic.handle == handle) {
        return APP_UAC_STREAM_MIC;
    }
    return APP_UAC_STREAM_SPK;
}

static void wide_to_ascii(const wchar_t *src, char *dst, size_t dst_len)
{
    if (!dst || dst_len == 0) {
        return;
    }
    dst[0] = '\0';
    if (!src) {
        return;
    }
    for (size_t i = 0; i + 1 < dst_len && src[i] != 0; i++) {
        dst[i] = src[i] < 0x80 ? (char)src[i] : '?';
        dst[i + 1] = '\0';
    }
}

static uint32_t alt_default_freq(const uac_host_dev_alt_param_t *alt)
{
    if (alt->sample_freq_type > 0) {
        return alt->sample_freq[0];
    }
    if (alt->sample_freq_lower <= 16000 && alt->sample_freq_upper >= 16000) {
        return 16000;
    }
    return alt->sample_freq_lower;
}

static bool alt_supports_freq(const uac_host_dev_alt_param_t *alt, uint32_t sample_freq)
{
    if (alt->sample_freq_type == 0) {
        return sample_freq >= alt->sample_freq_lower && sample_freq <= alt->sample_freq_upper;
    }
    for (uint8_t i = 0; i < alt->sample_freq_type && i < UAC_FREQ_NUM_MAX; i++) {
        if (alt->sample_freq[i] == sample_freq) {
            return true;
        }
    }
    return false;
}

static size_t stream_frame_bytes_locked(const app_uac_stream_ctx_t *ctx, uint32_t frame_ms)
{
    if (!ctx->opened || ctx->selected_alt == 0 || ctx->selected_alt > ctx->alt_num || ctx->selected_sample_freq == 0) {
        return 0;
    }
    const uac_host_dev_alt_param_t *alt = &ctx->alt[ctx->selected_alt - 1];
    const size_t bytes_per_frame = alt->channels * alt->subframe_size;
    if (bytes_per_frame == 0) {
        return 0;
    }
    size_t bytes = (size_t)(((uint64_t)ctx->selected_sample_freq * bytes_per_frame * frame_ms) / 1000);
    bytes -= bytes % bytes_per_frame;
    return bytes;
}

static uint32_t stream_bytes_to_ms_locked(const app_uac_stream_ctx_t *ctx, size_t bytes)
{
    if (!ctx->opened || ctx->selected_alt == 0 || ctx->selected_alt > ctx->alt_num || ctx->selected_sample_freq == 0) {
        return 0;
    }
    const uac_host_dev_alt_param_t *alt = &ctx->alt[ctx->selected_alt - 1];
    const uint64_t bytes_per_second = (uint64_t)ctx->selected_sample_freq * alt->channels * alt->subframe_size;
    if (bytes_per_second == 0) {
        return 0;
    }
    // Round up so sub-millisecond packets still have a non-zero audio-time budget.
    return (uint32_t)(((uint64_t)bytes * 1000 + bytes_per_second - 1) / bytes_per_second);
}

static uint32_t spk_refill_slow_ms_locked(void)
{
    const uint32_t refill_budget_ms = stream_bytes_to_ms_locked(&s_spk, APP_UAC_SPK_REFILL_BYTES);
    const uint32_t slow_ms = refill_budget_ms / 2;
    return slow_ms > APP_UAC_SPK_REFILL_MIN_SLOW_MS ? slow_ms : APP_UAC_SPK_REFILL_MIN_SLOW_MS;
}

static uint32_t spk_refill_budget_slow_ms_locked(void)
{
    const uint32_t refill_budget_ms = stream_bytes_to_ms_locked(&s_spk, APP_UAC_SPK_REFILL_BYTES);
    return (refill_budget_ms * 3 + 1) / 2;
}

static esp_err_t stream_start_locked(app_uac_stream_id_t stream)
{
    app_uac_stream_ctx_t *ctx = stream_ctx(stream);
    if (!ctx->opened || ctx->selected_alt == 0 || ctx->selected_alt > ctx->alt_num) {
        ESP_LOGE(TAG, "%s start failed, device is not ready", stream_name(stream));
        return ESP_ERR_INVALID_STATE;
    }
    if (ctx->started) {
        if (stream == APP_UAC_STREAM_MIC) {
            esp_err_t reserve_ret = mic_buffers_reserve();
            if (reserve_ret != ESP_OK) {
                ESP_LOGE(TAG, "MIC resume failed, RX buffer is unavailable: %s", esp_err_to_name(reserve_ret));
                return reserve_ret;
            }
        }
        return uac_host_device_resume(ctx->handle);
    }

    if (stream == APP_UAC_STREAM_MIC) {
        esp_err_t reserve_ret = mic_buffers_reserve();
        if (reserve_ret != ESP_OK) {
            ESP_LOGE(TAG, "MIC start failed, RX buffer is unavailable: %s", esp_err_to_name(reserve_ret));
            return reserve_ret;
        }
    }

    const uac_host_dev_alt_param_t *alt = &ctx->alt[ctx->selected_alt - 1];
    const uac_host_stream_config_t stream_config = {
        .channels = alt->channels,
        .bit_resolution = alt->bit_resolution,
        .sample_freq = ctx->selected_sample_freq,
        .flags = stream == APP_UAC_STREAM_SPK ? FLAG_STREAM_SUSPEND_AFTER_START : 0,
    };
    esp_err_t ret = uac_host_device_start(ctx->handle, &stream_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "%s start failed: %s", stream_name(stream), esp_err_to_name(ret));
        return ret;
    }
    ctx->started = true;

    ret = uac_host_device_set_mute(ctx->handle, ctx->mute);
    ctx->mute_supported = ret != ESP_ERR_NOT_SUPPORTED;
    if (ret != ESP_OK && ret != ESP_ERR_NOT_SUPPORTED) {
        ESP_LOGW(TAG, "%s set mute failed: %s", stream_name(stream), esp_err_to_name(ret));
    }

    ret = uac_host_device_set_volume(ctx->handle, ctx->volume);
    ctx->volume_supported = ret != ESP_ERR_NOT_SUPPORTED;
    if (ret != ESP_OK && ret != ESP_ERR_NOT_SUPPORTED) {
        ESP_LOGW(TAG, "%s set volume failed: %s", stream_name(stream), esp_err_to_name(ret));
    }

    if (stream == APP_UAC_STREAM_SPK) {
        ret = uac_host_device_resume(ctx->handle);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "SPK resume failed: %s", esp_err_to_name(ret));
            return ret;
        }
    }
    return ESP_OK;
}

static void spk_refill_budget_reset_locked(void)
{
    s_spk_refill_budget_outstanding = 0;
    s_spk_refill_budget_packets = 0;
    s_spk_refill_budget_grant_us = 0;
    s_spk_waiting_first_packet = false;
}

static void stream_close_locked(app_uac_stream_ctx_t *ctx)
{
    if (ctx == &s_spk) {
        spk_refill_budget_reset_locked();
    }
    if (ctx->handle) {
        esp_err_t ret = uac_host_device_close(ctx->handle);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Close UAC device failed: %s", esp_err_to_name(ret));
        }
    }
    memset(ctx, 0, sizeof(*ctx));
    ctx->volume = 80;
    ctx->volume_supported = true;
    ctx->mute_supported = true;
}

static void mic_frame_reset(void)
{
    s_mic_frame_len = 0;
}

static void mic_buffers_free(void)
{
    free(s_mic_rx_buf);
    s_mic_rx_buf = NULL;
    s_mic_rx_buf_size = 0;

    free(s_mic_frame_buf);
    s_mic_frame_buf = NULL;
    s_mic_frame_buf_size = 0;
    s_mic_frame_len = 0;
}

static esp_err_t mic_frame_reserve(size_t target_len)
{
    if (target_len == 0 || target_len > APP_UAC_MIC_MAX_FRAME_BYTES || target_len > UINT16_MAX) {
        ESP_LOGW(TAG, "Invalid MIC frame length: %u", (unsigned)target_len);
        mic_frame_reset();
        return ESP_ERR_INVALID_SIZE;
    }
    if (s_mic_frame_buf_size >= target_len) {
        return ESP_OK;
    }

    uint8_t *buf = realloc(s_mic_frame_buf, target_len);
    if (!buf) {
        ESP_LOGE(TAG, "Allocate MIC frame buffer failed len=%u", (unsigned)target_len);
        mic_buffers_free();
        return ESP_ERR_NO_MEM;
    }
    s_mic_frame_buf = buf;
    s_mic_frame_buf_size = target_len;
    s_mic_frame_len = 0;
    return ESP_OK;
}

static esp_err_t mic_buffers_reserve(void)
{
    if (s_mic_rx_buf_size >= APP_UAC_RX_THRESHOLD) {
        return ESP_OK;
    }

    uint8_t *buf = realloc(s_mic_rx_buf, APP_UAC_RX_THRESHOLD);
    if (!buf) {
        ESP_LOGE(TAG, "Allocate MIC RX buffer failed len=%u", (unsigned)APP_UAC_RX_THRESHOLD);
        mic_buffers_free();
        return ESP_ERR_NO_MEM;
    }
    s_mic_rx_buf = buf;
    s_mic_rx_buf_size = APP_UAC_RX_THRESHOLD;
    return ESP_OK;
}

static void uac_device_callback(uac_host_device_handle_t handle, const uac_host_device_event_t event, void *arg)
{
    // Keep the UAC callback non-blocking; the manager task owns all heavy work.
    app_uac_event_t evt = {
        .group = APP_UAC_EVENT_DEVICE,
        .device.handle = handle,
        .device.event = event,
    };
    if (s_event_queue && xQueueSend(s_event_queue, &evt, 0) != pdTRUE) {
        ESP_LOGW(TAG, "UAC device event queue full event: %d", event);
    }
}

static void uac_driver_callback(uint8_t addr, uint8_t iface_num, const uac_host_driver_event_t event, void *arg)
{
    app_uac_event_t evt = {
        .group = APP_UAC_EVENT_DRIVER,
        .driver.addr = addr,
        .driver.iface_num = iface_num,
        .driver.event = event,
    };
    if (s_event_queue && xQueueSend(s_event_queue, &evt, 0) != pdTRUE) {
        ESP_LOGW(TAG, "UAC driver event queue full");
    }
}

static void open_stream(uint8_t addr, uint8_t iface_num, uac_host_driver_event_t event)
{
    // Open the reported RX/TX interface and cache its alternate setting list for the web UI.
    const app_uac_stream_id_t stream = event == UAC_HOST_DRIVER_EVENT_RX_CONNECTED ? APP_UAC_STREAM_MIC : APP_UAC_STREAM_SPK;
    app_uac_stream_ctx_t *ctx = stream_ctx(stream);
    const uac_host_device_config_t config = {
        .addr = addr,
        .iface_num = iface_num,
        .buffer_size = stream == APP_UAC_STREAM_MIC ? APP_UAC_RX_BUFFER_SIZE : APP_UAC_TX_BUFFER_SIZE,
        .buffer_threshold = stream == APP_UAC_STREAM_MIC ? APP_UAC_RX_THRESHOLD : APP_UAC_TX_THRESHOLD,
        .callback = uac_device_callback,
        .callback_arg = NULL,
    };
    uac_host_device_handle_t handle = NULL;
    esp_err_t ret = uac_host_device_open(&config, &handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "%s open failed: %s", stream_name(stream), esp_err_to_name(ret));
        app_web_broadcast_state();
        return;
    }

    xSemaphoreTake(s_lock, portMAX_DELAY);
    stream_close_locked(ctx);
    ctx->handle = handle;
    ctx->addr = addr;
    ctx->iface_num = iface_num;
    ctx->connected = true;
    ctx->opened = true;
    ctx->volume = 80;
    ctx->volume_supported = true;
    ctx->mute_supported = true;

    ret = uac_host_get_device_info(handle, &ctx->info);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "%s get device info failed: %s", stream_name(stream), esp_err_to_name(ret));
        stream_close_locked(ctx);
        xSemaphoreGive(s_lock);
        app_web_broadcast_state();
        return;
    }

    ctx->alt_num = ctx->info.iface_alt_num > APP_UAC_MAX_ALT_NUM ? APP_UAC_MAX_ALT_NUM : ctx->info.iface_alt_num;
    for (uint8_t i = 0; i < ctx->alt_num; i++) {
        ret = uac_host_get_device_alt_param(handle, i + 1, &ctx->alt[i]);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "%s get alt %u failed: %s", stream_name(stream), i + 1, esp_err_to_name(ret));
            ctx->alt_num = i;
            break;
        }
    }
    if (ctx->alt_num > 0) {
        ctx->selected_alt = 1;
        ctx->selected_sample_freq = alt_default_freq(&ctx->alt[0]);
    }
    ESP_LOGI(TAG, "%s connected addr=%u iface=%u alt_num=%u", stream_name(stream), addr, iface_num, ctx->alt_num);
    xSemaphoreGive(s_lock);
    app_web_broadcast_state();
}

static esp_err_t grant_spk_refill_budget_locked(uint32_t *bytes)
{
    if (!bytes) {
        return ESP_ERR_INVALID_ARG;
    }
    *bytes = 0;
    if (!s_spk.opened || !s_spk.enabled || !s_spk.started) {
        return ESP_ERR_INVALID_STATE;
    }
    if (s_spk_refill_budget_outstanding > 0) {
        return ESP_OK;
    }
    // TX_DONE means the UAC TX ring buffer is below threshold, so grant browser a byte budget to refill it.
    s_spk_refill_budget_outstanding = APP_UAC_SPK_REFILL_BYTES;
    s_spk_refill_budget_seq++;
    s_spk_refill_budget_packets = 0;
    s_spk_refill_budget_grant_us = esp_timer_get_time();
    s_spk_waiting_first_packet = true;
    *bytes = APP_UAC_SPK_REFILL_BYTES;
    ESP_LOGI(TAG, "SPK refill budget granted seq=%"PRIu32" bytes=%u outstanding=%"PRIu32, s_spk_refill_budget_seq, APP_UAC_SPK_REFILL_BYTES, s_spk_refill_budget_outstanding);
    return ESP_OK;
}

static esp_err_t send_spk_refill_budget_locked(void)
{
    uint32_t bytes = 0;
    esp_err_t ret = grant_spk_refill_budget_locked(&bytes);
    if (ret != ESP_OK || bytes == 0) {
        return ret;
    }
    ret = app_web_send_spk_refill_budget_bytes(bytes);
    if (ret != ESP_OK) {
        // The browser did not receive this refill budget window, so release it and let the next TX_DONE retry.
        spk_refill_budget_reset_locked();
        ESP_LOGW(TAG, "Queue SPK refill budget failed, refill budget reset: %s", esp_err_to_name(ret));
    }
    return ret;
}

static void send_mic_frames(const uint8_t *data, size_t len)
{
    if (!data || len == 0) {
        return;
    }

    xSemaphoreTake(s_lock, portMAX_DELAY);
    size_t target_len = stream_frame_bytes_locked(&s_mic, APP_UAC_MIC_FRAME_MS);
    esp_err_t reserve_ret = mic_frame_reserve(target_len);
    xSemaphoreGive(s_lock);
    if (reserve_ret != ESP_OK) {
        return;
    }

    size_t off = 0;
    while (off < len) {
        const size_t free_len = target_len - s_mic_frame_len;
        const size_t copy_len = (len - off) < free_len ? (len - off) : free_len;
        memcpy(s_mic_frame_buf + s_mic_frame_len, data + off, copy_len);
        s_mic_frame_len += copy_len;
        off += copy_len;
        if (s_mic_frame_len == target_len) {
            esp_err_t ret = app_web_send_mic_pcm(s_mic_frame_buf, s_mic_frame_len);
            if (ret != ESP_OK) {
                ESP_LOGW(TAG, "MIC PCM send failed: %s", esp_err_to_name(ret));
            }
            mic_frame_reset();
        }
    }
}

static void handle_device_event(uac_host_device_handle_t handle, uac_host_device_event_t event)
{
    if (event == UAC_HOST_DRIVER_EVENT_DISCONNECTED) {
        xSemaphoreTake(s_lock, portMAX_DELAY);
        app_uac_stream_id_t stream = stream_from_handle(handle);
        stream_close_locked(stream_ctx(stream));
        if (stream == APP_UAC_STREAM_MIC) {
            mic_buffers_free();
        }
        xSemaphoreGive(s_lock);
        ESP_LOGI(TAG, "UAC device disconnected");
        app_web_broadcast_state();
        return;
    }

    app_uac_stream_id_t stream = stream_from_handle(handle);
    app_uac_stream_ctx_t *ctx = stream_ctx(stream);
    if (event == UAC_HOST_DEVICE_EVENT_TRANSFER_ERROR) {
        ESP_LOGE(TAG, "%s transfer error", stream_name(stream));
        app_web_send_error("uac_transfer_error", "UAC transfer error");
        return;
    }

    if (stream == APP_UAC_STREAM_MIC && event == UAC_HOST_DEVICE_EVENT_RX_DONE) {
        // Read all available microphone data and drop it when no browser is listening.
        uint32_t rx_size = 0;
        uint8_t *rx_buf = NULL;
        size_t rx_buf_size = 0;
        bool can_read = false;
        bool enabled = false;
        bool handle_match = false;
        bool has_rx_buf = false;
        bool has_client = app_web_has_client();
        xSemaphoreTake(s_lock, portMAX_DELAY);
        enabled = ctx->enabled;
        handle_match = ctx->handle == handle;
        has_rx_buf = s_mic_rx_buf && s_mic_rx_buf_size > 0;
        can_read = enabled && handle_match && has_rx_buf && has_client;
        if (can_read) {
            rx_buf = s_mic_rx_buf;
            rx_buf_size = s_mic_rx_buf_size;
        }
        xSemaphoreGive(s_lock);
        if (!can_read) {
            ESP_LOGD(TAG, "Skip MIC RX read: enabled=%d handle_match=%d rx_buf=%d client=%d", enabled, handle_match, has_rx_buf, has_client);
            return;
        }
        while (uac_host_device_read(handle, rx_buf, rx_buf_size, &rx_size, 0) == ESP_OK && rx_size > 0) {
            send_mic_frames(rx_buf, rx_size);
        }
        return;
    }

    if (stream == APP_UAC_STREAM_SPK && event == UAC_HOST_DEVICE_EVENT_TX_DONE) {
        xSemaphoreTake(s_lock, portMAX_DELAY);
        esp_err_t ret = (ctx->handle == handle) ? send_spk_refill_budget_locked() : ESP_ERR_INVALID_STATE;
        xSemaphoreGive(s_lock);
        if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
            ESP_LOGW(TAG, "Send SPK refill budget failed: %s", esp_err_to_name(ret));
        }
    }
}

static void uac_manager_task(void *arg)
{
    app_uac_event_t evt = {0};
    while (1) {
        if (xQueueReceive(s_event_queue, &evt, portMAX_DELAY) != pdTRUE) {
            continue;
        }
        if (evt.group == APP_UAC_EVENT_DRIVER) {
            open_stream(evt.driver.addr, evt.driver.iface_num, evt.driver.event);
        } else if (evt.group == APP_UAC_EVENT_DEVICE) {
            handle_device_event(evt.device.handle, evt.device.event);
        }
    }
}

esp_err_t app_uac_manager_start(void)
{
    s_event_queue = xQueueCreate(APP_UAC_EVENT_QUEUE_LEN, sizeof(app_uac_event_t));
    s_lock = xSemaphoreCreateMutex();
    if (!s_event_queue || !s_lock) {
        ESP_LOGE(TAG, "Create UAC event queue failed");
        return ESP_ERR_NO_MEM;
    }
    s_mic.volume = 80;
    s_spk.volume = 80;
    s_mic.volume_supported = true;
    s_spk.volume_supported = true;
    s_mic.mute_supported = true;
    s_spk.mute_supported = true;

    const uac_host_driver_config_t config = {
        .create_background_task = true,
        .task_priority = configMAX_PRIORITIES - 1,
        .stack_size = 4096,
        .core_id = 0,
        .callback = uac_driver_callback,
        .callback_arg = NULL,
    };
    esp_err_t ret = uac_host_install(&config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "UAC host install failed: %s", esp_err_to_name(ret));
        return ret;
    }
    if (xTaskCreatePinnedToCore(uac_manager_task, "uac_mgr", 8192, NULL, 5, NULL, 0) != pdPASS) {
        ESP_LOGE(TAG, "Create UAC manager task failed");
        return ESP_ERR_NO_MEM;
    }
    ESP_LOGI(TAG, "UAC host installed");
    return ESP_OK;
}

static cJSON *create_stream_json(const app_uac_stream_ctx_t *ctx)
{
    char manufacturer[UAC_STR_DESC_MAX_LENGTH + 1];
    char product[UAC_STR_DESC_MAX_LENGTH + 1];
    char serial[UAC_STR_DESC_MAX_LENGTH + 1];
    wide_to_ascii(ctx->info.iManufacturer, manufacturer, sizeof(manufacturer));
    wide_to_ascii(ctx->info.iProduct, product, sizeof(product));
    wide_to_ascii(ctx->info.iSerialNumber, serial, sizeof(serial));

    cJSON *stream = cJSON_CreateObject();
    if (!stream) {
        return NULL;
    }
    cJSON_AddBoolToObject(stream, "connected", ctx->connected);
    cJSON_AddBoolToObject(stream, "enabled", ctx->enabled);
    cJSON_AddBoolToObject(stream, "started", ctx->started);
    cJSON_AddBoolToObject(stream, "mute", ctx->mute);
    cJSON_AddNumberToObject(stream, "volume", ctx->volume);
    cJSON_AddBoolToObject(stream, "volume_supported", ctx->volume_supported);
    cJSON_AddBoolToObject(stream, "mute_supported", ctx->mute_supported);
    cJSON_AddNumberToObject(stream, "addr", ctx->addr);
    cJSON_AddNumberToObject(stream, "iface_num", ctx->iface_num);
    cJSON_AddNumberToObject(stream, "vid", ctx->info.VID);
    cJSON_AddNumberToObject(stream, "pid", ctx->info.PID);
    cJSON_AddStringToObject(stream, "manufacturer", manufacturer);
    cJSON_AddStringToObject(stream, "product", product);
    cJSON_AddStringToObject(stream, "serial", serial);
    cJSON_AddNumberToObject(stream, "selected_alt", ctx->selected_alt);
    cJSON_AddNumberToObject(stream, "sample_freq", ctx->selected_sample_freq);

    cJSON *alts = cJSON_AddArrayToObject(stream, "alts");
    if (!alts) {
        cJSON_Delete(stream);
        return NULL;
    }
    for (uint8_t i = 0; i < ctx->alt_num; i++) {
        const uac_host_dev_alt_param_t *alt = &ctx->alt[i];
        cJSON *item = cJSON_CreateObject();
        if (!item) {
            cJSON_Delete(stream);
            return NULL;
        }
        cJSON_AddNumberToObject(item, "alt", i + 1);
        cJSON_AddNumberToObject(item, "channels", alt->channels);
        cJSON_AddNumberToObject(item, "bit_resolution", alt->bit_resolution);
        cJSON_AddNumberToObject(item, "subframe_size", alt->subframe_size);
        cJSON_AddNumberToObject(item, "sample_freq_type", alt->sample_freq_type);
        cJSON *freqs = cJSON_AddArrayToObject(item, "freqs");
        if (!freqs) {
            cJSON_Delete(item);
            cJSON_Delete(stream);
            return NULL;
        }
        if (alt->sample_freq_type == 0) {
            cJSON_AddItemToArray(freqs, cJSON_CreateNumber(alt->sample_freq_lower));
            cJSON_AddItemToArray(freqs, cJSON_CreateNumber(alt->sample_freq_upper));
        } else {
            for (uint8_t j = 0; j < alt->sample_freq_type && j < UAC_FREQ_NUM_MAX; j++) {
                cJSON_AddItemToArray(freqs, cJSON_CreateNumber(alt->sample_freq[j]));
            }
        }
        cJSON_AddItemToArray(alts, item);
    }
    return stream;
}

esp_err_t app_uac_get_state_json(char *buf, size_t len)
{
    if (!buf || len == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    cJSON *root = cJSON_CreateObject();
    if (!root) {
        ESP_LOGE(TAG, "Create state JSON failed");
        return ESP_ERR_NO_MEM;
    }
    cJSON_AddStringToObject(root, "type", "state");

    xSemaphoreTake(s_lock, portMAX_DELAY);
    cJSON *mic = create_stream_json(&s_mic);
    cJSON *spk = create_stream_json(&s_spk);
    xSemaphoreGive(s_lock);
    if (!mic || !spk) {
        ESP_LOGE(TAG, "Create stream state JSON failed");
        cJSON_Delete(mic);
        cJSON_Delete(spk);
        cJSON_Delete(root);
        return ESP_ERR_NO_MEM;
    }
    cJSON_AddItemToObject(root, "mic", mic);
    cJSON_AddItemToObject(root, "spk", spk);

    if (!cJSON_PrintPreallocated(root, buf, len, false)) {
        ESP_LOGE(TAG, "Print state JSON failed");
        cJSON_Delete(root);
        return ESP_ERR_NO_MEM;
    }
    cJSON_Delete(root);
    return ESP_OK;
}

esp_err_t app_uac_set_enabled(app_uac_stream_id_t stream, bool enabled)
{
    xSemaphoreTake(s_lock, portMAX_DELAY);
    app_uac_stream_ctx_t *ctx = stream_ctx(stream);
    esp_err_t ret = ESP_OK;
    if (!ctx->opened) {
        ret = ESP_ERR_INVALID_STATE;
    } else if (enabled) {
        ctx->enabled = true;
        ret = stream_start_locked(stream);
    } else {
        ctx->enabled = false;
        if (stream == APP_UAC_STREAM_SPK) {
            spk_refill_budget_reset_locked();
        }
        if (ctx->started) {
            ret = uac_host_device_suspend(ctx->handle);
            if (ret != ESP_OK) {
                ESP_LOGE(TAG, "%s suspend failed: %s", stream_name(stream), esp_err_to_name(ret));
            }
        }
        if (stream == APP_UAC_STREAM_MIC) {
            mic_frame_reset();
        }
    }
    xSemaphoreGive(s_lock);
    app_web_broadcast_state();
    return ret;
}

esp_err_t app_uac_set_mute(app_uac_stream_id_t stream, bool mute)
{
    xSemaphoreTake(s_lock, portMAX_DELAY);
    app_uac_stream_ctx_t *ctx = stream_ctx(stream);
    esp_err_t ret = ESP_ERR_INVALID_STATE;
    if (ctx->opened && ctx->started) {
        ret = uac_host_device_set_mute(ctx->handle, mute);
        if (ret == ESP_OK) {
            ctx->mute = mute;
        } else if (ret == ESP_ERR_NOT_SUPPORTED) {
            ctx->mute_supported = false;
            ESP_LOGW(TAG, "%s mute is not supported", stream_name(stream));
        } else {
            ESP_LOGE(TAG, "%s set mute failed: %s", stream_name(stream), esp_err_to_name(ret));
        }
    }
    xSemaphoreGive(s_lock);
    app_web_broadcast_state();
    return ret;
}

esp_err_t app_uac_set_volume(app_uac_stream_id_t stream, uint8_t volume)
{
    if (volume > 100) {
        return ESP_ERR_INVALID_ARG;
    }
    xSemaphoreTake(s_lock, portMAX_DELAY);
    app_uac_stream_ctx_t *ctx = stream_ctx(stream);
    esp_err_t ret = ESP_ERR_INVALID_STATE;
    if (ctx->opened && ctx->started) {
        ret = uac_host_device_set_volume(ctx->handle, volume);
        if (ret == ESP_OK) {
            uint8_t actual = volume;
            if (uac_host_device_get_volume(ctx->handle, &actual) == ESP_OK) {
                ctx->volume = actual;
            } else {
                ctx->volume = volume;
            }
        } else if (ret == ESP_ERR_NOT_SUPPORTED) {
            ctx->volume_supported = false;
            ESP_LOGW(TAG, "%s volume is not supported", stream_name(stream));
        } else {
            ESP_LOGE(TAG, "%s set volume failed: %s", stream_name(stream), esp_err_to_name(ret));
        }
    } else if (ctx->opened) {
        ctx->volume = volume;
        ret = ESP_OK;
    }
    xSemaphoreGive(s_lock);
    app_web_broadcast_state();
    return ret;
}

esp_err_t app_uac_set_format(app_uac_stream_id_t stream, uint8_t alt, uint32_t sample_freq)
{
    xSemaphoreTake(s_lock, portMAX_DELAY);
    app_uac_stream_ctx_t *ctx = stream_ctx(stream);
    esp_err_t ret = ESP_OK;
    if (!ctx->opened || alt == 0 || alt > ctx->alt_num || !alt_supports_freq(&ctx->alt[alt - 1], sample_freq)) {
        ret = ESP_ERR_INVALID_ARG;
    } else {
        const bool was_enabled = ctx->enabled;
        if (ctx->started) {
            ret = uac_host_device_stop(ctx->handle);
            if (ret != ESP_OK) {
                ESP_LOGE(TAG, "%s stop for format change failed: %s", stream_name(stream), esp_err_to_name(ret));
            }
            ctx->started = false;
        }
        ctx->selected_alt = alt;
        ctx->selected_sample_freq = sample_freq;
        if (stream == APP_UAC_STREAM_MIC) {
            mic_buffers_free();
        } else {
            spk_refill_budget_reset_locked();
        }
        ctx->enabled = was_enabled;
        if (was_enabled) {
            ret = stream_start_locked(stream);
        }
    }
    xSemaphoreGive(s_lock);
    app_web_broadcast_state();
    return ret;
}

esp_err_t app_uac_queue_spk_pcm(const uint8_t *data, size_t len)
{
    if (!data || len == 0 || len > APP_UAC_PCM_MAX_PAYLOAD) {
        return ESP_ERR_INVALID_ARG;
    }
    xSemaphoreTake(s_lock, portMAX_DELAY);
    if (!s_spk.opened || !s_spk.enabled || !s_spk.started || !s_spk.handle) {
        xSemaphoreGive(s_lock);
        ESP_LOGW(TAG, "Drop SPK PCM because speaker is not ready");
        return ESP_ERR_INVALID_STATE;
    }

    // uac_host_device_write copies PCM into the UAC TX ring buffer before returning.
    esp_err_t ret = uac_host_device_write(s_spk.handle, (uint8_t *)data, len, pdMS_TO_TICKS(50));
    if (ret == ESP_OK) {
        const int64_t now_us = esp_timer_get_time();
        s_spk_refill_budget_packets++;
        if (s_spk_waiting_first_packet) {
            const int64_t delay_ms = s_spk_refill_budget_grant_us > 0 ? (now_us - s_spk_refill_budget_grant_us) / 1000 : 0;
            const uint32_t slow_ms = spk_refill_slow_ms_locked();
            if (slow_ms > 0 && delay_ms > slow_ms) {
                ESP_LOGW(TAG, "SPK refill slow seq=%"PRIu32" first_packet_delay=%lldms threshold=%"PRIu32"ms refill budget=%d written=%"PRIu32,
                         s_spk_refill_budget_seq, (long long)delay_ms, slow_ms, APP_UAC_SPK_REFILL_BYTES, s_spk_write_count);
            }
            s_spk_waiting_first_packet = false;
        }
        s_spk_refill_budget_outstanding = s_spk_refill_budget_outstanding > len ? s_spk_refill_budget_outstanding - len : 0;
        if (s_spk_refill_budget_outstanding < APP_UAC_PCM_MAX_PAYLOAD) {
            // Ignore unusable tail refill budget so the next TX_DONE can grant a fresh browser window.
            s_spk_refill_budget_outstanding = 0;
        }
        if (s_spk_refill_budget_outstanding == 0 && s_spk_refill_budget_grant_us > 0) {
            const int64_t duration_ms = (now_us - s_spk_refill_budget_grant_us) / 1000;
            const uint32_t slow_ms = spk_refill_budget_slow_ms_locked();
            if (slow_ms > 0 && duration_ms > slow_ms) {
                ESP_LOGW(TAG, "SPK refill budget slow consume seq=%"PRIu32" duration=%lldms threshold=%"PRIu32"ms packets=%"PRIu32" refill budget=%d",
                         s_spk_refill_budget_seq, (long long)duration_ms, slow_ms, s_spk_refill_budget_packets, APP_UAC_SPK_REFILL_BYTES);
            }
            s_spk_refill_budget_packets = 0;
            s_spk_refill_budget_grant_us = 0;
            s_spk_waiting_first_packet = false;
        }
    } else {
        ESP_LOGW(TAG, "SPK write failed len=%u seq=%"PRIu32" outstanding=%"PRIu32" packets=%"PRIu32": %s",
                 (unsigned)len, s_spk_refill_budget_seq, s_spk_refill_budget_outstanding, s_spk_refill_budget_packets, esp_err_to_name(ret));
        spk_refill_budget_reset_locked();
    }
    xSemaphoreGive(s_lock);
    if (ret != ESP_OK) {
        s_spk_defer_count++;
        if (s_spk_defer_count == 1 || (s_spk_defer_count % 32) == 0) {
            ESP_LOGW(TAG, "SPK direct write deferred count=%"PRIu32": %s", s_spk_defer_count, esp_err_to_name(ret));
        }
    }
    s_spk_write_count++;
    if (s_spk_write_count == 1 || (s_spk_write_count % 32) == 0) {
        ESP_LOGI(TAG, "SPK PCM written count=%"PRIu32" len=%u", s_spk_write_count, (unsigned)len);
    }
    return ESP_OK;
}

esp_err_t app_uac_request_spk_refill_budget(uint32_t *bytes)
{
    if (!bytes) {
        return ESP_ERR_INVALID_ARG;
    }
    xSemaphoreTake(s_lock, portMAX_DELAY);
    if (!s_spk.opened || !s_spk.enabled || !s_spk.started || !s_spk.handle) {
        xSemaphoreGive(s_lock);
        ESP_LOGW(TAG, "Request SPK refill budget failed, speaker is not ready");
        return ESP_ERR_INVALID_STATE;
    }
    esp_err_t ret = uac_host_device_resume(s_spk.handle);
    if (ret != ESP_OK) {
        xSemaphoreGive(s_lock);
        ESP_LOGE(TAG, "Resume SPK for refill budget failed: %s", esp_err_to_name(ret));
        return ret;
    }
    // Playback requests are the start of a new browser-side refill budget window.
    spk_refill_budget_reset_locked();
    ret = grant_spk_refill_budget_locked(bytes);
    xSemaphoreGive(s_lock);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Request SPK refill budget failed: %s", esp_err_to_name(ret));
    } else {
        ESP_LOGI(TAG, "Request SPK refill budget result bytes=%"PRIu32, *bytes);
    }
    return ret;
}

void app_uac_reset_spk_refill_budget(void)
{
    xSemaphoreTake(s_lock, portMAX_DELAY);
    spk_refill_budget_reset_locked();
    xSemaphoreGive(s_lock);
}

esp_err_t app_uac_clear_spk_queue(void)
{
    xSemaphoreTake(s_lock, portMAX_DELAY);
    if (!s_spk.opened || !s_spk.started || !s_spk.handle) {
        xSemaphoreGive(s_lock);
        return ESP_ERR_INVALID_STATE;
    }
    // Stop playback by suspending the OUT stream; the next refill request resumes it.
    int64_t start_us = esp_timer_get_time();
    spk_refill_budget_reset_locked();
    esp_err_t ret = uac_host_device_suspend(s_spk.handle);
    xSemaphoreGive(s_lock);
    int64_t duration_ms = (esp_timer_get_time() - start_us) / 1000;
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Flush SPK TX ring buffer failed after %lldms: %s", (long long)duration_ms, esp_err_to_name(ret));
        return ret;
    }
    ESP_LOGI(TAG, "SPK TX ring buffer flushed and suspended in %lldms", (long long)duration_ms);
    return ESP_OK;
}
