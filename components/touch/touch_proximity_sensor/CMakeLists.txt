idf_component_register(SRCS "touch_proximity_sensor.c"
                    INCLUDE_DIRS "include"
                    PRIV_REQUIRES driver esp_timer touch_sensor_fsm touch_sensor_lowlevel)

include(package_manager)
cu_pkg_define_version(${CMAKE_CURRENT_LIST_DIR})

if(CONFIG_TOUCH_PROXIMITY_SENSOR_DEBUG)
    idf_component_get_property(touch_sensor_fsm_dir touch_sensor_fsm COMPONENT_DIR)
    target_include_directories(${COMPONENT_LIB} PRIVATE
                               "${touch_sensor_fsm_dir}/private_include")
endif()
