/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <stdint.h>
#include "keyboard.h"
#include "keycodes.h"
#include "quantum_keycodes.h"

#ifdef __cplusplus
extern "C" {
#endif

static uint16_t keymaps[][KBD_ROW_NUM][KBD_COL_NUM] = {
    /*
    * ┌───┬───┬───┬───┬───┬───┬───┬───┬───┬───┬───┬───┬───┬───┬───┬───┐
    * │Esc│F1 │F2 │F3 │F4 │   │F5 │F6 │F7 │F8 │   │F9 │F10│F11│F12│Del│
    * ├───┼───┼───┼───┼───┼───┼───┼───┼───┼───┼───┼───┼───┼───┴───┼───┤
    * │ ` │ 1 │ 2 │ 3 │ 4 │ 5 │ 6 │ 7 │ 8 │ 9 │ 0 │ - │ = │ Backsp│Hom│
    * ├───┴─┬─┴─┬─┴─┬─┴─┬─┴─┬─┴─┬─┴─┬─┴─┬─┴─┬─┴─┬─┴─┬─┴─┬─┴─┬─────┼───┤
    * │ Tab │ Q │ W │ E │ R │ T │ Y │ U │ I │ O │ P │ [ │ ] │  \  │PgU│
    * ├─────┴┬──┴┬──┴┬──┴┬──┴┬──┴┬──┴┬──┴┬──┴┬──┴┬──┴┬──┴┬──┴─────┼───┤
    * │ Caps │ A │ S │ D │ F │ G │ H │ J │ K │ L │ ; │ ' │  Enter │PgD│
    * ├──────┴─┬─┴─┬─┴─┬─┴─┬─┴─┬─┴─┬─┴─┬─┴─┬─┴─┬─┴─┬─┴─┬─┴────┬───┼───┤
    * │ Shift  │ Z │ X │ C │ V │ B │ N │ M │ , │ . │ / │ Shift│ ↑ │End│
    * ├────┬───┴┬──┴─┬─┴───┴───┴───┴───┴───┴──┬┴──┬┴──┬┴──┬───┼───┼───┤
    * │Ctrl│GUI │Alt │                        │Alt│GUI│Ctl│ ← │ ↓ │ → │
    * └────┴────┴────┴────────────────────────┴───┴───┴───┴───┴───┴───┘
    */
    [0] = {
        {KC_ESC,  KC_NO,   KC_F1,   KC_F2,   KC_F3,   KC_F4,   KC_F5,   KC_F6,   KC_F7,    KC_F8,   KC_F9,   KC_F10,    KC_F11,  KC_F12,     KC_DEL},
        {KC_GRV,  KC_1,    KC_2,    KC_3,    KC_4,    KC_5,    KC_6,    KC_7,    KC_8,     KC_9,    KC_0,    KC_MINS,   KC_EQL,  KC_BSPC,    KC_HOME},
        {KC_TAB,  KC_Q,    KC_W,    KC_E,    KC_R,    KC_T,    KC_Y,    KC_U,    KC_I,     KC_O,    KC_P,    KC_LBRC,   KC_RBRC, KC_BSLS,    KC_PGUP},
        {KC_CAPS, KC_A,    KC_S,    KC_D,    KC_F,    KC_G,    KC_H,    KC_J,    KC_K,     KC_L,    KC_SCLN, KC_QUOT,   KC_ENT,  KC_NO,      KC_PGDN},
        {KC_LSFT, KC_Z,    KC_X,    KC_C,    KC_V,    KC_B,    KC_N,    KC_M,    KC_COMMA, KC_DOT,  KC_SLSH, KC_RSFT,   KC_UP,   KC_NO,      KC_END},
        {KC_LCTL, KC_LGUI, KC_LALT, KC_NO,   KC_NO,   KC_SPC,  KC_NO,   KC_NO,   KC_RALT,  MO(1),  KC_RCTL, KC_LEFT,   KC_DOWN, KC_NO,      KC_RGHT},
    },
    [1] = {
        {KC_ESC,  KC_NO,   QK_OUTPUT_USB,   QK_OUTPUT_BLUETOOTH,   KC_F3,   KC_F4,   KC_F5,   KC_F6,   KC_F7,    KC_F8,   KC_F9,   KC_KB_MUTE, KC_KB_VOLUME_UP, KC_KB_VOLUME_DOWN, KC_DEL},
        {KC_GRV,  KC_1,    KC_2,    KC_3,    KC_4,    KC_5,    KC_6,    KC_7,    KC_8,     KC_9,    KC_0,    KC_MINS,   KC_EQL,  KC_BSPC,    RGB_TOG},
        {KC_TAB,  KC_Q,    KC_W,    KC_E,    KC_R,    KC_T,    KC_Y,    KC_U,    KC_I,     KC_O,    KC_P,    KC_LBRC,   KC_RBRC, KC_BSLS,    RGB_MODE_FORWARD},
        {KC_CAPS, KC_A,    KC_S,    KC_D,    KC_F,    KC_G,    KC_H,    KC_J,    KC_K,     KC_L,    KC_SCLN, KC_QUOT,   KC_ENT,  KC_NO,      RGB_MODE_REVERSE},
        {KC_LSFT, KC_Z,    KC_X,    KC_C,    KC_V,    KC_B,    KC_N,    KC_M,    KC_COMMA, KC_DOT,  KC_SLSH, KC_RSFT,   QK_BACKLIGHT_UP,   KC_NO,      KC_NUM_LOCK},
        {KC_LCTL, KC_LGUI, KC_LALT, KC_NO,   KC_NO,   QK_BACKLIGHT_TOGGLE,  KC_NO,   KC_NO,   KC_RALT,  MO(1),  KC_RCTL, RGB_SPD,   QK_BACKLIGHT_DOWN, KC_NO,      RGB_SPI},
    },
};

#ifdef __cplusplus
}
#endif
