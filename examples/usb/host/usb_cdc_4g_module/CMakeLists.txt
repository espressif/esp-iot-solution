# The following lines of boilerplate have to be in your project's CMakeLists
# in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.5)

if(DEFINED ENV{IOT_SOLUTION_PATH})
    OPTION(DEBUG_LOCAL "Debug local component" ON)
else()
    OPTION(DEBUG_LOCAL "Debug local component" OFF) #idf.py -DDEBUG_LOCAL=ON set-target esp32s3
endif()

if(DEBUG_LOCAL)
if(NOT DEFINED ENV{IOT_SOLUTION_PATH})
get_filename_component(IOT_SOLUTION_PATH "${CMAKE_SOURCE_DIR}/../../../.." ABSOLUTE)
set(ENV{IOT_SOLUTION_PATH} ${IOT_SOLUTION_PATH})
message(WARNING "Can't detect IOT_SOLUTION_PATH in your environment, we infer it is $ENV{IOT_SOLUTION_PATH}")
endif()
message(STATUS "Build with local component from $ENV{IOT_SOLUTION_PATH}")
set(EXTRA_COMPONENT_DIRS $ENV{IOT_SOLUTION_PATH}/components/usb/iot_usbh
                        $ENV{IOT_SOLUTION_PATH}/components/usb/iot_usbh_cdc
                        $ENV{IOT_SOLUTION_PATH}/components/usb/iot_usbh_modem
                        $ENV{IOT_SOLUTION_PATH}/components/led/led_indicator
                        )
endif()

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(usb_cdc_4g_module)
