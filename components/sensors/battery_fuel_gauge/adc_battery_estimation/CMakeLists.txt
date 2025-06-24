idf_component_register(SRCS "adc_battery_estimation.c"
                       INCLUDE_DIRS include
                       REQUIRES "esp_adc" "esp_timer")

include(package_manager)
cu_pkg_define_version(${CMAKE_CURRENT_LIST_DIR})
