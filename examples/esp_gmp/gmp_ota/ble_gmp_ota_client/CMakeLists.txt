cmake_minimum_required(VERSION 3.16)

add_compile_options(-fdiagnostics-color=always)

set(EXTRA_COMPONENT_DIRS "${CMAKE_CURRENT_LIST_DIR}/../common_components")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(ble_gmp_ota_client)
