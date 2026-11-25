set(hw_init_dir "hw_init")
if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${hw_init_dir}")
    set(hw_init_dir "../../common/hw_init")
endif()

set(srcs
    "main.c"
    "hw_multi.c"
    "${hw_init_dir}/helpers/lcd_orientation_helper.c"
)
set(include_dirs
    "."
    "${hw_init_dir}"
    "${hw_init_dir}/helpers"
)

idf_component_register(
    SRCS ${srcs}
    INCLUDE_DIRS ${include_dirs}
    REQUIRES esp_lcd
)
