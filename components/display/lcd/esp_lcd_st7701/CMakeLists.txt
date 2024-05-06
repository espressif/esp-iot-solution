idf_component_register(SRCS "esp_lcd_st7701.c" "esp_lcd_st7701_rgb.c" "esp_lcd_st7701_mipi.c"
                       INCLUDE_DIRS "include"
                       PRIV_INCLUDE_DIRS "priv_include"
                       REQUIRES "esp_lcd" "driver")

include(package_manager)
cu_pkg_define_version(${CMAKE_CURRENT_LIST_DIR})
