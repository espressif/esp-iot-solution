file(GLOB SOURCES
    "*.c"
    "app/*.c"
    "hal/*.c")
set(INC_DIRS "app/include/" "hal/include/")
idf_component_register(SRCS ${SRC_FILES} ${SOURCES}
                    INCLUDE_DIRS "." ${INC_DIRS}
                    )
idf_component_get_property(bldc_lib esp_sensorless_bldc_control COMPONENT_LIB)
cmake_policy(SET CMP0079 NEW)
target_link_libraries(${bldc_lib} PUBLIC ${COMPONENT_LIB})
