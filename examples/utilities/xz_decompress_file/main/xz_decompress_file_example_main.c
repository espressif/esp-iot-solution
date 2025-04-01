/*
 * SPDX-FileCopyrightText: 2022-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include <string.h>

#include "esp_err.h"
#include "esp_log.h"

#include "xz_decompress.h"
#include "xz.h"

#define IO_BUFFER_LEN (2048)

extern const uint8_t origin_file_start[] asm("_binary_hello_txt_start");
extern const uint8_t origin_file_end[] asm("_binary_hello_txt_end");
extern const uint8_t xz_compressed_file_start[] asm("_binary_hello_txt_xz_start");
extern const uint8_t xz_compressed_file_end[] asm("_binary_hello_txt_xz_end");

static int decompressed_count;
static size_t compressed_file_length;
static size_t filled_length;

static const char *TAG = "xz decompress";

static void error(const char *msg)
{
    ESP_LOGE("xz_test", "%s", msg);
}

static int fill(void *buf, unsigned int size)
{
    uint32_t len = (compressed_file_length - filled_length > size) ? size : compressed_file_length - filled_length;
    if (len > 0) {
        memcpy(buf, xz_compressed_file_start + filled_length, len);
        filled_length += len;
    }

    return len;
}

static int flush(void *buf, unsigned int size)
{
    return fwrite(buf, 1, size, stdout); // print the output
}

static void test_buf_to_buf(void)
{
    unsigned char *out_buf = (unsigned char *)malloc(IO_BUFFER_LEN * sizeof(unsigned char));
    memset(out_buf, 0, IO_BUFFER_LEN * sizeof(unsigned char)); // Note, out_buf need to be large enough
    /**
     * Read compressed data from in_buf, and write decompressed data to out_buf.
     * When you can estimate how much data will be extracted, you can use this API like this.
     */
    int ret = xz_decompress((unsigned char *)xz_compressed_file_start, compressed_file_length, NULL, NULL, (unsigned char *)out_buf, &decompressed_count, &error);
    // Display the read contents from the decompressed buf
    ESP_LOGI(TAG, "decompress data:\n%s", out_buf);
    ESP_LOGI(TAG, "ret = %d, decompressed count is %d", ret, decompressed_count);
    free(out_buf);
}

static void test_buf_to_cb(void)
{
    /**
     * Read compressed data from in_buf, and call flush() to write the decompressed data
     * to the specified partition/buffer chunk by chunk.
     * When you can not estimate how much data will be extracted, you can use this API like this.
     */
    int ret = xz_decompress((unsigned char *)xz_compressed_file_start, compressed_file_length, NULL, &flush, NULL, &decompressed_count, &error);
    ESP_LOGI(TAG, "ret = %d; decompressed count = %d", ret, decompressed_count);
}

static void test_cb_to_cb(void)
{
    /**
     * Call fill() to read compressed data from partition/buffer, and call flush() to write the decompressed data
     * to the specified partition/buffer chunk by chunk.
     * This is the memory friendly usage of the API, because it can read and write data in chunks instead of read
     * or write all data at once.
     */
    int ret = xz_decompress(NULL, 0, &fill, &flush, NULL, &decompressed_count, &error);
    ESP_LOGI(TAG, "ret = %d; decompressed count = %d", ret, decompressed_count);
}

static void test_cb_to_buf(void)
{
    unsigned char *out_buf = (unsigned char *)malloc(IO_BUFFER_LEN * sizeof(unsigned char));
    memset(out_buf, 0, IO_BUFFER_LEN * sizeof(unsigned char)); // Note, out_buf need to be large enough
    unsigned char *in_buf = (unsigned char *)malloc(IO_BUFFER_LEN * sizeof(unsigned char));
    memset(in_buf, 0, IO_BUFFER_LEN * sizeof(unsigned char));
    /**
     * Call fill() to read compressed data from partition/buffer chunk by chunk, and write decompressed data to out_buf.
     * The in_buf used to store the data tramsmited by fill().
     */
    int ret = xz_decompress(in_buf, 0, &fill, NULL, out_buf, &decompressed_count, &error);
    ESP_LOGI(TAG, "decompress:\n%s", out_buf);
    ESP_LOGI(TAG, "ret = %d; decompressed count = %d", ret, decompressed_count);
    free(out_buf);
    free(in_buf);
}

#define CHUNK_SIZE 4096
static void test_chunked_api(void)
{
    ESP_LOGI(TAG, "Using chunked decompression API");

    // Initialize the decoder with prealloc mode to control memory usage
    // Using a smaller dictionary size reduces memory requirements
    struct xz_dec *decoder = xz_dec_init(XZ_PREALLOC, 1 << 16); // 64KB dictionary
    if (!decoder) {
        ESP_LOGE(TAG, "Failed to initialize XZ decoder");
        return;
    }

    uint8_t *in_buffer = malloc(CHUNK_SIZE);
    uint8_t *out_buffer = malloc(CHUNK_SIZE);

    if (!in_buffer || !out_buffer) {
        ESP_LOGE(TAG, "Failed to allocate buffers");
        free(in_buffer);
        free(out_buffer);
        xz_dec_end(decoder);
        return;
    }

    struct xz_buf buffer = {
        .in = in_buffer,
        .in_pos = 0,
        .in_size = 0,
        .out = out_buffer,
        .out_pos = 0,
        .out_size = CHUNK_SIZE
    };

    // Track total bytes processed
    size_t total_in = 0;
    size_t total_out = 0;
    size_t remaining = compressed_file_length;

    // Process the compressed data in chunks
    enum xz_ret ret = XZ_OK;
    while (ret != XZ_STREAM_END) {
        if (buffer.in_pos == buffer.in_size && remaining > 0) {
            size_t read_size = remaining > CHUNK_SIZE ? CHUNK_SIZE : remaining;

            memcpy(in_buffer, xz_compressed_file_start + total_in, read_size);

            buffer.in_pos = 0;
            buffer.in_size = read_size;

            total_in += read_size;
            remaining -= read_size;
        }

        ret = xz_dec_run(decoder, &buffer);

        if (buffer.out_pos > 0) {
            // Only for this example, as we would be printing the entire decompressed data
            out_buffer[buffer.out_pos] = '\0';
            ESP_LOGI(TAG, "Decompressed chunk (%u bytes)", buffer.out_pos);

            total_out += buffer.out_pos;
            buffer.out_pos = 0;
        }

        if (ret != XZ_OK && ret != XZ_STREAM_END) {
            ESP_LOGE(TAG, "Decompression error: %d", ret);
            break;
        }
    }

    ESP_LOGI(TAG, "Chunked decompression stats:");
    ESP_LOGI(TAG, "Input bytes processed: %u", total_in);
    ESP_LOGI(TAG, "Output bytes produced: %u", total_out);
    ESP_LOGI(TAG, "Decompressed data:\n%s", out_buffer);

    // Clean up
    free(in_buffer);
    free(out_buffer);
    xz_dec_end(decoder);
}

void app_main(void)
{
    size_t origin_file_length = origin_file_end - origin_file_start;
    compressed_file_length = xz_compressed_file_end - xz_compressed_file_start;
    ESP_LOGI(TAG, "origin file size is %u, compressed file size is %u bytes\n", origin_file_length, compressed_file_length);

    ESP_LOGI(TAG, "*****************test buf to buf begin****************\n");
    // Read and display the contents of a small text file (hello.txt)
    test_buf_to_buf();
    ESP_LOGI(TAG, "*****************test buf to buf end******************\n");

    ESP_LOGI(TAG, "*****************test callback to callback begin******\n");
    filled_length = 0;
    test_cb_to_cb();
    ESP_LOGI(TAG, "*****************test callback to callback end********\n");

    ESP_LOGI(TAG, "*****************test callback to buf begin***********\n");
    filled_length = 0;
    test_cb_to_buf();
    ESP_LOGI(TAG, "*****************test callback to buf end*************\n");

    ESP_LOGI(TAG, "*****************test buf to callback begin***********\n");
    filled_length = 0;
    test_buf_to_cb();
    ESP_LOGI(TAG, "*****************test buf to callback end*************\n");

    ESP_LOGI(TAG, "*****************test chunked API begin***************\n");
    test_chunked_api();
    ESP_LOGI(TAG, "*****************test chunked API end*****************\n");

    // All done
    ESP_LOGI(TAG, "TASK FINISH");
}
