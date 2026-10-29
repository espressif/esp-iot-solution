idf_component_register(
    SRCS "bmm150.c" "common/bmm150_common.c"
    INCLUDE_DIRS "." "common"
    REQUIRES "i2c_bus" "esp_timer" "esp_driver_i2c"
)

target_compile_definitions(${COMPONENT_LIB} PUBLIC BMM150_USE_FLOATING_POINT)

# BMI270 AUX adapter is compiled only when the project already has bmi270_sensor.
# Direct I2C users (and CI test_apps) must not pull that component.
idf_build_get_property(build_components BUILD_COMPONENTS)
if("bmi270_sensor" IN_LIST build_components OR "espressif__bmi270_sensor" IN_LIST build_components)
    target_sources(${COMPONENT_LIB} PRIVATE "bmm150_aux_adapter.c")
    idf_component_optional_requires(PUBLIC bmi270_sensor espressif__bmi270_sensor)
endif()
