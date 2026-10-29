cmake_minimum_required(VERSION 3.16)

# Enable colored compilation output
add_compile_options(-fdiagnostics-color=always)

# Set ESP-IDF project
include($ENV{IDF_PATH}/tools/cmake/project.cmake)

# Project definition
project(ble_spp_server)
