# The following five lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.16)

# ensure the spiffs directory exists
file(MAKE_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/spiffs)

# project version
set(PROJECT_VER "0.1.0")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

list(APPEND CMAKE_CXX_FLAGS
    "-Wno-deprecated-enum-enum-conversion"
    "-Wno-write-strings"
    "-Wno-cast-user-defined"
)

add_compile_options(
    -Wno-format
    -Wno-unused-variable
    -Wno-missing-field-initializers
    -Wno-unused-but-set-variable
    -Wno-cast-function-type
    -Wno-ignored-qualifiers
)

project(ragtime_panthera)

if(NOT DEFINED ENV{SKIP_RECEIVER_BUILD})
    message(STATUS "building receiver firmware")
    add_custom_command(
        OUTPUT ${CMAKE_CURRENT_SOURCE_DIR}/spiffs/merged-binary.bin
        COMMAND ${CMAKE_COMMAND} -E remove -f ${CMAKE_CURRENT_SOURCE_DIR}/spiffs/merged-binary.bin
        COMMAND bash -c "cd ${CMAKE_CURRENT_SOURCE_DIR}/receiver && idf.py set-target esp32c6 && idf.py merge-bin"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_CURRENT_SOURCE_DIR}/receiver/build/merged-binary.bin ${CMAKE_CURRENT_SOURCE_DIR}/spiffs/merged-binary.bin
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        COMMENT "Building receiver with target esp32c6 and copying merged-binary.bin to spiffs"
        VERBATIM
    )

    add_custom_target(build_receiver ALL
        DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/spiffs/merged-binary.bin
    )
else()
    message(STATUS "SKIP_RECEIVER_BUILD is set; skipping receiver firmware build")
endif()
