/*
 * SPDX-FileCopyrightText: 2024-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <inttypes.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_check.h"
#include "esp_spiffs.h"
#include "usb/usb_host.h"
#include "usb/uac_host.h"
#include "esp_audio_simple_player.h"
#include "esp_gmf_bit_cvt.h"
#include "esp_gmf_ch_cvt.h"
#include "esp_gmf_element.h"
#include "esp_gmf_pipeline.h"
#include "esp_gmf_port.h"
#include "esp_gmf_pool.h"
#include "esp_gmf_rate_cvt.h"

static const char *TAG = "usb_audio_player";

#define USB_HOST_TASK_PRIORITY  5
#define UAC_TASK_PRIORITY       5
#define USER_TASK_PRIORITY      2
#define SPIFFS_BASE             "/spiffs"
#define MP3_FILE_NAME           "/new_epic.mp3"
#define MP3_FILE_URI            "file://" SPIFFS_BASE MP3_FILE_NAME
#define BIT1_SPK_START          (0x01 << 0)
#define DEFAULT_VOLUME          45
#define DEFAULT_UAC_FREQ        48000
#define DEFAULT_UAC_BITS        16
#define DEFAULT_UAC_CH          2
#define UAC_PLAYER_MIN(a, b)    ((a) < (b) ? (a) : (b))

typedef struct {
    uint32_t sample_rate;
    uint8_t bits;
    uint8_t channels;
} uac_audio_format_t;

esp_gmf_err_t esp_audio_simple_player_get_pool(esp_asp_handle_t handle, esp_gmf_pool_handle_t *pool);
esp_gmf_err_t esp_audio_simple_player_set_pipeline(esp_asp_handle_t handle, const char *in_name, const char *el_name[], int num_of_el_name, const char *out_name);
esp_gmf_err_t esp_audio_simple_player_get_pipeline(esp_asp_handle_t handle, esp_gmf_pipeline_handle_t *pipe);

static QueueHandle_t s_event_queue = NULL;
static SemaphoreHandle_t s_uac_lock = NULL;
static uac_host_device_handle_t s_spk_dev_handle = NULL;
static uac_audio_format_t s_uac_target_format = {
    .sample_rate = DEFAULT_UAC_FREQ,
    .bits = DEFAULT_UAC_BITS,
    .channels = DEFAULT_UAC_CH,
};
static esp_asp_handle_t s_player = NULL;
static void uac_device_callback(uac_host_device_handle_t uac_device_handle, const uac_host_device_event_t event, void *arg);
/**
 * @brief event group
 *
 * APP_EVENT            - General control event
 * UAC_DRIVER_EVENT     - UAC Host Driver event, such as device connection
 * UAC_DEVICE_EVENT     - UAC Host Device event, such as rx/tx completion, device disconnection
 */
typedef enum {
    APP_EVENT = 0,
    UAC_DRIVER_EVENT,
    UAC_DEVICE_EVENT,
} event_group_t;

typedef enum {
    APP_EVENT_PLAY = 0,
    APP_EVENT_SHUTDOWN,
} app_event_t;

/**
 * @brief event queue
 *
 * This event is used for delivering the UAC Host event from callback to the uac_lib_task
 */
typedef struct {
    event_group_t event_group;
    union {
        struct {
            uint8_t addr;
            uint8_t iface_num;
            uac_host_driver_event_t event;
            void *arg;
        } driver_evt;
        struct {
            uac_host_device_handle_t handle;
            uac_host_device_event_t event;
            void *arg;
        } device_evt;
        struct {
            app_event_t event;
        } app_evt;
    };
} s_event_queue_t;

static void queue_play_request(void)
{
    if (s_event_queue == NULL) {
        ESP_LOGE(TAG, "Failed to queue play request: event queue is not ready");
        return;
    }
    s_event_queue_t evt_queue = {
        .event_group = APP_EVENT,
        .app_evt.event = APP_EVENT_PLAY,
    };
    if (xQueueSend(s_event_queue, &evt_queue, 0) != pdTRUE) {
        ESP_LOGE(TAG, "Failed to queue play request");
    }
}

static esp_err_t uac_take_lock(void)
{
    if (s_uac_lock == NULL) {
        ESP_LOGE(TAG, "Failed to lock UAC speaker: lock is not ready");
        return ESP_ERR_INVALID_STATE;
    }
    if (xSemaphoreTake(s_uac_lock, portMAX_DELAY) != pdTRUE) {
        ESP_LOGE(TAG, "Failed to lock UAC speaker");
        return ESP_FAIL;
    }
    return ESP_OK;
}

static esp_err_t uac_set_speaker_handle(uac_host_device_handle_t uac_device_handle)
{
    esp_err_t ret = uac_take_lock();
    if (ret != ESP_OK) {
        return ret;
    }
    // Update the shared handle under lock so player callbacks cannot race with connect or cleanup paths.
    s_spk_dev_handle = uac_device_handle;
    xSemaphoreGive(s_uac_lock);
    return ESP_OK;
}

static esp_err_t uac_set_mute(bool mute)
{
    esp_err_t ret = uac_take_lock();
    if (ret != ESP_OK) {
        return ret;
    }
    if (s_spk_dev_handle == NULL) {
        ESP_LOGE(TAG, "Failed to set mute: speaker is not ready");
        xSemaphoreGive(s_uac_lock);
        return ESP_ERR_INVALID_STATE;
    }
    ESP_LOGI(TAG, "mute setting: %s", mute ? "mute" : "unmute");
    // Some UAC devices may not support mute, so do not check the return value here.
    if (!mute) {
        uac_host_device_set_volume(s_spk_dev_handle, DEFAULT_VOLUME);
        uac_host_device_set_mute(s_spk_dev_handle, false);
    } else {
        uac_host_device_set_volume(s_spk_dev_handle, 0);
        uac_host_device_set_mute(s_spk_dev_handle, true);
    }
    xSemaphoreGive(s_uac_lock);
    return ESP_OK;
}

static int simple_player_out_data_callback(uint8_t *data, int data_size, void *ctx)
{
    esp_err_t ret = uac_take_lock();
    if (ret != ESP_OK) {
        return ESP_FAIL;
    }
    if (s_spk_dev_handle == NULL) {
        ESP_LOGE(TAG, "Failed to write audio data: speaker is not ready");
        xSemaphoreGive(s_uac_lock);
        return ESP_FAIL;
    }
    ret = uac_host_device_write(s_spk_dev_handle, data, data_size, portMAX_DELAY);
    xSemaphoreGive(s_uac_lock);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to write audio data, ret: %s", esp_err_to_name(ret));
        return ESP_FAIL;
    }
    return data_size;
}

static esp_gmf_err_io_t simple_player_port_acquire_write(void *handle, esp_gmf_payload_t *load, uint32_t wanted_size, int wait_ticks)
{
    return ESP_GMF_IO_OK;
}

static esp_gmf_err_io_t simple_player_port_release_write(void *handle, esp_gmf_payload_t *load, int wait_ticks)
{
    if (load->valid_size == 0) {
        return ESP_GMF_IO_OK;
    }
    int ret = simple_player_out_data_callback(load->buf, load->valid_size, NULL);
    return ret < 0 ? ESP_GMF_IO_FAIL : ESP_GMF_IO_OK;
}

static bool uac_alt_is_pcm(const uac_host_dev_alt_param_t *alt)
{
    return alt->format == UAC_TYPE_I_PCM;
}

static bool uac_alt_supports_rate(const uac_host_dev_alt_param_t *alt, uint32_t sample_rate)
{
    if (alt->sample_freq_type == 0) {
        return alt->sample_freq_lower <= sample_rate && sample_rate <= alt->sample_freq_upper;
    }
    uint8_t freq_count = alt->sample_freq_type > UAC_FREQ_NUM_MAX ? UAC_FREQ_NUM_MAX : alt->sample_freq_type;
    for (int i = 0; i < freq_count; i++) {
        if (alt->sample_freq[i] == sample_rate) {
            return true;
        }
    }
    return false;
}

static esp_err_t uac_device_set_stream_state(bool resume)
{
    esp_err_t ret = uac_take_lock();
    if (ret != ESP_OK) {
        return ret;
    }
    if (s_spk_dev_handle == NULL) {
        xSemaphoreGive(s_uac_lock);
        ESP_LOGW(TAG, "Skip UAC stream %s: speaker is not ready", resume ? "resume" : "suspend");
        return ESP_ERR_INVALID_STATE;
    }
    // Hold the UAC lock while using the handle so disconnect cleanup cannot close it concurrently.
    ret = resume ? uac_host_device_resume(s_spk_dev_handle) : uac_host_device_suspend(s_spk_dev_handle);
    xSemaphoreGive(s_uac_lock);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to %s UAC speaker stream, ret: %s", resume ? "resume" : "suspend", esp_err_to_name(ret));
    }
    return ret;
}

static uint32_t uac_select_fallback_rate(const uac_host_dev_alt_param_t *alt)
{
    const uint32_t preferred_rates[] = {DEFAULT_UAC_FREQ, 44100, 32000, 16000, 8000};
    for (int i = 0; i < sizeof(preferred_rates) / sizeof(preferred_rates[0]); i++) {
        if (uac_alt_supports_rate(alt, preferred_rates[i])) {
            return preferred_rates[i];
        }
    }
    if (alt->sample_freq_type == 0) {
        return alt->sample_freq_lower;
    }
    return alt->sample_freq[0];
}

static esp_err_t uac_select_target_format(uac_host_device_handle_t uac_device_handle, uac_audio_format_t *target_format)
{
    uac_host_dev_info_t dev_info = {0};
    esp_err_t ret = uac_host_get_device_info(uac_device_handle, &dev_info);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get UAC device info, ret: %s", esp_err_to_name(ret));
        return ret;
    }
    for (int i = 1; i <= dev_info.iface_alt_num; i++) {
        uac_host_dev_alt_param_t alt = {0};
        ret = uac_host_get_device_alt_param(uac_device_handle, i, &alt);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to get UAC alt %d param, ret: %s", i, esp_err_to_name(ret));
            return ret;
        }
        if (uac_alt_is_pcm(&alt) && alt.channels > 0 && alt.bit_resolution > 0) {
            target_format->sample_rate = uac_select_fallback_rate(&alt);
            target_format->bits = alt.bit_resolution;
            target_format->channels = alt.channels;
            ESP_LOGI(TAG, "Selected UAC format from alt %d: %"PRIu32" Hz, %u bits, %u channels", i, target_format->sample_rate, target_format->bits, target_format->channels);
            return ESP_OK;
        }
    }
    ESP_LOGE(TAG, "No supported PCM UAC speaker format found");
    return ESP_ERR_NOT_FOUND;
}

static esp_err_t uac_start_stream_with_target_format(uac_host_device_handle_t uac_device_handle)
{
    const uac_host_stream_config_t stm_config = {
        .channels = s_uac_target_format.channels,
        .bit_resolution = s_uac_target_format.bits,
        .sample_freq = s_uac_target_format.sample_rate,
        .flags = FLAG_STREAM_SUSPEND_AFTER_START,
    };
    esp_err_t ret = uac_host_device_start(uac_device_handle, &stm_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start UAC speaker stream, ret: %s", esp_err_to_name(ret));
    }
    return ret;
}

static int simple_player_event_callback(esp_asp_event_pkt_t *event, void *ctx)
{
    if (event->type == ESP_ASP_EVENT_TYPE_MUSIC_INFO) {
        if (event->payload == NULL || event->payload_size == 0) {
            ESP_LOGE(TAG, "Failed to handle music info event: invalid payload");
            return 0;
        }
        esp_asp_music_info_t info = {0};
        memcpy(&info, event->payload, UAC_PLAYER_MIN(event->payload_size, sizeof(info)));
        ESP_LOGI(TAG, "Audio info, rate:%d, channels:%d, bits:%d; output target: %"PRIu32" Hz, %u bits, %u channels", info.sample_rate, info.channels, info.bits, s_uac_target_format.sample_rate, s_uac_target_format.bits, s_uac_target_format.channels);
    } else if (event->type == ESP_ASP_EVENT_TYPE_STATE) {
        if (event->payload == NULL || event->payload_size == 0) {
            ESP_LOGE(TAG, "Failed to handle player state event: invalid payload");
            return 0;
        }
        esp_asp_state_t state = 0;
        memcpy(&state, event->payload, UAC_PLAYER_MIN(event->payload_size, sizeof(state)));
        ESP_LOGI(TAG, "Audio player state: %s", esp_audio_simple_player_state_to_str(state));
        if (state == ESP_ASP_STATE_RUNNING) {
            uac_set_mute(false);
            uac_device_set_stream_state(true);
        } else if (state == ESP_ASP_STATE_FINISHED) {
            if (uac_device_set_stream_state(false) == ESP_OK) {
                queue_play_request();
            }
        } else if (state == ESP_ASP_STATE_STOPPED || state == ESP_ASP_STATE_ERROR) {
            uac_device_set_stream_state(false);
        }
    }
    return 0;
}

static esp_err_t simple_player_start(void)
{
    if (s_player == NULL) {
        ESP_LOGE(TAG, "Failed to start playback: player is not ready");
        return ESP_ERR_INVALID_STATE;
    }
    if (s_spk_dev_handle == NULL) {
        ESP_LOGE(TAG, "Failed to start playback: speaker is not ready");
        return ESP_ERR_INVALID_STATE;
    }
    ESP_LOGI(TAG, "Playing '%s'", MP3_FILE_NAME);
    esp_gmf_err_t ret = esp_audio_simple_player_run(s_player, MP3_FILE_URI, NULL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start simple player, ret: %s", esp_err_to_name(ret));
    }
    return ret;
}

static esp_err_t simple_player_register_uac_converters(void)
{
    esp_gmf_pool_handle_t pool = NULL;
    esp_gmf_err_t ret = esp_audio_simple_player_get_pool(s_player, &pool);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get simple player pool, ret: %s", esp_err_to_name(ret));
        return ret;
    }
    esp_ae_rate_cvt_cfg_t rate_cvt_cfg = DEFAULT_ESP_GMF_RATE_CVT_CONFIG();
    rate_cvt_cfg.dest_rate = s_uac_target_format.sample_rate;
    esp_gmf_element_handle_t rate_cvt = NULL;
    ret = esp_gmf_rate_cvt_init(&rate_cvt_cfg, &rate_cvt);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create rate converter, ret: %s", esp_err_to_name(ret));
        return ret;
    }
    ret = esp_gmf_pool_register_element_at_head(pool, rate_cvt, NULL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register rate converter, ret: %s", esp_err_to_name(ret));
        esp_gmf_element_deinit(rate_cvt);
        return ret;
    }
    esp_ae_ch_cvt_cfg_t ch_cvt_cfg = DEFAULT_ESP_GMF_CH_CVT_CONFIG();
    ch_cvt_cfg.dest_ch = s_uac_target_format.channels;
    esp_gmf_element_handle_t ch_cvt = NULL;
    ret = esp_gmf_ch_cvt_init(&ch_cvt_cfg, &ch_cvt);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create channel converter, ret: %s", esp_err_to_name(ret));
        return ret;
    }
    ret = esp_gmf_pool_register_element_at_head(pool, ch_cvt, NULL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register channel converter, ret: %s", esp_err_to_name(ret));
        esp_gmf_element_deinit(ch_cvt);
        return ret;
    }
    esp_ae_bit_cvt_cfg_t bit_cvt_cfg = DEFAULT_ESP_GMF_BIT_CVT_CONFIG();
    bit_cvt_cfg.dest_bits = s_uac_target_format.bits;
    esp_gmf_element_handle_t bit_cvt = NULL;
    ret = esp_gmf_bit_cvt_init(&bit_cvt_cfg, &bit_cvt);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create bit converter, ret: %s", esp_err_to_name(ret));
        return ret;
    }
    ret = esp_gmf_pool_register_element_at_head(pool, bit_cvt, NULL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register bit converter, ret: %s", esp_err_to_name(ret));
        esp_gmf_element_deinit(bit_cvt);
        return ret;
    }
    const char *pipeline_names[] = {"aud_dec", "aud_rate_cvt", "aud_ch_cvt", "aud_bit_cvt"};
    ret = esp_audio_simple_player_set_pipeline(s_player, NULL, pipeline_names, sizeof(pipeline_names) / sizeof(pipeline_names[0]), NULL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set simple player pipeline, ret: %s", esp_err_to_name(ret));
        return ret;
    }
    esp_gmf_pipeline_handle_t pipe = NULL;
    ret = esp_audio_simple_player_get_pipeline(s_player, &pipe);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get simple player pipeline, ret: %s", esp_err_to_name(ret));
        return ret;
    }
    // The advanced API pre-creates the pipeline, so register the final output port explicitly.
    esp_gmf_port_handle_t out_port = NEW_ESP_GMF_PORT_OUT_BYTE(simple_player_port_acquire_write, simple_player_port_release_write, NULL, NULL, 2048, ESP_GMF_MAX_DELAY);
    if (out_port == NULL) {
        ESP_LOGE(TAG, "Failed to create simple player output port");
        return ESP_ERR_NO_MEM;
    }
    ret = esp_gmf_pipeline_reg_el_port(pipe, "aud_bit_cvt", ESP_GMF_IO_DIR_WRITER, out_port);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register simple player output port, ret: %s", esp_err_to_name(ret));
        esp_gmf_port_deinit(out_port);
        return ret;
    }
    ESP_LOGI(TAG, "Configured simple player output conversion target: %"PRIu32" Hz, %u bits, %u channels", s_uac_target_format.sample_rate, s_uac_target_format.bits, s_uac_target_format.channels);
    return ESP_OK;
}

static esp_err_t simple_player_init(void)
{
    if (s_player != NULL) {
        return ESP_OK;
    }
    esp_asp_cfg_t cfg = {
        .out.cb = simple_player_out_data_callback,
        .out.user_ctx = NULL,
        .task_prio = 1,
        .task_stack = 6 * 1024,
    };
    esp_gmf_err_t ret = esp_audio_simple_player_new(&cfg, &s_player);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create simple player, ret: %s", esp_err_to_name(ret));
        return ret;
    }
    ret = esp_audio_simple_player_set_event(s_player, simple_player_event_callback, NULL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register simple player event callback, ret: %s", esp_err_to_name(ret));
        esp_audio_simple_player_destroy(s_player);
        s_player = NULL;
        return ret;
    }
    ret = simple_player_register_uac_converters();
    if (ret != ESP_OK) {
        esp_audio_simple_player_destroy(s_player);
        s_player = NULL;
        return ret;
    }
    return ret;
}

static void simple_player_deinit(void)
{
    if (s_player == NULL) {
        return;
    }
    esp_gmf_err_t ret = esp_audio_simple_player_stop(s_player);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Simple player stop returned: %s", esp_err_to_name(ret));
    }
    ret = esp_audio_simple_player_destroy(s_player);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to destroy simple player, ret: %s", esp_err_to_name(ret));
    }
    s_player = NULL;
}

static void reset_uac_target_format(void)
{
    s_uac_target_format = (uac_audio_format_t) {
        .sample_rate = DEFAULT_UAC_FREQ,
        .bits = DEFAULT_UAC_BITS,
        .channels = DEFAULT_UAC_CH,
    };
}

static void close_uac_speaker(uac_host_device_handle_t uac_device_handle, bool stop_first)
{
    if (uac_device_handle == NULL) {
        return;
    }
    esp_err_t lock_ret = uac_take_lock();
    if (lock_ret != ESP_OK) {
        return;
    }
    if (s_spk_dev_handle == uac_device_handle) {
        // Clear the shared handle before closing it so later callbacks cannot observe a stale device.
        s_spk_dev_handle = NULL;
    }
    xSemaphoreGive(s_uac_lock);
    if (stop_first) {
        esp_err_t stop_ret = uac_host_device_stop(uac_device_handle);
        if (stop_ret != ESP_OK) {
            ESP_LOGW(TAG, "UAC speaker stop returned: %s", esp_err_to_name(stop_ret));
        }
    }
    esp_err_t ret = uac_host_device_close(uac_device_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to close UAC speaker, ret: %s", esp_err_to_name(ret));
    }
}

static void uac_device_callback(uac_host_device_handle_t uac_device_handle, const uac_host_device_event_t event, void *arg)
{
    // Send uac device event to the event queue
    s_event_queue_t evt_queue = {
        .event_group = UAC_DEVICE_EVENT,
        .device_evt.handle = uac_device_handle,
        .device_evt.event = event,
        .device_evt.arg = arg
    };
    // should not block here
    if (xQueueSend(s_event_queue, &evt_queue, 0) != pdTRUE) {
        ESP_LOGE(TAG, "Failed to queue UAC device event: %d", event);
    }
}

static void uac_host_lib_callback(uint8_t addr, uint8_t iface_num, const uac_host_driver_event_t event, void *arg)
{
    // Send uac driver event to the event queue
    s_event_queue_t evt_queue = {
        .event_group = UAC_DRIVER_EVENT,
        .driver_evt.addr = addr,
        .driver_evt.iface_num = iface_num,
        .driver_evt.event = event,
        .driver_evt.arg = arg
    };
    if (xQueueSend(s_event_queue, &evt_queue, 0) != pdTRUE) {
        ESP_LOGE(TAG, "Failed to queue UAC driver event: %d", event);
    }
}

/**
 * @brief Start USB Host install and handle common USB host library events while app pin not low
 *
 * @param[in] arg  Not used
 */
static void usb_lib_task(void *arg)
{
    const usb_host_config_t host_config = {
        .skip_phy_setup = false,
        .intr_flags = ESP_INTR_FLAG_LEVEL1,
    };

    ESP_ERROR_CHECK(usb_host_install(&host_config));
    ESP_LOGI(TAG, "USB Host installed");
    xTaskNotifyGive(arg);

    while (true) {
        uint32_t event_flags;
        usb_host_lib_handle_events(portMAX_DELAY, &event_flags);
        // In this example, there is only one client registered
        // So, once we deregister the client, this call must succeed with ESP_OK
        if (event_flags & USB_HOST_LIB_EVENT_FLAGS_NO_CLIENTS) {
            ESP_ERROR_CHECK(usb_host_device_free_all());
            break;
        }
    }

    ESP_LOGI(TAG, "USB Host shutdown");
    // Clean up USB Host
    vTaskDelay(10); // Short delay to allow clients clean-up
    ESP_ERROR_CHECK(usb_host_uninstall());
    vTaskDelete(NULL);
}

static void uac_lib_task(void *arg)
{
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    uac_host_driver_config_t uac_config = {
        .create_background_task = true,
        .task_priority = UAC_TASK_PRIORITY,
        .stack_size = 4096,
        .core_id = 0,
        .callback = uac_host_lib_callback,
        .callback_arg = NULL
    };

    ESP_ERROR_CHECK(uac_host_install(&uac_config));
    ESP_LOGI(TAG, "UAC Class Driver installed");
    s_event_queue_t evt_queue = {0};
    bool task_run = true;
    while (task_run) {
        if (xQueueReceive(s_event_queue, &evt_queue, portMAX_DELAY)) {
            if (UAC_DRIVER_EVENT ==  evt_queue.event_group) {
                uac_host_driver_event_t event = evt_queue.driver_evt.event;
                uint8_t addr = evt_queue.driver_evt.addr;
                uint8_t iface_num = evt_queue.driver_evt.iface_num;
                switch (event) {
                case UAC_HOST_DRIVER_EVENT_TX_CONNECTED: {
                    esp_err_t ret = ESP_OK;
                    uac_host_device_handle_t uac_device_handle = NULL;
                    const uac_host_device_config_t dev_config = {
                        .addr = addr,
                        .iface_num = iface_num,
                        .buffer_size = 16000,
                        .buffer_threshold = 4000,
                        .callback = uac_device_callback,
                        .callback_arg = NULL,
                    };
                    ret = uac_host_device_open(&dev_config, &uac_device_handle);
                    if (ret != ESP_OK) {
                        ESP_LOGE(TAG, "Failed to open UAC speaker, ret: %s", esp_err_to_name(ret));
                        break;
                    }
                    ESP_LOGI(TAG, "UAC Device connected: SPK");
                    uac_host_printf_device_param(uac_device_handle);
                    ret = uac_select_target_format(uac_device_handle, &s_uac_target_format);
                    if (ret != ESP_OK) {
                        ESP_LOGE(TAG, "Failed to select UAC target format, ret: %s", esp_err_to_name(ret));
                        close_uac_speaker(uac_device_handle, false);
                        reset_uac_target_format();
                        break;
                    }
                    ret = uac_start_stream_with_target_format(uac_device_handle);
                    if (ret != ESP_OK) {
                        close_uac_speaker(uac_device_handle, true);
                        reset_uac_target_format();
                        break;
                    }
                    ret = uac_set_speaker_handle(uac_device_handle);
                    if (ret != ESP_OK) {
                        ESP_LOGE(TAG, "Failed to set UAC speaker handle, ret: %s", esp_err_to_name(ret));
                        close_uac_speaker(uac_device_handle, true);
                        reset_uac_target_format();
                        break;
                    }
                    ret = simple_player_init();
                    if (ret != ESP_OK) {
                        ESP_LOGE(TAG, "Failed to initialize simple player, ret: %s", esp_err_to_name(ret));
                        close_uac_speaker(uac_device_handle, true);
                        reset_uac_target_format();
                        break;
                    }
                    ret = simple_player_start();
                    if (ret != ESP_OK) {
                        ESP_LOGE(TAG, "Failed to start simple player, ret: %s", esp_err_to_name(ret));
                        simple_player_deinit();
                        close_uac_speaker(uac_device_handle, true);
                        reset_uac_target_format();
                        break;
                    }
                    break;
                }
                case UAC_HOST_DRIVER_EVENT_RX_CONNECTED: {
                    // we don't support MIC in this example
                    ESP_LOGI(TAG, "UAC Device connected: MIC");
                    break;
                }
                default:
                    break;
                }
            } else if (UAC_DEVICE_EVENT == evt_queue.event_group) {
                uac_host_device_event_t event = evt_queue.device_evt.event;
                switch (event) {
                case UAC_HOST_DRIVER_EVENT_DISCONNECTED:
                    // Keep player and UAC device teardown serialized in this task.
                    simple_player_deinit();
                    close_uac_speaker(evt_queue.device_evt.handle, false);
                    reset_uac_target_format();
                    ESP_LOGI(TAG, "UAC Device disconnected");
                    break;
                case UAC_HOST_DEVICE_EVENT_RX_DONE:
                    break;
                case UAC_HOST_DEVICE_EVENT_TX_DONE:
                    break;
                case UAC_HOST_DEVICE_EVENT_TRANSFER_ERROR:
                    break;
                default:
                    break;
                }
            } else if (APP_EVENT == evt_queue.event_group) {
                if (evt_queue.app_evt.event == APP_EVENT_PLAY) {
                    esp_err_t ret = simple_player_start();
                    if (ret != ESP_OK) {
                        ESP_LOGE(TAG, "Failed to handle play request, ret: %s", esp_err_to_name(ret));
                    }
                } else if (evt_queue.app_evt.event == APP_EVENT_SHUTDOWN) {
                    task_run = false;
                }
            }
        }
    }

    ESP_LOGI(TAG, "UAC Driver uninstall");
    ESP_ERROR_CHECK(uac_host_uninstall());
}

void app_main(void)
{
    s_event_queue = xQueueCreate(10, sizeof(s_event_queue_t));
    assert(s_event_queue != NULL);
    s_uac_lock = xSemaphoreCreateMutex();
    assert(s_uac_lock != NULL);

    esp_vfs_spiffs_conf_t conf = {
        .base_path = SPIFFS_BASE,
        .partition_label = NULL,
        .max_files = 2,
        .format_if_mount_failed = true,
    };
    ESP_ERROR_CHECK(esp_vfs_spiffs_register(&conf));

    static TaskHandle_t uac_task_handle = NULL;
    BaseType_t ret = xTaskCreatePinnedToCore(uac_lib_task, "uac_events", 4096, NULL,
                                             USER_TASK_PRIORITY, &uac_task_handle, 0);
    assert(ret == pdTRUE);
    ret = xTaskCreatePinnedToCore(usb_lib_task, "usb_events", 4096, (void *)uac_task_handle,
                                  USB_HOST_TASK_PRIORITY, NULL, 0);
    assert(ret == pdTRUE);

    while (1) {
        vTaskDelay(100);
    }

}
