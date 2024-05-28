idf_component_register(SRCS "touch_proximity_sensor.c"
                    INCLUDE_DIRS "include"
                    PRIV_REQUIRES driver esp_timer)

include(package_manager)
cu_pkg_define_version(${CMAKE_CURRENT_LIST_DIR})
