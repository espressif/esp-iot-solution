idf_component_register(SRCS "sht3x.c"
                        INCLUDE_DIRS include
                        REQUIRES esp_timer)

include(package_manager)
cu_pkg_define_version(${CMAKE_CURRENT_LIST_DIR})
