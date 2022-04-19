# The following lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.5)

#If IOT_SOLUTION_PATH is not defined, use relative path as default value
if(NOT DEFINED ENV{IOT_SOLUTION_PATH})
    get_filename_component(IOT_SOLUTION_PATH "${CMAKE_SOURCE_DIR}/../../.." ABSOLUTE)
    set(ENV{IOT_SOLUTION_PATH} ${IOT_SOLUTION_PATH})
    message(WARNING "Can't detect IOT_SOLUTION_PATH in your environment, we infer it is $ENV{IOT_SOLUTION_PATH}")
endif()

list(APPEND EXTRA_COMPONENT_DIRS "$ENV{IOT_SOLUTION_PATH}/examples/camera/camera_components/camera_example_common" "$ENV{IOT_SOLUTION_PATH}/examples/camera/camera_components/esp32-camera")
include($ENV{IOT_SOLUTION_PATH}/component.cmake)
# add_compile_options(-fdiagnostics-color=always)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(camera_pic_server)