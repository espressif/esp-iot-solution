/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include "esp_log.h"
#include "esp_check.h"
#include "modem_at_parser.h"

static const char *TAG = "Modem AT";

#define AT_COMMAND_RSP_BIT BIT(0)
#define AT_COMMAND_STOPPED_BIT BIT(1)

#define BUFFER_END_CHECK_LEN 4
#define BUFFER_END_CHECK_MARKER 0x5a1221a5

typedef struct at_parser_s {
    at_command_io_interface_t io;
    char *send_buffer;                        /*!< Internal buffer for command sending */
    char *recv_buffer;                        /*!< Internal buffer to store response lines/data from DCE */
    size_t send_buffer_length;                /*!< Length of the send buffer */
    size_t recv_buffer_length;                /*!< Length of the receive buffer */
    size_t received_length;              /*!< Length of the received data in the buffer */
    handle_line_func_t handle_line; /*!< Function to handle line response */
    void *handle_line_ctx;           /*!< Context for handle_line function */
    SemaphoreHandle_t send_cmd_lock;         /*!< Mutex for send command */
    EventGroupHandle_t process_event_group;       /*!< Event group used for indicating DTE state changes */
    bool skip_first_line;   /*!< Whether to skip the first line of response */
    at_state_t state;
} at_parser_t;

at_handle_t modem_at_parser_create(const modem_at_config_t *cfg)
{
    void *ret = NULL;
    ESP_RETURN_ON_FALSE(cfg != NULL, NULL, TAG, "cfg is NULL");
    ESP_RETURN_ON_FALSE(cfg->send_buffer_length > 32, NULL, TAG, "send_buffer_length is too small");
    ESP_RETURN_ON_FALSE(cfg->recv_buffer_length > 32, NULL, TAG, "recv_buffer_length is too small");
    ESP_RETURN_ON_FALSE(cfg->io.send_cmd != NULL, NULL, TAG, "io send is NULL");

    at_parser_t *at_hdl = calloc(1, sizeof(at_parser_t));
    ESP_RETURN_ON_FALSE(at_hdl != NULL, NULL, TAG, "Failed to allocate memory for at_handle");

    /* malloc memory for receive and send buffer */
    at_hdl->recv_buffer_length = cfg->recv_buffer_length;
    at_hdl->recv_buffer = calloc(1, at_hdl->recv_buffer_length + BUFFER_END_CHECK_LEN); // Add extra space for overflow detection
    ESP_GOTO_ON_FALSE(at_hdl->recv_buffer != NULL, NULL, err, TAG, "calloc line memory failed");
    at_hdl->received_length = 0;
    uint32_t *end_checker = (uint32_t *)(at_hdl->recv_buffer + at_hdl->recv_buffer_length);
    *end_checker = BUFFER_END_CHECK_MARKER; // Add a marker to the end of the buffer to detect overflow

    at_hdl->send_buffer_length = cfg->send_buffer_length;
    at_hdl->send_buffer = calloc(1, cfg->send_buffer_length);
    ESP_GOTO_ON_FALSE(at_hdl->send_buffer != NULL, NULL, err, TAG, "Failed to allocate memory for command buffer");

    /* initialize linked list */
    at_hdl->send_cmd_lock = xSemaphoreCreateMutex();
    ESP_GOTO_ON_FALSE(at_hdl->send_cmd_lock != NULL, NULL, err, TAG, "Failed to create mutex for send command");
    at_hdl->process_event_group = xEventGroupCreate();
    ESP_GOTO_ON_FALSE(at_hdl->process_event_group != NULL, NULL, err, TAG, "Failed to create event group");
    at_hdl->io = (cfg->io);
    at_hdl->state = AT_STATE_IDLE;
    at_hdl->skip_first_line = cfg->skip_first_line;
    xEventGroupSetBits(at_hdl->process_event_group, AT_COMMAND_STOPPED_BIT);
    return (at_handle_t)at_hdl;
err:
    if (at_hdl->send_cmd_lock != NULL) {
        vSemaphoreDelete(at_hdl->send_cmd_lock);
    }
    if (at_hdl->process_event_group != NULL) {
        vEventGroupDelete(at_hdl->process_event_group);
    }
    if (at_hdl->send_buffer != NULL) {
        free(at_hdl->send_buffer);
    }
    if (at_hdl->recv_buffer != NULL) {
        free(at_hdl->recv_buffer);
    }
    free(at_hdl);
    return ret;
}

esp_err_t modem_at_parser_destroy(at_handle_t at_handle)
{
    ESP_RETURN_ON_FALSE(at_handle != NULL, ESP_ERR_INVALID_ARG, TAG, "at_handle is NULL");
    at_parser_t *at_hdl = (at_parser_t *)at_handle;
    ESP_RETURN_ON_FALSE((AT_COMMAND_STOPPED_BIT & xEventGroupGetBits(at_hdl->process_event_group)), ESP_ERR_INVALID_STATE, TAG, "AT parser is not stopped");

    xSemaphoreTake(at_hdl->send_cmd_lock, portMAX_DELAY); // wait for send command to finish
    vSemaphoreDelete(at_hdl->send_cmd_lock);
    vEventGroupDelete(at_hdl->process_event_group);
    free(at_hdl->send_buffer);
    free(at_hdl->recv_buffer);
    free(at_hdl);
    return ESP_OK;
}

esp_err_t modem_at_start(at_handle_t at_handle)
{
    ESP_RETURN_ON_FALSE(at_handle != NULL, ESP_ERR_INVALID_ARG, TAG, "at_handle is NULL");
    at_parser_t *at_hdl = (at_parser_t *)at_handle;
    xEventGroupClearBits(at_hdl->process_event_group, AT_COMMAND_STOPPED_BIT);
    return ESP_OK;
}

esp_err_t modem_at_stop(at_handle_t at_handle)
{
    ESP_RETURN_ON_FALSE(at_handle != NULL, ESP_ERR_INVALID_ARG, TAG, "at_handle is NULL");
    at_parser_t *at_hdl = (at_parser_t *)at_handle;
    xEventGroupSetBits(at_hdl->process_event_group, AT_COMMAND_STOPPED_BIT);
    return ESP_OK;
}

esp_err_t modem_at_send_command(at_handle_t at_handle, const char *command, uint32_t timeout_ms, handle_line_func_t handle_line, void *ctx)
{
    esp_err_t ret = ESP_OK;
    ESP_RETURN_ON_FALSE(at_handle != NULL, ESP_ERR_INVALID_ARG, TAG, "at_handle is NULL");

    at_parser_t *at_hdl = (at_parser_t *)at_handle;
    xSemaphoreTake(at_hdl->send_cmd_lock, portMAX_DELAY);
    ESP_GOTO_ON_FALSE(at_hdl->state == AT_STATE_IDLE, ESP_ERR_INVALID_STATE, err, TAG, "AT parser is not in idle state");
    ESP_GOTO_ON_FALSE(handle_line != NULL, ESP_ERR_INVALID_STATE, err, TAG, "AT parser handle_line is NULL");
    ESP_GOTO_ON_FALSE(!(AT_COMMAND_STOPPED_BIT & xEventGroupGetBits(at_hdl->process_event_group)), ESP_ERR_INVALID_STATE, err, TAG, "AT parser is stopped");
    size_t len = strlen(command);
    ESP_GOTO_ON_FALSE(len < at_hdl->send_buffer_length - 3, ESP_ERR_INVALID_ARG, err, TAG, "command length is too long"); // reserve space for "\r\n" and null terminator
    at_hdl->state = AT_STATE_PROCESSING;
    at_hdl->handle_line = handle_line;
    at_hdl->handle_line_ctx = ctx;
    at_hdl->received_length = 0; // clear buffer
    strcpy(at_hdl->send_buffer, command);
    bool has_crlf = false;
    if (strcmp(at_hdl->send_buffer, "+++") != 0) { // exit command mode
        strcat(at_hdl->send_buffer, "\r\n"); // add cmd endstr only if not exit command
        has_crlf = true;
        len += 2;
    }
#if CONFIG_MODEM_AT_DEBUG_HEXDUMP
    ESP_LOG_BUFFER_HEXDUMP("at-send", at_hdl->send_buffer, len, ESP_LOG_INFO);
#endif

    ret = at_hdl->io.send_cmd(at_hdl->send_buffer, len, at_hdl->io.usr_data);
    ESP_GOTO_ON_ERROR(ret, err, TAG, "Failed to send command");

    EventBits_t bits = xEventGroupWaitBits(at_hdl->process_event_group, AT_COMMAND_RSP_BIT | AT_COMMAND_STOPPED_BIT,
                                           pdFALSE, pdFALSE, timeout_ms == portMAX_DELAY ? portMAX_DELAY : pdMS_TO_TICKS(timeout_ms));
    xEventGroupClearBits(at_hdl->process_event_group, AT_COMMAND_RSP_BIT);

    if (has_crlf) {
        at_hdl->send_buffer[len - 2] = '\0'; // remove "\r\n" for pretty print
    }
    ESP_GOTO_ON_FALSE(bits & AT_COMMAND_RSP_BIT, ESP_ERR_TIMEOUT, err, TAG, "command \"%s\" response timeout", at_hdl->send_buffer);
    ESP_GOTO_ON_FALSE(!(bits & AT_COMMAND_STOPPED_BIT), ESP_ERR_NOT_FINISHED, err, TAG, "command \"%s\" not finished due to stopped", at_hdl->send_buffer);
err:
    at_hdl->state = AT_STATE_IDLE;
    at_hdl->handle_line = NULL;
    at_hdl->received_length = 0; // clear buffer
    xSemaphoreGive(at_hdl->send_cmd_lock);
    return ret;
}

esp_err_t modem_at_get_response_buffer(at_handle_t at_handle, char **buf, size_t *remain_len)
{
    *remain_len = 0; // clear remain_len
    at_parser_t *at_hdl = (at_parser_t *)at_handle;
    ESP_RETURN_ON_FALSE(at_handle != NULL, ESP_ERR_INVALID_ARG, TAG, "at_handle is NULL");
    ESP_RETURN_ON_FALSE(remain_len != NULL, ESP_ERR_INVALID_ARG, TAG, "remain_len is NULL");
    ESP_RETURN_ON_FALSE(buf != NULL, ESP_ERR_INVALID_ARG, TAG, "buf pointer is NULL");
    *buf = at_hdl->recv_buffer + at_hdl->received_length;
    *remain_len = at_hdl->recv_buffer_length - at_hdl->received_length;
    return ESP_OK;
}

esp_err_t modem_at_write_response_done(at_handle_t at_handle, size_t write_len)
{
    at_parser_t *at_hdl = (at_parser_t *)at_handle;
    size_t total_len = write_len + at_hdl->received_length;
    ESP_RETURN_ON_FALSE(at_handle != NULL, ESP_ERR_INVALID_ARG, TAG, "at_handle is NULL");
    ESP_RETURN_ON_FALSE(!(AT_COMMAND_STOPPED_BIT & xEventGroupGetBits(at_hdl->process_event_group)), ESP_ERR_INVALID_STATE, TAG, "AT parser is stopped");
#if CONFIG_MODEM_AT_DEBUG_HEXDUMP
    ESP_LOG_BUFFER_HEXDUMP("at-response: ", at_hdl->recv_buffer, total_len, ESP_LOG_INFO);
#endif
    uint32_t *end_checker = (uint32_t *)(at_hdl->recv_buffer + at_hdl->recv_buffer_length);
    if (*end_checker != BUFFER_END_CHECK_MARKER) {
        ESP_LOGE(TAG, "!!! AT parser buffer is corrupted !!!. Force to stop");
        modem_at_stop(at_handle);
        return ESP_ERR_INVALID_STATE;
    }

    ESP_RETURN_ON_FALSE((total_len <= at_hdl->recv_buffer_length), ESP_ERR_INVALID_SIZE, TAG, "AT parser buffer is full");
    at_hdl->received_length = total_len;
    at_hdl->recv_buffer[at_hdl->received_length] = '\0'; // force add null terminator

    // Start to parse the buffer
    esp_err_t ret = ESP_FAIL;
    size_t len = at_hdl->received_length;
    char *lines = at_hdl->recv_buffer;
    /* check end of "\r\n" lines */
    if (len > 4 && (strcmp(&lines[len - 2], "\r\n") == 0)) {
        if (at_hdl->handle_line == NULL) {
            ESP_LOGW(TAG, "AT parser handle_line is NULL but receive: \"%s\"", lines);
            return ESP_ERR_INVALID_STATE;
        }
        if (at_hdl->skip_first_line) {
            lines = strstr(lines, "\r\n") + 2; // skip the first echo line
        }
        if (at_hdl->handle_line(at_handle, lines) == true) {
            xEventGroupSetBits(at_hdl->process_event_group, AT_COMMAND_RSP_BIT);
            ret = ESP_OK;
        }
    }
    return ret;
}

const char *modem_at_get_current_cmd(at_handle_t at_handle)
{
    ESP_RETURN_ON_FALSE(at_handle != NULL, NULL, TAG, "at_handle is NULL");
    at_parser_t *at_hdl = (at_parser_t *)at_handle;
    ESP_RETURN_ON_FALSE(at_hdl->state == AT_STATE_PROCESSING, NULL, TAG, "AT parser is not processing");
    return at_hdl->send_buffer;
}

void *modem_at_get_handle_line_ctx(at_handle_t at_handle)
{
    ESP_RETURN_ON_FALSE(at_handle != NULL, NULL, TAG, "at_handle is NULL");
    at_parser_t *at_hdl = (at_parser_t *)at_handle;
    ESP_RETURN_ON_FALSE(at_hdl->handle_line_ctx != NULL, NULL, TAG, "AT parser handle_line_ctx is NULL");
    return at_hdl->handle_line_ctx;
}
