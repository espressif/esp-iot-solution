set(srcs
    "hw_init.c"
    "input/touch_init.c"
    "input/touch_rotation_helper.c"
    "input/knob_init.c"
    "helpers/lcd_orientation_helper.c"
)

set(include_dirs "." "lcd" "input" "helpers")

set(component_requires driver esp_lcd esp_lvgl_adapter)

# Get the list of available components
idf_build_get_property(build_components BUILD_COMPONENTS)

if(IDF_TARGET STREQUAL "esp32p4")
    list(APPEND srcs "lcd/lcd_init_mipi_dsi.c")
    if("esp_lcd_ek79007" IN_LIST build_components)
        list(APPEND component_requires esp_lcd_ek79007)
    endif()
    if("espressif__esp_lcd_touch_gt911" IN_LIST build_components)
        list(APPEND component_requires espressif__esp_lcd_touch_gt911)
    endif()
elseif(IDF_TARGET STREQUAL "esp32c3")
    list(APPEND srcs "lcd/lcd_init_spi_without_psram.c")
    if("espressif__esp_lcd_gc9a01" IN_LIST build_components)
        list(APPEND component_requires espressif__esp_lcd_gc9a01)
    endif()
    # Conditionally add knob and button components if available
    if("espressif__knob" IN_LIST build_components)
        list(APPEND component_requires espressif__knob)
    endif()
    if("espressif__button" IN_LIST build_components)
        list(APPEND component_requires espressif__button)
    endif()
else()
    list(APPEND srcs "lcd/lcd_init_spi_with_psram.c")
    if("espressif__esp_lcd_ili9341" IN_LIST build_components)
        list(APPEND component_requires espressif__esp_lcd_ili9341)
    endif()
    if("espressif__esp_lcd_touch_gt911" IN_LIST build_components)
        list(APPEND component_requires espressif__esp_lcd_touch_gt911)
    endif()
endif()

idf_component_register(
    SRCS ${srcs}
    INCLUDE_DIRS ${include_dirs}
    REQUIRES ${component_requires}
)

if(IDF_TARGET STREQUAL "esp32p4")
    message(STATUS "hw_init: MIPI DSI + Touch")
elseif(IDF_TARGET STREQUAL "esp32c3")
    message(STATUS "hw_init: SPI (No PSRAM) + Encoder")
else()
    message(STATUS "hw_init: SPI (PSRAM) + Touch")
endif()
