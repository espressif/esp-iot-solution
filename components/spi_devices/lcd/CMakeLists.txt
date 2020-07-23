
set(srcs "adaptation.cpp"
            "font7s.c"
            "glcdfont.c"
            "iot_lcd.cpp"
            "spi_lcd.c"
            "Adafruit-GFX-Library/Adafruit_GFX.cpp"
            # "Adafruit-GFX-Library/Adafruit_SPITFT.cpp"
            "Adafruit-GFX-Library/glcdfont.c")

set(incs "."
            "include"
            "Adafruit-GFX-Library/Fonts"
            "Adafruit-GFX-Library")

idf_component_register(SRCS "${srcs}"
                        INCLUDE_DIRS "${incs}"
                        PRIV_REQUIRES  spi_flash)

add_compile_options(-D__AVR_ATtiny85__)
