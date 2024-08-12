idf_component_register(SRCS "esp_lcd_hx8399.c"
                       INCLUDE_DIRS "include"
                       REQUIRES "esp_lcd"
                       PRIV_REQUIRES "esp_driver_gpio")

include(package_manager)
cu_pkg_define_version(${CMAKE_CURRENT_LIST_DIR})
