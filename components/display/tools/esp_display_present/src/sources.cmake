# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Apache-2.0
# Shared explicit source inventory for IDF and Host. Paths are absolute so the
# including build directory does not affect resolution. No recursive globbing.
set(PRESENT_OS_SOURCES
    ${CMAKE_CURRENT_LIST_DIR}/platform/present_os.c)
set(PRESENT_PIXEL_OPS_SOURCES
    ${CMAKE_CURRENT_LIST_DIR}/pixel/present_pixel_ops.c)
set(PRESENT_PIXEL_SOURCES
    ${CMAKE_CURRENT_LIST_DIR}/pixel/present_pixel_engine.c
    ${PRESENT_PIXEL_OPS_SOURCES}
    ${CMAKE_CURRENT_LIST_DIR}/pixel/executor/present_executor.c
    ${CMAKE_CURRENT_LIST_DIR}/pixel/executor/present_executor_cpu.c
    ${CMAKE_CURRENT_LIST_DIR}/pixel/executor/present_executor_ppa.c
    ${CMAKE_CURRENT_LIST_DIR}/pixel/executor/present_executor_dma2d.c)
set(PRESENT_ENGINE_SOURCES
    ${PRESENT_OS_SOURCES}
    ${CMAKE_CURRENT_LIST_DIR}/platform/te/present_te_timing.c
    ${CMAKE_CURRENT_LIST_DIR}/platform/te/present_te_timer.c
    ${CMAKE_CURRENT_LIST_DIR}/backend/present_image_repair.c
    ${CMAKE_CURRENT_LIST_DIR}/backend/present_frame_core.c
    ${CMAKE_CURRENT_LIST_DIR}/backend/present_backend_validate.c
    ${CMAKE_CURRENT_LIST_DIR}/config/present_plan.c
    ${CMAKE_CURRENT_LIST_DIR}/config/present_mode.c
    ${CMAKE_CURRENT_LIST_DIR}/core/present_engine.c
    ${CMAKE_CURRENT_LIST_DIR}/core/present_transfer_tracker.c
    ${CMAKE_CURRENT_LIST_DIR}/core/present_completion_tracker.c
    ${PRESENT_PIXEL_SOURCES}
    ${CMAKE_CURRENT_LIST_DIR}/backend/present_backend_region.c
    ${CMAKE_CURRENT_LIST_DIR}/platform/lcd/present_lcd_io.c
    ${CMAKE_CURRENT_LIST_DIR}/platform/lcd/present_lcd_rgb_info.c
    ${CMAKE_CURRENT_LIST_DIR}/platform/lcd/present_lcd_fb_scanout.c
    ${CMAKE_CURRENT_LIST_DIR}/backend/present_te_composer.c
    ${CMAKE_CURRENT_LIST_DIR}/backend/present_backend_te.c
    ${CMAKE_CURRENT_LIST_DIR}/backend/present_backend_fb.c)
set(PRESENT_RESOURCE_SOURCES
    ${CMAKE_CURRENT_LIST_DIR}/resources/present_resources.c)
set(PRESENT_FACADE_SOURCES
    ${CMAKE_CURRENT_LIST_DIR}/esp_display_presenter.c
    ${CMAKE_CURRENT_LIST_DIR}/config/present_geometry.c)
set(PRESENT_TE_SYNC_SOURCES
    ${CMAKE_CURRENT_LIST_DIR}/platform/te/present_te_sync.c)
