/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

/* EAF4 accepts dimensions <=1024, divisible by 4; 1..10000 frames;
 * 10..10000 ms frame periods; files <=32 MiB. Functions return 0 on success
 * and negative values on failure; the esp_eaf_player API maps these to esp_err_t. */
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Frozen frame modes inside the EAF4 container (see docs/NAMING.md).
 *
 * Header offset 52..63 (previously reserved zeros) carries container metadata:
 *   - h[52] = pixel_format:
 *       0 = EAF4_PIXFMT_BGRA8888 (4 bytes/pixel, default for v1 compatibility)
 *       1 = EAF4_PIXFMT_RGB565   (2 bytes/pixel)
 *       2 = EAF4_PIXFMT_RGB565A8 (3 bytes/pixel: 2*W*H RGB565 followed by W*H A8)
 *   - h[53] = codec_id:
 *       0 = auto from frame mode (v1 compatibility), or 0x04 / 0x05
 *
 * Frame modes are stored in index entry byte 16, in a separate namespace from
 * codec IDs. FULL is an absolute frame payload, still compressed with zlib.
 * TILE_DELTA_RGB565 and TILE_JOINT_DELTA_RGB565 use the pixel format to select
 * RGB565 or RGB565A8 (optional A8). Bounded RGB565 has no continuous alpha.
 */
typedef enum {
    EAF4_PIXFMT_BGRA8888 = 0,
    EAF4_PIXFMT_RGB565   = 1,
    EAF4_PIXFMT_RGB565A8 = 2,
} eaf4_pixfmt_t;

typedef enum {
    EAF4_FRAME_FULL                        = 0,
    EAF4_FRAME_XOR_BGRA8888                = 1,
    EAF4_FRAME_XOR_PLANAR_BGRA8888         = 2,
    EAF4_FRAME_TILE_XOR_PLANAR_BGRA8888    = 3,
    EAF4_FRAME_TILE_XOR_PACKED_BGRA8888    = 4,
    EAF4_FRAME_TILE_JOINT_DELTA_BGRA8888   = 5,
    EAF4_FRAME_TILE_BOUNDED_BGRA8888       = 6,
    EAF4_FRAME_TILE_DELTA_RGB565           = 7,
    EAF4_FRAME_TILE_JOINT_DELTA_RGB565     = 8,
    EAF4_FRAME_TILE_BOUNDED_RGB565         = 9,
    EAF4_FRAME_HSUB_BGRA8888               = 10,
    EAF4_FRAME_TILE_SUB_PACKED_BGRA8888    = 11,
    EAF4_FRAME_MAX                         = 12,
} eaf4_frame_mode_t;

const char *eaf4_frame_mode_name(eaf4_frame_mode_t frame_mode);

typedef struct eaf4 eaf4_t;
typedef struct {
    uint16_t width, height;
    uint32_t frames, period_ms, frame_bytes;
    uint8_t pixel_format; /*!< eaf4_pixfmt_t (0=BGRA8888, 1=RGB565, 2=RGB565A8) */
    uint8_t codec_id;     /*!< Top-level codec ID (0x04..0x05, or 0 for v1 default) */
    bool has_alpha;       /*!< True for BGRA8888 and RGB565A8 */
} eaf4_info_t;
typedef struct {
    uint32_t read_us, inflate_us, reconstruct_us, crc_us;
} eaf4_timing_t;

typedef struct {
    uint16_t x1, y1, x2, y2;
    bool valid;
} eaf4_dirty_rect_t;

const eaf4_timing_t *eaf4_timing(const eaf4_t *player);
void eaf4_get_dirty_rect(const eaf4_t *player, eaf4_dirty_rect_t *rect);

/* Streaming file decoder. Returned pixels use info.pixel_format (straight alpha).
 * Each next() verifies the compressed CRC. Host builds also verify the
 * reconstructed frame CRC. No LVGL dependency; the buffer is owned by the decoder. */
int eaf4_open(const char *path, eaf4_t **out);
/* Borrow immutable bytes; caller keeps the mapping alive until close. */
int eaf4_open_memory(const void *data, size_t size, eaf4_t **out);
const eaf4_info_t *eaf4_info(const eaf4_t *player);
int eaf4_next(eaf4_t *player, const uint8_t **pixels, uint32_t *frame_index);
int eaf4_seek(eaf4_t *player, uint32_t target_frame, const uint8_t **pixels);
/* Drop optional compressed/seek caches; decoding and seeking remain available. */
void eaf4_release_cache(eaf4_t *player);
void eaf4_close(eaf4_t *player);

#ifdef __cplusplus
}
#endif
