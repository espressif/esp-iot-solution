set(srcs
    "hw_init.c"
    "input/touch_init.c"
    "input/touch_rotation_helper.c"
    "input/knob_init.c"
    "helpers/lcd_orientation_helper.c"
)

set(include_dirs "." "lcd" "input" "helpers")

set(component_requires driver esp_lcd esp_lvgl_adapter)

if(IDF_TARGET STREQUAL "esp32p4")
    list(APPEND srcs "lcd/lcd_init_mipi_dsi.c")
    list(APPEND component_requires esp_lcd_ek79007)
    list(APPEND component_requires espressif__esp_lcd_touch_gt911)
elseif(IDF_TARGET STREQUAL "esp32c3")
    list(APPEND srcs "lcd/lcd_init_spi_without_psram.c")
    list(APPEND component_requires espressif__esp_lcd_gc9a01)
    list(APPEND component_requires espressif__knob)
    list(APPEND component_requires espressif__button)
else()
    list(APPEND srcs "lcd/lcd_init_spi_with_psram.c")
    list(APPEND component_requires espressif__esp_lcd_ili9341)
    list(APPEND component_requires espressif__esp_lcd_touch_gt911)
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
