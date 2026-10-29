cmake_minimum_required(VERSION 3.16)

add_compile_options(-fdiagnostics-color=always)

set(ENV{IDF_COMPONENT_MANAGER} "0")
set(EXTRA_COMPONENT_DIRS
    "${CMAKE_CURRENT_LIST_DIR}/../../../components/esp_flux"
    "${CMAKE_CURRENT_LIST_DIR}/../../../components/esp_gmp"
    "${CMAKE_CURRENT_LIST_DIR}/../../esp_flux/common_components/ble_session_manager"
    "${CMAKE_CURRENT_LIST_DIR}/../../../tools/cmake_utilities"
)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(esp_file_transfer_receiver_demo)
