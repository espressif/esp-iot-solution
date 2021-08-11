
add_compile_options(-fdiagnostics-color=always)

list(APPEND EXTRA_COMPONENT_DIRS 
                                "$ENV{IOT_SOLUTION_PATH}/components"
                                "$ENV{IOT_SOLUTION_PATH}/components/audio"
                                "$ENV{IOT_SOLUTION_PATH}/examples/common_components"
                                "$ENV{IOT_SOLUTION_PATH}/components/bus"
                                "$ENV{IOT_SOLUTION_PATH}/components/button"
                                "$ENV{IOT_SOLUTION_PATH}/components/display"
                                "$ENV{IOT_SOLUTION_PATH}/components/display/digital_tube"
                                "$ENV{IOT_SOLUTION_PATH}/components/expander/io_expander"
                                "$ENV{IOT_SOLUTION_PATH}/components/gui"
                                "$ENV{IOT_SOLUTION_PATH}/components/led"
                                "$ENV{IOT_SOLUTION_PATH}/components/motor"
                                "$ENV{IOT_SOLUTION_PATH}/components/sensors"
                                "$ENV{IOT_SOLUTION_PATH}/components/sensors/gesture"
                                "$ENV{IOT_SOLUTION_PATH}/components/sensors/humiture"
                                "$ENV{IOT_SOLUTION_PATH}/components/sensors/imu"
                                "$ENV{IOT_SOLUTION_PATH}/components/sensors/light_sensor"
                                "$ENV{IOT_SOLUTION_PATH}/components/sensors/pressure"
                                "$ENV{IOT_SOLUTION_PATH}/components/storage"
                                "$ENV{IOT_SOLUTION_PATH}/components/storage/eeprom"
                                "$ENV{IOT_SOLUTION_PATH}/components/FreeRTOS-Plus-CLI"
                                )

