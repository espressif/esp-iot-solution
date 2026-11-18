/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <unistd.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <arpa/inet.h>
#include "esp_timer.h"
#include "esp_random.h"
#include "esp_log.h"
#include "esp_idf_version.h"
#include "esp_heap_caps.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "freertos/semphr.h"
#include "esp_xiaozhi_udp.h"
#include "esp_xiaozhi_udp_probe.h"

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(6, 0, 0)
#include "psa/crypto.h"
typedef struct {
    psa_key_id_t key_id;
    bool key_imported;
} esp_xiaozhi_chat_aes_ctr_context_t;
#else
#include "mbedtls/aes.h"
#include "mbedtls/md.h"
typedef mbedtls_aes_context esp_xiaozhi_chat_aes_ctr_context_t;
#endif

#define ESP_XIAOZHI_CHAT_AES_NONCE_SIZE 16
#define UDP_SEND_BACKPRESSURE_DELAY_MS 10
#define UDP_ENABLED BIT0
#define UDP_STOP BIT1
#define UDP_EXITED BIT2
#define UDP_IDLE BIT3

static const char *TAG = "ESP_XIAOZHI_UDP";

struct esp_xiaozhi_udp_s {
    SemaphoreHandle_t mutex;
    EventGroupHandle_t events;
    TaskHandle_t task;
    int socket;
    unsigned io_count;
    uint32_t generation;
    uint32_t local_sequence;
    uint32_t remote_sequence;
    int64_t last_activity;
    int64_t last_pong_time;
    bool failed;
    bool active;
    bool configured;
    bool probe_enabled;
    char server[256];
    int port;
    uint8_t key[16];
    uint8_t nonce[16];
    esp_xiaozhi_chat_aes_ctr_context_t aes;
    esp_xiaozhi_udp_probe_t probe;
    esp_xiaozhi_udp_audio_cb_t on_audio;
    esp_xiaozhi_udp_error_cb_t on_error;
    void *ctx;
};

#if CONFIG_XIAOZHI_AUDIO_TASK_ALLOC_STATIC
static StaticTask_t s_udp_task_tcb;
static StackType_t s_udp_task_stack[CONFIG_XIAOZHI_AUDIO_TASK_STACK_SIZE];
#endif

static void esp_xiaozhi_chat_aes_ctr_init(esp_xiaozhi_chat_aes_ctr_context_t *ctx)
{
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(6, 0, 0)
    ctx->key_id = 0;
    ctx->key_imported = false;
#else
    mbedtls_aes_init(ctx);
#endif
}

static int esp_xiaozhi_chat_aes_ctr_setkey(esp_xiaozhi_chat_aes_ctr_context_t *ctx,
                                           const uint8_t *key,
                                           size_t key_bits)
{
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(6, 0, 0)
    if (ctx->key_imported) {
        psa_destroy_key(ctx->key_id);
        ctx->key_id = 0;
        ctx->key_imported = false;
    }

    psa_status_t status = psa_crypto_init();
    if (status != PSA_SUCCESS) {
        return (int)status;
    }

    psa_key_attributes_t attributes = PSA_KEY_ATTRIBUTES_INIT;
    psa_set_key_usage_flags(&attributes, PSA_KEY_USAGE_ENCRYPT);
    psa_set_key_algorithm(&attributes, PSA_ALG_CTR);
    psa_set_key_type(&attributes, PSA_KEY_TYPE_AES);
    psa_set_key_bits(&attributes, key_bits);

    status = psa_import_key(&attributes, key, key_bits / 8, &ctx->key_id);
    psa_reset_key_attributes(&attributes);
    if (status != PSA_SUCCESS) {
        ctx->key_id = 0;
        return (int)status;
    }

    ctx->key_imported = true;
    return 0;
#else
    return mbedtls_aes_setkey_enc(ctx, key, (unsigned int)key_bits);
#endif
}

static int esp_xiaozhi_chat_aes_ctr_crypt(esp_xiaozhi_chat_aes_ctr_context_t *ctx,
                                          size_t length,
                                          uint8_t nonce_counter[ESP_XIAOZHI_CHAT_AES_NONCE_SIZE],
                                          const uint8_t *input,
                                          uint8_t *output)
{
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(6, 0, 0)
    if (!ctx->key_imported) {
        return PSA_ERROR_BAD_STATE;
    }

    psa_cipher_operation_t operation = PSA_CIPHER_OPERATION_INIT;
    psa_status_t status = psa_cipher_encrypt_setup(&operation, ctx->key_id, PSA_ALG_CTR);
    if (status != PSA_SUCCESS) {
        return (int)status;
    }

    status = psa_cipher_set_iv(&operation, nonce_counter, ESP_XIAOZHI_CHAT_AES_NONCE_SIZE);
    if (status != PSA_SUCCESS) {
        psa_cipher_abort(&operation);
        return (int)status;
    }

    size_t output_len = 0;
    if (input == output) {
        /* PSA configurations may forbid overlapping buffers. Stage small CTR
         * chunks instead of keeping a second full receive buffer on the heap. */
        uint8_t block[64];
        while (output_len < length) {
            size_t chunk = length - output_len;
            if (chunk > sizeof(block)) {
                chunk = sizeof(block);
            }
            size_t written = 0;
            status = psa_cipher_update(&operation, input + output_len, chunk, block, sizeof(block), &written);
            if (status != PSA_SUCCESS || written != chunk) {
                psa_cipher_abort(&operation);
                return status != PSA_SUCCESS ? (int)status : PSA_ERROR_GENERIC_ERROR;
            }
            memcpy(output + output_len, block, written);
            output_len += written;
        }
    } else {
        status = psa_cipher_update(&operation, input, length, output, length, &output_len);
    }
    if (status != PSA_SUCCESS) {
        psa_cipher_abort(&operation);
        return (int)status;
    }

    uint8_t finish_buf[16] = {0};
    size_t finish_len = 0;
    status = psa_cipher_finish(&operation, finish_buf, sizeof(finish_buf), &finish_len);
    if (status != PSA_SUCCESS) {
        psa_cipher_abort(&operation);
        return (int)status;
    }

    return (output_len == length && finish_len == 0) ? 0 : PSA_ERROR_GENERIC_ERROR;
#else
    size_t nc_off = 0;
    uint8_t stream_block[ESP_XIAOZHI_CHAT_AES_NONCE_SIZE] = {0};
    return mbedtls_aes_crypt_ctr(ctx, length, &nc_off, nonce_counter, stream_block, input, output);
#endif
}

static void esp_xiaozhi_chat_aes_ctr_free(esp_xiaozhi_chat_aes_ctr_context_t *ctx)
{
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(6, 0, 0)
    if (ctx->key_imported) {
        psa_destroy_key(ctx->key_id);
        ctx->key_id = 0;
        ctx->key_imported = false;
    }
#else
    mbedtls_aes_free(ctx);
#endif
}

static bool udp_decode_hex(const char *hex, uint8_t output[16])
{
    if (!hex || strlen(hex) != 32) {
        return false;
    }
    for (unsigned i = 0; i < 32; i++) {
        if (!isxdigit((unsigned char)hex[i])) {
            return false;
        }
    }
    for (unsigned i = 0; i < 16; i++) {
        unsigned value;
        if (sscanf(hex + i * 2, "%2x", &value) != 1) {
            return false;
        }
        output[i] = value;
    }
    return true;
}

bool esp_xiaozhi_udp_probe_mac(const uint8_t key[16], const uint8_t header[16], uint8_t tag[16])
{
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(6, 0, 0)
    if (psa_crypto_init() != PSA_SUCCESS) {
        return false;
    }
    psa_key_attributes_t attr = PSA_KEY_ATTRIBUTES_INIT;
    psa_key_id_t id = 0;
    const psa_algorithm_t algorithm = PSA_ALG_TRUNCATED_MAC(PSA_ALG_HMAC(PSA_ALG_SHA_256), 16);
    psa_set_key_type(&attr, PSA_KEY_TYPE_HMAC);
    psa_set_key_bits(&attr, 128);
    psa_set_key_algorithm(&attr, algorithm);
    psa_set_key_usage_flags(&attr, PSA_KEY_USAGE_SIGN_MESSAGE);
    psa_status_t status = psa_import_key(&attr, key, 16, &id);
    psa_reset_key_attributes(&attr);
    if (status != PSA_SUCCESS) {
        return false;
    }
    size_t length = 0;
    status = psa_mac_compute(id, algorithm, header, 16, tag, 16, &length);
    psa_destroy_key(id);
    return status == PSA_SUCCESS && length == 16;
#else
    uint8_t hash[32];
    const mbedtls_md_info_t *info = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
    if (!info || mbedtls_md_hmac(info, key, 16, header, 16, hash)) {
        return false;
    }
    memcpy(tag, hash, 16);
    memset(hash, 0, sizeof(hash));
    return true;
#endif
}

static void udp_io_begin_locked(esp_xiaozhi_udp_handle_t udp)
{
    udp->io_count++;
    xEventGroupClearBits(udp->events, UDP_IDLE);
}

static void udp_io_end_locked(esp_xiaozhi_udp_handle_t udp)
{
    udp->io_count--;
    if (udp->io_count == 0) {
        if (!udp->active && udp->socket >= 0) {
            close(udp->socket);
            udp->socket = -1;
        }
        xEventGroupSetBits(udp->events, UDP_IDLE);
    }
}

static void udp_fail(esp_xiaozhi_udp_handle_t udp, uint32_t generation, esp_err_t error, const char *source)
{
    xSemaphoreTake(udp->mutex, portMAX_DELAY);
    bool report = udp->active && udp->generation == generation;
    if (report) {
        udp->active = false;
        udp->failed = true;
        udp->generation++;
        udp->probe.pending = false;
        xEventGroupClearBits(udp->events, UDP_ENABLED);
        if (udp->socket >= 0) {
            shutdown(udp->socket, SHUT_RDWR);
            if (udp->io_count == 0) {
                close(udp->socket);
                udp->socket = -1;
            }
        }
    }
    xSemaphoreGive(udp->mutex);
    if (report && udp->on_error) {
        udp->on_error(udp->ctx, error, source);
    }
}

static void udp_worker(void *arg)
{
    esp_xiaozhi_udp_handle_t udp = arg;
    const size_t capacity = CONFIG_XIAOZHI_UDP_RECEIVE_BUFFER_SIZE;
    uint8_t *buffer = malloc(capacity);
    unsigned errors = 0;
    uint32_t error_generation = 0;
    while (buffer) {
        EventBits_t bits = xEventGroupWaitBits(udp->events, UDP_ENABLED | UDP_STOP, pdFALSE, pdFALSE, portMAX_DELAY);
        if (bits & UDP_STOP) {
            break;
        }
        uint8_t ping[32];
        bool send_ping = false;
        bool failed = false;
        bool mac_failed = false;
        xSemaphoreTake(udp->mutex, portMAX_DELAY);
        uint32_t generation = udp->generation;
        int socket = udp->socket;
        if (generation != error_generation) {
            errors = 0;
            error_generation = generation;
        }
        if (!udp->active || socket < 0) {
            xSemaphoreGive(udp->mutex);
            continue;
        }
        int64_t now = esp_timer_get_time();
        if (udp->probe_enabled) {
            esp_xiaozhi_probe_action_t action = esp_xiaozhi_udp_probe_poll(&udp->probe, now);
            failed = action == ESP_XIAOZHI_PROBE_FAILED;
            if (action == ESP_XIAOZHI_PROBE_SEND) {
                memcpy(ping, udp->probe.packet, sizeof(ping));
                send_ping = esp_xiaozhi_udp_probe_mac(udp->key, ping, ping + 16);
                mac_failed = !send_ping;
                failed = mac_failed;
            }
        }
        bool idle_timeout = now - udp->last_activity > 120000000;
        udp_io_begin_locked(udp);
        xSemaphoreGive(udp->mutex);
        if (send_ping) {
            (void)send(socket, ping, sizeof(ping), 0);
        }
        int length = (failed || idle_timeout) ? -1 : recv(socket, buffer, capacity, 0);
        int socket_error = (failed || idle_timeout || length >= 0) ? 0 : errno;
        int64_t received_at = esp_timer_get_time();
        bool audio = false;
        size_t audio_length = 0;
        uint32_t timestamp = 0;
        unsigned attempts = 0;
        int64_t elapsed = 0;
        xSemaphoreTake(udp->mutex, portMAX_DELAY);
        bool current = udp->active && udp->generation == generation;
        if (current && length > 0) {
            errors = 0;
            if (buffer[0] == 3) {
                uint8_t tag[16];
                if (udp->probe_enabled && esp_xiaozhi_udp_probe_matches(&udp->probe, buffer, length, received_at) &&
                        esp_xiaozhi_udp_probe_mac(udp->key, buffer, tag)) {
                    volatile unsigned difference = 0;
                    for (unsigned i = 0; i < 16; i++) {
                        difference |= tag[i] ^ buffer[16 + i];
                    }
                    if (!difference) {
                        elapsed = received_at - udp->probe.started;
                        attempts = udp->probe.attempts;
                        udp->last_pong_time = received_at;
                        esp_xiaozhi_udp_probe_complete(&udp->probe, received_at, esp_random());
                    }
                }
            } else if (length >= 16 && buffer[0] == 1) {
                audio_length = ((size_t)buffer[2] << 8) | buffer[3];
                uint32_t sequence;
                memcpy(&sequence, buffer + 12, sizeof(sequence));
                sequence = ntohl(sequence);
                if ((size_t)length == audio_length + 16 && sequence > udp->remote_sequence) {
                    uint8_t nonce[16];
                    memcpy(nonce, buffer, 16);
                    if (!esp_xiaozhi_chat_aes_ctr_crypt(&udp->aes, audio_length, nonce, buffer + 16, buffer + 16)) {
                        memcpy(&timestamp, buffer + 8, sizeof(timestamp));
                        timestamp = ntohl(timestamp);
                        udp->remote_sequence = sequence;
                        udp->last_activity = received_at;
                        audio = true;
                    }
                }
            }
        } else if (current && length < 0 && socket_error != EAGAIN && socket_error != EWOULDBLOCK && socket_error != EINTR) {
            errors++;
        }
        xSemaphoreGive(udp->mutex);
        if (attempts == 1) {
            ESP_LOGI(TAG, "UDP RTT: %lld.%03lld ms", (long long)(elapsed / 1000), (long long)(elapsed % 1000));
        } else if (attempts > 1) {
            ESP_LOGI(TAG, "UDP RTT: unknown after retransmission (attempts=%u, elapsed=%lld.%03lld ms)",
                     attempts, (long long)(elapsed / 1000), (long long)(elapsed % 1000));
        }
        if (audio && udp->on_audio) {
            udp->on_audio(udp->ctx, buffer + 16, audio_length, timestamp);
        }
        xSemaphoreTake(udp->mutex, portMAX_DELAY);
        udp_io_end_locked(udp);
        xSemaphoreGive(udp->mutex);
        if (current && (failed || idle_timeout || errors >= CONFIG_XIAOZHI_MAX_CONSECUTIVE_UDP_ERRORS)) {
            udp_fail(udp, generation, mac_failed ? ESP_FAIL : failed || idle_timeout ? ESP_ERR_TIMEOUT : ESP_FAIL,
                     mac_failed ? "udp_probe_auth" : failed ? "udp_probe_timeout" : idle_timeout ? "receive_timeout" : "udp_receive");
        }
    }
    if (!buffer) {
        xSemaphoreTake(udp->mutex, portMAX_DELAY);
        uint32_t generation = udp->generation;
        xSemaphoreGive(udp->mutex);
        udp_fail(udp, generation, ESP_ERR_NO_MEM, "udp_buffer");
    }
    free(buffer);
    xEventGroupSetBits(udp->events, UDP_EXITED);
#if CONFIG_XIAOZHI_AUDIO_TASK_ALLOC_DYNAMIC
    vTaskDeleteWithCaps(NULL);
#else
    vTaskDelete(NULL);
#endif
}

esp_err_t esp_xiaozhi_udp_init(esp_xiaozhi_udp_handle_t *handle, esp_xiaozhi_udp_audio_cb_t audio,
                               esp_xiaozhi_udp_error_cb_t error, void *ctx)
{
    if (!handle) {
        return ESP_ERR_INVALID_ARG;
    }
    *handle = NULL;
    esp_xiaozhi_udp_handle_t udp = calloc(1, sizeof(*udp));
    if (!udp) {
        return ESP_ERR_NO_MEM;
    }
    udp->socket = -1;
    udp->mutex = xSemaphoreCreateMutex();
    udp->events = xEventGroupCreate();
    if (!udp->mutex || !udp->events) {
        if (udp->mutex) {
            vSemaphoreDelete(udp->mutex);
        }
        if (udp->events) {
            vEventGroupDelete(udp->events);
        }
        free(udp);
        return ESP_ERR_NO_MEM;
    }
    xEventGroupSetBits(udp->events, UDP_EXITED | UDP_IDLE);
    esp_xiaozhi_chat_aes_ctr_init(&udp->aes);
    udp->on_audio = audio;
    udp->on_error = error;
    udp->ctx = ctx;
    *handle = udp;
    return ESP_OK;
}

esp_err_t esp_xiaozhi_udp_close(esp_xiaozhi_udp_handle_t udp)
{
    if (!udp) {
        return ESP_OK;
    }
    xSemaphoreTake(udp->mutex, portMAX_DELAY);
    udp->active = false;
    udp->generation++;
    udp->probe.pending = false;
    xEventGroupClearBits(udp->events, UDP_ENABLED);
    if (udp->socket >= 0) {
        shutdown(udp->socket, SHUT_RDWR);
        if (udp->io_count == 0) {
            close(udp->socket);
            udp->socket = -1;
        }
    }
    bool self = xTaskGetCurrentTaskHandle() == udp->task;
    xSemaphoreGive(udp->mutex);
    if (self) {
        return ESP_OK;
    }
    EventBits_t bits = xEventGroupWaitBits(udp->events, UDP_IDLE, pdFALSE, pdFALSE, pdMS_TO_TICKS(2000));
    return bits & UDP_IDLE ? ESP_OK : ESP_ERR_TIMEOUT;
}

esp_err_t esp_xiaozhi_udp_configure(esp_xiaozhi_udp_handle_t udp, const char *server, int port,
                                    const char *key_hex, const char *nonce_hex, bool probe)
{
    uint8_t key[16], nonce[16];
    if (!udp || !server || !server[0] || strlen(server) >= sizeof(udp->server) || port < 1 || port > 65535 ||
            !udp_decode_hex(key_hex, key) || !udp_decode_hex(nonce_hex, nonce)) {
        memset(key, 0, sizeof(key));
        return ESP_ERR_INVALID_ARG;
    }
    if (esp_xiaozhi_udp_close(udp) != ESP_OK) {
        memset(key, 0, sizeof(key));
        return ESP_ERR_TIMEOUT;
    }
    xSemaphoreTake(udp->mutex, portMAX_DELAY);
    if (udp->io_count) {
        xSemaphoreGive(udp->mutex);
        memset(key, 0, sizeof(key));
        return ESP_ERR_INVALID_STATE;
    }
    /* Reusing this tuple would restart probe sequence numbers under the same MAC key. */
    if (probe && udp->configured && udp->probe_enabled &&
            !memcmp(udp->key, key, sizeof(key)) && !memcmp(udp->nonce + 4, nonce + 4, 4)) {
        xSemaphoreGive(udp->mutex);
        memset(key, 0, sizeof(key));
        return ESP_ERR_INVALID_STATE;
    }
    int result = esp_xiaozhi_chat_aes_ctr_setkey(&udp->aes, key, 128);
    if (!result) {
        memcpy(udp->key, key, 16);
        memcpy(udp->nonce, nonce, 16);
        strcpy(udp->server, server);
        udp->port = port;
        udp->probe_enabled = probe;
        udp->local_sequence = 0;
        udp->remote_sequence = 0;
        udp->last_pong_time = 0;
        udp->failed = false;
        esp_xiaozhi_udp_probe_init(&udp->probe, nonce + 4);
    }
    udp->configured = result == 0;
    xSemaphoreGive(udp->mutex);
    memset(key, 0, sizeof(key));
    return result ? ESP_FAIL : ESP_OK;
}

esp_err_t esp_xiaozhi_udp_open(esp_xiaozhi_udp_handle_t udp)
{
    if (!udp || !udp->configured) {
        return ESP_ERR_INVALID_STATE;
    }
    if (xTaskGetCurrentTaskHandle() == udp->task) {
        return ESP_ERR_INVALID_STATE;
    }
    struct addrinfo hints = {.ai_family = AF_UNSPEC, .ai_socktype = SOCK_DGRAM};
    struct addrinfo *address = NULL;
    char port[8];
    snprintf(port, sizeof(port), "%d", udp->port);
    if (getaddrinfo(udp->server, port, &hints, &address)) {
        return ESP_ERR_NOT_FOUND;
    }
    int socket_fd = socket(address->ai_family, address->ai_socktype, address->ai_protocol);
    bool connected = socket_fd >= 0 && !connect(socket_fd, address->ai_addr, address->ai_addrlen);
    freeaddrinfo(address);
    if (!connected) {
        if (socket_fd >= 0) {
            close(socket_fd);
        }
        return ESP_FAIL;
    }
    struct timeval receive_timeout = {.tv_sec = 0, .tv_usec = 100000};
    struct timeval send_timeout = {.tv_sec = 0, .tv_usec = 20000};
    if (setsockopt(socket_fd, SOL_SOCKET, SO_RCVTIMEO, &receive_timeout, sizeof(receive_timeout)) ||
            setsockopt(socket_fd, SOL_SOCKET, SO_SNDTIMEO, &send_timeout, sizeof(send_timeout))) {
        close(socket_fd);
        return ESP_FAIL;
    }
    xSemaphoreTake(udp->mutex, portMAX_DELAY);
    if (udp->socket >= 0 || udp->io_count) {
        xSemaphoreGive(udp->mutex);
        close(socket_fd);
        return ESP_ERR_INVALID_STATE;
    }
    udp->socket = socket_fd;
    udp->generation++;
    udp->active = true;
    udp->last_activity = esp_timer_get_time();
    if (xEventGroupGetBits(udp->events) & UDP_EXITED) {
        xEventGroupClearBits(udp->events, UDP_EXITED | UDP_STOP);
#if CONFIG_XIAOZHI_AUDIO_TASK_ALLOC_STATIC
        udp->task = xTaskCreateStaticPinnedToCore(udp_worker, "xiaozhi_udp", CONFIG_XIAOZHI_AUDIO_TASK_STACK_SIZE,
                                                  udp, CONFIG_XIAOZHI_AUDIO_TASK_PRIORITY, s_udp_task_stack, &s_udp_task_tcb,
                                                  CONFIG_XIAOZHI_AUDIO_TASK_CORE < 0 ? tskNO_AFFINITY : CONFIG_XIAOZHI_AUDIO_TASK_CORE);
#else
        UBaseType_t caps = MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT;
#if CONFIG_XIAOZHI_STACK_IN_PSRAM
        caps = MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT;
#endif
        BaseType_t core = CONFIG_XIAOZHI_AUDIO_TASK_CORE < 0 ? tskNO_AFFINITY : CONFIG_XIAOZHI_AUDIO_TASK_CORE;
        BaseType_t created = xTaskCreatePinnedToCoreWithCaps(udp_worker, "xiaozhi_udp",
                                                             CONFIG_XIAOZHI_AUDIO_TASK_STACK_SIZE, udp,
                                                             CONFIG_XIAOZHI_AUDIO_TASK_PRIORITY, &udp->task, core, caps);
#if CONFIG_XIAOZHI_STACK_IN_PSRAM
        if (created != pdPASS) {
            created = xTaskCreatePinnedToCoreWithCaps(udp_worker, "xiaozhi_udp",
                                                      CONFIG_XIAOZHI_AUDIO_TASK_STACK_SIZE, udp,
                                                      CONFIG_XIAOZHI_AUDIO_TASK_PRIORITY, &udp->task, core,
                                                      MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
        }
#endif
        if (created != pdPASS) {
            udp->task = NULL;
        }
#endif
        if (!udp->task) {
            close(udp->socket);
            udp->socket = -1;
            udp->active = false;
            xEventGroupSetBits(udp->events, UDP_EXITED);
            xSemaphoreGive(udp->mutex);
            return ESP_ERR_NO_MEM;
        }
    }
    xEventGroupSetBits(udp->events, UDP_ENABLED);
    xSemaphoreGive(udp->mutex);
    return ESP_OK;
}

esp_err_t esp_xiaozhi_udp_send(esp_xiaozhi_udp_handle_t udp, const uint8_t *data, size_t length, uint32_t timestamp)
{
    if (!udp || !data || !length || length > CONFIG_XIAOZHI_UDP_MAX_AUDIO_PAYLOAD_SIZE || length > UINT16_MAX) {
        return ESP_ERR_INVALID_ARG;
    }
    uint8_t *packet = malloc(length + 16);
    if (!packet) {
        return ESP_ERR_NO_MEM;
    }
    xSemaphoreTake(udp->mutex, portMAX_DELAY);
    if (!udp->active || udp->socket < 0 || udp->local_sequence == UINT32_MAX) {
        xSemaphoreGive(udp->mutex);
        free(packet);
        return ESP_ERR_INVALID_STATE;
    }
    memcpy(packet, udp->nonce, 16);
    packet[0] = 1;
    packet[2] = length >> 8;
    packet[3] = length;
    uint32_t value = htonl(timestamp);
    memcpy(packet + 8, &value, 4);
    value = htonl(++udp->local_sequence);
    memcpy(packet + 12, &value, 4);
    uint8_t nonce[16];
    memcpy(nonce, packet, 16);
    int error = esp_xiaozhi_chat_aes_ctr_crypt(&udp->aes, length, nonce, data, packet + 16);
    int socket_fd = udp->socket;
    uint32_t generation = udp->generation;
    udp_io_begin_locked(udp);
    xSemaphoreGive(udp->mutex);
    int sent = error ? -1 : send(socket_fd, packet, length + 16, 0);
    int send_errno = !error && sent < 0 ? errno : 0;
    free(packet);
    xSemaphoreTake(udp->mutex, portMAX_DELAY);
    bool current = generation == udp->generation && udp->active;
    udp_io_end_locked(udp);
    xSemaphoreGive(udp->mutex);
    if (current && send_errno) {
        ESP_LOGW(TAG, "UDP send failed: errno=%d (%s)", send_errno, strerror(send_errno));
        if (send_errno == ENOMEM || send_errno == EAGAIN || send_errno == EWOULDBLOCK) {
            vTaskDelay(pdMS_TO_TICKS(UDP_SEND_BACKPRESSURE_DELAY_MS));
        }
    }
    return current && sent == (int)length + 16 ? ESP_OK : ESP_FAIL;
}

bool esp_xiaozhi_udp_is_open(esp_xiaozhi_udp_handle_t udp)
{
    if (!udp) {
        return false;
    }
    xSemaphoreTake(udp->mutex, portMAX_DELAY);
    bool active = udp->active;
    xSemaphoreGive(udp->mutex);
    return active;
}

void esp_xiaozhi_udp_note_activity(esp_xiaozhi_udp_handle_t udp)
{
    if (udp) {
        xSemaphoreTake(udp->mutex, portMAX_DELAY);
        udp->last_activity = esp_timer_get_time();
        xSemaphoreGive(udp->mutex);
    }
}

esp_err_t esp_xiaozhi_udp_deinit(esp_xiaozhi_udp_handle_t udp)
{
    if (!udp) {
        return ESP_OK;
    }
    if (xTaskGetCurrentTaskHandle() == udp->task) {
        return ESP_ERR_INVALID_STATE;
    }
    xEventGroupSetBits(udp->events, UDP_STOP);
    esp_err_t ret = esp_xiaozhi_udp_close(udp);
    EventBits_t bits = xEventGroupWaitBits(udp->events, UDP_EXITED, pdFALSE, pdFALSE, pdMS_TO_TICKS(2000));
    if (ret != ESP_OK || !(bits & UDP_EXITED)) {
        return ESP_ERR_TIMEOUT;
    }
    esp_xiaozhi_chat_aes_ctr_free(&udp->aes);
    memset(udp->key, 0, sizeof(udp->key));
    vEventGroupDelete(udp->events);
    vSemaphoreDelete(udp->mutex);
    free(udp);
    return ESP_OK;
}

esp_err_t esp_xiaozhi_udp_get_health(esp_xiaozhi_udp_handle_t udp, esp_xiaozhi_udp_health_info_t *info)
{
    if (!udp || !info) {
        return ESP_ERR_INVALID_ARG;
    }
    xSemaphoreTake(udp->mutex, portMAX_DELAY);
    *info = (esp_xiaozhi_udp_health_info_t) {
        .state = udp->failed ? ESP_XIAOZHI_UDP_HEALTH_FAILED : !udp->active ? ESP_XIAOZHI_UDP_HEALTH_CLOSED :
                 !udp->probe_enabled ? ESP_XIAOZHI_UDP_HEALTH_LEGACY_UNVERIFIED :
                 udp->last_pong_time ? ESP_XIAOZHI_UDP_HEALTH_HEALTHY : ESP_XIAOZHI_UDP_HEALTH_PROBING,
                 .last_pong_time_us = udp->last_pong_time, .probe_sequence = udp->probe.sequence,
                 .attempts = udp->probe.attempts,
    };
    xSemaphoreGive(udp->mutex);
    return ESP_OK;
}
