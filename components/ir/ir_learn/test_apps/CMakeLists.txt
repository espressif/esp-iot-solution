# The following lines of boilerplate have to be in your project's CMakeLists
# in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.5)

if(EXISTS "$ENV{IDF_PATH}/tools/test_apps/components/test_utils")
    set(IDF_TEST_COMPONENTS "$ENV{IDF_PATH}/tools/test_apps/components")
else()
    set(IDF_TEST_COMPONENTS "$ENV{IDF_PATH}/tools/unit-test-app/components")
endif()

set(EXTRA_COMPONENT_DIRS "${IDF_TEST_COMPONENTS}" "../../ir_learn")
include($ENV{IDF_PATH}/tools/cmake/project.cmake)

project(ir_learn_test)
