cmake_minimum_required(VERSION 3.5)

list(APPEND EXTRA_COMPONENT_DIRS "$ENV{IDF_PATH}/examples/bluetooth/common/ble_uart")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(emote_ble_uart)
