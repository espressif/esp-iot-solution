/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>

#include <esp_err.h>
#include <esp_log.h>
#include <esp_random.h>
#include <esp_idf_version.h>
#include <driver/uart.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include <protocomm_ext_transports.h>
#include <protocomm_ext_console.h>

static const char *TAG = "protocomm_ext_transport_console";

#define CONSOLE_DEFAULT_BAUD_RATE       115200
#define CONSOLE_DEFAULT_TIMEOUT_MS      5000
#define CONSOLE_UART_RX_BUF_SIZE        4096
#define CONSOLE_UART_TX_BUF_SIZE        2048
#define CONSOLE_MAX_RESP_HEX_LEN        8192  /* max hex chars (~4KB binary) */
#define CONSOLE_READ_CHUNK_SIZE         256

typedef struct {
    protocomm_ext_console_config_t config;
    uint32_t session_id;
    bool driver_installed_by_us;
    bool connected;
} protocomm_console_t;

static bool is_hex_char(char c)
{
    return ((c >= '0' && c <= '9') ||
            (c >= 'a' && c <= 'f') ||
            (c >= 'A' && c <= 'F'));
}

/**
 * Return true if @p line is empty or a pure even-length hex string.
 * @p is_empty is set when the line has zero length after trim.
 */
static bool is_hex_line(const char *line, size_t len, bool *is_empty)
{
    while (len > 0 && (line[0] == ' ' || line[0] == '\t')) {
        line++;
        len--;
    }
    while (len > 0 && (line[len - 1] == ' ' || line[len - 1] == '\t')) {
        len--;
    }

    if (is_empty) {
        *is_empty = (len == 0);
    }
    if (len == 0) {
        return true;
    }
    if (len & 1) {
        return false;
    }
    for (size_t i = 0; i < len; i++) {
        if (!is_hex_char(line[i])) {
            return false;
        }
    }
    return true;
}

static bool line_is_prompt(const char *line, size_t len)
{
    while (len > 0 && (line[0] == ' ' || line[0] == '\t')) {
        line++;
        len--;
    }
    if (len >= 2 && line[0] == '>' && line[1] == '>') {
        return true;
    }
    return false;
}

static void bin2hex(const uint8_t *bin, size_t bin_len, char *hex_out)
{
    static const char hex_digits[] = "0123456789abcdef";
    for (size_t i = 0; i < bin_len; i++) {
        hex_out[i * 2] = hex_digits[(bin[i] >> 4) & 0x0f];
        hex_out[i * 2 + 1] = hex_digits[bin[i] & 0x0f];
    }
    hex_out[bin_len * 2] = '\0';
}

static ssize_t hex2bin(const char *hexstr, size_t hex_len, uint8_t *bytes)
{
    if (hex_len & 1) {
        return -1;
    }
    ssize_t bytes_len = (ssize_t)(hex_len / 2);
    for (ssize_t i = 0; i < bytes_len; i++) {
        unsigned int byte = 0;
        if (sscanf(hexstr + (i * 2), "%2x", &byte) != 1) {
            return -1;
        }
        bytes[i] = (uint8_t)byte;
    }
    return bytes_len;
}

static void console_drain_rx(protocomm_console_t *console)
{
    uint8_t discard[64];
    while (uart_read_bytes(console->config.uart_num, discard, sizeof(discard), 0) > 0) {
        /* discard drain */
    }
    uart_flush_input(console->config.uart_num);
}

/**
 * Scan accumulated RX text for a hex response line.
 * Ignores prompts and non-hex lines (e.g. command echo).
 * Prefer a non-empty hex line; if a prompt appears after noise with no
 * non-empty hex, treat as empty success (outlen == 0 on server).
 *
 * @return 1 found (or empty OK), 0 need more data, -1 overflow/error
 */
static int try_extract_hex_response(const char *buf, size_t buf_len,
                                    char *hex_out, size_t hex_out_size,
                                    size_t *hex_len_out, bool *empty_ok)
{
    size_t line_start = 0;
    bool saw_non_hex = false;
    bool saw_prompt_after_noise = false;

    *hex_len_out = 0;
    *empty_ok = false;

    for (size_t i = 0; i <= buf_len; i++) {
        bool at_end = (i == buf_len);
        bool is_eol = (!at_end && (buf[i] == '\n' || buf[i] == '\r'));

        if (!at_end && !is_eol) {
            continue;
        }

        size_t line_len = i - line_start;
        const char *line = buf + line_start;

        if (line_len > 0 || is_eol) {
            if (line_is_prompt(line, line_len)) {
                if (saw_non_hex) {
                    saw_prompt_after_noise = true;
                }
            } else {
                bool is_empty = false;
                if (is_hex_line(line, line_len, &is_empty)) {
                    if (!is_empty) {
                        /* trim and copy */
                        while (line_len > 0 && (line[0] == ' ' || line[0] == '\t')) {
                            line++;
                            line_len--;
                        }
                        while (line_len > 0 && (line[line_len - 1] == ' ' || line[line_len - 1] == '\t')) {
                            line_len--;
                        }
                        if (line_len + 1 > hex_out_size) {
                            ESP_LOGE(TAG, "Response hex exceeds max (%u)", (unsigned)CONSOLE_MAX_RESP_HEX_LEN);
                            return -1;
                        }
                        memcpy(hex_out, line, line_len);
                        hex_out[line_len] = '\0';
                        *hex_len_out = line_len;
                        return 1;
                    }
                } else if (line_len > 0) {
                    saw_non_hex = true;
                }
            }
        }

        if (at_end) {
            break;
        }
        /* skip contiguous CR/LF */
        while (i + 1 < buf_len && (buf[i + 1] == '\n' || buf[i + 1] == '\r')) {
            i++;
        }
        line_start = i + 1;
    }

    if (saw_prompt_after_noise) {
        *empty_ok = true;
        return 1;
    }
    return 0;
}

static esp_err_t console_init(protocomm_ext_transport_handle_t *handle, const void *config)
{
    if (!handle || !config) {
        ESP_LOGE(TAG, "Invalid arguments");
        return ESP_ERR_INVALID_ARG;
    }

    const protocomm_ext_console_config_t *cfg = (const protocomm_ext_console_config_t *)config;
    protocomm_console_t *console = (protocomm_console_t *)calloc(1, sizeof(protocomm_console_t));
    if (!console) {
        ESP_LOGE(TAG, "Failed to allocate console instance");
        return ESP_ERR_NO_MEM;
    }

    console->config = *cfg;
    if (console->config.baud_rate == 0) {
        console->config.baud_rate = CONSOLE_DEFAULT_BAUD_RATE;
    }
    if (console->config.timeout_ms == 0) {
        console->config.timeout_ms = CONSOLE_DEFAULT_TIMEOUT_MS;
    }

    if (uart_is_driver_installed(console->config.uart_num)) {
        /*
         * Reuse already-installed UART driver; baud/pins are left as configured
         * by the previous owner. We will not call uart_driver_delete on deinit.
         */
        ESP_LOGI(TAG, "Reusing installed UART driver on port %d", console->config.uart_num);
        console->driver_installed_by_us = false;
    } else {
        uart_config_t uart_config = {
            .baud_rate = console->config.baud_rate,
            .data_bits = UART_DATA_8_BITS,
            .parity = UART_PARITY_DISABLE,
            .stop_bits = UART_STOP_BITS_1,
            .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
            .source_clk = UART_SCLK_DEFAULT,
#endif
        };

        esp_err_t ret = uart_param_config(console->config.uart_num, &uart_config);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "uart_param_config failed: %s", esp_err_to_name(ret));
            free(console);
            return ret;
        }

        ret = uart_set_pin(console->config.uart_num,
                           console->config.tx_io_num,
                           console->config.rx_io_num,
                           UART_PIN_NO_CHANGE,
                           UART_PIN_NO_CHANGE);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "uart_set_pin failed: %s", esp_err_to_name(ret));
            free(console);
            return ret;
        }

        ret = uart_driver_install(console->config.uart_num,
                                  CONSOLE_UART_RX_BUF_SIZE,
                                  CONSOLE_UART_TX_BUF_SIZE,
                                  0, NULL, 0);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "uart_driver_install failed: %s", esp_err_to_name(ret));
            free(console);
            return ret;
        }
        console->driver_installed_by_us = true;
        ESP_LOGI(TAG, "UART driver installed on port %d @ %d baud",
                 console->config.uart_num, console->config.baud_rate);
    }

    *handle = (protocomm_ext_transport_handle_t)console;
    ESP_LOGI(TAG, "Console transport initialized");
    return ESP_OK;
}

static esp_err_t console_deinit(protocomm_ext_transport_handle_t handle)
{
    if (!handle) {
        ESP_LOGE(TAG, "Invalid handle");
        return ESP_ERR_INVALID_ARG;
    }

    protocomm_console_t *console = (protocomm_console_t *)handle;
    console->connected = false;

    if (console->driver_installed_by_us) {
        esp_err_t ret = uart_driver_delete(console->config.uart_num);
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "uart_driver_delete failed: %s", esp_err_to_name(ret));
        }
        console->driver_installed_by_us = false;
    }

    free(console);
    ESP_LOGI(TAG, "Console transport cleaned up");
    return ESP_OK;
}

static esp_err_t console_connect(protocomm_ext_transport_handle_t handle, const void *config)
{
    (void)config;

    if (!handle) {
        ESP_LOGE(TAG, "Invalid handle");
        return ESP_ERR_INVALID_ARG;
    }

    protocomm_console_t *console = (protocomm_console_t *)handle;

    /* Generate a non-zero session id once per connect */
    do {
        console->session_id = esp_random();
    } while (console->session_id == 0);

    console_drain_rx(console);
    console->connected = true;

    ESP_LOGI(TAG, "Console connect, session_id=%u", (unsigned)console->session_id);
    return ESP_OK;
}

static esp_err_t console_disconnect(protocomm_ext_transport_handle_t handle)
{
    if (!handle) {
        ESP_LOGE(TAG, "Invalid handle");
        return ESP_ERR_INVALID_ARG;
    }

    protocomm_console_t *console = (protocomm_console_t *)handle;
    uart_wait_tx_done(console->config.uart_num, pdMS_TO_TICKS(console->config.timeout_ms));
    console->connected = false;

    ESP_LOGI(TAG, "Disconnected from console service");
    return ESP_OK;
}

static esp_err_t console_send_data(protocomm_ext_transport_handle_t handle,
                                   const char *ep_name,
                                   const uint8_t *data, ssize_t data_len,
                                   uint8_t **out_data, ssize_t *out_data_len)
{
    if (!handle || !ep_name || data_len < 0 || (data_len > 0 && !data) || !out_data || !out_data_len) {
        ESP_LOGE(TAG, "Invalid arguments");
        return ESP_ERR_INVALID_ARG;
    }

    protocomm_console_t *console = (protocomm_console_t *)handle;
    if (!console->connected) {
        ESP_LOGE(TAG, "Console transport not connected");
        return ESP_ERR_INVALID_STATE;
    }

    *out_data = NULL;
    *out_data_len = 0;

    char *hex_payload = (char *)malloc((size_t)data_len * 2 + 1);
    if (!hex_payload) {
        ESP_LOGE(TAG, "Failed to allocate hex payload");
        return ESP_ERR_NO_MEM;
    }
    bin2hex(data, (size_t)data_len, hex_payload);

    char *cmd = NULL;
    int cmd_len = asprintf(&cmd, "%s %u %s\r", ep_name, (unsigned)console->session_id, hex_payload);
    free(hex_payload);
    if (cmd_len < 0 || !cmd) {
        ESP_LOGE(TAG, "Failed to allocate command");
        return ESP_ERR_NO_MEM;
    }

    ESP_LOGD(TAG, "TX cmd len=%d ep=%s session=%u", cmd_len, ep_name, (unsigned)console->session_id);

    int written = uart_write_bytes(console->config.uart_num, cmd, cmd_len);
    free(cmd);
    if (written != cmd_len) {
        ESP_LOGE(TAG, "uart_write_bytes failed (%d/%d)", written, cmd_len);
        return ESP_FAIL;
    }
    uart_wait_tx_done(console->config.uart_num, pdMS_TO_TICKS(console->config.timeout_ms));

    /* Collect RX until hex response, empty OK (prompt), or timeout */
    const size_t rx_cap = CONSOLE_MAX_RESP_HEX_LEN * 2;
    char *rx_buf = (char *)malloc(rx_cap + 1);
    char *hex_resp = (char *)malloc(CONSOLE_MAX_RESP_HEX_LEN + 1);
    if (!rx_buf || !hex_resp) {
        free(rx_buf);
        free(hex_resp);
        ESP_LOGE(TAG, "Failed to allocate RX buffers");
        return ESP_ERR_NO_MEM;
    }

    size_t rx_len = 0;
    TickType_t start = xTaskGetTickCount();
    TickType_t timeout_ticks = pdMS_TO_TICKS(console->config.timeout_ms);
    if (timeout_ticks == 0) {
        timeout_ticks = 1;
    }

    esp_err_t ret = ESP_ERR_TIMEOUT;
    uint8_t chunk[CONSOLE_READ_CHUNK_SIZE];

    while ((xTaskGetTickCount() - start) < timeout_ticks) {
        TickType_t elapsed = xTaskGetTickCount() - start;
        TickType_t remaining = (elapsed < timeout_ticks) ? (timeout_ticks - elapsed) : 0;
        if (remaining == 0) {
            break;
        }

        int n = uart_read_bytes(console->config.uart_num, chunk, sizeof(chunk), remaining);
        if (n > 0) {
            if (rx_len + (size_t)n > rx_cap) {
                ESP_LOGE(TAG, "RX buffer overflow");
                ret = ESP_ERR_NO_MEM;
                break;
            }
            memcpy(rx_buf + rx_len, chunk, (size_t)n);
            rx_len += (size_t)n;
            rx_buf[rx_len] = '\0';

            size_t hex_len = 0;
            bool empty_ok = false;
            int found = try_extract_hex_response(rx_buf, rx_len, hex_resp,
                                                 CONSOLE_MAX_RESP_HEX_LEN + 1,
                                                 &hex_len, &empty_ok);
            if (found < 0) {
                ret = ESP_ERR_INVALID_SIZE;
                break;
            }
            if (found > 0) {
                if (empty_ok || hex_len == 0) {
                    *out_data = NULL;
                    *out_data_len = 0;
                    ret = ESP_OK;
                    break;
                }
                ssize_t bin_len = (ssize_t)(hex_len / 2);
                uint8_t *bin = (uint8_t *)malloc((size_t)bin_len);
                if (!bin) {
                    ESP_LOGE(TAG, "Failed to allocate response binary");
                    ret = ESP_ERR_NO_MEM;
                    break;
                }
                ssize_t decoded = hex2bin(hex_resp, hex_len, bin);
                if (decoded < 0) {
                    free(bin);
                    ESP_LOGE(TAG, "Failed to decode hex response");
                    ret = ESP_FAIL;
                    break;
                }
                *out_data = bin;
                *out_data_len = decoded;
                ret = ESP_OK;
                break;
            }
        }
    }

    if (ret == ESP_ERR_TIMEOUT) {
        ESP_LOGE(TAG, "Timed out waiting for console response");
    }

    free(rx_buf);
    free(hex_resp);
    return ret;
}

const protocomm_ext_transport_t protocomm_ext_transport_console = {
    .init = console_init,
    .deinit = console_deinit,
    .connect = console_connect,
    .disconnect = console_disconnect,
    .send_data = console_send_data,
};
