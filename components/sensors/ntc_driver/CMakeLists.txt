idf_component_register(SRCS "ntc_driver.c"
                       REQUIRES esp_adc
                       INCLUDE_DIRS "include")

 include(package_manager)
 cu_pkg_define_version(${CMAKE_CURRENT_LIST_DIR})
