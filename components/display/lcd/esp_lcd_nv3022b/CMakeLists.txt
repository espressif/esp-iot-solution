idf_component_register(SRCS "esp_lcd_nv3022b.c"
                       INCLUDE_DIRS "include"
                       REQUIRES "esp_lcd"
                       PRIV_REQUIRES "driver")

include(package_manager)
cu_pkg_define_version(${CMAKE_CURRENT_LIST_DIR})
