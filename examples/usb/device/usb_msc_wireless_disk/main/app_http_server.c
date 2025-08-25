/* SPDX-FileCopyrightText: 2022-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <sys/param.h>
#include <dirent.h>
#include <ctype.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <inttypes.h>
#include "soc/gpio_sig_map.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_log.h"
#include "esp_vfs.h"
#include "esp_rom_gpio.h"
#include "esp_heap_caps.h"
#include "esp_http_server.h"
#include "freertos/ringbuf.h"
#include "esp_random.h"
#include "sdkconfig.h"
#include "esp_vfs_fat.h"

#ifdef CONFIG_WIFI_HTTP_ACCESS
/* Forward declaration */
struct file_server_data;

/* Max length a file path can have on storage */
#define FILE_PATH_MAX (ESP_VFS_PATH_MAX + CONFIG_FATFS_MAX_LFN)

/* Max size of an individual file. */
#define MAX_FILE_SIZE_MB  (CONFIG_HTTP_UPLOAD_MAX_FILE_SIZE_MB)
#define MAX_FILE_SIZE   (MAX_FILE_SIZE_MB * 1024 * 1024)
#define STR_HELPER(x) #x
#define STR(x) STR_HELPER(x)
#define MAX_FILE_SIZE_MB_STR  STR(MAX_FILE_SIZE_MB) "MB"

/* Read scratch buffer size */
#define FILE_READ_BUFFER_SIZE  (CONFIG_FILE_READ_BUFFER_SIZE * 1024)

/* Define actual values to use in code */
#define FILE_WRITE_TASK_STACK_SIZE (CONFIG_FILE_WRITE_TASK_STACK_SIZE)
#define FILE_WRITE_BUFFER_COUNT (CONFIG_FILE_WRITE_BUFFER_COUNT)
#define FILE_WRITE_BUFFER_SIZE (CONFIG_FILE_WRITE_BUFFER_SIZE * 1024)
#define FILE_DMA_BUFFER_SIZE (CONFIG_FILE_DMA_BUFFER_SIZE * 1024)
#define RINGBUF_SIZE (FILE_WRITE_BUFFER_SIZE * FILE_WRITE_BUFFER_COUNT)

/* All file uploads use async write system */

/* Item types for different data chunks - encoded in high 2 bits of file_id */
typedef enum {
    CHUNK_TYPE_FIRST = 0,  /* First chunk containing filepath (00) */
    CHUNK_TYPE_MIDDLE,     /* Middle chunk with no filepath  (01) */
    CHUNK_TYPE_FINAL       /* Final chunk of the file        (10) */
} chunk_type_t;

/* Bit manipulation macros for file_id */
#define FILE_ID_MASK 0x3FFF         /* Lower 14 bits for actual ID (0011 1111 1111 1111) */
#define TYPE_SHIFT 14               /* Shift amount for type bits  */
#define TYPE_MASK 0xC000            /* Upper 2 bits for type       (1100 0000 0000 0000) */

/* Encode type and ID into a single uint16_t */
#define ENCODE_FILE_ID(type, id) (((uint16_t)(type) << TYPE_SHIFT) | ((id) & FILE_ID_MASK))

/* Extract type from encoded file_id */
#define GET_CHUNK_TYPE(file_id) (((file_id) & TYPE_MASK) >> TYPE_SHIFT)

/* Extract actual ID from encoded file_id */
#define GET_FILE_ID(file_id) ((file_id) & FILE_ID_MASK)

/* Item header structure for ringbuffer items - fixed size and compact */
typedef struct __attribute__((packed))
{
    uint16_t file_id;               /* Encoded ID and type: [TT][ID 14bits] */
    uint16_t path_len;              /* Length of filepath string (only in FIRST chunks) */
    uint32_t data_size;             /* Actual data size */
} item_header_t;

/* Note: item_header_t should be 8 bytes with packed attribute */
#define ITEM_HEADER_SIZE sizeof(item_header_t)

/* Event group bits for file write synchronization */
#define FILE_WRITE_SUCCESS_BIT BIT0
#define FILE_WRITE_ERROR_BIT   BIT1
#define FILE_WRITE_DONE_BIT    (FILE_WRITE_SUCCESS_BIT | FILE_WRITE_ERROR_BIT)

struct file_server_data {
    /* Base path of file storage */
    char base_path[ESP_VFS_PATH_MAX + 1];

    /* Read scratch buffer for temporary storage during file transfer */
    char *file_read_buffer;

    /* RingBuffer for file data transfer */
    RingbufHandle_t file_ring_buffer;

    /* DMA buffer for file writing */
    char *file_dma_buffer;

    /* Event group for file write completion synchronization */
    EventGroupHandle_t file_event_group;
};

static const char *TAG = "file_server";

/* File writing task handle */
static TaskHandle_t file_write_task_handle = NULL;

/* Centralized error handling function for file write task */
static void handle_file_write_error(struct file_server_data *server_data,
                                    int *current_fd,
                                    char *current_filepath,
                                    uint32_t *current_file_id,
                                    size_t *dma_buffer_used,
                                    const char *error_reason)
{
    /* Log the error */
    ESP_LOGE(TAG, "File write error: %s", error_reason);

    /* Clean up file if open */
    if (*current_fd >= 0) {
        close(*current_fd);
        *current_fd = -1;

        /* Delete incomplete/failed file */
        if (current_filepath[0] != '\0') {
            unlink(current_filepath);
            ESP_LOGE(TAG, "Deleted failed file: %s", current_filepath);
        }
    }

    /* Reset all state variables */
    current_filepath[0] = '\0';
    *current_file_id = 0;
    *dma_buffer_used = 0;

    /* Notify user of error */
    xEventGroupSetBits(server_data->file_event_group, FILE_WRITE_ERROR_BIT);
}

/* File writing task function */
static void file_write_task(void *pvParameters)
{
    struct file_server_data *server_data = (struct file_server_data *)pvParameters;
    int current_fd = -1;  /* Use file descriptor instead of FILE pointer */
    char current_filepath[FILE_PATH_MAX] = {0};
    uint32_t current_file_id = 0;
    size_t dma_buffer_used = 0;  /* Track how much of DMA buffer is currently filled */

    ESP_LOGI(TAG, "File writing task started");

    while (1) {
        /* Receive item from ring buffer */
        size_t item_size;
        void *item = xRingbufferReceive(server_data->file_ring_buffer, &item_size, portMAX_DELAY);
        if (!item) {
            handle_file_write_error(server_data, &current_fd, current_filepath,
                                    &current_file_id, &dma_buffer_used,
                                    "Failed to receive item from ring buffer");
            continue;
        }

        /* Extract item header */
        item_header_t *header = (item_header_t *)item;

        /* Calculate data start position and size */
        void *data;
        size_t data_size = header->data_size;

        /* Process different types of data chunks */
        chunk_type_t chunk_type = (chunk_type_t)GET_CHUNK_TYPE(header->file_id);
        if (chunk_type == CHUNK_TYPE_FIRST) {
            /* First data chunk contains file path, read from after the header */
            char *filepath_ptr = (char *)item + sizeof(item_header_t);
            data = filepath_ptr + header->path_len + 1; // +1 for null terminator

            /* Close previous file if open and flush any pending data */
            if (current_fd >= 0) {
                handle_file_write_error(server_data, &current_fd, current_filepath,
                                        &current_file_id, &dma_buffer_used,
                                        "Closing previous file without FINAL chunk");
                /* Do not return item - continue processing this first chunk */
            }

            /* Open new file and reset DMA buffer */
            current_fd = open(filepath_ptr, O_WRONLY | O_CREAT | O_TRUNC, 0644);
            dma_buffer_used = 0;  // Reset DMA buffer for new file
            ESP_LOGI(TAG, "Opening file for writing: %s", filepath_ptr);
            if (current_fd < 0) {
                handle_file_write_error(server_data, &current_fd, current_filepath,
                                        &current_file_id, &dma_buffer_used,
                                        "Failed to open file for writing");
                vRingbufferReturnItem(server_data->file_ring_buffer, item);
                continue;
            }

            /* Save file path and ID */
            strncpy(current_filepath, filepath_ptr, FILE_PATH_MAX - 1);
            current_filepath[FILE_PATH_MAX - 1] = '\0'; // Ensure null termination
            current_file_id = GET_FILE_ID(header->file_id);
        } else {
            /* Middle and final chunks data follows directly after the header */
            data = (char *)item + sizeof(item_header_t);

            /* Check if file ID matches */
            uint16_t chunk_file_id = GET_FILE_ID(header->file_id);
            if (chunk_file_id != current_file_id || current_fd < 0) {
                /* Received chunk with non-matching file ID, or no file is currently open */
                ESP_LOGE(TAG, "Received chunk for unknown file ID: %u (current: %u)",
                         (unsigned int)chunk_file_id, (unsigned int)current_file_id);
                handle_file_write_error(server_data, &current_fd, current_filepath,
                                        &current_file_id, &dma_buffer_used,
                                        "File ID mismatch or no file open");
                vRingbufferReturnItem(server_data->file_ring_buffer, item);
                continue;
            }
        }

        /* Add data to DMA buffer using batched writing for optimal SD card performance */
        size_t remaining = data_size;
        char* data_ptr = (char*)data;

        /* Accumulate data in DMA buffer and write when full */
        while (remaining > 0) {
            size_t space_available = FILE_DMA_BUFFER_SIZE - dma_buffer_used;
            size_t copy_size = (remaining < space_available) ? remaining : space_available;

            /* Copy data into DMA buffer */
            memcpy(server_data->file_dma_buffer + dma_buffer_used, data_ptr, copy_size);
            dma_buffer_used += copy_size;
            data_ptr += copy_size;
            remaining -= copy_size;

            /* Write DMA buffer when full */
            if (dma_buffer_used == FILE_DMA_BUFFER_SIZE) {
                uint32_t start_time = esp_log_timestamp();
                ssize_t written = write(current_fd, server_data->file_dma_buffer, FILE_DMA_BUFFER_SIZE);
                uint32_t end_time = esp_log_timestamp();

                if (written != FILE_DMA_BUFFER_SIZE) {
                    ESP_LOGE(TAG, "DMA buffer write failed! %s (%d of %u bytes written)",
                             current_filepath, (int)written, (unsigned int)FILE_DMA_BUFFER_SIZE);
                    handle_file_write_error(server_data, &current_fd, current_filepath,
                                            &current_file_id, &dma_buffer_used,
                                            "DMA buffer write failed");
                    /* Return current item and continue to next iteration */
                    vRingbufferReturnItem(server_data->file_ring_buffer, item);
                    continue;
                }

                ESP_LOGD(TAG, "Wrote DMA buffer %u bytes to %s in %u ms (%.2f KB/s)",
                         (unsigned int)FILE_DMA_BUFFER_SIZE, current_filepath,
                         (unsigned int)(end_time - start_time),
                         (float)FILE_DMA_BUFFER_SIZE / (float)(end_time - start_time));
                dma_buffer_used = 0;
            }
        }

        /* Return item to the ring buffer */
        vRingbufferReturnItem(server_data->file_ring_buffer, item);

        /* If this is the final part of the file, close it */
        if (chunk_type == CHUNK_TYPE_FINAL) {
            /* Flush any remaining data in DMA buffer */
            if (dma_buffer_used > 0) {
                ssize_t written = write(current_fd, server_data->file_dma_buffer, dma_buffer_used);

                if (written != (ssize_t)dma_buffer_used) {
                    ESP_LOGE(TAG, "Final DMA buffer flush failed! %s (%d of %u bytes written)",
                             current_filepath, (int)written, (unsigned int)dma_buffer_used);
                    handle_file_write_error(server_data, &current_fd, current_filepath,
                                            &current_file_id, &dma_buffer_used,
                                            "Final DMA buffer flush failed");
                    continue;
                } else {
                    ESP_LOGD(TAG, "Final flush: %u bytes to %s",
                             (unsigned int)dma_buffer_used, current_filepath);
                }
                dma_buffer_used = 0;
            }

            /* Always close the file descriptor */
            if (current_fd >= 0) {
                fsync(current_fd);  /* Ensure data is written to storage device */
                close(current_fd);
                current_fd = -1;
            }

            /* Successful completion - no errors occurred */
            ESP_LOGI(TAG, "File writing complete: %s", current_filepath);
            xEventGroupSetBits(server_data->file_event_group, FILE_WRITE_SUCCESS_BIT);

            /* Reset state for next file */
            current_filepath[0] = '\0';
            current_file_id = 0;
            dma_buffer_used = 0;
        }
    }
}

/* URL decode function to convert percent-encoded characters to their original form */
static size_t url_decode(char *dst, const char *src, size_t dst_size)
{
    size_t dst_len = 0;
    const char *src_ptr = src;
    char *dst_ptr = dst;

    while (*src_ptr && dst_len < dst_size - 1) {
        if (*src_ptr == '%' && src_ptr[1] && src_ptr[2]) {
            /* Check if next two characters are valid hex digits */
            if (isxdigit((unsigned char)src_ptr[1]) && isxdigit((unsigned char)src_ptr[2])) {
                /* Convert hex digits to character */
                uint8_t hex1 = isdigit((unsigned char)src_ptr[1]) ? src_ptr[1] - '0' :
                               tolower((unsigned char)src_ptr[1]) - 'a' + 10;
                uint8_t hex2 = isdigit((unsigned char)src_ptr[2]) ? src_ptr[2] - '0' :
                               tolower((unsigned char)src_ptr[2]) - 'a' + 10;

                *dst_ptr++ = (char)((hex1 << 4) | hex2);
                src_ptr += 3;
                dst_len++;
            } else {
                /* Invalid hex sequence, copy as-is */
                *dst_ptr++ = *src_ptr++;
                dst_len++;
            }
        } else if (*src_ptr == '+') {
            /* Convert '+' to space (mainly for query parameters, but harmless in paths) */
            *dst_ptr++ = ' ';
            src_ptr++;
            dst_len++;
        } else {
            /* Copy character as-is */
            *dst_ptr++ = *src_ptr++;
            dst_len++;
        }
    }

    *dst_ptr = '\0';
    return dst_len;
}

/* URL encode function to convert characters to percent-encoded form for safe use in URLs */
static size_t url_encode(char *dst, const char *src, size_t dst_size)
{
    size_t dst_len = 0;
    const char *src_ptr = src;
    char *dst_ptr = dst;

    while (*src_ptr && dst_len < dst_size - 1) {
        char c = *src_ptr;

        /* Check if character needs encoding */
        if (isalnum((unsigned char)c) || c == '-' || c == '_' || c == '.' || c == '~' || c == '/') {
            /* Safe character, copy as-is */
            *dst_ptr++ = c;
            dst_len++;
        } else {
            /* Unsafe character, encode as %XX */
            if (dst_len + 3 >= dst_size) {
                /* Not enough space for encoding */
                break;
            }
            uint8_t high = (c >> 4) & 0x0F;
            uint8_t low = c & 0x0F;
            *dst_ptr++ = '%';
            *dst_ptr++ = (high < 10) ? ('0' + high) : ('A' + high - 10);
            *dst_ptr++ = (low < 10) ? ('0' + low) : ('A' + low - 10);
            dst_len += 3;
        }
        src_ptr++;
    }

    *dst_ptr = '\0';
    return dst_len;
}

/* Handler to redirect incoming GET request for /index.html to /
 * This can be overridden by uploading file with same name */
static esp_err_t index_html_get_handler(httpd_req_t *req)
{
    httpd_resp_set_status(req, "307 Temporary Redirect");
    httpd_resp_set_hdr(req, "Location", "/");
    httpd_resp_send(req, NULL, 0);  // Response body can be empty
    return ESP_OK;
}

/* Handler to respond with an icon file embedded in flash.
 * Browsers expect to GET website icon at URI /favicon.ico.
 * This can be overridden by uploading file with same name */
static esp_err_t favicon_get_handler(httpd_req_t *req)
{
    extern const unsigned char favicon_ico_start[] asm("_binary_favicon_ico_start");
    extern const unsigned char favicon_ico_end[]   asm("_binary_favicon_ico_end");
    const size_t favicon_ico_size = (favicon_ico_end - favicon_ico_start);
    httpd_resp_set_type(req, "image/x-icon");
    httpd_resp_send(req, (const char *)favicon_ico_start, favicon_ico_size);
    return ESP_OK;
}

/* Handler to respond with an style file embedded in flash.
 * Browsers expect to GET website style at URI /styles.css.
 * This can be overridden by uploading file with same name */
static esp_err_t styles_get_handler(httpd_req_t *req)
{
    extern const unsigned char styles_css_start[] asm("_binary_styles_css_start");
    extern const unsigned char styles_css_end[]   asm("_binary_styles_css_end");
    const size_t styles_css_size = (styles_css_end - styles_css_start);
    httpd_resp_set_type(req, "text/css");
    httpd_resp_send(req, (const char *)styles_css_start, styles_css_size);
    return ESP_OK;
}

/* Handler to respond with an upload page embedded in flash.
 * Browsers expect to GET website page at URI /upload.html.
 * This can be overridden by uploading file with same name */
static esp_err_t upload_page_get_handler(httpd_req_t *req)
{
    extern const unsigned char upload_html_start[] asm("_binary_upload_html_start");
    extern const unsigned char upload_html_end[]   asm("_binary_upload_html_end");
    const size_t upload_html_size = (upload_html_end - upload_html_start);
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, (const char *)upload_html_start, upload_html_size);
    return ESP_OK;
}

/* Handler to respond with an upload page embedded in flash.
 * Browsers expect to GET website page at URI /settings.html.
 * This can be overridden by uploading file with same name */
static esp_err_t settings_page_get_handler(httpd_req_t *req)
{
    extern const unsigned char settings_html_start[] asm("_binary_settings_html_start");
    extern const unsigned char settings_html_end[]   asm("_binary_settings_html_end");
    const size_t settings_html_size = (settings_html_end - settings_html_start);
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, (const char *)settings_html_start, settings_html_size);
    return ESP_OK;
}

/* Send HTTP response with a run-time generated html consisting of
 * a list of all files and folders under the requested path.
 * In case of SPIFFS this returns empty list when path is any
 * string other than '/', since SPIFFS doesn't support directories */
static esp_err_t http_resp_dir_html(httpd_req_t *req, const char *dirpath)
{
    char entrypath[FILE_PATH_MAX];
    char entrysize[16];
    const char *entrytype;

    struct dirent *entry;
    struct stat entry_stat;

    DIR *dir = opendir(dirpath);
    const size_t dirpath_len = strlen(dirpath);

    /* Retrieve the base path of file storage to construct the full path */
    strlcpy(entrypath, dirpath, sizeof(entrypath));

    if (!dir) {
        ESP_LOGE(TAG, "Failed to stat dir : %s", dirpath);
        /* Respond with 404 Not Found */
        httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "Directory does not exist");
        return ESP_FAIL;
    }

    /* Get handle to embedded file the header of file list page */
    extern const unsigned char file_list_1_start[] asm("_binary_file_list_1_html_start");
    extern const unsigned char file_list_1_end[]   asm("_binary_file_list_1_html_end");
    const size_t file_list_1_size = (file_list_1_end - file_list_1_start);

    extern const unsigned char file_list_2_start[] asm("_binary_file_list_2_html_start");
    extern const unsigned char file_list_2_end[]   asm("_binary_file_list_2_html_end");
    const size_t file_list_2_size = (file_list_2_end - file_list_2_start);

    httpd_resp_send_chunk(req, (const char *)file_list_1_start, file_list_1_size);

    /* Iterate over all files / folders and fetch their names and sizes */
    uint32_t file_num_count = 0;
    uint32_t dir_num_count = 0;
    while ((entry = readdir(dir)) != NULL) {
        entrytype = (entry->d_type == DT_DIR ? "directory" : "file");
        if (entry->d_type == DT_DIR) {
            /* Skip the current and parent directory entries */
            if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
                continue;
            }
            dir_num_count++;
        } else {
            file_num_count++;
        }
        strlcpy(entrypath + dirpath_len, entry->d_name, sizeof(entrypath) - dirpath_len);
        if (stat(entrypath, &entry_stat) == -1) {
            ESP_LOGE(TAG, "Failed to stat %s : %s", entrytype, entry->d_name);
            continue;
        }
        sprintf(entrysize, "%ld", entry_stat.st_size);
        ESP_LOGD(TAG, "Found %s : %s (%s bytes)", entrytype, entry->d_name, entrysize);

        /* URL encode the filename for safe use in HTML href attributes */
        char encoded_filename[CONFIG_FATFS_MAX_LFN * 3 + 1];
        url_encode(encoded_filename, entry->d_name, sizeof(encoded_filename));

        /* Send chunk of HTML file containing table entries with file name and size */
        httpd_resp_sendstr_chunk(req, "<tr><td class=\"");
        httpd_resp_sendstr_chunk(req, entrytype);
        httpd_resp_sendstr_chunk(req, "\"><a href=\"");
        httpd_resp_sendstr_chunk(req, req->uri);
        httpd_resp_sendstr_chunk(req, encoded_filename);
        if (entry->d_type == DT_DIR) {
            httpd_resp_sendstr_chunk(req, "/");
        }
        httpd_resp_sendstr_chunk(req, "\">");
        httpd_resp_sendstr_chunk(req, entry->d_name);
        httpd_resp_sendstr_chunk(req, "</a></td><td>");
        httpd_resp_sendstr_chunk(req, entrysize);
        httpd_resp_sendstr_chunk(req, "</td><td>");
        httpd_resp_sendstr_chunk(req, "<button class=\"deleteButton\" filepath=\"");
        httpd_resp_sendstr_chunk(req, req->uri);
        httpd_resp_sendstr_chunk(req, encoded_filename);
        httpd_resp_sendstr_chunk(req, "\">Delete</button>");
        httpd_resp_sendstr_chunk(req, "</td></tr>\n");
    }
    closedir(dir);
    ESP_LOGI(TAG, "Found %u files and %u directories in %s",
             (unsigned int)file_num_count, (unsigned int)dir_num_count, dirpath);

    /* Finish the file list table */
    httpd_resp_send_chunk(req, (const char *)file_list_2_start, file_list_2_size);

    /* Send empty chunk to signal HTTP response completion */
    httpd_resp_sendstr_chunk(req, NULL);
    return ESP_OK;
}

#define IS_FILE_EXT(filename, ext) \
    (strcasecmp(&filename[strlen(filename) - sizeof(ext) + 1], ext) == 0)

/* Set HTTP response content type according to file extension */
static esp_err_t set_content_type_from_file(httpd_req_t *req, const char *filename)
{
    if (IS_FILE_EXT(filename, ".pdf")) {
        return httpd_resp_set_type(req, "application/pdf");
    } else if (IS_FILE_EXT(filename, ".html")) {
        return httpd_resp_set_type(req, "text/html");
    } else if (IS_FILE_EXT(filename, ".png")) {
        return httpd_resp_set_type(req, "image/png");
    } else if (IS_FILE_EXT(filename, ".jpg") || IS_FILE_EXT(filename, ".jpeg")) {
        return httpd_resp_set_type(req, "image/jpeg");
    } else if (IS_FILE_EXT(filename, ".gif")) {
        return httpd_resp_set_type(req, "image/gif");
    } else if (IS_FILE_EXT(filename, ".ico")) {
        return httpd_resp_set_type(req, "image/x-icon");
    }
    /* This is a limited set only */
    /* For any other type always set as plain text */
    return httpd_resp_set_type(req, "text/plain");
}

/* Copies the full path into destination buffer and returns
 * pointer to path (skipping the preceding base path) with URL decoding */
static const char *get_path_from_uri(char *dest, const char *base_path, const char *uri, size_t destsize)
{
    const size_t base_pathlen = strlen(base_path);
    size_t pathlen = strlen(uri);

    const char *quest = strchr(uri, '?');
    if (quest) {
        pathlen = MIN(pathlen, quest - uri);
    }
    const char *hash = strchr(uri, '#');
    if (hash) {
        pathlen = MIN(pathlen, hash - uri);
    }

    /* Create temporary buffer for URL decoding */
    char *temp_uri = alloca(pathlen + 1);
    strlcpy(temp_uri, uri, pathlen + 1);

    /* URL decode the path portion */
    char *decoded_path = alloca(pathlen + 1);
    size_t decoded_len = url_decode(decoded_path, temp_uri, pathlen + 1);

    /* Check if full path (base + decoded path) fits in destination buffer */
    if (base_pathlen + decoded_len + 1 > destsize) {
        /* Full path string won't fit into destination buffer */
        return NULL;
    }

    /* Construct full path (base + decoded path) */
    strcpy(dest, base_path);
    strcpy(dest + base_pathlen, decoded_path);

    /* Return pointer to path, skipping the base */
    return dest + base_pathlen;
}

/* Handler to download a file kept on the server */
static esp_err_t download_get_handler(httpd_req_t *req)
{
    char filepath[FILE_PATH_MAX];
    struct stat file_stat;
    const char *filename = get_path_from_uri(filepath, ((struct file_server_data *)req->user_ctx)->base_path,
                                             req->uri, sizeof(filepath));
    if (!filename) {
        ESP_LOGE(TAG, "Filename is too long");
        /* Respond with 500 Internal Server Error */
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Filename too long");
        return ESP_FAIL;
    }

    /* If name has trailing '/', respond with directory contents */
    if (filename[strlen(filename) - 1] == '/') {
        return http_resp_dir_html(req, filepath);
    }

    if (stat(filepath, &file_stat) == -1) {
        /* If file not present on SPIFFS check if URI
         * corresponds to one of the hardcoded paths */
        if (strcmp(filename, "/index.html") == 0) {
            return index_html_get_handler(req);
        } else if (strcmp(filename, "/favicon.ico") == 0) {
            return favicon_get_handler(req);
        } else if (strcmp(filename, "/settings.html") == 0) {
            return settings_page_get_handler(req);
        } else if (strcmp(filename, "/upload.html") == 0) {
            return upload_page_get_handler(req);
        } else if (strcmp(filename, "/styles.css") == 0) {
            return styles_get_handler(req);
        }
        ESP_LOGE(TAG, "Failed to stat file : %s", filepath);
        /* Respond with 404 Not Found */
        httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "File does not exist");
        return ESP_FAIL;
    }

    int fd = open(filepath, O_RDONLY, 0);
    if (fd == -1) {
        ESP_LOGE(TAG, "Failed to read existing file : %s", filepath);
        /* Respond with 500 Internal Server Error */
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to read existing file");
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "Sending file : %s (%ld bytes)...", filename, file_stat.st_size);
    set_content_type_from_file(req, filename);

    /* Retrieve the pointer to file_read_buffer buffer for temporary storage */
    char *chunk = ((struct file_server_data *)req->user_ctx)->file_read_buffer;
    size_t chunksize;
    do {
        /* Read file in chunks into the file_read_buffer buffer */
        chunksize = read(fd, chunk, FILE_READ_BUFFER_SIZE);
        ESP_LOGD(TAG, "Read %u bytes", (unsigned int)chunksize);

        if (chunksize > 0) {
            /* Send the buffer contents as HTTP response chunk */
            if (httpd_resp_send_chunk(req, chunk, chunksize) != ESP_OK) {
                close(fd);
                ESP_LOGE(TAG, "File sending failed!");
                /* Abort sending file */
                httpd_resp_sendstr_chunk(req, NULL);
                /* Respond with 500 Internal Server Error */
                httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to send file");
                return ESP_FAIL;
            }
        }

        /* Keep looping till the whole file is sent */
    } while (chunksize != 0);

    /* Close file after sending complete */
    close(fd);
    ESP_LOGI(TAG, "File sending complete");

    /* Respond with an empty chunk to signal HTTP response completion */
    httpd_resp_send_chunk(req, NULL, 0);
    return ESP_OK;
}

#if defined (CONFIG_IDF_TARGET_ESP32S2) || defined (CONFIG_IDF_TARGET_ESP32S3)
// To generate a disconnect event
static void usbd_vbus_enable(bool enable)
{
    esp_rom_gpio_connect_in_signal(enable ? GPIO_MATRIX_CONST_ONE_INPUT : GPIO_MATRIX_CONST_ZERO_INPUT, USB_OTG_VBUSVALID_IN_IDX, 0);
    esp_rom_gpio_connect_in_signal(enable ? GPIO_MATRIX_CONST_ONE_INPUT : GPIO_MATRIX_CONST_ZERO_INPUT, USB_SRP_BVALID_IN_IDX, 0);
    esp_rom_gpio_connect_in_signal(enable ? GPIO_MATRIX_CONST_ONE_INPUT : GPIO_MATRIX_CONST_ZERO_INPUT, USB_SRP_SESSEND_IN_IDX, 1);
    return;
}
#elif defined (CONFIG_IDF_TARGET_ESP32P4)
static void usbd_vbus_enable(bool enable)
{
    esp_rom_gpio_connect_in_signal(enable ? GPIO_MATRIX_CONST_ONE_INPUT : GPIO_MATRIX_CONST_ZERO_INPUT, USB_OTG11_VBUSVALID_PAD_IN_IDX, 0);
    esp_rom_gpio_connect_in_signal(enable ? GPIO_MATRIX_CONST_ONE_INPUT : GPIO_MATRIX_CONST_ZERO_INPUT, USB_SRP_BVALID_PAD_IN_IDX, 0);
    esp_rom_gpio_connect_in_signal(enable ? GPIO_MATRIX_CONST_ONE_INPUT : GPIO_MATRIX_CONST_ZERO_INPUT, USB_SRP_SESSEND_PAD_IN_IDX, 1);
    return;
}
#endif

/* Handler to upload a file onto the server */
static esp_err_t upload_post_handler(httpd_req_t *req)
{
    char filepath[FILE_PATH_MAX];
    struct stat file_stat;
    struct file_server_data *server_data = (struct file_server_data *)req->user_ctx;

    /* Skip leading "/upload" from URI to get filename */
    /* Note sizeof() counts NULL termination hence the -1 */
    const char *filename = get_path_from_uri(filepath, server_data->base_path,
                                             req->uri + sizeof("/upload") - 1, sizeof(filepath));
    if (!filename) {
        /* Respond with 500 Internal Server Error */
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Filename too long");
        return ESP_FAIL;
    }

    /* Filename cannot have a trailing '/' */
    if (filename[strlen(filename) - 1] == '/') {
        ESP_LOGE(TAG, "Invalid filename : %s", filename);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Invalid filename");
        return ESP_FAIL;
    }

    if (stat(filepath, &file_stat) == 0) {
        ESP_LOGE(TAG, "File already exists : %s", filepath);
        /* Respond with 400 Bad Request */
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "File already exists");
        return ESP_FAIL;
    }

    /* File cannot be larger than a limit, unless unlimited (-1) */
    if (MAX_FILE_SIZE_MB != -1 && req->content_len > MAX_FILE_SIZE) {
        ESP_LOGE(TAG, "File too large : %d bytes", req->content_len);
        /* Respond with 400 Bad Request */
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST,
                            "File size must be less than " MAX_FILE_SIZE_MB_STR);
        /* Return failure to close underlying connection else the
         * incoming file content will keep the socket busy */
        return ESP_FAIL;
    }

    /* Use the async write system for all files */
    int remaining = req->content_len;
    bool file_write_success = true;
    xEventGroupClearBits(server_data->file_event_group, FILE_WRITE_DONE_BIT);
    /* Generate random file ID to identify different chunks of the same file, limited to 14 bits */
    uint16_t file_id = (uint16_t)(esp_random() & FILE_ID_MASK);
    bool is_first_chunk = true;

    ESP_LOGI(TAG, "Starting upload of %s with file ID: %u, size: %d bytes",
             filename, (unsigned int)file_id, req->content_len);

    while (remaining > 0 && file_write_success) {
        ESP_LOGD(TAG, "Remaining size: %d bytes", remaining);
        int to_read = MIN(remaining, FILE_WRITE_BUFFER_SIZE);

        /* Calculate required memory size */
        size_t header_size = sizeof(item_header_t);
        size_t path_size = 0;
        size_t buffer_size = 0;

        if (is_first_chunk) {
            /* First chunk needs extra space to store the file path */
            path_size = strlen(filepath) + 1; // Including null terminator
            buffer_size = header_size + path_size + to_read;
        } else {
            /* Middle and final chunks only need header plus data space */
            buffer_size = header_size + to_read;
        }

        /* Get buffer of sufficient size from the RingBuffer */
        void *buf = NULL;
        if (xRingbufferSendAcquire(server_data->file_ring_buffer, &buf, buffer_size, portMAX_DELAY) != pdTRUE) {
            ESP_LOGE(TAG, "Failed to acquire ring buffer");
            file_write_success = false;
            break;
        }

        /* First set up the item header */
        item_header_t *header = (item_header_t *)buf;

        /* Set header information */
        if (is_first_chunk) {
            /* First chunk: encode type and save path length */
            header->file_id = ENCODE_FILE_ID(CHUNK_TYPE_FIRST, file_id);
            header->path_len = path_size - 1; // Not including null terminator

            /* Copy file path after the header */
            char *path_ptr = (char*)buf + header_size;
            strcpy(path_ptr, filepath);

            /* Mark that it's no longer the first chunk */
            is_first_chunk = false;
        } else if (remaining - to_read <= 0) {
            /* Final chunk: encode type as FINAL */
            header->file_id = ENCODE_FILE_ID(CHUNK_TYPE_FINAL, file_id);
            header->path_len = 0;
        } else {
            /* Middle chunk: encode type as MIDDLE */
            header->file_id = ENCODE_FILE_ID(CHUNK_TYPE_MIDDLE, file_id);
            header->path_len = 0;
        }

        header->data_size = to_read;

        /* Prepare location to receive HTTP data */
        char *data_buf;
        if (GET_CHUNK_TYPE(header->file_id) == CHUNK_TYPE_FIRST) {
            data_buf = (char*)buf + header_size + path_size;
        } else {
            data_buf = (char*)buf + header_size;
        }

        int total_received = 0;

        while (total_received < to_read) {
            int received = httpd_req_recv(req, data_buf + total_received, to_read - total_received);
            if (received <= 0) {
                if (received == HTTPD_SOCK_ERR_TIMEOUT) {
                    continue;
                }
                /* Cancel buffer acquisition on error */
                xRingbufferSendComplete(server_data->file_ring_buffer, NULL);
                unlink(filepath);
                ESP_LOGE(TAG, "File reception failed!");
                httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to receive file");
                return ESP_FAIL;
            }
            total_received += received;
        }

        /* Complete buffer sending */
        if (xRingbufferSendComplete(server_data->file_ring_buffer, buf) != pdTRUE) {
            ESP_LOGE(TAG, "Failed to complete buffer send");
            file_write_success = false;
            break;
        }

        remaining -= total_received;
    }

    if (!file_write_success) {
        /* Send error if queueing failed */
        unlink(filepath);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Server write queue full");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "File writing in background: %s (%d bytes)", filename, req->content_len);

    /* Wait for file write completion or timeout */
    TickType_t timeout_ticks = (CONFIG_FILE_WRITE_TIMEOUT_MS <= 0) ? portMAX_DELAY : pdMS_TO_TICKS(CONFIG_FILE_WRITE_TIMEOUT_MS);
    EventBits_t bits = xEventGroupWaitBits(
                           server_data->file_event_group,
                           FILE_WRITE_DONE_BIT,
                           pdTRUE,  /* Clear bits after waiting */
                           pdFALSE, /* Wait for any bit (OR) */
                           timeout_ticks
                       );

    if (bits & FILE_WRITE_SUCCESS_BIT) {
        ESP_LOGI(TAG, "File upload and write successful: %s", filename);
    } else if (bits & FILE_WRITE_ERROR_BIT) {
        ESP_LOGE(TAG, "File write failed: %s", filename);
        /* File was already deleted by the write task */
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "File write failed");
        return ESP_FAIL;
    } else {
        ESP_LOGE(TAG, "File write timeout after %d ms: %s", CONFIG_FILE_WRITE_TIMEOUT_MS, filename);
        /* Timeout occurred, file might still be writing */
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "File write timeout");
        return ESP_FAIL;
    }

#ifdef CONFIG_HTTP_REDIRECT_TO_ROOT
    /* Redirect onto root to see the updated file list */
    httpd_resp_set_status(req, "303 See Other");
    httpd_resp_set_hdr(req, "Location", "/");
    httpd_resp_sendstr(req, "File uploaded successfully");
#else
    /* Just send success response without redirect */
    httpd_resp_set_status(req, "200 OK");
    httpd_resp_sendstr(req, "File uploaded successfully");
#endif

    return ESP_OK;
}

/* Handler to delete a file from the server */
static esp_err_t delete_post_handler(httpd_req_t *req)
{
    char filepath[FILE_PATH_MAX];
    struct stat file_stat;

    /* Skip leading "/delete" from URI to get filename */
    /* Note sizeof() counts NULL termination hence the -1 */
    const char *filename = get_path_from_uri(filepath, ((struct file_server_data *)req->user_ctx)->base_path,
                                             req->uri  + sizeof("/delete") - 1, sizeof(filepath));
    if (!filename) {
        /* Respond with 500 Internal Server Error */
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Filename too long");
        return ESP_FAIL;
    }

    /* Filename cannot have a trailing '/' */
    if (filename[strlen(filename) - 1] == '/') {
        ESP_LOGE(TAG, "Invalid filename : %s", filename);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Invalid filename");
        return ESP_FAIL;
    }

    if (stat(filepath, &file_stat) == -1) {
        ESP_LOGE(TAG, "File does not exist : %s", filename);
        /* Respond with 400 Bad Request */
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "File does not exist");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Deleting file : %s", filename);
    /* Delete file */
    unlink(filepath);

#ifdef CONFIG_HTTP_REDIRECT_TO_ROOT
    /* Redirect onto root to see the updated file list */
    httpd_resp_set_status(req, "303 See Other");
    httpd_resp_set_hdr(req, "Location", "/");
    httpd_resp_sendstr(req, "File deleted successfully");
#else
    /* Just send success response without redirect */
    httpd_resp_set_status(req, "200 OK");
    httpd_resp_sendstr(req, "File deleted successfully");
#endif
    return ESP_OK;
}

extern esp_err_t app_wifi_set_wifi(char * mode, char *ap_ssid, char *ap_passwd, char *sta_ssid, char *sta_passwd);

/* Handler to set a setting from the server */
static esp_err_t setting_get_handler(httpd_req_t *req)
{
    char query[160];
    char mode[16], ap_ssid[32], ap_passwd[32], sta_ssid[32], sta_passwd[32];

    if (httpd_req_get_url_query_str(req, query, sizeof(query)) == ESP_OK) {
        ESP_LOGI(TAG, "Query: %s", query);

        if (httpd_query_key_value(query, "wifimode", mode, sizeof(mode)) == ESP_OK) {
            ESP_LOGI(TAG, "WIFI Mode: %s", mode);
        }

        if (httpd_query_key_value(query, "apssid", ap_ssid, sizeof(ap_ssid)) == ESP_OK) {
            ESP_LOGI(TAG, "AP SSID: %s", ap_ssid);
        }

        if (httpd_query_key_value(query, "appassword", ap_passwd, sizeof(ap_passwd)) == ESP_OK) {
            ESP_LOGI(TAG, "AP password: %s", ap_passwd);
        }

        if (httpd_query_key_value(query, "stassid", sta_ssid, sizeof(sta_ssid)) == ESP_OK) {
            ESP_LOGI(TAG, "STA SSID: %s", sta_ssid);
        }

        if (httpd_query_key_value(query, "stapassword", sta_passwd, sizeof(sta_passwd)) == ESP_OK) {
            ESP_LOGI(TAG, "STA password: %s", sta_passwd);
        }

        app_wifi_set_wifi(mode, ap_ssid, ap_passwd, sta_ssid, sta_passwd);
    } else {
        return settings_page_get_handler(req);
    }

    httpd_resp_send(req, "Settings updated! Please reconnect!", HTTPD_RESP_USE_STRLEN);
    // Reset to configured wifi mode
    esp_restart();
    return ESP_OK;
}

static esp_err_t reset_msc_get_handler(httpd_req_t *req)
{
    usbd_vbus_enable(false);
    vTaskDelay(20 / portTICK_PERIOD_MS);
    usbd_vbus_enable(true);

    httpd_resp_set_status(req, "200 OK");
    httpd_resp_set_type(req, "text/plain");
    httpd_resp_sendstr(req, "MSC Reset Success");
    return ESP_OK;
}

/* Function to start the file server */
esp_err_t start_file_server(const char *base_path)
{
    static struct file_server_data *server_data = NULL;

    /* Validate file storage base path */
    if (!base_path) { // || strcmp(base_path, "/spiflash") != 0) {
        ESP_LOGE(TAG, "base path can't be NULL");
        return ESP_ERR_INVALID_ARG;
    }

    if (server_data) {
        ESP_LOGE(TAG, "File server already started");
        return ESP_ERR_INVALID_STATE;
    }

    /* Allocate memory for server data */
    server_data = calloc(1, sizeof(struct file_server_data));
    if (!server_data) {
        ESP_LOGE(TAG, "Failed to allocate memory for server data");
        return ESP_ERR_NO_MEM;
    }
    server_data->file_dma_buffer = heap_caps_malloc(FILE_DMA_BUFFER_SIZE, MALLOC_CAP_DMA);
    if (!server_data->file_dma_buffer) {
        ESP_LOGE(TAG, "Failed to allocate DMA buffer");
        free(server_data);
        return ESP_ERR_NO_MEM;
    }

    /* Allocate memory for file read buffer */
    server_data->file_read_buffer = heap_caps_malloc(FILE_READ_BUFFER_SIZE, MALLOC_CAP_DMA);
    if (!server_data->file_read_buffer) {
        ESP_LOGE(TAG, "Failed to allocate read buffer");
        heap_caps_free(server_data->file_dma_buffer);
        free(server_data);
        return ESP_ERR_NO_MEM;
    }

    strlcpy(server_data->base_path, base_path,
            sizeof(server_data->base_path));

    /* Create ring buffer for zero-copy data transfer */
    server_data->file_ring_buffer = xRingbufferCreate(RINGBUF_SIZE, RINGBUF_TYPE_NOSPLIT);
    if (!server_data->file_ring_buffer) {
        ESP_LOGE(TAG, "Failed to create ring buffer");
        free(server_data->file_read_buffer);
        heap_caps_free(server_data->file_dma_buffer);
        free(server_data);
        return ESP_ERR_NO_MEM;
    }

    /* Create event group for file write synchronization */
    server_data->file_event_group = xEventGroupCreate();
    if (!server_data->file_event_group) {
        ESP_LOGE(TAG, "Failed to create event group");
        vRingbufferDelete(server_data->file_ring_buffer);
        free(server_data->file_read_buffer);
        heap_caps_free(server_data->file_dma_buffer);
        free(server_data);
        return ESP_ERR_NO_MEM;
    }

    /* Print buffer configuration information */
    ESP_LOGI(TAG, "File server buffer configuration:");
    ESP_LOGI(TAG, "  - Read buffer: %u KB", (unsigned int)(FILE_READ_BUFFER_SIZE / 1024));
    ESP_LOGI(TAG, "  - DMA buffer: %u KB", (unsigned int)(FILE_DMA_BUFFER_SIZE / 1024));
    ESP_LOGI(TAG, "  - Write buffer size: %u KB × %u items = %u KB total",
             (unsigned int)(FILE_WRITE_BUFFER_SIZE / 1024),
             (unsigned int)FILE_WRITE_BUFFER_COUNT,
             (unsigned int)(RINGBUF_SIZE / 1024));

    /* Start the httpd server */
    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.stack_size = 8192;
    config.keep_alive_enable = true;

    /* Create file write task */
    BaseType_t task_created = xTaskCreate(
                                  file_write_task,
                                  "file_write_task",
                                  FILE_WRITE_TASK_STACK_SIZE,
                                  server_data,
                                  config.task_priority + 1,
                                  &file_write_task_handle);

    if (task_created != pdPASS) {
        ESP_LOGE(TAG, "Failed to create file write task");
        /* Free resources */
        vEventGroupDelete(server_data->file_event_group);
        vRingbufferDelete(server_data->file_ring_buffer);
        free(server_data->file_read_buffer);
        heap_caps_free(server_data->file_dma_buffer);
        free(server_data);
        return ESP_FAIL;
    }

    /* Use the URI wildcard matching function in order to
     * allow the same handler to respond to multiple different
     * target URIs which match the wildcard scheme */
    config.uri_match_fn = httpd_uri_match_wildcard;

    ESP_LOGI(TAG, "Starting HTTP Server");
    if (httpd_start(&server, &config) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start file server!");
        return ESP_FAIL;
    }

    /* URI handler for set a setting from server */
    httpd_uri_t setting = {
        .uri       = "/settings.html*",   // Match all URIs of type /delete/path/to/file
        .method    = HTTP_GET,
        .handler   = setting_get_handler,
        .user_ctx  = server_data    // Pass server data as context
    };
    httpd_register_uri_handler(server, &setting);

    /* URI handler for reset_msc */
    httpd_uri_t reset_msc = {
        .uri       = "/reset_msc",   // Match all URIs of type /delete/path/to/file
        .method    = HTTP_GET,
        .handler   = reset_msc_get_handler,
        .user_ctx  = server_data    // Pass server data as context
    };
    httpd_register_uri_handler(server, &reset_msc);

    /* URI handler for getting uploaded files */
    httpd_uri_t file_download = {
        .uri       = "/*",  // Match all URIs of type /path/to/file
        .method    = HTTP_GET,
        .handler   = download_get_handler,
        .user_ctx  = server_data    // Pass server data as context
    };
    httpd_register_uri_handler(server, &file_download);

    /* URI handler for uploading files to server */
    httpd_uri_t file_upload = {
        .uri       = "/upload/*",   // Match all URIs of type /upload/path/to/file
        .method    = HTTP_POST,
        .handler   = upload_post_handler,
        .user_ctx  = server_data    // Pass server data as context
    };
    httpd_register_uri_handler(server, &file_upload);

    /* URI handler for deleting files from server */
    httpd_uri_t file_delete = {
        .uri       = "/delete/*",   // Match all URIs of type /delete/path/to/file
        .method    = HTTP_POST,
        .handler   = delete_post_handler,
        .user_ctx  = server_data    // Pass server data as context
    };
    httpd_register_uri_handler(server, &file_delete);

    return ESP_OK;
}
#endif
