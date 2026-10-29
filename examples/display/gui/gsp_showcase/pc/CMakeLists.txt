# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.20)
project(gsp_showcase_backend LANGUAGES C)

if(NOT ESP_GSP_COMPONENT_DIR)
    get_filename_component(ESP_GSP_COMPONENT_DIR "../managed_components/espressif__esp-gsp" ABSOLUTE)
endif()
if(NOT EXISTS "${ESP_GSP_COMPONENT_DIR}/tools/sim_bridge/CMakeLists.txt")
    message(FATAL_ERROR "Install espressif/esp-gsp first, or set ESP_GSP_COMPONENT_DIR")
endif()

add_subdirectory("${ESP_GSP_COMPONENT_DIR}/tools/sim_bridge" sim_bridge)
gsp_add_backend(gsp_showcase_backend
    SOURCES ../main/showcase_ui.c platform_pc.c
    INCLUDE_DIRECTORIES ../main
    SCENES ../scenes/controls.json ../scenes/media.json ../scenes/motion.json ../scenes/data.json)
target_compile_definitions(gsp_showcase_backend PRIVATE
    SHOWCASE_PC=1
    SHOWCASE_BADGE_PATH="${CMAKE_CURRENT_LIST_DIR}/../main/runtime_badge.png")
