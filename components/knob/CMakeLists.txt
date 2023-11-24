idf_component_register(SRCS "iot_knob.c" 
                       INCLUDE_DIRS "."
                       REQUIRES driver
                       PRIV_REQUIRES esp_timer)

include(package_manager)
cu_pkg_define_version(${CMAKE_CURRENT_LIST_DIR})