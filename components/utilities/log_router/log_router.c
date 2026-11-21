/*
 * SPDX-FileCopyrightText: 2025-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "log_router.h"

#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <stdatomic.h>
#include <errno.h>
#include <inttypes.h>
#include <sys/cdefs.h>
#include <sys/queue.h>
#include <sys/stat.h>
#include <unistd.h>
#include "esp_check.h"
#include "esp_err.h"
#include "esp_log_level.h"
#include "esp_vfs_fat.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/projdefs.h"
#include "freertos/semphr.h"
#include "freertos/timers.h"
#include "freertos/ringbuf.h"
#include "esp_log_color.h"

#include "portmacro.h"
#include "sdkconfig.h"
#include "tlsf_control_functions.h"
#include "xtensa_context.h"

#include "log_router_macros.h"

static const char *TAG = "log_router";

// Truncated messages will be immediately followed by this message
#if (CONFIG_LOG_ROUTER_APPEND_STRING_ON_TRUNCATE)
#if (CONFIG_LOG_COLORS)
static const char *TRUNCATE_MESSAGE = LOG_ITALIC(LOG_COLOR_PURPLE) "" CONFIG_LOG_ROUTER_TRUNCATE_STRING "" LOG_RESET_COLOR "\n" ;
#else
static const char *TRUNCATE_MESSAGE = CONFIG_LOG_ROUTER_TRUNCATE_STRING "\n" ;
#endif
#endif

// ---------------------------- //

#if (CONFIG_LOG_ROUTER_RINGBUFFER)
//! Message buffer. Thread-safe.
//! @note A slice of bytes can be reserved in advance with xRingbufferSendAcquire(), and used instead of a formatting buffer
static RingbufHandle_t g_ringbuf = NULL;
#else
//! Formatting buffer. Not thread-safe.
static char vprintf_buffer[CONFIG_LOG_ROUTER_FORMAT_BUFFER_SIZE];
#endif

#if (CONFIG_LOG_ROUTER_RINGBUFFER && CONFIG_LOG_ROUTER_RINGBUFFER_STATIC)
#if (LOG_ROUTER_RINGBUFFER_IN_EXT)
EXT_RAM_ATTR static uint8_t ringbufferStorage[CONFIG_LOG_ROUTER_RINGBUFFER_SIZE];
EXT_RAM_ATTR static StaticRingbuffer_t staticRingbuffer;
#else
static uint8_t ringbufferStorage[CONFIG_LOG_ROUTER_RINGBUFFER_SIZE];
static StaticRingbuffer_t staticRingbuffer;
#endif
#endif

#if (CONFIG_LOG_ROUTER_RINGBUFFER)
//! Global mutex ensuring messages are printed in the same order they enter the ringbuffer
//! Otherwise, messages could be re-ordered while draining the ringbuffer, due to a race-condition
//! However, the race is over a small and simple but of code, so it's unlikely that it would happen in practice
static SemaphoreHandle_t g_ringbuf_drain_mutex = NULL;
#endif

#if (CONFIG_LOG_ROUTER_RINGBUFFER || CONFIG_LOG_ROUTER_BATCH_WRITE_BUFFER)
//! Low-priority task used to (1) Drain the ringbuffer and/or (2) flush buffers after a timeout period
static TaskHandle_t g_log_router_drain_task = NULL;
#endif

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

//! Global mutex that protects the linked list.
//! Any operation that traverses the list, or uses any of its items, must lock the mutex
//! until it is completely finished.
static SemaphoreHandle_t g_slist_mutex = NULL;

//! Used to mark functions that expect the mutex to be locked
static bool MUTEX_LOCKED(SemaphoreHandle_t mut)
{
    return xTaskGetCurrentTaskHandle() == xSemaphoreGetMutexHolder(mut);
}

typedef struct _esp_log_router_slist_t {
    char *file_path;                                               /*!< File path for log */
    char *tag;                                                     /*!< Tag for log */
#if (CONFIG_LOG_ROUTER_BATCH_WRITE)
    char *buffer;                                                  /*!< Buffer for log data */
    size_t buffer_size;                                            /*!< Buffer size for batch writing */
    size_t buffer_pos;                                             /*!< Current position in buffer */
    uint32_t flush_timeout_ms;                                     /*!< Flush timeout in milliseconds */
    uint32_t buffer_time_ms;                                       /*!< Oldest data in buffer. Ignored when (buffer_pos == 0) */
#endif
    FILE *log_fp;
    esp_log_level_t level;
    SLIST_ENTRY(_esp_log_router_slist_t) next;
} esp_log_router_slist_t;

static SLIST_HEAD(_esp_log_router_head_t, _esp_log_router_slist_t) g_esp_log_router_slist_head = SLIST_HEAD_INITIALIZER(g_esp_log_router_slist_head);
static bool g_esp_log_router_keep_console = true;                  /*!< Whether to keep console output */
static vprintf_like_t g_esp_log_router_vprintf = NULL;             /*!< Original vprintf function */

//! Highest log level currently in use, used to skip processing for messages that won't be written anyway
//! Lock-free, because atomic is sufficient for correctness.
static _Atomic(esp_log_level_t) g_level_max = 0;

static esp_err_t router_write_direct(const void *__restrict buffer, size_t count, esp_log_router_slist_t *item)
{
    assert(MUTEX_LOCKED(g_slist_mutex));

    FILE *fp = item->log_fp;

    if (!fp) {
        log_router_debug_printf("Failed to open file: %s", strerror(errno));
        return ESP_ERR_NO_MEM;
    }

    size_t written = fwrite(buffer, 1, count, fp);
    if (written != count) {
        log_router_debug_printf("Failed to write %d bytes to file '%s', returned: %zu\n", count, item->file_path, written);
        // Try to seek to the beginning of the file and start fresh
        if (fseek(fp, 0, SEEK_SET) == 0 && ftruncate(fileno(fp), 0) == 0) {
            log_router_debug_printf("File truncated, retrying write\n");
            written = fwrite(buffer, 1, count, fp);
            if (written != count) {
                log_router_debug_printf("Retry direct write also failed\n");
                return ESP_FAIL;
            }
        } else {
            log_router_debug_printf("Failed to seek/truncate file for direct write\n");
            return ESP_FAIL;
        }
    }
    fflush(fp);
    fsync(fileno(fp));  // Would sync() be more suitable here?

    return ESP_OK;
}

#if (CONFIG_LOG_ROUTER_BATCH_WRITE)

static esp_err_t esp_log_router_flush_buffer(esp_log_router_slist_t *item)
{
    assert(MUTEX_LOCKED(g_slist_mutex));

    if (item->buffer_pos == 0) {
        return ESP_OK;  // Nothing to do
    }

    assert(item);
    assert(item->buffer);

    log_router_debug_printf("Flushing %d bytes to file: %s\n", item->buffer_pos, item->file_path);

    LOG_ROUTER_DEBUG_TIMING_BEGIN();

    ESP_ERROR_CHECK_WITHOUT_ABORT(router_write_direct(item->buffer, item->buffer_pos, item));

    LOG_ROUTER_DEBUG_TIMING_END_MSG(, "Flushed %d bytes -> file: %s\n", item->buffer_pos, item->file_path);

    item->buffer_pos = 0;
    return ESP_OK;
}

#endif

//! Write already-formatted message to buffer, flushing to the file if the buffer is full
static void router_write_buffered(const void *__restrict buffer, size_t count, esp_log_router_slist_t *item)
{
    assert(MUTEX_LOCKED(g_slist_mutex));

#if (!CONFIG_LOG_ROUTER_BATCH_WRITE)
    LOG_ROUTER_DEBUG_TIMING_BEGIN();

    router_write_direct(buffer, count, item);

    LOG_ROUTER_DEBUG_TIMING_END_MSG(, "Direct write, size: %d, file: %s\n", count, item->file_path);

    return;
#else

    // Record when the oldest data in the buffer was written, for time-out purposes
    if (item->buffer_pos == 0) {
        item->buffer_time_ms = (uint32_t)(esp_timer_get_time() / 1000);
    }

    // Repeat until all bytes are buffered or written
    size_t written = 0;
    while (written < count) {
        size_t todo = count - written;
        size_t free_space = item->buffer_size - item->buffer_pos;

        size_t min = MIN(todo, free_space);
        memcpy(item->buffer + item->buffer_pos, buffer + written, min);
        written += min;
        item->buffer_pos += min;

        assert(item->buffer_pos == item->buffer_size || count == written);

        // Buffer full, flush and continue
        if (item->buffer_pos == item->buffer_size) {
            ESP_ERROR_CHECK_WITHOUT_ABORT(esp_log_router_flush_buffer(item));
        }
    }

    return;

#endif
}

static esp_log_level_t char_to_level(const char level_char)
{
    switch (level_char) {
    case 'E': return ESP_LOG_ERROR;
    case 'W': return ESP_LOG_WARN;
    case 'I': return ESP_LOG_INFO;
    case 'D': return ESP_LOG_DEBUG;
    case 'V': return ESP_LOG_VERBOSE;
    default:  return ESP_LOG_NONE;
    }
}

//! Write an already-formatted message to all files that match the log level and/or tag
static esp_err_t router_fork_message(const char *__restrict buffer, size_t count)
{
    assert(MUTEX_LOCKED(g_slist_mutex));

    const char *p = strchr(buffer, '(');
    if (!p) {
        ESP_ERROR_CHECK_WITHOUT_ABORT(ESP_ERR_INVALID_ARG); // Should have been checked before the message was formatted
    }

    char level_char = *(p - 2);
    esp_log_level_t msg_level = char_to_level(level_char);

    // Extract tag from formatted buffer (find the colon ':')
    // If system time is printed, then we need to skip two more (see format string in esp_log_system_timestamp())
    const char *colon_pos = strchr(buffer, ':');
#ifdef CONFIG_LOG_TIMESTAMP_SOURCE_SYSTEM
    colon_pos = colon_pos ? strchr(colon_pos, ':') : NULL;
    colon_pos = colon_pos ? strchr(colon_pos, ':') : NULL;
#endif
    if (NULL == colon_pos) {
        log_router_debug_printf("Not enough colons\n");
        return ESP_ERR_INVALID_ARG;
    }

    const char *tag_start = NULL;
    size_t tag_len = 0;

    if (colon_pos) {
        // Find the start of tag (after the closing parenthesis)
        tag_start = strchr(buffer, ')');
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

    esp_log_router_slist_t *item;
    SLIST_FOREACH(item, &g_esp_log_router_slist_head, next) {
        // Check level and tag match
        bool level_match = (msg_level <= item->level);
        bool tag_match = (item->tag == NULL) || (tag_len > 0 && strncmp(tag_start, item->tag, tag_len) == 0 && item->tag[tag_len] == '\0');

        if (level_match && tag_match) {
            router_write_buffered(buffer, count, item);

#if (CONFIG_LOG_ROUTER_APPEND_STRING_ON_TRUNCATE)
#if (CONFIG_LOG_ROUTER_RINGBUFFER)
            size_t truncate_threshold = xRingbufferGetMaxItemSize(g_ringbuf);
#else
            size_t truncate_threshold = CONFIG_LOG_ROUTER_FORMAT_BUFFER_SIZE;
#endif
            truncate_threshold -= 1;    // Null-terminator wastes 1 byte of the buffer

            if (count >= truncate_threshold) {
                // Either it was truncated, or it was exactly the same length as the buffer.
                router_write_buffered(TRUNCATE_MESSAGE, strlen(TRUNCATE_MESSAGE), item);
            }
#endif
        }
    }

    return ESP_OK;
}

#if (CONFIG_LOG_ROUTER_RINGBUFFER)

static UBaseType_t rb_items_waiting()
{
    UBaseType_t items_waiting;
    vRingbufferGetInfo(g_ringbuf, NULL, NULL, NULL, NULL, &items_waiting);
    return items_waiting;
}

//! Thread-safe (waits for mutex)
//! \param [out] b_count    Optional counter, incremented by number of bytes in message (not including null-terminator)
static esp_err_t drain_one_from_ringbuffer(TickType_t xTicksToWait, size_t* b_count)
{
    // This prevents a potential race between two tasks from xRingbufferReceive() -> router_fork_message(), which
    // could result in messages being printed out-of-order.
    xSemaphoreTake(g_ringbuf_drain_mutex, portMAX_DELAY);

    // Receive an item from no-split ring buffer (thread-safe). Does not copy data.
    size_t rb_slice_size;
    void* rb_slice = xRingbufferReceive(g_ringbuf, &rb_slice_size, xTicksToWait);
    if (NULL == rb_slice) {
        xSemaphoreGive(g_ringbuf_drain_mutex);
        return ESP_ERR_TIMEOUT;
    }
    const char* rb_slice_pchar = (char*)rb_slice;

    // Do not print NULL terminator
    rb_slice_size -= 1;
    assert(rb_slice_pchar[rb_slice_size] == '\0');

    log_router_debug_printf("ringbuffer --> %d bytes (%d items waiting)\n", rb_slice_size, rb_items_waiting());

    // Lock mutex while accessing the list
    xSemaphoreTake(g_slist_mutex, portMAX_DELAY);

    // Release the (potential) other task so it can get started reading from the ringbuffer.
    assert(MUTEX_LOCKED(g_slist_mutex));    // We've locked writing, so the race-condition is over
    xSemaphoreGive(g_ringbuf_drain_mutex);

    esp_err_t err = router_fork_message(rb_slice_pchar, rb_slice_size);
    if (err != ESP_OK) {
        log_router_debug_printf("Invalid message: %.*s\n", rb_slice_size, rb_slice_pchar);
    }

    // Release mutex
    xSemaphoreGive(g_slist_mutex);

    // Return the item's memory to the ringbuffer.
    vRingbufferReturnItem(g_ringbuf, rb_slice);

    if (b_count != NULL) {
        *b_count += rb_slice_size;
    }

    return ESP_OK;
}

/*! Drain all messages that are currently in the ringbuffer

    Only attempts to drain the number of messages currently in the ringbuffer,
    so it will always exit in finite time.

    Messages that arrive after the  call to rb_items_waiting() may-or-may-not
    be drained, because other tasks may call drain_one_from_ringbuffer() while
    this function is working.

    @param [out]    n_messages  Increments by 1 for each message successfully received
    @param [out]    b_count     Incremented by number of bytes in received messages
    @param [in]     yield_task  If true, calls \c vPortYield() after each message
*/
static esp_err_t drain_ringbuffer(bool yield_task)
{
    const size_t todo = rb_items_waiting(); // Only flush current messages

    log_router_debug_printf("begin draining ringbuffer (%d messages)\n", todo);

    LOG_ROUTER_DEBUG_TIMING_BEGIN();

    size_t count = 0;
    size_t b_count = 0;
    for (int i = 0; i < todo; i++) {
        esp_err_t err = drain_one_from_ringbuffer(0, &b_count);
        if (ESP_ERR_TIMEOUT == err) {
            // A higher-priority task preempted us and drained some messages
            break;
        }
        ESP_RETURN_ON_ERROR(err, TAG, "");

        count++;
        if (yield_task) {
            vPortYield();
        }
    }

    LOG_ROUTER_DEBUG_TIMING_END_MSG(, "drained %d messages (totalling %d bytes)\n", count, b_count);
    if (rb_items_waiting() > 0) {
        log_router_debug_printf("%d new messages arrived while background task drained %d messages \n", rb_items_waiting(), count);
    }

    return ESP_OK;
}

//! Print a message straight into the ringbuffer (avoids redundant copies)
//! If there's not enough space, flushes the oldest message(s) to make room.
//! Thread-safe, but the order of messages may be different.
static void vprintf_ringbuffer(const char *format, va_list args, size_t predicted_len)
{
    // Need a null-terminator - even though the ringbuffer records the size,
    // vsnprintf() insists on adding a null-terminator, unfortunately.
    size_t predicted_size = predicted_len + 1;

    // We may have to truncate it, if the message is too large to fit into the ringbuffer
    size_t rb_slice_size = MIN(predicted_size, xRingbufferGetMaxItemSize(g_ringbuf));
    if (rb_slice_size < predicted_size) {
        log_router_debug_printf("Message too long to fit in the ringbuffer, truncated %d bytes\n", (predicted_size - rb_slice_size));
    }

#ifdef CONFIG_LOG_ROUTER_DEBUG_OUTPUT
    static size_t free_space_hwm = CONFIG_LOG_ROUTER_RINGBUFFER_SIZE;
    size_t free_space = xRingbufferGetCurFreeSize(g_ringbuf);
    free_space_hwm = MIN(free_space_hwm, free_space);

    const size_t max_val_possible = xRingbufferGetMaxItemSize(g_ringbuf);
    if (free_space >= max_val_possible) {
        log_router_debug_printf("Ringbuffer free size >= %d bytes (hwm %d bytes)\n", max_val_possible, free_space_hwm);
    } else {
        log_router_debug_printf("Ringbuffer free size = %d bytes (hwm %d bytes)\n", free_space, free_space_hwm);
    }

    log_router_debug_printf("ringbuffer <-- %d bytes (%d items waiting)\n", rb_slice_size, rb_items_waiting());
#endif

    // Ask the ringbuffer for a slice of its bytes.
    // If we've filled up the ringbuffer, we'll need to make room.
    void* rb_slice;

    LOG_ROUTER_DEBUG_TIMING_BEGIN();

    size_t count = 0;
    size_t b_count = 0;
    while (true) {
        BaseType_t success = xRingbufferSendAcquire(g_ringbuf, &rb_slice, rb_slice_size, 0);
        if (pdTRUE == success) {
            if (count > 0) {
                LOG_ROUTER_DEBUG_TIMING_END();
                log_router_debug_printf("ringbuffer OOM, drained %d messages (totalling %d bytes) in %ums \n", count, b_count, LOG_ROUTER_DEBUG_TIMING_RESULT() / 1000);
            }
            break;
        }
        // Flush some messages to make room in the buffer, then try again
        esp_err_t err = drain_one_from_ringbuffer(0, &b_count);
        if (err != ESP_OK) {
            ; // This could theoretically happen if you had two VERY large messages and a race-condition with log_router_drain_task()
        } else {
            count++;
        }
    }
    assert(rb_slice != NULL);

    char* rb_slice_pchar = (char*)rb_slice;

    // Format the message, directly into the bytes of the ringbuffer
    int intended_size = vsnprintf(rb_slice_pchar, rb_slice_size, format, args) + 1;   // +1 for null-terminator
    assert(intended_size == predicted_size);  // Behaviour is undefined if pointed-to args change during the call to a printf-like function

    // Tell the ringbuffer that the message is ready
    xRingbufferSendComplete(g_ringbuf, rb_slice);

    return;
}

#endif

#if (!CONFIG_LOG_ROUTER_RINGBUFFER)

static void vprintf_no_ringbuffer(const char *format, va_list args, size_t predicted_len)
{
    // Lock mutex while accessing the list
    xSemaphoreTake(g_slist_mutex, portMAX_DELAY);

    int intended_len = vsnprintf(vprintf_buffer, CONFIG_LOG_ROUTER_FORMAT_BUFFER_SIZE, format, args);
    assert(intended_len == predicted_len);  // Behaviour is undefined if pointed-to args change during the call to a printf-like function

    int truncated_len = MIN(intended_len, (CONFIG_LOG_ROUTER_FORMAT_BUFFER_SIZE - 1));

    esp_err_t err = router_fork_message(vprintf_buffer, truncated_len);
    if (err != ESP_OK) {
        log_router_debug_printf("Invalid message: %.*s\n", truncated_len, vprintf_buffer);
    }

    // Release mutex
    xSemaphoreGive(g_slist_mutex);
}

#endif

#if (CONFIG_LOG_ROUTER_BATCH_WRITE)

static uint32_t process_timeout(bool yield_task, uint32_t fuzziness_ms)
{
    uint32_t next_timeout = UINT32_MAX;
    int count = 0;

    // Lock mutex while accessing the list
    xSemaphoreTake(g_slist_mutex, portMAX_DELAY);

    uint32_t time = (uint32_t)(esp_timer_get_time() / 1000);

    esp_log_router_slist_t *item;
    SLIST_FOREACH(item, &g_esp_log_router_slist_head, next) {
        uint32_t timeout = item->flush_timeout_ms;  // Shorter name
        uint32_t age = time - item->buffer_time_ms; // overflow-correct
        if (item->buffer_pos > 0) {
            if ((age + fuzziness_ms) >= timeout) {
                log_router_debug_printf("Timeout flush for file: '%s'\n", item->file_path);
                ESP_ERROR_CHECK_WITHOUT_ABORT(esp_log_router_flush_buffer(item));
                count += 1;
            } else {
                next_timeout = MIN(next_timeout, timeout - age);
            }
        }

        // Ensure each buffer is check once per timeout period, as it is possible for writes to occur
        // without waking the drain_task
        next_timeout = MIN(next_timeout, timeout);

        if (yield_task) {
            vPortYield();
        }
    }

    // Release mutex
    xSemaphoreGive(g_slist_mutex);

    log_router_debug_printf("Timeout flushed %d files\n", count);
    log_router_debug_printf("Next timeout in %d ms\n", next_timeout);

    return next_timeout;
}

#endif

#if (CONFIG_LOG_ROUTER_RINGBUFFER || CONFIG_LOG_ROUTER_BATCH_WRITE)

__attribute__((noreturn))
static void log_router_drain_task(void *pvParameter)
{
    while (true) {
#if (CONFIG_LOG_ROUTER_BATCH_WRITE)
        uint32_t next_check_delay_ms = process_timeout(true, CONFIG_LOG_ROUTER_FLUSH_TIMEOUT_HYSTERESIS_MS);
#else
        uint32_t next_check_delay_ms = portMAX_DELAY;
#endif

#if (CONFIG_LOG_ROUTER_RINGBUFFER)
        // Block until an item is available, then process it
        log_router_debug_printf("Waiting %d ticks\n", pdMS_TO_TICKS(next_check_delay_ms));
        esp_err_t err = drain_one_from_ringbuffer(pdMS_TO_TICKS(next_check_delay_ms), NULL);
        if (err != ESP_ERR_TIMEOUT) {
            ESP_ERROR_CHECK(err);   // Shouldn't fail
        } else {
            vPortYield();   // Give higher-priority tasks the opportunity to run between each item
            drain_ringbuffer(true);
        }
#else
        vTaskDelay(pdMS_TO_TICKS(next_check_delay_ms));
#endif
    }
    __unreachable();
}
#endif

int esp_log_router_flash_vprintf(const char *format, va_list args)
{
    int ret = 0;
    if (g_esp_log_router_keep_console && g_esp_log_router_vprintf) {
        ret = g_esp_log_router_vprintf(format, args);
    } else {
        // We'll need to figure out how many bytes are needed ourselves
        ret = vsnprintf(NULL, 0, format, args);
    }

    if (ret < 0) {
        log_router_debug_printf("console vprintf() returned %d: %s\n", ret, strerror(errno));
    } else if (ret == 0) {
        return ret; // No point saving completely blank messages
    }

    // Now we know how many bytes we need to fit the message.
    size_t predicted_len = ret;

    // Optimisation: don't bother with e.g. DEBUG messages if the highest level is INFO
    // This means we don't have to (1) format the message (2) fill/empty the ringbuffer
    // and (3) lock and iterate through the list.
    const char *p = strchr(format, '(');
    if (!p || p <= format || (p - format) < 2) {
        log_router_debug_printf("Parse failure in format string: '%s'", format);
        return ret;
    }
    char level_char = *(p - 2);
    esp_log_level_t msg_level = char_to_level(level_char);
    if (msg_level > g_level_max) {
        return ret;
    }

#if (CONFIG_LOG_ROUTER_RINGBUFFER)
    vprintf_ringbuffer(format, args, predicted_len);
#else
    vprintf_no_ringbuffer(format, args, predicted_len);
#endif

    return ret;
}

//! Find the log file in the list.
//! @retval NULL    File is not currently a log target
static esp_log_router_slist_t* lookup_item(const char* file_path)
{
    assert(MUTEX_LOCKED(g_slist_mutex));

    esp_log_router_slist_t* found = NULL;

    esp_log_router_slist_t *item;
    SLIST_FOREACH(item, &g_esp_log_router_slist_head, next) {
        if (strcmp(item->file_path, file_path) == 0) {
            found = item;
            break;
        }
    }

    return found;
}

// Mark redundant free() calls; the esp32 will clear the heap when it restarts
#define LOG_ROUTER_FREE_MEMORY_ON_SHUTDOWN false

#if (CONFIG_LOG_ROUTER_FLUSH_BUFFERS_ON_RESTART)

//! Callback executed by esp_restart() - i.e. a 'clean' restart (not a panic).
//! Ensures that all messages are saved to disk before restarting.
static void esp_log_router_shutdown(void)
{
    log_router_debug_printf("Flush buffers and close files prior to restart\n");

    // Restore original vprintf function - prevents new messages from being added to the log_router
    if (g_esp_log_router_vprintf) {
        esp_log_set_vprintf(g_esp_log_router_vprintf);
        g_esp_log_router_vprintf = NULL;
    }

#if (CONFIG_LOG_ROUTER_RINGBUFFER)
    // Process queued messages in the ringbuffer.

    const size_t todo = rb_items_waiting(); // Only those messages currently in the ringbuffer

    for (int i = 0; i < todo; i++) {
        esp_err_t err = drain_one_from_ringbuffer(0, NULL);
        if (err != ESP_OK) {
            break;
        }
    }
#endif

    // Lock mutex while accessing the list.
    BaseType_t ret = xSemaphoreTake(g_slist_mutex, pdMS_TO_TICKS(CONFIG_LOG_ROUTER_SHUTDOWN_HANDLER_TIMEOUT_MS));
    if (ret != pdTRUE) {
        // Another task either forgot to release the mutex, or got stuck while using it
        ESP_ERROR_CHECK(ESP_ERR_TIMEOUT);   // Abort immediately, showing the stack trace.
        __unreachable();
    }

    esp_log_router_slist_t *item, *temp;
    SLIST_FOREACH_SAFE(item, &g_esp_log_router_slist_head, next, temp) {

#if (CONFIG_LOG_ROUTER_BATCH_WRITE)
        // Flush batch buffer to disk.
        esp_log_router_flush_buffer(item);
#endif

        // make sure file data is on disk before closing it
        if (item->log_fp) {
            fflush(item->log_fp);
            fsync(fileno(item->log_fp));
            fclose(item->log_fp);
            item->log_fp = NULL;
        }

#if (LOG_ROUTER_FREE_MEMORY_ON_SHUTDOWN)
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
#endif
    }

    // We may as well keep the mutex locked for the rest of esp_restart().
    assert(MUTEX_LOCKED(g_slist_mutex));

    return;
}

#endif

static esp_err_t init_globals_once()
{
    // Create mutex for thread safety, the first time this function is called.
    g_slist_mutex = xSemaphoreCreateMutex();
    ESP_RETURN_ON_FALSE(NULL != g_slist_mutex, ESP_ERR_NO_MEM, TAG, "Failed to create slist mutex!");

#if (CONFIG_LOG_ROUTER_RINGBUFFER)
    // Initialise the ringbuffer
    // Depending on KConfig options it may be statically allocated, or even allocated in PSRAM.
#if (CONFIG_LOG_ROUTER_RINGBUFFER_STATIC)
    g_ringbuf = xRingbufferCreateStatic(CONFIG_LOG_ROUTER_RINGBUFFER_SIZE, RINGBUF_TYPE_NOSPLIT, ringbufferStorage, &staticRingbuffer);
#else
#if (CONFIG_LOG_ROUTER_RINGBUFFER_IN_EXT)
    UBaseType_t uxMemoryCaps = HEAP_CAP_DEFAULT_SPIRAM
#else
    UBaseType_t uxMemoryCaps = HEAP_CAP_DEFAULT_INTERNAL;
#endif
                               g_ringbuf = xRingbufferCreateWithCaps(CONFIG_LOG_ROUTER_RINGBUFFER_SIZE, RINGBUF_TYPE_NOSPLIT, uxMemoryCaps);
#endif
    ESP_RETURN_ON_FALSE(NULL != g_ringbuf, ESP_ERR_NO_MEM, TAG, "Failed to create ringbuffer!");

    g_ringbuf_drain_mutex = xSemaphoreCreateMutex();
    ESP_RETURN_ON_FALSE(NULL != g_ringbuf_drain_mutex, ESP_ERR_NO_MEM, TAG, "Failed to create ringbuffer mutex!");

    if (g_ringbuf_drain_mutex == NULL) {
        ESP_LOGE(TAG, "Failed to create ringbuffer mutex!");
        return ESP_ERR_NO_MEM;
    }

#endif

#if (CONFIG_LOG_ROUTER_BATCH_WRITE || CONFIG_LOG_ROUTER_RINGBUFFER)
    // This task has two responsibilities:
    // (1) Draining the ringbuffer in the background
    // (2) Flushing the batch buffers after a defined timeout period
    ESP_RETURN_ON_FALSE(NULL == g_log_router_drain_task, ESP_ERR_INVALID_STATE, TAG, "");
    BaseType_t ret = xTaskCreatePinnedToCore(
                         &log_router_drain_task,
                         CONFIG_LOG_ROUTER_TASK_NAME,
                         CONFIG_LOG_ROUTER_TASK_STACK_DEPTH,
                         NULL,
                         CONFIG_LOG_ROUTER_TASK_PRIORITY,
                         &g_log_router_drain_task,
                         CONFIG_LOG_ROUTER_TASK_CORE_AFFINITY
                     );
    if (ret != pdPASS) {
        return ESP_FAIL;    // errno NOT set
    }
#endif

#if (CONFIG_LOG_ROUTER_FLUSH_BUFFERS_ON_RESTART)
    // Shutdown handler ensures buffers are flushed, and files are closed and sync'd
    ESP_RETURN_ON_ERROR(esp_register_shutdown_handler(esp_log_router_shutdown), TAG, "");
#endif

    return ESP_OK;
}

esp_err_t esp_log_router_to_file(const char* file_path, const char* tag, esp_log_level_t level)
{
    static bool once = false;
    if (!once) {
        ESP_ERROR_CHECK(init_globals_once());
        once = true;
    }

    if (!file_path) {
        return ESP_ERR_INVALID_ARG;
    }

    // Lock mutex while accessing the list
    xSemaphoreTake(g_slist_mutex, portMAX_DELAY);

    // Check if file path already exists
    esp_log_router_slist_t *item = lookup_item(file_path);
    if (item) {
        // File path already exists, update configuration
        item->level = level;
        g_level_max = MAX(g_level_max, item->level);

        // Update tag
        if (item->tag) {
            free(item->tag);
            item->tag = NULL;
        }
        if (tag) {
            item->tag = strdup(tag);
            if (!item->tag) {
                xSemaphoreGive(g_slist_mutex);
                return ESP_ERR_NO_MEM;
            }
        }
        xSemaphoreGive(g_slist_mutex);
        return ESP_OK;
    }

    // Create new node
    esp_log_router_slist_t *new_log_router = calloc(1, sizeof(esp_log_router_slist_t));
    if (!new_log_router) {
        xSemaphoreGive(g_slist_mutex);
        return ESP_ERR_NO_MEM;
    }

    // Allocate and copy file path
    new_log_router->file_path = strdup(file_path);
    if (!new_log_router->file_path) {
        free(new_log_router);
        xSemaphoreGive(g_slist_mutex);
        return ESP_ERR_NO_MEM;
    }

    // Allocate and copy tag
    if (tag) {
        new_log_router->tag = strdup(tag);
        if (!new_log_router->tag) {
            free(new_log_router->file_path);
            free(new_log_router);
            xSemaphoreGive(g_slist_mutex);
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
        xSemaphoreGive(g_slist_mutex);
        return ESP_FAIL;
    }

    // Set log level
    new_log_router->level = level;
    g_level_max = MAX(g_level_max, new_log_router->level);

#if (CONFIG_LOG_ROUTER_BATCH_WRITE)
    // Initialize buffer and flush settings
    new_log_router->buffer_size = CONFIG_LOG_ROUTER_BUFFER_SIZE;
#if (CONFIG_LOG_ROUTER_BATCH_WRITE_BUFFER_PREFER_EXT_RAM)
    new_log_router->buffer = CALLOC_PREFER_EXTERNAL(1, new_log_router->buffer_size);
#else
    new_log_router->buffer = CALLOC_PREFER_PERFORMANCE(1, new_log_router->buffer_size);
#endif
    new_log_router->flush_timeout_ms = CONFIG_LOG_ROUTER_FLUSH_TIMEOUT_MS;
    if (!new_log_router->buffer) {
        fclose(new_log_router->log_fp);
        free(new_log_router->file_path);
        free(new_log_router);
        xSemaphoreGive(g_slist_mutex);
        return ESP_ERR_NO_MEM;
    }

    new_log_router->flush_timeout_ms = CONFIG_LOG_ROUTER_FLUSH_TIMEOUT_MS;
    new_log_router->buffer_pos = 0;
//    new_log_router->last_flush_time = (uint32_t)(esp_timer_get_time() / 1000);    // ignored when buffer_pos = 0
#endif

    // The list should still be under lock
    assert(MUTEX_LOCKED(g_slist_mutex));

    // Insert after the first node (if exists) or as first node
    if (SLIST_EMPTY(&g_esp_log_router_slist_head)) {
        // If list is empty, insert as first node and set vprintf redirect
        SLIST_INSERT_HEAD(&g_esp_log_router_slist_head, new_log_router, next);
        g_esp_log_router_vprintf = esp_log_set_vprintf(esp_log_router_flash_vprintf);
    } else {
        // Find the last node and insert after it
        esp_log_router_slist_t *last = SLIST_FIRST(&g_esp_log_router_slist_head);
        while (SLIST_NEXT(last, next) != NULL) {
            last = SLIST_NEXT(last, next);
        }
        SLIST_INSERT_AFTER(last, new_log_router, next);
    }

    // Release mutex
    xSemaphoreGive(g_slist_mutex);

    return ESP_OK;
}

void esp_log_router_to_console(bool output_console)
{
    g_esp_log_router_keep_console = output_console;
}

esp_err_t esp_log_router_sync_file(const char* file_path, struct stat *sbuf)
{
    if (!file_path) {
        return ESP_ERR_INVALID_ARG;
    }

    ESP_LOGI(TAG, "Sync logs to file: %s", file_path);

#if (CONFIG_LOG_ROUTER_RINGBUFFER)
    // There may be messages in the ringbuffer!
    drain_ringbuffer(true);
#endif

#if (!CONFIG_LOG_ROUTER_BATCH_WRITE)
    return ESP_OK;
#else
    // A per-file buffer needs to be flushed as well

    // Lock mutex while accessing the list
    xSemaphoreTake(g_slist_mutex, portMAX_DELAY);

    esp_log_router_slist_t *item = lookup_item(file_path);
    if (!item) {
        xSemaphoreGive(g_slist_mutex);
        return ESP_ERR_NOT_FOUND;
    }

    // Flush buffer. Also calls fsync.
    esp_err_t err = esp_log_router_flush_buffer(item);
    if (err != ESP_OK) {
        xSemaphoreGive(g_slist_mutex);
        return ESP_ERR_NOT_FOUND;
    }

    if (sbuf != NULL) {
        // While we have the mutex locked, check how large the file is
        int ret = fstat(fileno(item->log_fp), sbuf);

        if (ret != 0) {
            log_router_debug_printf("fstat failed: %s", strerror(errno));
            xSemaphoreGive(g_slist_mutex);
            return ESP_FAIL;
        }
    }

    // Release mutex
    xSemaphoreGive(g_slist_mutex);
    return ESP_OK;
#endif
}

esp_err_t esp_log_router_stop_file(const char* file_path)
{
    if (!file_path) {
        return ESP_ERR_INVALID_ARG;
    }

#if (CONFIG_LOG_ROUTER_RINGBUFFER)
    // There may be messages in the ringbuffer!
    drain_ringbuffer(true);
#endif

    // Lock mutex while accessing the list
    xSemaphoreTake(g_slist_mutex, portMAX_DELAY);

    esp_err_t found = ESP_ERR_NOT_FOUND;

    esp_log_router_slist_t *item, *prev = NULL;
    SLIST_FOREACH(item, &g_esp_log_router_slist_head, next) {
        if (strcmp(item->file_path, file_path) == 0) {
            if (prev) {
                SLIST_REMOVE_AFTER(prev, next);
            } else {
                SLIST_REMOVE_HEAD(&g_esp_log_router_slist_head, next);
            }

#if (CONFIG_LOG_ROUTER_BATCH_WRITE)
            // Flush buffer before closing
            esp_log_router_flush_buffer(item);
            free(item->buffer);
#endif
            // make sure file data is on disk before closing it
            fflush(item->log_fp);
            fsync(fileno(item->log_fp));
            fclose(item->log_fp);
            item->log_fp = NULL;

            free(item->file_path);
            if (item->tag) {
                free(item->tag);
            }

            free(item);

            // If this was the last item, restore original vprintf
            if (SLIST_EMPTY(&g_esp_log_router_slist_head) && g_esp_log_router_vprintf) {
                esp_log_set_vprintf(g_esp_log_router_vprintf);
                g_esp_log_router_vprintf = NULL;
            }

            ESP_LOGI(TAG, "Log router deleted: %s", file_path);

            found = ESP_OK;
            break;
        }
        prev = item;
    }

    // The maximum level may have reduced
    esp_log_level_t new_max = 0;    // a.k.a. ESP_LOG_NONE
//    esp_log_router_slist_t *item;
    SLIST_FOREACH(item, &g_esp_log_router_slist_head, next) {
        new_max = MAX(new_max, item->level);
    }
    g_level_max = new_max;  // Atomic write

    // Release mutex
    xSemaphoreGive(g_slist_mutex);

    return found;
}

esp_err_t esp_log_router_dump_file_messages(const char* file_path)
{
    if (!file_path) {
        return ESP_ERR_INVALID_ARG;
    }

    // Sync all pending messages, and record the file size (while the mutex is locked).
    struct stat st;
    esp_err_t err = esp_log_router_sync_file(file_path, &st);
    if (err != ESP_OK) {
        if (ESP_ERR_NOT_FOUND == err) {
            // The file is not being logged to at the moment, but that's fine.
            printf("Note: file '%s' is not currently a log target", file_path);

            int ret = stat(file_path, &st);
            if (ret != 0) {
                // No file at all?
                log_router_debug_printf("unable to stat '%s': %s", file_path, strerror(errno));
                return ESP_FAIL;
            }
        } else {
            return err;
        }
    }

    // Only print what's been sync'd - otherwise, there's a risk of printing message fragments.
    off_t todo = st.st_size;

    log_router_debug_printf("dumping %ld bytes from '%s'", todo, file_path);

    char dump_log_buffer[256];

    FILE *fp = fopen(file_path, "r");
    if (!fp) {
        printf("Failed to open file: %s\n", file_path);
        return ESP_FAIL;
    }

    printf("File content: %s\n", file_path);
    int line_count = 0;

    bool eof_or_error = false;
    while (todo > 0 && !eof_or_error) {
        size_t bytes_read = fread(dump_log_buffer, 1, sizeof(dump_log_buffer), fp);
        eof_or_error = bytes_read < sizeof(dump_log_buffer);

        printf("%s", dump_log_buffer);

        // Count number of lines
        for (int i = 0; i < bytes_read; i++) {
            if (dump_log_buffer[i] == '\n') {
                line_count++;
            }
        }

        todo -= bytes_read;

        vTaskDelay(pdMS_TO_TICKS(10)); /*!< Avoid triggering watchdog */
    }

    if (ferror(fp)) {
        printf("Read error on line %d\n", line_count + 1);
        return ESP_FAIL;
    }

    printf("End of file (total %d lines)\n", line_count);
    fclose(fp);

    assert(err == ESP_OK || err == ESP_ERR_NOT_FOUND);
    return err;
}
