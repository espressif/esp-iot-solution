/* XZ decompress API been used to decompress the specified buffer.

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <string.h>

#include "esp_err.h"
#include "esp_log.h"

#include "esp_xz_decompressor.h"

#define IO_BUFFER_LEN (2048)

extern const uint8_t origin_file_start[] asm("_binary_hello_txt_start");
extern const uint8_t origin_file_end[] asm("_binary_hello_txt_end");
extern const uint8_t xz_compressed_file_start[] asm("_binary_hello_txt_xz_start");
extern const uint8_t xz_compressed_file_end[] asm("_binary_hello_txt_xz_end");

static int decryped_count;
static size_t compressed_file_length;
static size_t filled_length;

static const char *TAG = "xz decry";

static void error(char *msg)
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
    int ret = esp_xz_decompress((unsigned char *)xz_compressed_file_start, compressed_file_length, NULL, NULL, (unsigned char *)out_buf, &decryped_count, error);
    // Display the read contents from the decompressed buf
    ESP_LOGI(TAG, "decompress data:\n%s", out_buf);
    ESP_LOGI(TAG, "ret = %d, decrypted count is %d", ret, decryped_count);
    free(out_buf);
}

static void test_buf_to_cb(void)
{
    /**
     * Read compressed data from in_buf, and call flush() to write the decompressed data
     * to the specified partition/buffer chunk by chunk.
     * When you can not estimate how much data will be extracted, you can use this API like this.
     */
    int ret = esp_xz_decompress((unsigned char *)xz_compressed_file_start, compressed_file_length, NULL, &flush, NULL, &decryped_count, &error);
    ESP_LOGI(TAG, "ret = %d; decryped count = %d", ret, decryped_count);
}

static void test_cb_to_cb(void)
{
    /**
     * Call fill() to read compressed data from partition/buffer, and call flush() to write the decompressed data
     * to the specified partition/buffer chunk by chunk.
     * This is the memory friendly usage of the API, because it can read and write data in chunks instead of read
     * or write all data at once.
     */
    int ret = esp_xz_decompress(NULL, 0, &fill, &flush, NULL, &decryped_count, &error);
    ESP_LOGI(TAG, "ret = %d; decryped count = %d", ret, decryped_count);
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
    int ret = esp_xz_decompress(in_buf, 0, &fill, NULL, out_buf, &decryped_count, &error);
    ESP_LOGI(TAG, "decompress:\n%s", out_buf);
    ESP_LOGI(TAG, "ret = %d; decryped count = %d", ret, decryped_count);
    free(out_buf);
    free(in_buf);
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

    // All done
    ESP_LOGI(TAG, "TESK FINISH");
}
