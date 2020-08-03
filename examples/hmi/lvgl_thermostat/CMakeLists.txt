# The following lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.5)

#If IOT_SOLUTION_PATH is not defined, use relative path as default value
if(NOT DEFINED ENV{IOT_SOLUTION_PATH})
    get_filename_component(IOT_SOLUTION_PATH "${CMAKE_SOURCE_DIR}/../../.." ABSOLUTE)
    set(ENV{IOT_SOLUTION_PATH} ${IOT_SOLUTION_PATH})
    message(WARNING "Can't detect IOT_SOLUTION_PATH in your environment, we infer it is $ENV{IOT_SOLUTION_PATH}")
endif()

set(EXTRA_COMPONENT_DIRS 
                        $ENV{IOT_SOLUTION_PATH}/components
                        $ENV{IOT_SOLUTION_PATH}/components/general/param
                        $ENV{IOT_SOLUTION_PATH}/components/spi_devices
                        $ENV{IOT_SOLUTION_PATH}/components/hmi/lvgl_gui
                        $ENV{IOT_SOLUTION_PATH}/components/hmi/ugfx_gui
                        $ENV{IOT_SOLUTION_PATH}/components/hmi/gdrivers
                        $ENV{IOT_SOLUTION_PATH}/components/i2c_devices/i2c_bus
                        $ENV{IOT_SOLUTION_PATH}/components/i2c_devices/others/ssd1306
                        $ENV{IOT_SOLUTION_PATH}/components/i2s_devices
                        $ENV{IOT_SOLUTION_PATH}/components/features/touchpad
                        $ENV{IOT_SOLUTION_PATH}/components/i2c_devices/sensor/ft5x06
                        $ENV{IOT_SOLUTION_PATH}/components/spi_devices/lcd
                        $ENV{IOT_SOLUTION_PATH}/components/spi_devices/xpt2046
                        )

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

project(lvgl_thermostat)
