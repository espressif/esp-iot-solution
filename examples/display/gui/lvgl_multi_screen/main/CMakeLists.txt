set(srcs
    "main.c"
    "hw_multi.c"
    "../../common/hw_init/helpers/lcd_orientation_helper.c"
)
set(include_dirs
    "."
    "../../common/hw_init/helpers"
)

idf_component_register(
    SRCS ${srcs}
    INCLUDE_DIRS ${include_dirs}
    REQUIRES esp_lcd
)
