/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <inttypes.h>
#include <sys/queue.h>
#include <unistd.h>
#include "esp_check.h"
#include "esp_vfs_fat.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "log_router.h"

static const char *TAG = "log_router";

// Global mutex for thread safety
static SemaphoreHandle_t g_log_router_mutex = NULL;

// Custom debug output function
static void log_router_debug_printf(const char *format, ...)
{
#ifdef CONFIG_LOG_ROUTER_DEBUG_OUTPUT
    va_list args;
    va_start(args, format);
    printf("[LOG_ROUTER] ");
    vprintf(format, args);
    va_end(args);
#endif
}

typedef struct _esp_log_router_slist_t {
    char *file_path;                                               /*!< File path for log */
    char *tag;                                                     /*!< Tag for log */
    char *buffer;                                                  /*!< Buffer for log data */
    size_t buffer_size;                                            /*!< Buffer size for batch writing */
    size_t buffer_pos;                                             /*!< Current position in buffer */
    size_t flush_threshold;                                        /*!< Flush threshold */
    uint32_t flush_timeout_ms;                                     /*!< Flush timeout in milliseconds */
    uint32_t last_flush_time;                                      /*!< Last flush time */
    FILE *log_fp;
    esp_log_level_t level;
    SLIST_ENTRY(_esp_log_router_slist_t) next;
} esp_log_router_slist_t;

static SLIST_HEAD(_esp_log_router_head_t, _esp_log_router_slist_t) g_esp_log_router_slist_head = SLIST_HEAD_INITIALIZER(g_esp_log_router_slist_head);
static bool g_esp_log_router_keep_console = true;                  /*!< Whether to keep console output */
static vprintf_like_t g_esp_log_router_vprintf = NULL;             /*!< Original vprintf function */
static char dump_log_buffer[CONFIG_LOG_ROUTER_FORMAT_BUFFER_SIZE]; /*!< Buffer for dump log messages */
static char vprintf_buffer[CONFIG_LOG_ROUTER_FORMAT_BUFFER_SIZE];  /*!< Buffer for formatted log messages */

static esp_err_t esp_log_router_flush_buffer(esp_log_router_slist_t *item)
{
    if (!item || !item->buffer || item->buffer_pos == 0 || !item->log_fp) {
        return ESP_FAIL;
    }

    size_t written = fwrite(item->buffer, 1, item->buffer_pos, item->log_fp);
    if (written != item->buffer_pos) {
        log_router_debug_printf("Failed to write complete buffer to file, written: %zu, expected: %zu\n", written, item->buffer_pos);

        // Try to seek to beginning of file and start over
        if (fseek(item->log_fp, 0, SEEK_SET) == 0) {
            log_router_debug_printf("Seeking to beginning of file due to write failure\n");
            // Clear the file by truncating it
            if (ftruncate(fileno(item->log_fp), 0) == 0) {
                log_router_debug_printf("File truncated, retrying write\n");
                // Retry the write
                written = fwrite(item->buffer, 1, item->buffer_pos, item->log_fp);
                if (written != item->buffer_pos) {
                    log_router_debug_printf("Retry write also failed, written: %zu, expected: %zu\n", written, item->buffer_pos);
                    return ESP_FAIL;
                }
            } else {
                log_router_debug_printf("Failed to truncate file\n");
                return ESP_FAIL;
            }
        } else {
            log_router_debug_printf("Failed to seek to beginning of file\n");
            return ESP_FAIL;
        }
    }

    fflush(item->log_fp);
    fsync(fileno(item->log_fp));
    item->buffer_pos = 0;
    item->last_flush_time = (uint32_t)(esp_timer_get_time() / 1000);

    return ESP_OK;
}

int esp_log_router_flash_vprintf(const char *format, va_list args)
{
    int ret = 0;
    if (g_esp_log_router_keep_console && g_esp_log_router_vprintf) {
        ret = g_esp_log_router_vprintf(format, args);
    }

    const char *p = strchr(format, '(');
    if (!p || p <= format || (p - format) < 2) {
        return ESP_ERR_INVALID_ARG;
    }

    char level_char = *(p - 2);
    esp_log_level_t msg_level;

    switch (level_char) {
    case 'E': msg_level = ESP_LOG_ERROR; break;
    case 'W': msg_level = ESP_LOG_WARN; break;
    case 'I': msg_level = ESP_LOG_INFO; break;
    case 'D': msg_level = ESP_LOG_DEBUG; break;
    case 'V': msg_level = ESP_LOG_VERBOSE; break;
    default:  msg_level = ESP_LOG_NONE;
    }

    // Lock mutex for thread safety
    xSemaphoreTake(g_log_router_mutex, portMAX_DELAY);

    // Write to all files that match the log level
    esp_log_router_slist_t *item;
    int len = vsnprintf(vprintf_buffer, sizeof(vprintf_buffer) - 1, format, args);
    if (len > 0) {
        vprintf_buffer[len] = '\0';
        uint32_t now = (uint32_t)(esp_timer_get_time() / 1000);

        // Extract tag from formatted buffer (find the colon ':')
        const char *colon_pos = strchr(vprintf_buffer, ':');
        const char *tag_start = NULL;
        size_t tag_len = 0;

        if (colon_pos) {
            // Find the start of tag (after the closing parenthesis)
            tag_start = strchr(vprintf_buffer, ')');
            if (tag_start && tag_start < colon_pos) {
                tag_start++; // Skip ')'
                // Skip any spaces
                while (tag_start < colon_pos && (*tag_start == ' ' || *tag_start == '\t')) {
                    tag_start++;
                }
                // Calculate tag length
                tag_len = colon_pos - tag_start;
                if (tag_len > 0) {
                    log_router_debug_printf("Extracted tag: '%.*s' (len=%zu)\n", (int)tag_len, tag_start, tag_len);
                }
            }
        }

        SLIST_FOREACH(item, &g_esp_log_router_slist_head, next) {
            // Check level and tag match
            bool level_match = (msg_level <= item->level);
            bool tag_match = (item->tag == NULL) || (tag_len > 0 && strncmp(tag_start, item->tag, tag_len) == 0 && item->tag[tag_len] == '\0');

            if (level_match && tag_match && item->log_fp) {
                bool should_flush = false;

                // Check if we need to flush due to threshold
                if ((item->buffer_pos + len) >= item->flush_threshold) {
                    should_flush = true;
                    log_router_debug_printf("Flush buffer due to threshold\n");
                }

                // Check if we need to flush due to timeout
                if ((now - item->last_flush_time) >= item->flush_timeout_ms) {
                    should_flush = true;
                    log_router_debug_printf("Flush buffer due to timeout\n");
                }

                // Flush buffer if needed
                if (should_flush) {
#ifdef CONFIG_LOG_ROUTER_DEBUG_OUTPUT
                    uint64_t flush_start_time = esp_timer_get_time();
#endif
                    esp_err_t flush_ret = esp_log_router_flush_buffer(item);
#ifdef CONFIG_LOG_ROUTER_DEBUG_OUTPUT
                    uint32_t flush_duration_us = esp_timer_get_time() - flush_start_time;
                    log_router_debug_printf("Flush operation took %ums, file: %s\n", flush_duration_us / 1000, item->file_path);
#endif
                    if (flush_ret != ESP_OK) {
                        log_router_debug_printf("Failed to flush buffer: %s\n", esp_err_to_name(flush_ret));
                        // Flush failed, write directly to file
                        size_t written = fwrite(vprintf_buffer, 1, len, item->log_fp);
                        if (written != len) {
                            log_router_debug_printf("Failed to write directly to file, written: %zu, expected: %d\n", written, len);
                            // Try to seek to beginning and retry
                            if (fseek(item->log_fp, 0, SEEK_SET) == 0 && ftruncate(fileno(item->log_fp), 0) == 0) {
                                log_router_debug_printf("File truncated, retrying direct write\n");
                                written = fwrite(vprintf_buffer, 1, len, item->log_fp);
                                if (written != len) {
                                    log_router_debug_printf("Retry direct write also failed\n");
                                    continue;
                                }
                            } else {
                                log_router_debug_printf("Failed to seek/truncate file for direct write\n");
                                continue;
                            }
                        }
                        fflush(item->log_fp);
                        fsync(fileno(item->log_fp));
                        continue; // Skip buffer write since we already wrote to file
                    }
                    // Flush successful, buffer is now empty, continue to buffer write
                }

                // Try to add to buffer, if still too large, write directly to file
                if ((item->buffer_pos + len) < item->buffer_size) {
                    memcpy(item->buffer + item->buffer_pos, vprintf_buffer, len);
                    item->buffer_pos += len;
                } else {
                    // Buffer is too small, write directly to file
#ifdef CONFIG_LOG_ROUTER_DEBUG_OUTPUT
                    uint64_t write_start_time = esp_timer_get_time();
#endif
                    size_t written = fwrite(vprintf_buffer, 1, len, item->log_fp);
                    if (written != len) {
                        log_router_debug_printf("Failed to write directly to file, written: %zu, expected: %d\n", written, len);
                        // Try to seek to beginning and retry
                        if (fseek(item->log_fp, 0, SEEK_SET) == 0 && ftruncate(fileno(item->log_fp), 0) == 0) {
                            log_router_debug_printf("File truncated, retrying direct write\n");
                            written = fwrite(vprintf_buffer, 1, len, item->log_fp);
                            if (written != len) {
                                log_router_debug_printf("Retry direct write also failed\n");
                                continue;
                            }
                        } else {
                            log_router_debug_printf("Failed to seek/truncate file for direct write\n");
                            continue;
                        }
                    }
                    fflush(item->log_fp);
                    fsync(fileno(item->log_fp));
#ifdef CONFIG_LOG_ROUTER_DEBUG_OUTPUT
                    uint32_t write_duration_us = esp_timer_get_time() - write_start_time;
                    log_router_debug_printf("Direct write took %ums, size: %d, file: %s\n", write_duration_us / 1000, len, item->file_path);
#endif
                }
            }
        }
    }

    // Release mutex
    xSemaphoreGive(g_log_router_mutex);

    return ret;
}

static void esp_log_router_shutdown(void)
{
    esp_log_router_slist_t *item, *temp;

    // Just close all file handles directly
    SLIST_FOREACH_SAFE(item, &g_esp_log_router_slist_head, next, temp) {
        if (item->log_fp) {
            fclose(item->log_fp);
            item->log_fp = NULL;
        }

        // Free allocated memory
        if (item->file_path) {
            free(item->file_path);
        }
        if (item->tag) {
            free(item->tag);
        }
        if (item->buffer) {
            free(item->buffer);
        }

        // Remove from list and free node
        SLIST_REMOVE(&g_esp_log_router_slist_head, item, _esp_log_router_slist_t, next);
        free(item);
    }

    // Restore original vprintf function
    if (g_esp_log_router_vprintf) {
        esp_log_set_vprintf(g_esp_log_router_vprintf);
        g_esp_log_router_vprintf = NULL;
    }

    // Reset global state
    g_esp_log_router_keep_console = true;
}

esp_err_t esp_log_router_to_file(const char* file_path, const char* tag, esp_log_level_t level)
{
    // Create mutex for thread safety, the first time this function is called.
    if (g_log_router_mutex == NULL) {
        g_log_router_mutex = xSemaphoreCreateMutex();
        if (g_log_router_mutex == NULL) {
            ESP_LOGE(TAG, "Failed to create mutex for log router");
            return ESP_ERR_NO_MEM;
        }
    }

    if (!file_path) {
        return ESP_ERR_INVALID_ARG;
    }

    // Check if file path already exists
    esp_log_router_slist_t *item;
    SLIST_FOREACH(item, &g_esp_log_router_slist_head, next) {
        if (strcmp(item->file_path, file_path) == 0) {
            // File path already exists, update configuration
            item->level = level;

            // Update tag
            if (item->tag) {
                free(item->tag);
            }
            if (tag) {
                item->tag = strdup(tag);
                if (!item->tag) {
                    return ESP_ERR_NO_MEM;
                }
            } else {
                item->tag = NULL;
            }

            return ESP_OK;
        }
    }

    // Create new node
    esp_log_router_slist_t *new_log_router = calloc(1, sizeof(esp_log_router_slist_t));
    if (!new_log_router) {
        return ESP_ERR_NO_MEM;
    }

    // Allocate and copy file path
    new_log_router->file_path = strdup(file_path);
    if (!new_log_router->file_path) {
        free(new_log_router);
        return ESP_ERR_NO_MEM;
    }

    // Allocate and copy tag
    if (tag) {
        new_log_router->tag = strdup(tag);
        if (!new_log_router->tag) {
            free(new_log_router->file_path);
            free(new_log_router);
            return ESP_ERR_NO_MEM;
        }
    } else {
        new_log_router->tag = NULL;
    }

    // Attempt to open log file
    new_log_router->log_fp = fopen(file_path, "a");
    if (!new_log_router->log_fp) {
        free(new_log_router->file_path);
        free(new_log_router);
        return ESP_FAIL;
    }

    // Set log level
    new_log_router->level = level;

    // Initialize buffer and flush settings
    new_log_router->buffer_size = CONFIG_LOG_ROUTER_BUFFER_SIZE;
    new_log_router->buffer = calloc(1, new_log_router->buffer_size);
    if (!new_log_router->buffer) {
        fclose(new_log_router->log_fp);
        free(new_log_router->file_path);
        free(new_log_router);
        return ESP_ERR_NO_MEM;
    }

    // Calculate flush threshold based on percentage
    new_log_router->flush_threshold = (new_log_router->buffer_size * CONFIG_LOG_ROUTER_FLUSH_THRESHOLD_PERCENT) / 100;
    new_log_router->flush_timeout_ms = CONFIG_LOG_ROUTER_FLUSH_TIMEOUT_MS;
    new_log_router->buffer_pos = 0;
    new_log_router->last_flush_time = (uint32_t)(esp_timer_get_time() / 1000);

    // Insert after the first node (if exists) or as first node
    if (SLIST_EMPTY(&g_esp_log_router_slist_head)) {
        // If list is empty, insert as first node and set vprintf redirect
        SLIST_INSERT_HEAD(&g_esp_log_router_slist_head, new_log_router, next);
        g_esp_log_router_vprintf = esp_log_set_vprintf(esp_log_router_flash_vprintf);
        esp_register_shutdown_handler(esp_log_router_shutdown);
    } else {
        // Find the last node and insert after it
        esp_log_router_slist_t *last = SLIST_FIRST(&g_esp_log_router_slist_head);
        while (SLIST_NEXT(last, next) != NULL) {
            last = SLIST_NEXT(last, next);
        }
        SLIST_INSERT_AFTER(last, new_log_router, next);
    }

    return ESP_OK;
}

void esp_log_router_to_console(bool output_console)
{
    g_esp_log_router_keep_console = output_console;
}

esp_err_t esp_log_delete_router(const char* file_path)
{
    if (!file_path) {
        return ESP_ERR_INVALID_ARG;
    }

    esp_log_router_slist_t *item, *prev = NULL;
    SLIST_FOREACH(item, &g_esp_log_router_slist_head, next) {
        if (strcmp(item->file_path, file_path) == 0) {
            if (prev) {
                SLIST_REMOVE_AFTER(prev, next);
            } else {
                SLIST_REMOVE_HEAD(&g_esp_log_router_slist_head, next);
            }

            // Flush buffer before closing
            esp_log_router_flush_buffer(item);

            fclose(item->log_fp);
            free(item->file_path);
            if (item->tag) {
                free(item->tag);
            }
            free(item->buffer);
            free(item);

            // If this was the last item, restore original vprintf
            if (SLIST_EMPTY(&g_esp_log_router_slist_head) && g_esp_log_router_vprintf) {
                esp_log_set_vprintf(g_esp_log_router_vprintf);
                g_esp_log_router_vprintf = NULL;
            }

            ESP_LOGI(TAG, "Log router deleted: %s", file_path);
            return ESP_OK;
        }
        prev = item;
    }
    return ESP_ERR_NOT_FOUND;
}

esp_err_t esp_log_router_dump_file_messages(const char* file_path)
{
    if (!file_path) {
        return ESP_ERR_INVALID_ARG;
    }

    FILE *fp = fopen(file_path, "r");
    if (!fp) {
        printf("Failed to open file: %s\n", file_path);
        return ESP_FAIL;
    }

    printf("File content: %s\n", file_path);
    int line_count = 0;

    while (fgets(dump_log_buffer, sizeof(dump_log_buffer), fp) != NULL) {
        printf("%s", dump_log_buffer);
        vTaskDelay(pdMS_TO_TICKS(10)); /*!< Avoid triggering watchdog */
        line_count++;
    }

    printf("End of file (total %d lines)\n", line_count);
    fclose(fp);
    return ESP_OK;
}
