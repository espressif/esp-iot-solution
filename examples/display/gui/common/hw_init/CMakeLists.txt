set(srcs
    "hw_init.c"
    "input/touch_init.c"
    "input/touch_rotation_helper.c"
    "input/knob_init.c"
    "helpers/lcd_orientation_helper.c"
)

if(CONFIG_EXAMPLE_LCD_INTERFACE_MIPI_DSI)
    list(APPEND srcs "lcd/lcd_init_mipi_dsi.c")
elseif(CONFIG_EXAMPLE_LCD_INTERFACE_QSPI)
    list(APPEND srcs "lcd/lcd_init_qspi.c")
elseif(CONFIG_EXAMPLE_LCD_INTERFACE_RGB)
    list(APPEND srcs "lcd/lcd_init_rgb.c")
elseif(CONFIG_EXAMPLE_LCD_INTERFACE_SPI_WITH_PSRAM)
    list(APPEND srcs "lcd/lcd_init_spi_with_psram.c")
elseif(CONFIG_EXAMPLE_LCD_INTERFACE_SPI_WITHOUT_PSRAM)
    list(APPEND srcs "lcd/lcd_init_spi_without_psram.c")
else()
    list(APPEND srcs "lcd/lcd_init_spi_with_psram.c")
endif()

set(include_dirs "." "lcd" "input" "helpers")

set(component_requires driver;esp_lcd;esp_lvgl_adapter)

if(CONFIG_EXAMPLE_LCD_INTERFACE_MIPI_DSI)
    list(APPEND component_requires esp_lcd_ek79007)
    list(APPEND component_requires espressif__esp_lcd_touch_gt911)
endif()

if(CONFIG_EXAMPLE_LCD_INTERFACE_QSPI)
    list(APPEND component_requires esp_lcd_st77916)
    list(APPEND component_requires espressif__esp_lcd_touch_cst816s)
endif()

if(CONFIG_EXAMPLE_LCD_INTERFACE_RGB)
    list(APPEND component_requires espressif__esp_lcd_touch_gt1151)
endif()

if(CONFIG_EXAMPLE_LCD_INTERFACE_SPI_WITH_PSRAM)
    list(APPEND component_requires espressif__esp_lcd_ili9341)
    list(APPEND component_requires espressif__esp_lcd_touch_gt911)
endif()

if(CONFIG_EXAMPLE_LCD_INTERFACE_SPI_WITHOUT_PSRAM)
    list(APPEND component_requires espressif__esp_lcd_gc9a01)
    if("espressif__knob" IN_LIST build_components)
        list(APPEND component_requires espressif__knob)
    elseif("knob" IN_LIST build_components)
        list(APPEND component_requires knob)
    endif()
    if("espressif__button" IN_LIST build_components)
        list(APPEND component_requires espressif__button)
    elseif("button" IN_LIST build_components)
        list(APPEND component_requires button)
    endif()
endif()

idf_component_register(
    SRCS ${srcs}
    INCLUDE_DIRS ${include_dirs}
    REQUIRES ${component_requires}
)

if(CONFIG_EXAMPLE_LCD_INTERFACE_MIPI_DSI)
    message(STATUS "hw_init: MIPI DSI + Touch")
elseif(CONFIG_EXAMPLE_LCD_INTERFACE_QSPI)
    message(STATUS "hw_init: QSPI + Touch")
elseif(CONFIG_EXAMPLE_LCD_INTERFACE_RGB)
    message(STATUS "hw_init: RGB + Touch")
elseif(CONFIG_EXAMPLE_LCD_INTERFACE_SPI_WITH_PSRAM)
    message(STATUS "hw_init: SPI (PSRAM) + Touch")
elseif(CONFIG_EXAMPLE_LCD_INTERFACE_SPI_WITHOUT_PSRAM)
    message(STATUS "hw_init: SPI (No PSRAM) + Encoder")
endif()
