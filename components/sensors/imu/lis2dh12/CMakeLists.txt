idf_component_register(SRCS "lis2dh12.c"
                        INCLUDE_DIRS include)

if(CONFIG_SENSOR_INCLUDED_IMU)
    target_link_libraries(${COMPONENT_LIB} INTERFACE "-u imu_lis2dh12_init")
endif()

include(package_manager)
cu_pkg_define_version(${CMAKE_CURRENT_LIST_DIR})
