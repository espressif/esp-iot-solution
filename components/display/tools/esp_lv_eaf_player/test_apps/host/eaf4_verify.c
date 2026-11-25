/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "esp_eaf_codec.h"

#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv)
{
    if (argc != 2) {
        fprintf(stderr, "usage: %s file.eaf4\n", argv[0]);
        return 2;
    }
    FILE *f = fopen(argv[1], "rb");
    if (!f) {
        fprintf(stderr, "open failed: %s\n", argv[1]);
        return 1;
    }
    uint8_t magic[4] = {0};
    size_t nread = fread(magic, 1, 4, f);
    fclose(f);
    esp_eaf_container_t ctype = esp_eaf_container_probe(magic, nread);
    if (ctype != ESP_EAF_CONTAINER_EAF4) {
        fprintf(stderr, "not an EAF4 container (detected=%d): %s\n", (int)ctype, argv[1]);
        return 1;
    }

    eaf4_t *player = NULL;
    if (eaf4_open(argv[1], &player) != 0) {
        fprintf(stderr, "open failed: %s\n", argv[1]);
        return 1;
    }
    const eaf4_info_t *info = eaf4_info(player);
    uint64_t read_us = 0, inflate_us = 0, reconstruct_us = 0;
    for (uint32_t i = 0; i < info->frames; i++) {
        const uint8_t *pixels = NULL;
        uint32_t index = 0;
        if (eaf4_next(player, &pixels, &index) != 0 || index != i) {
            fprintf(stderr, "decode failed at frame %u\n", i);
            eaf4_close(player);
            return 1;
        }
        const eaf4_timing_t *timing = eaf4_timing(player);
        read_us += timing->read_us;
        inflate_us += timing->inflate_us;
        reconstruct_us += timing->reconstruct_us;
    }
    printf("eaf4 %ux%u frames=%u period_ms=%u frame_bytes=%u\n",
           info->width, info->height, info->frames, info->period_ms, info->frame_bytes);
    static const char *pixel_names[] = {"BGRA8888", "RGB565", "RGB565A8"};
    printf("container=EAF4 pixel_format=%s codec=%s\n", pixel_names[info->pixel_format],
           info->codec_id ? esp_eaf_codec_name((esp_eaf_codec_id_t)info->codec_id) : "AUTO");
    printf("timing_us read=%llu inflate=%llu reconstruct=%llu\n",
           (unsigned long long)read_us, (unsigned long long)inflate_us,
           (unsigned long long)reconstruct_us);
    eaf4_close(player);
    return 0;
}
