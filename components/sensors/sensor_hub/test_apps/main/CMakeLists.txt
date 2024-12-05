idf_component_register(SRC_DIRS "."
                       PRIV_INCLUDE_DIRS "."
                       PRIV_REQUIRES unity test_utils sensor_hub)

if(CONFIG_SENSOR_INCLUDED_HUMITURE)
    target_link_libraries(${COMPONENT_LIB} INTERFACE "-u virtual_humiture_init")
endif()

if(CONFIG_SENSOR_INCLUDED_IMU)
    target_link_libraries(${COMPONENT_LIB} INTERFACE "-u virtual_imu_init")
endif()

if(CONFIG_SENSOR_INCLUDED_LIGHT)
    target_link_libraries(${COMPONENT_LIB} INTERFACE "-u virtual_light_init")
endif()
