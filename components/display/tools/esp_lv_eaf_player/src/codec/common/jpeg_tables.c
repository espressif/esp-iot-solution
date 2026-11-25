/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
/* SPDX-License-Identifier: Apache-2.0 */
#include <stdlib.h>
#include <string.h>
#include "codec_internal.h"
#include "eaf_port.h"
void eaf_decoder_cache_jpeg_tables(eaf_decoder_t *decoder, const uint8_t *jpg, size_t len)
{
    eaf_jpeg_state_t *state = eaf_decoder_state(decoder, ESP_EAF_CODEC_JPEG);
    if (!state) {
        return;
    }

    state->hdr_len = 0;
    if (len < 16 || jpg[0] != 0xFF || jpg[1] != 0xD8) {
        return;
    }
    size_t pos = 2;
    size_t out = 0;
    state->hdr[out++] = 0xFF;
    state->hdr[out++] = 0xD8;
    state->sof0_offset = -1;
    while (pos + 4 <= len && jpg[pos] == 0xFF) {
        uint8_t m = jpg[pos + 1];
        uint16_t seg_len = ((uint16_t)jpg[pos + 2] << 8) | jpg[pos + 3];
        if (seg_len < 2 || pos + 2 + seg_len > len) {
            break;
        }
        if (m == 0xDB || m == 0xC0 || m == 0xC4 || m == 0xDA) {
            if (out + 2 + seg_len <= sizeof(state->hdr)) {
                if (m == 0xC0) {
                    state->sof0_offset = (int)out;
                }
                memcpy(&state->hdr[out], &jpg[pos], 2 + seg_len);
                out += 2 + seg_len;
            }
            if (m == 0xDA) {
                state->hdr_len = out;
                return;
            }
        }
        pos += 2 + seg_len;
    }
}

void eaf_jpeg_reset(void *opaque)
{
    eaf_jpeg_state_t *state = opaque;
    state->hdr_len = 0;
    state->sof0_offset = -1;
}
