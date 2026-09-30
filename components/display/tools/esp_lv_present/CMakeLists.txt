# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
#
# SPDX-License-Identifier: Apache-2.0

idf_component_register(
    SRCS "src/esp_lv_present.c"
    INCLUDE_DIRS "include"
    REQUIRES esp_display_present lvgl
    PRIV_REQUIRES esp_timer
)

# IDF's atomic.h defines unused static helpers in the LVGL FreeRTOS backend.
# Keep this compatibility flag local to that source, not the whole LVGL target.
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
