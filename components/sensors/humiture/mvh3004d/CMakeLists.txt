idf_component_register(SRCS "mvh3004d.c"
                        INCLUDE_DIRS include)

if(CONFIG_SENSOR_INCLUDED_HUMITURE)
    target_link_libraries(${COMPONENT_LIB} INTERFACE "-u humiture_mvh3004d_init")
endif()

include(package_manager)
cu_pkg_define_version(${CMAKE_CURRENT_LIST_DIR})
