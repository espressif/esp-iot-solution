/*
 * SPDX-FileCopyrightText: 2025-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <stdio.h>
#include <stdbool.h>
#include <sys/stat.h>

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
 * The log router uses an intermediate ring-buffer to reduce the latency of calls to
 * ESP_LOG() functions. A background task flushes the buffer when the CPU has nothing
 * more important to do. The size of the buffer is set by KConfig options.
 *
 * The log_router also uses a buffer to batch together writes to each log file. This
 * can substantially reduce the number of flash operations. The size of the buffer,
 * and the time-out period is set by KConfig options.
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
 * @brief Stop routing logs to a file.
 *
 * Stop routing logs to the specified file. All pending messages destined to this file
 * will be written before closing the file. The file is not deleted.
 *
 * @param file_path Path to the log file registered with \c esp_log_router_to_file()
 *
 * @return
 *      - ESP_OK: Log router deleted successfully
 *      - ESP_ERR_INVALID_ARG: Invalid file path
 *      - ESP_ERR_NOT_FOUND: No log router found for the specified file path
 */
esp_err_t esp_log_router_stop_file(const char* file_path);

__attribute__((deprecated))
static inline esp_err_t esp_log_delete_router(const char* file_path)
{
    return esp_log_router_stop_file(file_path);
}

/**
 * @brief Check if a file is being used for logging
 *
 * @param [in]  file_path   Path to the log file
 *
 * @return
 *      - ESP_OK: File is currently a log target.
 *      - ESP_ERR_INVALID_ARG: Invalid file path
 *      - ESP_ERR_NOT_FOUND: File is not currently being used for logging
 */
esp_err_t esp_log_router_status_file(const char* file_path);

/**
 * @brief Flush all pending messages to a given logfile
 *
 * Ensures that all pending messages destined to this file have been processed,
 * and flushes associated buffers to disk before returning.
 *
 * An optional \p sbuf argument can be used to get the file size immediately after
 * sync has finished, before other tasks have an opportunity to add to it. All
 * messages up to that point in the file are guaranteed to be fully-written.
 *
 * @param [in]  file_path   Path to the log file to sync
 * @param [out] sbuf        Result of fstat() immediately after sync.
 *
 * @return
 *      - ESP_OK: Sync success
 *      - ESP_ERR_NOT_FOUND: File is not currently being used for logging
 *      - ESP_ERR_INVALID_ARG: Invalid file path
 *      - ESP_FAIL: Failed to fstat() the file
 */
esp_err_t esp_log_router_sync_file(const char* file_path, struct stat *sbuf);

/**
 * @brief Dump the contents of a log file to console
 *
 * This function will first flush buffers, ensuring that any in-flight logs intended
 * for \p file_path have been written to the file.
 *
 * Messages that arrive after that point will not be dumped; this prevents partially-written
 * message fragments from ending up in the output.
 *
 * @param file_path Path to the log file to dump
 *
 * @return
 *      - ESP_OK: File dumped successfully
 *      - ESP_ERR_NOT_FOUND: File dumped successfully, even though it is not currently a log target
 *      - ESP_ERR_INVALID_ARG: Invalid file path
 *      - ESP_FAIL: Failed to open or read file
 */
esp_err_t esp_log_router_dump_file_messages(const char* file_path);

#ifdef __cplusplus
}
#endif
