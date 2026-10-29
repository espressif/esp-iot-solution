/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif
#include <sys/queue.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/event_groups.h"

typedef struct at_parser_s *at_handle_t;

/**
 * @brief Function to handle line response
 *
 * @param at_handle AT parser handle
 * @param line line response
 *
 * @return true if the corresponding command is handled
 * @return false if not handled, need to continue processing
 */
typedef bool (*handle_line_func_t)(at_handle_t at_handle, const char *line);

typedef enum {
    AT_STATE_IDLE,
    AT_STATE_PROCESSING,
} at_state_t;

typedef struct {
    int (*send_cmd)(const char *cmd_str, size_t cmd_len, void *usr_data); /* send command callback (Required) */
    void *usr_data;
} at_command_io_interface_t;

typedef struct {
    size_t send_buffer_length;            /* buffer size for sending AT command */
    size_t recv_buffer_length;            /* buffer size for receiving AT response */
    const at_command_io_interface_t io;   /* IO interface */
    bool skip_first_line;                 /*!< Whether to skip the first line of response */
} modem_at_config_t;

/**
 * @brief Create AT parser
 *
 * This function creates and initializes an AT parser instance with the given configuration.
 * The parser will allocate necessary buffers and synchronization primitives.
 *
 * @param cfg Pointer to the AT parser configuration structure containing:
 *            - send_buffer_length: Size of the command sending buffer (must be > 32 bytes)
 *            - recv_buffer_length: Size of the response receiving buffer (must be > 32 bytes)
 *            - io: Structure containing IO interface callbacks and user data
 *
 * @return at_handle_t
 *         - AT parser handle on success
 *         - NULL if failed due to:
 *           * Invalid configuration parameters
 *           * Memory allocation failure
 *           * Missing required IO interface functions
 */
at_handle_t modem_at_parser_create(const modem_at_config_t *cfg);

/**
 * @brief Destroy AT parser
 *
 * This function cleans up and releases all resources associated with an AT parser instance.
 * The parser must be in stopped state before calling this function.
 *
 * @param at_handle AT parser handle to destroy
 *
 * @return esp_err_t
 *         - ESP_OK on success
 *         - ESP_ERR_INVALID_ARG if at_handle is NULL
 *         - ESP_ERR_INVALID_STATE if AT parser is not stopped
 */
esp_err_t modem_at_parser_destroy(at_handle_t at_handle);

/**
 * @brief Send AT command
 *
 * This function sends an AT command and waits for the response. The function will:
 * - Acquire command lock
 * - Format the command (add \r\n if not "+++")
 * - Call pre_send callback if provided
 * - Send the command using the configured IO interface
 * - Wait for response or timeout
 * - Call post_send callback if provided
 *
 * @param at_handle AT parser handle
 * @param command AT command string to send
 * @param timeout_ms Timeout in milliseconds for response
 * @param handle_line Callback function to handle response lines
 * @param ctx Context pointer passed to handle_line callback
 *
 * @return esp_err_t
 *         - ESP_OK on success
 *         - ESP_ERR_INVALID_ARG if invalid parameters
 *         - ESP_ERR_INVALID_STATE if parser not idle or stopped
 *         - ESP_ERR_TIMEOUT if command response timeout
 *         - ESP_ERR_NOT_FINISHED if command not finished due to stop
 */
esp_err_t modem_at_send_command(at_handle_t at_handle, const char *command, uint32_t timeout_ms, handle_line_func_t handle_line, void *ctx);

/**
 * @brief Start AT parser
 *
 * This function starts the AT parser by clearing the stopped bit,
 * allowing commands to be processed.
 *
 * @param at_handle AT parser handle
 *
 * @return esp_err_t
 *         - ESP_OK on success
 *         - ESP_ERR_INVALID_ARG if at_handle is NULL
 */
esp_err_t modem_at_start(at_handle_t at_handle);

/**
 * @brief Stop AT parser
 *
 * This function stops the AT parser by setting the stopped bit,
 * preventing new commands from being processed.
 *
 * @param at_handle AT parser handle
 *
 * @return esp_err_t
 *         - ESP_OK on success
 *         - ESP_ERR_INVALID_ARG if at_handle is NULL
 */
esp_err_t modem_at_stop(at_handle_t at_handle);

/**
 * @brief Get response buffer
 *
 * This function provides access to the receive buffer for writing response data.
 * The function returns a pointer to the current write position and remaining space.
 *
 * @param at_handle AT parser handle
 * @param buf Pointer to store the buffer address
 * @param remain_len Pointer to store the remaining buffer length
 *
 * @return esp_err_t
 *         - ESP_OK on success
 *         - ESP_ERR_INVALID_ARG if any parameter is NULL
 */
esp_err_t modem_at_get_response_buffer(at_handle_t at_handle, char **buf, size_t *remain_len);

/**
 * @brief Mark response data as written
 *
 * This function should be called after writing response data to indicate
 * that the data is ready for processing. It will:
 * - Check buffer integrity
 * - Parse the received data
 * - Call the line handler if any response is received. The modem may split the response data into several small blocks and
 *       send them in batches, so this function may be called multiple times in one AT command interaction.
 *
 * @param at_handle AT parser handle
 * @param write_len Length of data written to the buffer
 *
 * @return esp_err_t
 *         - ESP_OK if response was successfully processed
 *         - ESP_ERR_INVALID_ARG if at_handle is NULL
 *         - ESP_ERR_INVALID_STATE if parser is stopped
 *         - ESP_ERR_INVALID_SIZE if buffer overflow would occur
 */
esp_err_t modem_at_write_response_done(at_handle_t at_handle, size_t write_len);

/**
 * @brief Get handle line context
 *
 * This function returns the context pointer that was passed to the
 * current handle_line callback.
 *
 * @param at_handle AT parser handle
 *
 * @return void*
 *         - Context pointer on success
 *         - NULL if at_handle is NULL or no context is set
 */
void *modem_at_get_handle_line_ctx(at_handle_t at_handle);

/**
 * @brief Get current command
 *
 * This function returns the current command string being processed.
 *
 * @param at_handle AT parser handle
 *
 * @return const char*
 *         - Command string on success
 *         - NULL if at_handle is NULL
 */
const char *modem_at_get_current_cmd(at_handle_t at_handle);

#ifdef __cplusplus
}
#endif
