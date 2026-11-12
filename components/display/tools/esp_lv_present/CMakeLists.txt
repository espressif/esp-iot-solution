# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Apache-2.0
# Stable LVGL API and native binding share the presentation engine.
idf_component_register(SRCS src/esp_lv_present.c src/present_lvgl.c INCLUDE_DIRS include PRIV_INCLUDE_DIRS src
    REQUIRES esp_display_present lvgl PRIV_REQUIRES esp_timer)

# Same narrow IDF atomic.h workaround as the original esp_lv_present component.
idf_build_get_property(build_components BUILD_COMPONENTS)
if("lvgl" IN_LIST build_components)
    set(lvgl_component lvgl)
else()
    set(lvgl_component lvgl__lvgl)
endif()
idf_component_get_property(lvgl_dir ${lvgl_component} COMPONENT_DIR)
idf_component_get_property(lvgl_lib ${lvgl_component} COMPONENT_LIB)
if(TARGET ${lvgl_lib})
    set_property(SOURCE "${lvgl_dir}/src/osal/lv_freertos.c"
        TARGET_DIRECTORY ${lvgl_lib} APPEND PROPERTY COMPILE_OPTIONS -Wno-unused-function)
endif()

idf_component_get_property(_present_dir esp_display_present COMPONENT_DIR)
target_include_directories(${COMPONENT_LIB} PRIVATE "${_present_dir}/src")
