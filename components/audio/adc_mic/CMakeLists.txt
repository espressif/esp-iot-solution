idf_component_register(SRCS "adc_mic.c"
                       INCLUDE_DIRS "."
                       REQUIRES "esp_adc")

include(package_manager)
cu_pkg_define_version(${CMAKE_CURRENT_LIST_DIR})
