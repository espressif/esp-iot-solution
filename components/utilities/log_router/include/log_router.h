/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include "esp_err.h"
#include "esp_log.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Route ESP log messages to a file with specified log level and tag filter
 *
 * This function creates a log router that captures ESP log messages and writes them
 * to the specified file. Only messages with log level equal to or higher than the
 * specified level will be written to the file. If a tag is specified, only messages
 * from that specific tag will be captured.
 *
 * The log router uses buffered writing for better performance and automatically
 * flushes the buffer when it reaches the threshold or timeout.
 *
 * @param file_path Path to the log file where messages will be written
 * @param tag Tag filter for log messages. If NULL, captures all messages that meet the level requirement
 * @param level Minimum log level to capture (ESP_LOG_ERROR, ESP_LOG_WARN, etc.)
 *
 * @return
 *      - ESP_OK: Log router created successfully
 *      - ESP_ERR_INVALID_ARG: Invalid file path
 *      - ESP_ERR_NO_MEM: Memory allocation failed
 *      - ESP_FAIL: Failed to open/create log file
 */
esp_err_t esp_log_router_to_file(const char* file_path, const char* tag, esp_log_level_t level);

/**
 * @brief Enable or disable console output for log messages
 *
 * This function controls whether log messages are also printed to the console
 * in addition to being routed to files. By default, console output is enabled.
 *
 * @param output_console
 *      - true: Enable console output (default)
 *      - false: Disable console output, only write to files
 */
void esp_log_router_to_console(bool output_console);

/**
 * @brief Dump the contents of a log file to console
 *
 * This function reads and displays the entire contents of a log file to the console.
 * Useful for debugging and viewing log file contents without external tools.
 *
 * @param file_path Path to the log file to dump
 *
 * @return
 *      - ESP_OK: File dumped successfully
 *      - ESP_ERR_INVALID_ARG: Invalid file path
 *      - ESP_FAIL: Failed to open or read file
 */
esp_err_t esp_log_router_dump_file_messages(const char* file_path);

/**
 * @brief Delete a log router and close its associated file
 *
 * This function removes a log router from the system and closes its associated
 * log file. The file itself is not deleted from the filesystem.
 *
 * @param file_path Path to the log file whose router should be deleted
 *
 * @return
 *      - ESP_OK: Log router deleted successfully
 *      - ESP_ERR_INVALID_ARG: Invalid file path
 *      - ESP_ERR_NOT_FOUND: No log router found for the specified file path
 */
esp_err_t esp_log_delete_router(const char* file_path);

#ifdef __cplusplus
}
#endif
