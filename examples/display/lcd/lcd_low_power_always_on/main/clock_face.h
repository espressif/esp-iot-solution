/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */
#pragma once

#include "panel.h"

#define CLOCK_FACE_FIRST_DRAW (-1)

void clock_face_draw(const clock_panel_t *panel, int previous_minute, int current_minute);
