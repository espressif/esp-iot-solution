# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Apache-2.0
cmake_minimum_required(VERSION 3.16)
set(ENV{IDF_COMPONENT_MANAGER} "0")
set(EXTRA_COMPONENT_DIRS "${CMAKE_CURRENT_LIST_DIR}/../../.." "${PRESENT_DEPS_DIR}")
set(COMPONENTS main)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(present_rgb_handoff)
foreach(symbol IN ITEMS esp_lcd_panel_draw_bitmap esp_lcd_rgb_panel_register_event_callbacks present_worker_create)
    target_link_options(${CMAKE_PROJECT_NAME}.elf PRIVATE "-Wl,--wrap=${symbol}")
endforeach()
