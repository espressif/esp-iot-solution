/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"
#include "esp_gmp.h"
#include "esp_gmp_profile.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Base for component-specific esp_err_t values. */
#define ESP_ERR_FILE_TRANSFER_BASE        0x7500
#define ESP_FT_ERR_OPEN_FAILED            (ESP_ERR_FILE_TRANSFER_BASE + 0x01)
#define ESP_FT_ERR_REMOTE_NAME_INVALID    (ESP_ERR_FILE_TRANSFER_BASE + 0x02)
#define ESP_FT_ERR_BUSY                   (ESP_ERR_FILE_TRANSFER_BASE + 0x03)
#define ESP_FT_ERR_FILE_TOO_LARGE         (ESP_ERR_FILE_TRANSFER_BASE + 0x04)
#define ESP_FT_ERR_CAPABILITY_MISMATCH    (ESP_ERR_FILE_TRANSFER_BASE + 0x05)
#define ESP_FT_ERR_QUEUE_FULL             (ESP_ERR_FILE_TRANSFER_BASE + 0x06)

/** Local role in the current transfer. */
typedef enum {
    ESP_FILE_TRANSFER_ROLE_NONE = 0,
    ESP_FILE_TRANSFER_ROLE_SENDER,
    ESP_FILE_TRANSFER_ROLE_RECEIVER,
} esp_file_transfer_role_t;

/** File transfer event identifiers. */
typedef enum {
    ESP_FT_EVENT_STARTED = 0,
    ESP_FT_EVENT_META_SENT,
    ESP_FT_EVENT_META_RECEIVED,
    ESP_FT_EVENT_PEER_ACCEPTED,
    ESP_FT_EVENT_PEER_REJECTED,
    ESP_FT_EVENT_PROGRESS,
    ESP_FT_EVENT_VERIFYING,
    ESP_FT_EVENT_COMPLETED,
    ESP_FT_EVENT_FAILED,
    ESP_FT_EVENT_CANCELLED,
} esp_file_transfer_event_id_t;

/** Public transfer state snapshot. */
typedef enum {
    ESP_FILE_TRANSFER_STATE_IDLE = 0,
    ESP_FILE_TRANSFER_STATE_PREPARING,
    ESP_FILE_TRANSFER_STATE_WAIT_META_RSP,
    ESP_FILE_TRANSFER_STATE_WAIT_DATA_BLOCK,
    ESP_FILE_TRANSFER_STATE_SENDING_DATA,
    ESP_FILE_TRANSFER_STATE_WAIT_FINAL_CONFIRM,
    ESP_FILE_TRANSFER_STATE_FINALIZING,
    ESP_FILE_TRANSFER_STATE_ERROR,
} esp_file_transfer_state_t;

/** Event delivered to the application callback. */
typedef struct {
    esp_file_transfer_event_id_t event_id;
    esp_file_transfer_role_t role;
    const char *file_name;
    uint64_t bytes_transferred;
    uint64_t total_bytes;
    uint8_t percent;
    uint16_t reason_code;
    const char *saved_name;
    esp_err_t detail;
} esp_file_transfer_event_t;

/**
 * @brief Application event callback.
 *
 * The event and its string fields remain valid only for the duration of the
 * callback.
 */
typedef void (*esp_file_transfer_event_cb_t)(const esp_file_transfer_event_t *event,
                                             void *event_ctx);

/**
 * @brief Optional inbound-transfer accept hook.
 *
 * Called after META validation and before opening the receive file.
 * Return false to reject the transfer. NULL accept_cb means accept all.
 */
typedef bool (*esp_file_transfer_accept_cb_t)(const char *file_name, uint64_t file_size,
                                              const uint8_t sha256[32], void *ctx);

/** Component initialization configuration. */
typedef struct {
    const char *recv_dir;
    esp_gmp_link_t gmp_link;
    size_t max_file_size;
    size_t block_size;
    esp_file_transfer_event_cb_t event_cb;
    void *event_ctx;
    /**
     * Optional unified profile event callback.
     * At least one of @c event_cb or @c profile_event_cb must be set.
     */
    void (*profile_event_cb)(const esp_gmp_profile_event_t *event, void *ctx);
    void *profile_event_ctx;
    esp_file_transfer_accept_cb_t accept_cb; /* NULL = accept all */
    void *accept_ctx;
} esp_file_transfer_config_t;

/** Parameters for starting an outbound transfer. */
typedef struct {
    const char *src_path;
    const char *remote_name;
} esp_file_transfer_send_param_t;

/**
 * @brief Application-provided reader for stream sends.
 *
 * Must fill up to @p len bytes at @p offset into @p buf and return the actual
 * length via @p out_len. Return ESP_OK on success.
 */
typedef esp_err_t (*esp_file_transfer_read_fn_t)(void *ctx, size_t offset, uint8_t *buf,
                                                 size_t len, size_t *out_len);

/** Parameters for starting an outbound stream transfer (no filesystem path). */
typedef struct {
    esp_file_transfer_read_fn_t read_fn;
    void *read_ctx;
    uint64_t file_size;
    const char *remote_name;
    const uint8_t *sha256; /**< Optional precomputed digest; NULL = hash while reading. */
    esp_gmp_link_t link;   /**< NULL = use config gmp_link. */
} esp_file_transfer_send_stream_param_t;

/** Consistent snapshot returned by esp_file_transfer_get_status(). */
typedef struct {
    bool in_progress;
    esp_file_transfer_role_t role;
    esp_file_transfer_state_t state;
    char file_name[33];
    uint64_t bytes_transferred;
    uint64_t total_bytes;
    uint8_t percent;
    uint16_t reason_code;
} esp_file_transfer_status_t;

/**
 * @brief Initialize the singleton file transfer component.
 */
esp_err_t esp_file_transfer_init(const esp_file_transfer_config_t *config);

/**
 * @brief Deinitialize the singleton file transfer component.
 */
esp_err_t esp_file_transfer_deinit(void);

/**
 * @brief Start an asynchronous outbound file transfer.
 */
esp_err_t esp_file_transfer_send(const esp_file_transfer_send_param_t *param);

/**
 * @brief Start an asynchronous outbound stream transfer via read_fn.
 */
esp_err_t esp_file_transfer_send_stream(const esp_file_transfer_send_stream_param_t *param);

/**
 * @brief Cancel the active transfer.
 */
esp_err_t esp_file_transfer_abort(void);

/**
 * @brief Get the current transfer status snapshot.
 */
esp_err_t esp_file_transfer_get_status(esp_file_transfer_status_t *status);

/**
 * @brief Dispatch a GMP file transfer packet.
 *
 * This hook never takes ownership of pkt->frame_buf and always returns false.
 */
bool esp_file_transfer_on_packet(const esp_gmp_rx_t *pkt);

/**
 * @brief Notify the component before the configured GMP link is unregistered.
 */
void esp_file_transfer_on_link_down(esp_gmp_link_t link);

/**
 * @brief Notify the component of an unrecoverable transport error.
 */
void esp_file_transfer_on_transport_error(esp_gmp_link_t link, esp_err_t error);

/* Unified esp_gmp_ft_* aliases (profile naming). */
static inline esp_err_t esp_gmp_ft_init(const esp_file_transfer_config_t *config)
{
    return esp_file_transfer_init(config);
}

static inline esp_err_t esp_gmp_ft_deinit(void)
{
    return esp_file_transfer_deinit();
}

static inline esp_err_t esp_gmp_ft_send(const esp_file_transfer_send_param_t *param)
{
    return esp_file_transfer_send(param);
}

static inline esp_err_t esp_gmp_ft_send_stream(const esp_file_transfer_send_stream_param_t *param)
{
    return esp_file_transfer_send_stream(param);
}

static inline esp_err_t esp_gmp_ft_abort(void)
{
    return esp_file_transfer_abort();
}

static inline esp_err_t esp_gmp_ft_get_status(esp_file_transfer_status_t *status)
{
    return esp_file_transfer_get_status(status);
}

#ifdef __cplusplus
}
#endif
