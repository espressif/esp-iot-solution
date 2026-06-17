/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

/*
 * Start from the registry FreeType defaults, then apply LVGL adapter specific
 * overrides for stack and flash optimization.
 */
#include <freetype/config/ftoption.h>

#ifdef CONFIG_ESP_LVGL_ADAPTER_FREETYPE_MINIMAL_BUILD
#undef FT_CONFIG_OPTION_ENVIRONMENT_PROPERTIES
#undef FT_CONFIG_OPTION_USE_LZW
#undef FT_CONFIG_OPTION_USE_ZLIB
#undef FT_CONFIG_OPTION_INCREMENTAL
#undef FT_CONFIG_OPTION_SVG

#undef FT_MAX_MODULES
#define FT_MAX_MODULES 12
#endif

#ifdef CONFIG_ESP_LVGL_ADAPTER_FREETYPE_SMALL_RENDER_POOL
/*
 * Shrink the raster render pool used by ftgrays.c/ftraster.c. The upstream
 * default is 16KB and is allocated as a local array during glyph
 * rasterization, which forces large LVGL draw stacks.
 */
#undef FT_RENDER_POOL_SIZE
#define FT_RENDER_POOL_SIZE 4096L
#endif
