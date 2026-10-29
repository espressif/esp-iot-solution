# The following lines of boilerplate have to be in your project's CMakeLists
# in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.5)

if(EXISTS "$ENV{IDF_PATH}/tools/test_apps/components/test_utils")
    set(EXTRA_COMPONENT_DIRS "$ENV{IDF_PATH}/tools/test_apps/components")
elseif(EXISTS "$ENV{IDF_PATH}/tools/unit-test-app/components/test_utils")
    set(EXTRA_COMPONENT_DIRS "$ENV{IDF_PATH}/tools/unit-test-app/components")
endif()

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

set(COMPONENTS main)
project(i2c_bus_test)
