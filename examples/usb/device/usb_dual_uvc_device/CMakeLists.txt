# For more information about build system see
# https://docs.espressif.com/projects/esp-idf/en/latest/api-guides/build-system.html
# The following five lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.16)

#download .avi file from server if no file exit
set(URL "https://dl.espressif.com/esp-iot-solution/static/p4_introduce_480_270_mjpeg_pcm16.avi")
set(OUTPUT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/spiffs")
file(MAKE_DIRECTORY ${OUTPUT_DIR})

file(GLOB AVI_FILES "${OUTPUT_DIR}/*.avi")
if(NOT AVI_FILES)
    file(DOWNLOAD ${URL} ${OUTPUT_DIR}/p4_introduce.avi)
    message(STATUS "File downloaded to: ${OUTPUT_DIR}/p4_introduce.avi")
else()
    message(STATUS "AVI file(s) already exist(s) in the output directory: ${OUTPUT_DIR}")
endif()


add_compile_options(-Wno-maybe-uninitialized)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(usb_dual_uvc_device)
