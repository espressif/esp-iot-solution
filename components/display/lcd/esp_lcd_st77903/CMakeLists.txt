idf_component_register(
    SRCS "esp_lcd_st77903.c"  "st77903_interface_qspi.c"  "st77903_interface_rgb.c"
    INCLUDE_DIRS "include"
    PRIV_INCLUDE_DIRS "priv_include"
    PRIV_REQUIRES esp_timer esp_psram
    REQUIRES driver esp_lcd
)

include(package_manager)
cu_pkg_define_version(${CMAKE_CURRENT_LIST_DIR})
