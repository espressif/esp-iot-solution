
add_compile_options(-fdiagnostics-color=always)

set(EXTRA_COMPONENT_DIRS  ${EXTRA_COMPONENT_DIRS}
                        $ENV{IOT_SOLUTION_PATH}/components
                        $ENV{IOT_SOLUTION_PATH}/components/features
                        $ENV{IOT_SOLUTION_PATH}/components/general
                        $ENV{IOT_SOLUTION_PATH}/components/wifi
                        $ENV{IOT_SOLUTION_PATH}/components/spi_devices
                        $ENV{IOT_SOLUTION_PATH}/components/i2c_devices
                        $ENV{IOT_SOLUTION_PATH}/components/i2c_devices/sensor
                        $ENV{IOT_SOLUTION_PATH}/components/i2c_devices/others
                        $ENV{IOT_SOLUTION_PATH}/components/network
                        $ENV{IOT_SOLUTION_PATH}/components/platforms
                        $ENV{IOT_SOLUTION_PATH}/components/motor/stepper
                        $ENV{IOT_SOLUTION_PATH}/components/motor/servo
                        $ENV{IOT_SOLUTION_PATH}/components/framework
                        $ENV{IOT_SOLUTION_PATH}/components/i2s_devices
                        $ENV{IOT_SOLUTION_PATH}/components/hmi )
