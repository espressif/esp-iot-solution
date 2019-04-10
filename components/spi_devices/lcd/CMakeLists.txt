
# componet standalone mode
if(NOT CONFIG_IOT_SOLUTION_EMBED)
    set(COMPONENT_SRCS "adaptation.cpp"
                        "font7s.c"
                        "glcdfont.c"
                        "iot_lcd.cpp"
                        "spi_lcd.c"
                        "Adafruit-GFX-Library/Adafruit_GFX.cpp"
                        # "Adafruit-GFX-Library/Adafruit_SPITFT.cpp"
                        "Adafruit-GFX-Library/glcdfont.c")

    set(COMPONENT_ADD_INCLUDEDIRS "."
                                    "include"
                                    "Adafruit-GFX-Library/Fonts"
                                    "Adafruit-GFX-Library")
else()
    if(CONFIG_IOT_LCD_ENABLE)
        set(COMPONENT_SRCS "adaptation.cpp"
                            "font7s.c"
                            "glcdfont.c"
                            "iot_lcd.cpp"
                            "spi_lcd.c"
                            "Adafruit-GFX-Library/Adafruit_GFX.cpp"
                            # "Adafruit-GFX-Library/Adafruit_SPITFT.cpp"
                            "Adafruit-GFX-Library/glcdfont.c")

        set(COMPONENT_ADD_INCLUDEDIRS "."
                                        "include"
                                        "Adafruit-GFX-Library/Fonts"
                                        "Adafruit-GFX-Library")
    else()
        set(COMPONENT_SRCS "")
        set(COMPONENT_ADD_INCLUDEDIRS "")
        message(STATUS "Building empty lcd component due to configuration")
    endif()
endif()

# requirements can't depend on config
set(COMPONENT_REQUIRES spi_flash)

register_component()

add_compile_options(-D__AVR_ATtiny85__)
