cmake_minimum_required(VERSION 3.22)

list(APPEND EXTRA_COMPONENT_DIRS
     "${CMAKE_CURRENT_LIST_DIR}/../components/esl_product_common"
     "${CMAKE_CURRENT_LIST_DIR}/../components/esl_image_manager"
     "${CMAKE_CURRENT_LIST_DIR}/../../../../../tools/cmake_utilities")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

set(BLE_ESL_SUPPORTED_TARGETS esp32h2 esp32h21 esp32h4 esp32c5 esp32c6 esp32c61 esp32s31)
if(NOT IDF_TARGET IN_LIST BLE_ESL_SUPPORTED_TARGETS)
    message(FATAL_ERROR
        "ble_esl_ap requires a PAwR-capable target (ESP32-H2/H21/H4, C5/C6/C61, S31); current: ${IDF_TARGET}")
endif()

project(ble_esl_ap)
