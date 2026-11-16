/*
 * SPDX-FileCopyrightText: 2025-2026 Espressif Systems (Shanghai) CO LTD
 * SPDX-License-Identifier: Apache-2.0
 */
/**
 * @file container.c
 * @brief EAF4 container parser and frame reader; pixel reconstruction is delegated to codecs
 */

#define _POSIX_C_SOURCE 200809L
#include "eaf4.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "zlib.h"
#include "codec_internal.h"

#include "eaf_port.h"
#define NOW_US() eaf_port_now_us()
static void *allocate(size_t n)
{
    return eaf_alloc_aligned(n, 64, NULL);
}
static void release_buffer(void *data, size_t size)
{
    eaf_port_release_buffer(data, size);
}

struct eaf4 {
    FILE *file;
    const uint8_t *memory;
    eaf4_info_t info;
    uint8_t *index, *compressed, *scratch, *frame, *cached, *seek_snapshot;
    uint32_t cached_offset;
    uint32_t seek_snapshot_frame;
    bool has_seek_snapshot;
    bool failed;
    size_t cached_bytes;
    eaf4_timing_t timing;
    eaf4_dirty_rect_t dirty;
    uint32_t crc_table[8][256];
    uint32_t next, max_compressed, max_decoded;
};

static uint16_t u16(const uint8_t *p)
{
    return p[0] | ((uint16_t)p[1] << 8);
}
static uint32_t u32(const uint8_t *p)
{
    return p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

/* Slicing-by-eight IEEE CRC-32. Tables belong to the decoder and reside
 * in internal RAM on ESP, avoiding flash-table fetches on every input byte. */
static void crc_init(eaf4_t *p)
{
    for (unsigned i = 0; i < 256; ++i) {
        uint32_t c = i;
        for (unsigned bit = 0; bit < 8; ++bit) {
            c = (c >> 1) ^ ((c & 1) ? 0xedb88320U : 0);
        }
        p->crc_table[0][i] = c;
    }
    for (unsigned t = 1; t < 8; ++t)
        for (unsigned i = 0; i < 256; ++i) {
            uint32_t c = p->crc_table[t - 1][i];
            p->crc_table[t][i] = (c >> 8) ^ p->crc_table[0][c & 255];
        }
}

static uint32_t crc(eaf4_t *p, const void *data, size_t n)
{
    const uint8_t *b = data;
    const uint32_t (*t)[256] = p->crc_table;
    uint32_t c = 0xffffffffU;
    while (n >= 8) {
        uint32_t a = u32(b) ^ c, d = u32(b + 4);
        c = t[7][a & 255] ^ t[6][(a >> 8) & 255] ^
            t[5][(a >> 16) & 255] ^ t[4][a >> 24] ^
            t[3][d & 255] ^ t[2][(d >> 8) & 255] ^
            t[1][(d >> 16) & 255] ^ t[0][d >> 24];
        b += 8;
        n -= 8;
    }
    while (n--) {
        c = (c >> 8) ^ t[0][(c ^ *b++) & 255];
    }
    return ~c;
}

void eaf4_release_cache(eaf4_t *p)
{
    if (!p) {
        return;
    }
    release_buffer(p->cached, p->cached_bytes);
    release_buffer(p->seek_snapshot, p->info.frame_bytes);
    p->cached = NULL;
    p->seek_snapshot = NULL;
    p->cached_bytes = 0;
    p->has_seek_snapshot = false;
}

void eaf4_close(eaf4_t *p)
{
    if (!p) {
        return;
    }
    eaf4_release_cache(p);
    if (p->file) {
        fclose(p->file);
    }
    release_buffer(p->index, (size_t)p->info.frames * 24);
    release_buffer(p->compressed, p->max_compressed);
    release_buffer(p->scratch, p->max_decoded);
    release_buffer(p->frame, p->info.frame_bytes);
    free(p);
}

const eaf4_info_t *eaf4_info(const eaf4_t *p)
{
    return p ? &p->info : NULL;
}
const eaf4_timing_t *eaf4_timing(const eaf4_t *p)
{
    return p ? &p->timing : NULL;
}
void eaf4_get_dirty_rect(const eaf4_t *p, eaf4_dirty_rect_t *rect)
{
    if (!rect) {
        return;
    }
    if (p) {
        *rect = p->dirty;
    } else {
        *rect = (eaf4_dirty_rect_t) {
            0
        };
    }
}

static int open_source(const char *path, const void *memory, size_t memory_size, eaf4_t **out)
{
    if ((!path && !memory) || !out) {
        return -1;
    }
    *out = NULL;
    eaf4_t *p = eaf_port_alloc_internal(sizeof(*p));
    if (!p) {
        return -2;
    }
    memset(p, 0, sizeof(*p));
    crc_init(p);
    uint8_t h[64];
    long size;
    p->memory = memory;
    if (memory) {
        if (memory_size < 64 || memory_size > 32 * 1024 * 1024) {
            goto bad;
        }
        size = memory_size;
        memcpy(h, memory, 64);
    } else {
        p->file = fopen(path, "rb");
        if (!p->file || fseek(p->file, 0, SEEK_END)) {
            goto bad;
        }
        size = ftell(p->file);
        if (size < 64 || size > 32 * 1024 * 1024 || fseek(p->file, 0, SEEK_SET) || fread(h, 1, 64, p->file) != 64) {
            goto bad;
        }
    }
    if (memcmp(h, "EAF4", 4) || u16(h + 4) != 1 || u16(h + 6) != 64 || u16(h + 36) != 4 || u16(h + 38) != 4 ||
            u32(h + 48) != 1) {
        goto bad;
    }
    uint32_t expected = u32(h + 40);
    memset(h + 40, 0, 4);
    if (crc(p, h, 64) != expected) {
        goto bad;
    }
    uint8_t pixfmt = h[52];
    uint8_t top_codec = h[53];
    if (pixfmt > EAF4_PIXFMT_RGB565A8) {
        goto bad;
    }
    for (unsigned i = 54; i < 64; i++)
        if (h[i]) {
            goto bad;
        }
    uint32_t bpp = (pixfmt == EAF4_PIXFMT_BGRA8888) ? 4U : (pixfmt == EAF4_PIXFMT_RGB565A8) ? 3U : 2U;
    p->info = (eaf4_info_t) {
        .width = u16(h + 8),
        .height = u16(h + 10),
        .frames = u32(h + 12),
        .period_ms = u32(h + 16),
        .frame_bytes = u32(h + 20),
        .pixel_format = pixfmt,
        .codec_id = top_codec,
        .has_alpha = (pixfmt != EAF4_PIXFMT_RGB565),
    };
    eaf4_info_t *v = &p->info;
    if (!v->width || !v->height || v->width > 1024 || v->height > 1024 || v->width % 4 || v->height % 4 ||
            !v->frames || v->frames > 10000 || v->period_ms < 10 || v->period_ms > 10000 ||
            v->frame_bytes != (uint32_t)v->width * v->height * bpp || u32(h + 24) != 64 ||
            u32(h + 28) != 64 + v->frames * 24 || u32(h + 32) != (uint32_t)size) {
        goto bad;
    }
    uint32_t index_size = v->frames * 24, offset = u32(h + 28);
    if (offset > (uint32_t)size) {
        goto bad;
    }
    p->index = allocate(index_size);
    if (!p->index) {
        goto bad;
    }
    if (memory) {
        memcpy(p->index, p->memory + 64, index_size);
    }
    if ((!memory && fread(p->index, 1, index_size, p->file) != index_size) || crc(p, p->index, index_size) != u32(h + 44)) {
        goto bad;
    }
    uint32_t num_tiles = (uint32_t)v->width * v->height / 16;
    uint32_t mask_bytes = (num_tiles + 7) / 8;
    uint32_t max_possible_decoded = v->frame_bytes + num_tiles * 16 + 4096;
    for (uint32_t i = 0; i < v->frames; i++) {
        const uint8_t *r = p->index + i * 24;
        uint32_t length = u32(r + 4), decoded = u32(r + 8);
        uint8_t mode = r[16];
        bool is_key_allowed = (mode == EAF4_FRAME_FULL || mode == EAF4_FRAME_HSUB_BGRA8888 ||
                               mode == EAF4_FRAME_TILE_BOUNDED_BGRA8888 || mode == EAF4_FRAME_TILE_BOUNDED_RGB565);
        if (u32(r) != offset || !length || length > max_possible_decoded || mode >= EAF4_FRAME_MAX || r[17] ||
                u16(r + 18) || (!i && !is_key_allowed) ||
                offset > (uint32_t)size || length > (uint32_t)size - offset) {
            goto bad;
        }
        if ((mode <= EAF4_FRAME_XOR_PLANAR_BGRA8888 || mode == EAF4_FRAME_HSUB_BGRA8888) && decoded != v->frame_bytes) {
            goto bad;
        }
        if ((mode == EAF4_FRAME_XOR_BGRA8888 || mode == EAF4_FRAME_XOR_PLANAR_BGRA8888 || mode == EAF4_FRAME_HSUB_BGRA8888) &&
                pixfmt != EAF4_PIXFMT_BGRA8888) {
            goto bad;
        }
        if (top_codec != 0 && top_codec != ESP_EAF_CODEC_LOSSLESS && top_codec != ESP_EAF_CODEC_BOUNDED) {
            goto bad;
        }
        bool bounded = mode == EAF4_FRAME_TILE_BOUNDED_BGRA8888 || mode == EAF4_FRAME_TILE_BOUNDED_RGB565;
        if ((top_codec == ESP_EAF_CODEC_LOSSLESS && bounded) ||
                (top_codec == ESP_EAF_CODEC_BOUNDED && !bounded && mode != EAF4_FRAME_FULL)) {
            goto bad;
        }
        if ((mode == EAF4_FRAME_TILE_XOR_PLANAR_BGRA8888 || mode == EAF4_FRAME_TILE_XOR_PACKED_BGRA8888 ||
                mode == EAF4_FRAME_TILE_SUB_PACKED_BGRA8888) &&
                (pixfmt != EAF4_PIXFMT_BGRA8888 || decoded < mask_bytes || decoded > v->frame_bytes + mask_bytes || (decoded - mask_bytes) % 64)) {
            goto bad;
        }
        if (mode == EAF4_FRAME_TILE_JOINT_DELTA_BGRA8888 &&
                (pixfmt != EAF4_PIXFMT_BGRA8888 || decoded < mask_bytes || decoded > v->frame_bytes + num_tiles * 16 + mask_bytes)) {
            goto bad;
        }
        if (mode == EAF4_FRAME_TILE_BOUNDED_BGRA8888 &&
                (pixfmt != EAF4_PIXFMT_BGRA8888 || decoded < num_tiles || decoded > v->frame_bytes + num_tiles)) {
            goto bad;
        }
        if (mode == EAF4_FRAME_TILE_DELTA_RGB565 &&
                (pixfmt == EAF4_PIXFMT_BGRA8888 || decoded < mask_bytes || decoded > v->frame_bytes + mask_bytes ||
                 (decoded - mask_bytes) % (16 * bpp))) {
            goto bad;
        }
        if (mode == EAF4_FRAME_TILE_JOINT_DELTA_RGB565 &&
                (pixfmt == EAF4_PIXFMT_BGRA8888 || decoded < mask_bytes || decoded > v->frame_bytes + num_tiles * 16 + mask_bytes)) {
            goto bad;
        }
        if (mode == EAF4_FRAME_TILE_BOUNDED_RGB565 &&
                (pixfmt != EAF4_PIXFMT_RGB565 || decoded < num_tiles || decoded > v->frame_bytes + num_tiles)) {
            goto bad;
        }
        offset += length;
        if (length > p->max_compressed) {
            p->max_compressed = length;
        }
        if (mode != EAF4_FRAME_FULL && mode != EAF4_FRAME_HSUB_BGRA8888 && decoded > p->max_decoded) {
            p->max_decoded = decoded;
        }
    }
    if (offset != (uint32_t)size) {
        goto bad;
    }
    if (p->max_decoded == 0) {
        p->max_decoded = 64;
    }
    p->compressed = allocate(p->max_compressed);
    p->scratch = allocate(p->max_decoded);
    p->frame = allocate(v->frame_bytes);
    if (!p->compressed || !p->scratch || !p->frame) {
        goto bad;
    }
    memset(p->frame, 0, v->frame_bytes);
    /* Optional compressed-data cache. Keep the streaming buffers so allocation
     * failure still permits playback without changing the on-disk format. */
    p->cached_offset = 64 + index_size;
    size_t payload_size = (size_t)size - p->cached_offset;
    if (!memory && payload_size <= 4 * 1024 * 1024) {
        p->cached = allocate(payload_size);
        if (p->cached) {
            p->cached_bytes = payload_size;
            if (fseek(p->file, p->cached_offset, SEEK_SET) ||
                    fread(p->cached, 1, payload_size, p->file) != payload_size) {
                release_buffer(p->cached, p->cached_bytes);
                p->cached = NULL;
                p->cached_bytes = 0;
                clearerr(p->file);
            }
        }
    }
#ifdef ESP_PLATFORM
    ESP_LOGI("eaf4", "compressed cache: %s, payload=%lu bytes",
             p->cached ? "PSRAM" : "streaming", (unsigned long)payload_size);
#endif
    *out = p;
    return 0;
bad:
    eaf4_close(p);
    return -1;
}

static int decode_next(eaf4_t *p, const uint8_t **pixels, uint32_t *frame_index)
{
    if (!p || !pixels) {
        return -1;
    }
    const uint8_t *r = p->index + p->next * 24;
    uint32_t length = u32(r + 4), decoded = u32(r + 8);
    uint8_t mode = r[16];
    int64_t start = NOW_US();
    const uint8_t *compressed = p->memory ? p->memory + u32(r) : p->cached ? p->cached + u32(r) - p->cached_offset : p->compressed;
    if ((!p->memory && !p->cached && (fseek(p->file, u32(r), SEEK_SET) ||
                                      fread(p->compressed, 1, length, p->file) != length)) ||
            crc(p, compressed, length) != u32(r + 20)) {
        return -1;
    }
    p->timing.read_us = (uint32_t)(NOW_US() - start);
    start = NOW_US();
    uLongf dest = decoded;
    uLong input = length;
    bool direct_to_frame = (mode == EAF4_FRAME_FULL || mode == EAF4_FRAME_HSUB_BGRA8888);
    uint8_t *inflate_dst = direct_to_frame ? p->frame : p->scratch;
    if (uncompress2(inflate_dst, &dest, compressed, &input) != Z_OK || dest != decoded || input != length) {
        return -1;
    }
    p->timing.inflate_us = (uint32_t)(NOW_US() - start);
    start = NOW_US();
    const eaf_codec_ops_t *codec = eaf_codec_find(ESP_EAF_CONTAINER_EAF4, mode);
    if (!codec) {
        return -1;
    }
    eaf_codec_request_t request = {
        .container = ESP_EAF_CONTAINER_EAF4,
        .mode = mode,
        .pixel_format = p->info.pixel_format,
        .frame = p->frame,
        .scratch = direct_to_frame ? p->frame : p->scratch,
        .decoded_size = decoded,
        .frame_bytes = p->info.frame_bytes,
        .frame_width = p->info.width,
        .frame_height = p->info.height,
        .eaf4_dirty = &p->dirty,
    };
    if (codec->decode(&request) != 0) {
        return -1;
    }
    p->timing.reconstruct_us = (uint32_t)(NOW_US() - start);
#ifndef ESP_PLATFORM
    start = NOW_US();
    if (crc(p, p->frame, p->info.frame_bytes) != u32(r + 12)) {
        return -1;
    }
    p->timing.crc_us = (uint32_t)(NOW_US() - start);
#else
    p->timing.crc_us = 0;
#endif

    *pixels = p->frame;
    if (frame_index) {
        *frame_index = p->next;
    }
    p->next = (p->next + 1) % p->info.frames;
    return 0;
}

int eaf4_next(eaf4_t *p, const uint8_t **pixels, uint32_t *frame_index)
{
    if (!p || !pixels || p->failed) {
        return -1;
    }
    int result = decode_next(p, pixels, frame_index);
    if (result != 0) {
        p->failed = true;
        *pixels = NULL;
    }
    return result;
}

int eaf4_seek(eaf4_t *p, uint32_t target_frame, const uint8_t **pixels)
{
    if (!p || !pixels || target_frame >= p->info.frames) {
        return -1;
    }
    if (p->has_seek_snapshot && p->seek_snapshot && p->seek_snapshot_frame == target_frame) {
        memcpy(p->frame, p->seek_snapshot, p->info.frame_bytes);
        p->failed = false;
        p->next = (target_frame + 1) % p->info.frames;
        p->dirty = (eaf4_dirty_rect_t) {
            0, 0, p->info.width - 1, p->info.height - 1, true
        };
        if (pixels) {
            *pixels = p->frame;
        }
        return 0;
    }
    if (!p->failed && target_frame == p->next) {
        return eaf4_next(p, pixels, NULL);
    }
    uint32_t key = target_frame;
    while (key > 0 && p->index[key * 24 + 16] != EAF4_FRAME_FULL &&
            p->index[key * 24 + 16] != EAF4_FRAME_HSUB_BGRA8888) {
        key--;
    }
    p->next = key;
    p->failed = false;
    memset(p->frame, 0, p->info.frame_bytes);
    const uint8_t *px = NULL;
    for (uint32_t f = key; f <= target_frame; f++) {
        if (eaf4_next(p, &px, NULL) != 0) {
            return -1;
        }
    }
    if (key != target_frame) {
        p->dirty = (eaf4_dirty_rect_t) {
            0, 0, p->info.width - 1, p->info.height - 1, true
        };
        if (!p->seek_snapshot) {
            p->seek_snapshot = allocate(p->info.frame_bytes);
        }
        if (p->seek_snapshot) {
            memcpy(p->seek_snapshot, p->frame, p->info.frame_bytes);
            p->seek_snapshot_frame = target_frame;
            p->has_seek_snapshot = true;
        }
    }
    if (pixels) {
        *pixels = px;
    }
    return 0;
}

int eaf4_open(const char *path, eaf4_t **out)
{
    return open_source(path, NULL, 0, out);
}
int eaf4_open_memory(const void *data, size_t size, eaf4_t **out)
{
    return open_source(NULL, data, size, out);
}
