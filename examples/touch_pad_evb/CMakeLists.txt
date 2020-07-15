# The following lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.5)

set(EXTRA_COMPONENT_DIRS 
                         $ENV{IOT_SOLUTION_PATH}/components/features/touchpad
                         $ENV{IOT_SOLUTION_PATH}/components/general/led
                         $ENV{IOT_SOLUTION_PATH}/components/general/light
                         $ENV{IOT_SOLUTION_PATH}/components/i2c_devices/i2c_bus
                         $ENV{IOT_SOLUTION_PATH}/components/i2c_devices/others/ch450
                        )

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

project(touch_pad_evb)
