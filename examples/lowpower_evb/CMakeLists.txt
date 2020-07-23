# The following lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.5)

#If IOT_SOLUTION_PATH is not defined, use relative path as default value
if(NOT DEFINED ENV{IOT_SOLUTION_PATH})
    get_filename_component(IOT_SOLUTION_PATH "${CMAKE_SOURCE_DIR}/../.." ABSOLUTE)
    set(ENV{IOT_SOLUTION_PATH} ${IOT_SOLUTION_PATH})
    message(WARNING "Can't detect IOT_SOLUTION_PATH in your environment, we infer it is $ENV{IOT_SOLUTION_PATH}")
endif()

set(EXTRA_COMPONENT_DIRS 
                         $ENV{IOT_SOLUTION_PATH}/components/wifi/wifi_conn
                         $ENV{IOT_SOLUTION_PATH}/components/wifi/smart_config
                         $ENV{IOT_SOLUTION_PATH}/components/network/tcp
                         $ENV{IOT_SOLUTION_PATH}/components/general/led
                         $ENV{IOT_SOLUTION_PATH}/components/general/button
                         $ENV{IOT_SOLUTION_PATH}/components/general/param
                         $ENV{IOT_SOLUTION_PATH}/components/spi_devices/epaper
                         $ENV{IOT_SOLUTION_PATH}/components/i2c_devices/i2c_bus
                         $ENV{IOT_SOLUTION_PATH}/components/i2c_devices/sensor/hts221
                         $ENV{IOT_SOLUTION_PATH}/components/i2c_devices/sensor/bh1750
                         $ENV{IOT_SOLUTION_PATH}/components/framework/lowpower_framework
                        )

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

project(lowpower_evb_demo)
