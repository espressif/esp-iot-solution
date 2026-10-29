set(srcs
    "main.c"
    "hw_oled.c"
    "font_puhui_14_1.c"
)
set(include_dirs ".")

idf_component_register(
    SRCS ${srcs}
    INCLUDE_DIRS ${include_dirs}
)
