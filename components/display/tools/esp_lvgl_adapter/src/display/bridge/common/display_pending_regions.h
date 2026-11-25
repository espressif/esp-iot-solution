/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "lvgl.h"

/* Add a clipped, inclusive rectangle to conservative framebuffer damage.
 * Merge touching/overlapping rectangles. At capacity merge the incoming
 * rectangle with the existing rectangle that adds the least area. This may
 * expand damage, so it must never be used for current render coverage or
 * repair regions after subtraction.
 * Returns false for invalid arguments or zero capacity. */
bool display_pending_regions_add(lv_area_t *rects, uint16_t *count,
                                 uint16_t capacity, lv_area_t rect);
