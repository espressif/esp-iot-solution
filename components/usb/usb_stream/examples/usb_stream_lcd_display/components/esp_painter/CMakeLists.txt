file(GLOB_RECURSE FONT_SRCS font/*.c)

idf_component_register(
    SRCS
        ${FONT_SRCS}
        "esp_painter.c"
    INCLUDE_DIRS
        "include"
    REQUIRES
        esp_lcd
)
