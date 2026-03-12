# The following lines of boilerplate have to be in your project's CMakeLists
# in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.5)

# Add parent dir so as5600 component is found when building from test_apps.
# Support both IDF layout: test_utils in tools/test_apps/components (new) or tools/unit-test-app/components (old)
if(EXISTS "$ENV{IDF_PATH}/tools/test_apps/components")
    set(EXTRA_COMPONENT_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/.." "$ENV{IDF_PATH}/tools/test_apps/components")
else()
    set(EXTRA_COMPONENT_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/.." "$ENV{IDF_PATH}/tools/unit-test-app/components")
endif()
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
idf_build_set_property(MINIMAL_BUILD ON)
project(as5600_test)
