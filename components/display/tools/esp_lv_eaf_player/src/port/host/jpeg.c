/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
/* SPDX-License-Identifier: Apache-2.0 */
#include <stdio.h>
#include <stdlib.h>
#include <setjmp.h>
#include <jpeglib.h>
#include "eaf_internal.h"

typedef struct {
    struct jpeg_error_mgr base;
    jmp_buf jump;
} eaf_jpeg_error_t;
static void jpeg_failure(j_common_ptr jpeg)
{
    eaf_jpeg_error_t *error = (eaf_jpeg_error_t *)jpeg->err;
    longjmp(error->jump, 1);
}

static void jpeg_message(j_common_ptr jpeg, int level)
{
    if (level < 0) {
        jpeg_failure(jpeg);
    }
}

void eaf_jpeg_backend_retain(void) {}
void eaf_jpeg_backend_release(void) {}
esp_err_t eaf_jpeg_decode(const uint8_t *data, size_t size,
                          uint8_t *output, size_t *out_size, bool swap)
{
    if (!data || !size || !output || !out_size) {
        return ESP_ERR_INVALID_ARG;
    }
    /* Heap-owned context remains defined after libjpeg longjmp. */
    struct jpeg_decompress_struct *jpeg = calloc(1, sizeof(*jpeg));
    if (!jpeg) {
        return ESP_ERR_NO_MEM;
    }
    eaf_jpeg_error_t error;
    jpeg->err = jpeg_std_error(&error.base);
    error.base.error_exit = jpeg_failure;
    error.base.emit_message = jpeg_message;
    if (setjmp(error.jump)) {
        jpeg_destroy_decompress(jpeg); free(jpeg); return ESP_FAIL;
    }
    jpeg_create_decompress(jpeg);
    jpeg_mem_src(jpeg, data, size);
    jpeg_read_header(jpeg, TRUE);
    jpeg->out_color_space = JCS_RGB;
    jpeg_start_decompress(jpeg);
    size_t pixels = (size_t)jpeg->output_width * jpeg->output_height;
    if (pixels > *out_size / 2) {
        jpeg_destroy_decompress(jpeg); free(jpeg); return ESP_ERR_INVALID_SIZE;
    }
    JSAMPARRAY row = (*jpeg->mem->alloc_sarray)((j_common_ptr)jpeg, JPOOL_IMAGE, jpeg->output_width * 3, 1);
    size_t dest = 0;
    while (jpeg->output_scanline < jpeg->output_height) {
        jpeg_read_scanlines(jpeg, row, 1);
        for (unsigned x = 0; x < jpeg->output_width; x++) {
            const uint8_t *p = row[0] + x * 3;
            uint16_t color = ((p[0] & 0xf8) << 8) | ((p[1] & 0xfc) << 3) | (p[2] >> 3);
            ((uint16_t *)output)[dest++] = swap ? __builtin_bswap16(color) : color;
        }
    }
    jpeg_finish_decompress(jpeg);
    jpeg_destroy_decompress(jpeg); free(jpeg);
    *out_size = pixels * 2;
    return ESP_OK;
}
