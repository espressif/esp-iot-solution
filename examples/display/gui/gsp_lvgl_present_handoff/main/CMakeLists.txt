# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
#
# SPDX-License-Identifier: Apache-2.0

idf_component_register(
    SRCS
        "main.c"
        "handoff_stress_runner.c"
        "handoff_lvgl_screen.c"
    INCLUDE_DIRS "."
    PRIV_REQUIRES esp_lv_present hw_init lvgl esp_timer esp_lcd
)

set(handoff_scene_dir "${CMAKE_CURRENT_LIST_DIR}/../scenes")
if(CONFIG_EXAMPLE_LCD_INTERFACE_MIPI_DSI)
    set(handoff_scene "${handoff_scene_dir}/hello_1024.json")
elseif(CONFIG_EXAMPLE_LCD_INTERFACE_RGB)
    if(CONFIG_EXAMPLE_DISPLAY_ROTATION_90 OR
       CONFIG_EXAMPLE_DISPLAY_ROTATION_270)
        set(handoff_scene "${handoff_scene_dir}/hello_480.json")
    else()
        set(handoff_scene "${handoff_scene_dir}/hello_800.json")
    endif()
elseif(CONFIG_EXAMPLE_LCD_INTERFACE_SPI_WITH_PSRAM)
    set(handoff_scene "${handoff_scene_dir}/hello_320.json")
elseif(CONFIG_EXAMPLE_LCD_INTERFACE_SPI_WITHOUT_PSRAM)
    set(handoff_scene "${handoff_scene_dir}/hello_240.json")
else()
    set(handoff_scene "${handoff_scene_dir}/hello_360.json")
endif()

# Registry components are namespaced (espressif__esp-gsp). The bare
# `build_components` variable is not populated in a component CMakeLists;
# use the IDF property, GSP's own export, then the managed_components path.
set(_gsp_dir "")
get_property(_gsp_dir GLOBAL PROPERTY ESP_GSP_COMPONENT_DIR)
idf_build_get_property(_build_components BUILD_COMPONENTS)
if(_gsp_dir STREQUAL "")
    foreach(_name IN LISTS _build_components)
        if(_name MATCHES "esp-gsp$")
            idf_component_get_property(_gsp_dir ${_name} COMPONENT_DIR)
            break()
        endif()
    endforeach()
endif()
if(_gsp_dir STREQUAL "" AND
   EXISTS "${PROJECT_DIR}/managed_components/espressif__esp-gsp/cmake/gsp_bundle.cmake")
    set(_gsp_dir "${PROJECT_DIR}/managed_components/espressif__esp-gsp")
endif()
if(_gsp_dir STREQUAL "" OR NOT EXISTS "${_gsp_dir}/cmake/gsp_bundle.cmake")
    message(FATAL_ERROR
        "esp-gsp is required for gsp_lvgl_present_handoff. "
        "BUILD_COMPONENTS=${_build_components}")
endif()
include("${_gsp_dir}/cmake/gsp_bundle.cmake")

# Scene JSON references this font by basename. Keep it in-tree so CI does not
# fetch GitHub (registry esp-gsp excludes examples/fonts).
if(NOT EXISTS "${handoff_scene_dir}/DejaVuSans.ttf")
    message(FATAL_ERROR
        "DejaVuSans.ttf is required next to the scene JSON files in "
        "${handoff_scene_dir}.")
endif()

gsp_add_bundle(${COMPONENT_LIB}
    SCENES ${handoff_scene}
    PIXEL_FORMAT rgb565)
