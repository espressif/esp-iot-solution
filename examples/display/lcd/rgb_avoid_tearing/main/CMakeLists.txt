idf_component_register(
    SRCS "example_rgb_avoid_tearing.c"
    INCLUDE_DIRS "."
    REQUIRES esp_lcd)

idf_component_get_property(lvgl_lib lvgl__lvgl COMPONENT_LIB)
target_compile_options(${lvgl_lib} PRIVATE -Wno-format)
