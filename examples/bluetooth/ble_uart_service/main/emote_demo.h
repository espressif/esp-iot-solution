/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Start the emote demo UI.
 *
 * Initializes the board layer, starts the emote player, and registers the touch
 * callback used for permission decisions.
 */
void emote_demo_run(void);

/**
 * @brief Show a permission request prompt.
 *
 * @param perm_type Permission type to display, such as `"bash"` or `"edit"`.
 * @param title Permission title to display.
 * @param meta_compact Optional compact metadata string, usually formatted as
 * `"key: value"`.
 */
void emote_demo_show_permission(const char *perm_type, const char *title, const char *meta_compact);

/**
 * @brief Show permission timeout feedback.
 *
 * Clears the prompt and plays the timeout animation. The protocol reply remains
 * `"reject"`.
 */
void emote_demo_show_permission_timeout(void);

/**
 * @brief Clear the permission prompt UI.
 *
 * Used for cancellation, idle-state cleanup, and disconnect cleanup.
 */
void emote_demo_clear_permission(void);

/**
 * @brief Show OpenCode session status.
 *
 * @param status_type Session status string. Supported values are `"busy"`,
 * `"idle"`, and `"retry"`.
 *
 * @note This function only updates UI state. It must not send a BLE reply.
 */
void emote_demo_show_session_status(const char *status_type);

#ifdef __cplusplus
}
#endif
