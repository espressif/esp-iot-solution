idf_component_register(SRC_DIRS "."
    INCLUDE_DIRS "include"
    PRIV_REQUIRES touch_sensor_fsm touch_sensor_lowlevel)

if(CONFIG_TOUCH_SLIDER_SENSOR_DEBUG)
    idf_component_get_property(touch_sensor_fsm_dir touch_sensor_fsm COMPONENT_DIR)
    target_include_directories(${COMPONENT_LIB} PRIVATE
                               "${touch_sensor_fsm_dir}/private_include")
endif()
