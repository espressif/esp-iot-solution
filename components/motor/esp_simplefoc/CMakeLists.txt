set(SRC_FILES
    "port/esp/esp_hal_misc.cpp"
    "port/esp/esp_hal_serial.cpp"
    "port/esp/esp_hal_gpio.cpp"
    "port/esp/esp_hal_mcpwm.cpp"
    "port/esp/esp_hal_adc.cpp"
    "port/esp/esp_fast_math.cpp"
    "port/esp/esp_bldc_3pwm.cpp"
    "port/arduino/Print.cpp"
    "port/arduino/Stream.cpp"
    "port/angle_sensor/sensor_as5600.c")

set(INC_FILES "port/arduino/include/" "port/esp/include/" "port/angle_sensor")

set(IGNORE_FILES "port/esp/esp_hal_adc.cpp" "port/esp/esp_hal_mcpwm.cpp"
                 "port/esp/esp_bldc_3pwm.cpp")

idf_component_register(
  SRCS
  ${SRC_FILES}
  INCLUDE_DIRS
  ${INC_FILES}
  REQUIRES
  driver
  esp_timer
  esp_adc)

set_source_files_properties(
  ${IGNORE_FILES}
  PROPERTIES
    COMPILE_OPTIONS
    "-Wno-volatile;-Wno-deprecated-enum-float-conversion;-Wno-missing-field-initializers"
)

idf_component_get_property(arduino-foc_lib espressif__arduino-foc COMPONENT_LIB)
cmake_policy(SET CMP0079 NEW)
target_link_libraries(${arduino-foc_lib} PUBLIC ${COMPONENT_LIB})
