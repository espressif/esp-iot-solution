# The following lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.5)

#If IOT_SOLUTION_PATH is not defined, use relative path as default value
set(IOT_SOLUTION_PATH "$ENV{IOT_SOLUTION_PATH}")
if(NOT IOT_SOLUTION_PATH)
    # Documentation says you should set IOT_SOLUTION_PATH in your environment, but we
    # can infer it relative to PROJECT_PATH directory if it's not set.
    get_filename_component(IOT_SOLUTION_PATH "${PROJECT_PATH}/../.." ABSOLUTE)
endif()
file(TO_CMAKE_PATH "${IOT_SOLUTION_PATH}" IOT_SOLUTION_PATH)
set(ENV{IOT_SOLUTION_PATH} ${IOT_SOLUTION_PATH})

# set(COMPONENT_PRIV_INCLUDEDIRS ${PROJECT_PATH}/abstract ${PROJECT_PATH}/products)

include($ENV{IOT_SOLUTION_PATH}/CMakeLists.txt)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)

project(touch_pad_evb)
