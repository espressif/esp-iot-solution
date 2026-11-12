# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Apache-2.0
cmake_minimum_required(VERSION 3.16)
set(EXTRA_COMPONENT_DIRS "${CMAKE_CURRENT_LIST_DIR}/../../..")
set(COMPONENTS main)
if(PRESENT_LVGL_DIR)
    list(APPEND EXTRA_COMPONENT_DIRS "${PRESENT_LVGL_DIR}"
        "${CMAKE_CURRENT_LIST_DIR}/../../../../esp_lv_present")
    get_filename_component(PRESENT_LVGL_COMPONENT "${PRESENT_LVGL_DIR}" NAME)
    list(APPEND COMPONENTS esp_lv_present "${PRESENT_LVGL_COMPONENT}")
endif()
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(present_smoke)
