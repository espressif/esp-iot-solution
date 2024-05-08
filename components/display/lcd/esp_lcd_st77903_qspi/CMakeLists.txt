idf_component_register(
    SRCS "esp_lcd_st77903_qspi.c"
    INCLUDE_DIRS "include"
    PRIV_REQUIRES esp_timer esp_psram
    REQUIRES driver esp_lcd
)

include(package_manager)
cu_pkg_define_version(${CMAKE_CURRENT_LIST_DIR})
