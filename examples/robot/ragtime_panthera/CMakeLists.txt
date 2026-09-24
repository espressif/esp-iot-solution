# The following five lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.16)

# project version
set(PROJECT_VER "0.3.0")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-deprecated-enum-enum-conversion -Wno-write-strings -Wno-cast-user-defined")

add_compile_options(
    -Wno-format
    -Wno-unused-variable
    -Wno-missing-field-initializers
    -Wno-unused-but-set-variable
    -Wno-cast-function-type
    -Wno-ignored-qualifiers
    -Wno-deprecated-declarations
)

project(ragtime_panthera)
