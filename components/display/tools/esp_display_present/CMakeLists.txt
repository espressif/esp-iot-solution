# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
#
# SPDX-License-Identifier: Apache-2.0

# Requirements are collected during early expansion, before Kconfig
# runs, so hardware gating cannot use CONFIG_SOC_* here. The
# accelerator drivers register as empty libraries on chips without the
# peripheral, so they are safe unconditional requirements; the code is
# gated on CONFIG_SOC_PPA_SUPPORTED / CONFIG_SOC_DMA2D_SUPPORTED.
set(PRESENT_PRIV_REQ esp_timer esp_mm esp_psram esp_driver_ppa)

if(EXISTS "${IDF_PATH}/components/esp_driver_dma/include/esp_async_color_convert.h")
    set(_has_async_color_convert ON)
    list(APPEND PRESENT_PRIV_REQ esp_driver_dma)
elseif(EXISTS "${IDF_PATH}/components/esp_lcd/priv_include/esp_async_fbcpy.h")
    set(_has_async_fbcpy ON)
endif()

idf_component_register(
    SRCS
        "src/esp_display_presenter.c"
        "src/producer/bind/present_endpoint.c"
        "src/producer/mode/present_mode_ops.c"
        "src/runtime/present_drawbuf.c"
        "src/runtime/present_frame_tracker.c"
        "src/producer/stage/present_gram_transport.c"
        "src/producer/present_fb_render.c"
        "src/producer/present_fb_fence.c"
        "src/producer/present_tile_acquire.c"
        "src/producer/stage/present_transfer_wait.c"
        "src/producer/present_fb_tile_compose.c"
        "src/producer/present_async_copy.c"
        "src/target/present_target_region.c"
        "src/producer/stage/present_fb_pipeline.c"
        "src/producer/present_buffer_repair.c"
        "src/producer/present_fb_repair.c"
        "src/producer/present_te_render.c"
        "src/producer/present_te_pool.c"
        "src/producer/present_te_repair.c"
        "src/producer/stage/present_te_transport.c"
        "src/platform/present_te.c"
        "src/platform/present_cache.c"
        "src/platform/present_hw.c"
        "src/pixel/present_geometry.c"
        "src/pixel/present_blit.c"
        "src/pixel/present_rotate.c"
        "src/pixel/present_transform.c"
        "src/pixel/present_ppa.c"
        "src/pixel/present_dirty.c"
        "src/target/present_target.c"
        "src/target/present_target_resolve.c"
        "src/target/present_profile.c"
        "src/target/present_panel.c"
    INCLUDE_DIRS "include"
    PRIV_INCLUDE_DIRS "private_include"
    REQUIRES esp_lcd esp_driver_gpio
    PRIV_REQUIRES ${PRESENT_PRIV_REQ}
)

if(_has_async_color_convert)
    target_compile_definitions(${COMPONENT_LIB} PRIVATE ESP_ASYNC_COLOR_CONVERT_AVAILABLE=1)
endif()
if(_has_async_fbcpy)
    target_include_directories(${COMPONENT_LIB} PRIVATE
        "${IDF_PATH}/components/esp_lcd/priv_include")
    target_compile_definitions(${COMPONENT_LIB} PRIVATE ESP_ASYNC_FBCPY_AVAILABLE=1)
endif()
