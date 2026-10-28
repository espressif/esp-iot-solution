/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "esl_image_manager.h"

#include <inttypes.h>
#include <string.h>
#include "esp_log.h"

static const char *TAG = "img_mgr";

#define ESL_IMAGE_MANAGER_MAX_IMAGES   4
#define ESL_IMAGE_MANAGER_MAX_TAGS     8

typedef struct {
    uint16_t esl_addr;
    uint8_t current_image_index;
    bool transferred[ESL_IMAGE_MANAGER_MAX_IMAGES];
} esl_image_tag_state_t;

static esl_image_tag_state_t s_tag_states[ESL_IMAGE_MANAGER_MAX_TAGS];
static uint8_t s_tag_state_count;

/* Match SSD1680 full-frame size (128*296/8). 4 slots ≈ 18.9 KB on AP. */
#define ESL_IMAGE_RUNTIME_MAX_LEN  4736
#define ESL_IMAGE_RUNTIME_SLOTS    4

typedef struct {
    bool used;
    uint32_t image_id;
    uint8_t image_index;
    uint32_t version;
    uint32_t data_len;
    uint8_t data[ESL_IMAGE_RUNTIME_MAX_LEN];
} esl_image_runtime_slot_t;

static esl_image_runtime_slot_t s_runtime[ESL_IMAGE_RUNTIME_SLOTS];

/*
 * Full SSD1680 framebuffer: 128x296 1bpp = 4736 bytes.
 * Layout matches epaper screen_buffer / epaper_refresh_frame (linear).
 * s_image_0 is filled with an 8x8 checkerboard in esl_image_manager_init().
 */
#define ESL_FULL_FRAME_LEN  4736

static uint8_t s_image_0[ESL_FULL_FRAME_LEN];

static const uint8_t s_image_1[ESL_FULL_FRAME_LEN] = {
    [0 ... 4735] = 0xFF,
};

static const uint8_t s_image_2[ESL_FULL_FRAME_LEN] = {
    [0 ... 4735] = 0x00,
};

static const uint8_t s_image_3[ESL_FULL_FRAME_LEN] = {
    [0 ... 2367] = 0xAA,
    [2368 ... 4735] = 0x55,
};

typedef struct {
    uint32_t image_id;
    uint8_t image_index;
    uint32_t version;
    const uint8_t *data;
    uint32_t data_len;
} esl_image_entry_t;

static const esl_image_entry_t s_images[] = {
    { .image_id = 1, .image_index = 0, .version = 1, .data = s_image_0, .data_len = sizeof(s_image_0) },
    { .image_id = 2, .image_index = 1, .version = 1, .data = s_image_1, .data_len = sizeof(s_image_1) },
    { .image_id = 3, .image_index = 2, .version = 1, .data = s_image_2, .data_len = sizeof(s_image_2) },
    { .image_id = 4, .image_index = 3, .version = 1, .data = s_image_3, .data_len = sizeof(s_image_3) },
};

static esl_image_tag_state_t *find_or_create_tag_state(uint16_t esl_addr)
{
    for (uint8_t i = 0; i < s_tag_state_count; i++) {
        if (s_tag_states[i].esl_addr == esl_addr) {
            return &s_tag_states[i];
        }
    }

    if (s_tag_state_count >= ESL_IMAGE_MANAGER_MAX_TAGS) {
        return NULL;
    }

    esl_image_tag_state_t *state = &s_tag_states[s_tag_state_count++];
    memset(state, 0, sizeof(*state));
    state->esl_addr = esl_addr;
    return state;
}

static const esl_image_entry_t *find_by_image_id(uint32_t image_id)
{
    for (size_t i = 0; i < (sizeof(s_images) / sizeof(s_images[0])); i++) {
        if (s_images[i].image_id == image_id) {
            return &s_images[i];
        }
    }
    return NULL;
}

static const esl_image_entry_t *find_by_image_index(uint8_t image_index)
{
    for (size_t i = 0; i < (sizeof(s_images) / sizeof(s_images[0])); i++) {
        if (s_images[i].image_index == image_index) {
            return &s_images[i];
        }
    }
    return NULL;
}

static void fill_desc(const esl_image_entry_t *entry, esl_image_desc_t *out)
{
    out->image_id = entry->image_id;
    out->image_index = entry->image_index;
    out->version = entry->version;
    out->data = entry->data;
    out->data_len = entry->data_len;
}

esp_err_t esl_image_manager_init(void)
{
    memset(s_tag_states, 0, sizeof(s_tag_states));
    s_tag_state_count = 0;
    memset(s_runtime, 0, sizeof(s_runtime));

    /* 16 bytes/row × 296 rows; 8×8 pixel cells */
    for (int y = 0; y < 296; y++) {
        for (int xb = 0; xb < 16; xb++) {
            int cell = ((y / 8) + xb) & 1;
            s_image_0[y * 16 + xb] = cell ? 0x00 : 0xFF;
        }
    }

    ESP_LOGI(TAG, "Initialized const image backend (%u images, frame=%u)",
             (unsigned)(sizeof(s_images) / sizeof(s_images[0])), ESL_FULL_FRAME_LEN);
    return ESP_OK;
}

esp_err_t esl_image_manager_get(uint32_t image_id, esl_image_desc_t *out)
{
    if (out == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    for (int i = 0; i < ESL_IMAGE_RUNTIME_SLOTS; i++) {
        if (s_runtime[i].used && s_runtime[i].image_id == image_id) {
            out->image_id = s_runtime[i].image_id;
            out->image_index = s_runtime[i].image_index;
            out->version = s_runtime[i].version;
            out->data = s_runtime[i].data;
            out->data_len = s_runtime[i].data_len;
            return ESP_OK;
        }
    }

    const esl_image_entry_t *entry = find_by_image_id(image_id);
    if (entry == NULL) {
        return ESP_ERR_NOT_FOUND;
    }

    fill_desc(entry, out);
    return ESP_OK;
}

esp_err_t esl_image_manager_store_runtime(uint32_t image_id, uint8_t image_index, uint32_t version,
                                          const uint8_t *data, uint32_t data_len)
{
    if (data == NULL || data_len == 0 || data_len > ESL_IMAGE_RUNTIME_MAX_LEN) {
        return ESP_ERR_INVALID_ARG;
    }
    if (image_index >= ESL_IMAGE_MANAGER_MAX_IMAGES) {
        return ESP_ERR_INVALID_ARG;
    }

    esl_image_runtime_slot_t *slot = NULL;
    for (int i = 0; i < ESL_IMAGE_RUNTIME_SLOTS; i++) {
        if (s_runtime[i].used && s_runtime[i].image_id == image_id) {
            slot = &s_runtime[i];
            break;
        }
    }
    if (slot == NULL) {
        for (int i = 0; i < ESL_IMAGE_RUNTIME_SLOTS; i++) {
            if (!s_runtime[i].used) {
                slot = &s_runtime[i];
                break;
            }
        }
    }
    if (slot == NULL) {
        slot = &s_runtime[0];
    }

    slot->used = true;
    slot->image_id = image_id;
    slot->image_index = image_index;
    slot->version = version;
    slot->data_len = data_len;
    memcpy(slot->data, data, data_len);
    ESP_LOGI(TAG, "Stored runtime image_id=%" PRIu32 " index=%u len=%" PRIu32,
             image_id, image_index, data_len);
    return ESP_OK;
}

static esp_err_t step_for_tag(uint16_t esl_addr, int direction, esl_image_desc_t *out)
{
    if (out == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    esl_image_tag_state_t *state = find_or_create_tag_state(esl_addr);
    if (state == NULL) {
        return ESP_ERR_NO_MEM;
    }

    uint8_t start = state->current_image_index;
    for (int step = 1; step < ESL_IMAGE_MANAGER_MAX_IMAGES; step++) {
        int idx = (int)start + direction * step;
        while (idx < 0) {
            idx += ESL_IMAGE_MANAGER_MAX_IMAGES;
        }
        idx %= ESL_IMAGE_MANAGER_MAX_IMAGES;

        if (!state->transferred[idx]) {
            continue;
        }

        const esl_image_entry_t *entry = find_by_image_index((uint8_t)idx);
        if (entry == NULL) {
            continue;
        }

        state->current_image_index = (uint8_t)idx;
        fill_desc(entry, out);
        return ESP_OK;
    }

    return ESP_ERR_NOT_FOUND;
}

esp_err_t esl_image_manager_next_for_tag(uint16_t esl_addr, esl_image_desc_t *out)
{
    return step_for_tag(esl_addr, 1, out);
}

esp_err_t esl_image_manager_prev_for_tag(uint16_t esl_addr, esl_image_desc_t *out)
{
    return step_for_tag(esl_addr, -1, out);
}

esp_err_t esl_image_manager_mark_transferred(uint16_t esl_addr, uint32_t image_id, uint8_t image_index)
{
    if (image_index >= ESL_IMAGE_MANAGER_MAX_IMAGES) {
        return ESP_ERR_INVALID_ARG;
    }

    esl_image_tag_state_t *state = find_or_create_tag_state(esl_addr);
    if (state == NULL) {
        return ESP_ERR_NO_MEM;
    }

    state->transferred[image_index] = true;
    state->current_image_index = image_index;

    ESP_LOGI(TAG, "Tag 0x%04x transferred image_id=%" PRIu32 " index=%u",
             esl_addr, image_id, image_index);
    return ESP_OK;
}

uint32_t esl_image_manager_count(void)
{
    return (uint32_t)(sizeof(s_images) / sizeof(s_images[0]));
}
