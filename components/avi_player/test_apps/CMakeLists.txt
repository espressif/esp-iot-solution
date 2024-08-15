
# The following lines of boilerplate have to be in your project's CMakeLists
# in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.5)

set(EXTRA_COMPONENT_DIRS "$ENV{IDF_PATH}/tools/unit-test-app/components"
                         "../../avi_player")
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(avi_player_test)

set(URL "https://dl.espressif.com/AE/esp-iot-solution/p4_introduce_480_270_mjpeg_48000_2_1ch.avi")
set(OUTPUT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/spiffs")
file(MAKE_DIRECTORY ${OUTPUT_DIR})
file(GLOB AVI_FILES "${OUTPUT_DIR}/*.avi")
if(NOT AVI_FILES)
    file(DOWNLOAD ${URL} ${OUTPUT_DIR}/p4_introduce.avi)
    message(STATUS "File downloaded from ${URL} to: ${OUTPUT_DIR}/p4_introduce.avi")
else()
    message(STATUS "AVI file(s) already exist(s) in the output directory: ${OUTPUT_DIR}")
endif()
