idf_component_register(SRCS "veml6075.c"
                        INCLUDE_DIRS include)

if(CONFIG_SENSOR_INCLUDED_LIGHT)
    target_link_libraries(${COMPONENT_LIB} INTERFACE "-u light_sensor_veml6075_init")
endif()

include(package_manager)
cu_pkg_define_version(${CMAKE_CURRENT_LIST_DIR})
