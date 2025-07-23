# Detect IDF version first (IDF_VERSION* variables defined since >=4.4)
include($ENV{IDF_PATH}/tools/cmake/version.cmake)

if("${IDF_VERSION_MAJOR}.${IDF_VERSION_MINOR}" VERSION_LESS "4.4")
    return()
endif()

set(ADD_SRCS "")
set(ADD_LIBS "")
set(PRIV_REQ "")

# IDF target specific requirements
idf_build_get_property(target IDF_TARGET)
if(${target} STREQUAL "esp32p4")
    # PPA driver required on ESP32-P4
    list(APPEND ADD_LIBS idf::esp_driver_ppa)
    list(APPEND PRIV_REQ esp_driver_ppa)
endif()

# Always use these base deps
list(APPEND PRIV_REQ esp_mm)

# Register an INTERFACE component first; real sources added later after LVGL version detection
idf_component_register(
    INCLUDE_DIRS "."
    REQUIRES esp_lcd
    PRIV_REQUIRES ${PRIV_REQ}
)

# Determine LVGL component name and version
idf_build_get_property(build_components BUILD_COMPONENTS)
if(lvgl IN_LIST build_components)
    set(lvgl_name lvgl) # Local component
    # LVGL v9.2+ sets LVGL_VERSION env; use it when available
    set(lvgl_ver $ENV{LVGL_VERSION})
else()
    set(lvgl_name lvgl__lvgl) # Managed component from registry
    idf_component_get_property(lvgl_ver ${lvgl_name} COMPONENT_VERSION)
endif()

if("${lvgl_ver}" STREQUAL "")
    message("Could not determine LVGL version, assuming v9.x")
    set(lvgl_ver "9.0.0")
endif()

message(STATUS "LVGL version: ${lvgl_ver}")

# Select sources by LVGL version
if(lvgl_ver VERSION_LESS "9.0.0")
    set(PORT_SRCS
        "lvglv8/lvgl_port_v8.c"
        "lvgl_fps_stats.c"
    )
    set(PORT_INCLUDE_DIRS "lvglv8")
else()
    set(PORT_SRCS
        "lvglv9/lvgl_port_v9.c"
        "lvgl_fps_stats.c"
    )
    set(PORT_INCLUDE_DIRS "lvglv9")
endif()

# Optionally link touch component if present in the build
idf_build_get_property(build_components BUILD_COMPONENTS)
if("espressif__esp_lcd_touch" IN_LIST build_components)
    list(APPEND ADD_LIBS idf::espressif__esp_lcd_touch)
endif()
if("esp_lcd_touch" IN_LIST build_components)
    list(APPEND ADD_LIBS idf::esp_lcd_touch)
endif()

# Create the real library now that we know the LVGL version
add_library(lvgl_port_lib STATIC
    ${PORT_SRCS}
    ${ADD_SRCS}
)

target_include_directories(lvgl_port_lib PUBLIC ${PORT_INCLUDE_DIRS} ".")
idf_component_get_property(ESP_LCD_DIR esp_lcd COMPONENT_DIR)
if(ESP_LCD_DIR)
    # Access esp_lcd private headers like esp_async_fbcpy.h
    target_include_directories(lvgl_port_lib PRIVATE "${ESP_LCD_DIR}/priv_include")
endif()
target_link_libraries(lvgl_port_lib PUBLIC
    idf::esp_lcd
    idf::${lvgl_name}
)
target_link_libraries(lvgl_port_lib PRIVATE
    idf::esp_timer
    idf::esp_mm
    ${ADD_LIBS}
)

# Relax LVGL warning for format if available
idf_component_get_property(lvgl_comp_lib ${lvgl_name} COMPONENT_LIB)
if(lvgl_comp_lib)
    target_compile_options(${lvgl_comp_lib} PRIVATE -Wno-format)
endif()

# Finally, make this component link to the actual library
target_link_libraries(${COMPONENT_LIB} INTERFACE lvgl_port_lib)
