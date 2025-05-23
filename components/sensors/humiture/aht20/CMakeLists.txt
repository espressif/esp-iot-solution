idf_component_register(SRCS "aht20.c"
                    INCLUDE_DIRS "include")


if(CONFIG_SENSOR_INCLUDED_HUMITURE)
    target_link_libraries(${COMPONENT_LIB} INTERFACE "-u humiture_aht20_init")
endif()

include(package_manager)
cu_pkg_define_version(${CMAKE_CURRENT_LIST_DIR})
