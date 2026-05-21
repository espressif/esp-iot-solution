/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

/*
 * Keep the registry FreeType defaults, but shrink the raster render pool used
 * by ftgrays.c/ftraster.c. The upstream default is 16KB and is allocated as a
 * local array during glyph rasterization, which forces large LVGL draw stacks.
 */
#include <freetype/config/ftoption.h>

#undef FT_RENDER_POOL_SIZE
#define FT_RENDER_POOL_SIZE 4096L
