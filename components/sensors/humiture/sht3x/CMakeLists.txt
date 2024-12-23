idf_component_register(SRCS "sht3x.c"
                        INCLUDE_DIRS include
                        REQUIRES esp_timer)

if(CONFIG_SENSOR_INCLUDED_HUMITURE)
    target_link_libraries(${COMPONENT_LIB} INTERFACE "-u humiture_sht3x_init")
endif()

include(package_manager)
cu_pkg_define_version(${CMAKE_CURRENT_LIST_DIR})
