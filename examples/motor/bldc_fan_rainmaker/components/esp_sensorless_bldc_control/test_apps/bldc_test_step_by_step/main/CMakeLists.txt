idf_component_register(SRC_DIRS "."
                       INCLUDE_DIRS "."
                       WHOLE_ARCHIVE)

idf_component_get_property(bldc_lib esp_sensorless_bldc_control COMPONENT_LIB)
cmake_policy(SET CMP0079 NEW)
target_link_libraries(${bldc_lib} PUBLIC ${COMPONENT_LIB})
