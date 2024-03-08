cmake_minimum_required(VERSION 3.16)

add_compile_options("-Wno-deprecated-declarations")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(lp_environment_sensor)
