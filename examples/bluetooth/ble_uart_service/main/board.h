/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <stdint.h>

#include "esp_err.h"
#include "emote_gen_player.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief First capacitive touch pad channel.
 *
 * V1_2 boards use pad 7 and pad 6. V1_0 boards use only pad 7.
 */
#define BOARD_TOUCH_PAD_0 7u

/**
 * @brief Second capacitive touch pad channel.
 *
 * Only used when @ref BOARD_TOUCH_SLIDER_ENABLED is enabled.
 */
#define BOARD_TOUCH_PAD_1 6u

/**
 * @brief Enable touch slider handling on top of the shared touch low-level driver.
 *
 * Set this to 0 when using one touch key or when debugging key events without
 * the slider FSM.
 */
#define BOARD_TOUCH_SLIDER_ENABLED 1

/**
 * @brief Relative threshold for touch-button detection.
 *
 * Lower values are more sensitive. Tune this value if the touch key does not
 * trigger or false-triggers.
 */
#ifndef BOARD_TOUCH_BUTTON_THRESHOLD
#define BOARD_TOUCH_BUTTON_THRESHOLD 0.05f
#endif

/**
 * @brief Board runtime object.
 */
typedef struct {
    /**
     * @brief Emote player handle created by @ref board_start.
     */
    emote_gen_player_handle_t player;
} board_app_t;

/**
 * @brief Touch event source.
 */
typedef enum {
    /**
     * @brief Capacitive key pads.
     *
     * @c board_touch_event_t::code is a @c button_event_t value.
     */
    BOARD_TOUCH_SOURCE_KEY = 0,

    /**
     * @brief Panel touch controller.
     *
     * @c board_touch_event_t::code is a @c gfx_touch_event_type_t value.
     */
    BOARD_TOUCH_SOURCE_PANEL,
} board_touch_src_t;

/**
 * @brief Normalized board touch event.
 */
typedef struct {
    /**
     * @brief Event source.
     */
    board_touch_src_t src;

    /**
     * @brief Source-specific event code.
     *
     * For @ref BOARD_TOUCH_SOURCE_KEY this is a @c button_event_t value. For
     * @ref BOARD_TOUCH_SOURCE_PANEL this is a @c gfx_touch_event_type_t value.
     */
    int code;

    /**
     * @brief Key index for @ref BOARD_TOUCH_SOURCE_KEY events.
     *
     * This is 0 or 1 for key events and -1 for panel touch events.
     */
    int8_t key_index;
} board_touch_event_t;

/**
 * @brief Board touch callback.
 *
 * Called for every subscribed key event and every panel touch event.
 *
 * @param user_data User context passed to @ref board_set_touch_callback.
 * @param ev Touch event. The pointer is only valid during the callback.
 */
typedef void (*board_touch_cb_t)(void *user_data, const board_touch_event_t *ev);

/**
 * @brief Register a board touch callback.
 *
 * @param cb Callback to invoke. Pass NULL to clear the callback.
 * @param user_data User context passed back to @p cb.
 */
void board_set_touch_callback(board_touch_cb_t cb, void *user_data);

/**
 * @brief Initialize display, touch input, and emote player resources.
 *
 * @param app Board runtime object to initialize.
 *
 * @return
 *      - ESP_OK on success
 *      - ESP_ERR_INVALID_ARG if @p app is NULL
 *      - ESP_FAIL or another esp_err_t on initialization failure
 */
esp_err_t board_start(board_app_t *app);

/**
 * @brief Stop the board runtime and release resources.
 *
 * @param app Board runtime object returned by @ref board_start.
 */
void board_stop(board_app_t *app);

#ifdef __cplusplus
}
#endif
