/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
/* SPDX-License-Identifier: Apache-2.0 */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "esp_eaf_player.h"
#include "eaf_internal.h"

static size_t copy_frame(esp_eaf_player_t *p, uint8_t *out)
{
    const esp_eaf_stream_info_t *info = esp_eaf_player_get_info(p);
    const esp_eaf_frame_t *frame = esp_eaf_player_get_frame(p);
    memcpy(out, frame->pixels, frame->pixel_bytes);
    size_t n = frame->pixel_bytes;
    if (frame->alpha) {
        size_t alpha = (size_t)info->width * info->height;
        memcpy(out + n, frame->alpha, alpha);
        n += alpha;
    }
    return n;
}

static void verify_file(const char *path)
{
    FILE *f = fopen(path, "rb"); assert(f);
    assert(fseek(f, 0, SEEK_END) == 0);
    long size = ftell(f); assert(size > 0);
    rewind(f);
    uint8_t *bytes = malloc((size_t)size); assert(bytes);
    assert(fread(bytes, 1, (size_t)size, f) == (size_t)size); fclose(f);
    esp_eaf_player_t *memory = NULL, *file = NULL;
    assert(esp_eaf_player_open_memory(bytes, size, NULL, &memory) == ESP_OK);
    assert(esp_eaf_player_open_file(path, NULL, &file) == ESP_OK);
    const esp_eaf_stream_info_t info = *esp_eaf_player_get_info(memory);
    size_t capacity = (size_t)info.width * info.height * 5;
    uint8_t *snapshots = malloc(capacity * info.frame_count), *out = malloc(capacity);
    assert(snapshots && out);
    size_t n = 0;
    for (uint32_t i = 0; i < info.frame_count; i++) {
        assert(esp_eaf_player_seek(memory, i) == ESP_OK);
        n = copy_frame(memory, snapshots + i * capacity);
    }
    for (uint32_t step = 0; step < info.frame_count * 3; step++) {
        uint32_t index = (step * 7 + step / info.frame_count) % info.frame_count;
        assert(esp_eaf_player_seek(file, index) == ESP_OK);
        assert(copy_frame(file, out) == n);
        assert(memcmp(out, snapshots + index * capacity, n) == 0);
    }
    esp_eaf_player_release_cache(file);
    assert(esp_eaf_player_seek(file, 0) == ESP_OK);
    assert(copy_frame(file, out) == n);
    assert(memcmp(out, snapshots, n) == 0);
    esp_eaf_player_close(memory); esp_eaf_player_close(file);
    for (size_t len = 0; len < (size_t)size; len += 1 + (size_t)size / 64) {
        esp_eaf_player_t *p = NULL;
        esp_err_t err = esp_eaf_player_open_memory(bytes, len, NULL, &p);
        if (err == ESP_OK) {
            esp_eaf_player_close(p);
        } else {
            assert(p == NULL);
        }
    }
    free(snapshots); free(out); free(bytes);
}

static void malformed_entropy(void)
{
    uint8_t out[4], short_rle[] = {1}, overflow_rle[] = {5, 0};
    size_t n = sizeof(out);
    assert(eaf_rle_decode(short_rle, sizeof(short_rle), out, &n, false) != ESP_OK);
    assert(eaf_rle_decode(overflow_rle, sizeof(overflow_rle), out, &n, false) != ESP_OK);
    uint8_t huffman[] = {4, 0, 0, 42, 1, 0, 0}; /* eight symbols into a four-byte output */
    assert(eaf_huffman_decode(huffman, sizeof(huffman), out, &n, false) != ESP_OK);
    /* Exercise truncated/unbounded dictionaries without an output overrun. */
    for (unsigned length = 0; length <= sizeof(huffman); length++) {
        n = sizeof(out);
        (void)eaf_huffman_decode(huffman, length, out, &n, false);
    }
    assert(eaf_huffman_decode(huffman, sizeof(huffman), out, NULL, false) != ESP_OK);
    eaf_format_handle_t parser = NULL;
    assert(eaf_format_init(NULL, 0, &parser) != ESP_OK);
    assert(eaf_format_get_frame_data(NULL, -1) == NULL);
    assert(eaf_format_get_frame_size(NULL, -1) == -1);
}

int main(int argc, char **argv)
{
    malformed_entropy();
    for (int i = 1; i < argc; i++) {
        verify_file(argv[i]);
    }
    assert(argc > 1);
    printf("Host core lifecycle/malformed-input checks: %d fixtures PASS\n", argc - 1);
    return 0;
}
